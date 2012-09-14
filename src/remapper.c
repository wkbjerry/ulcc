/*
 * Copyright (c) 2012, Xiaoning Ding, Kaibo Wang, Xiaodong Zhang
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define _GNU_SOURCE
#include <sys/mman.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "remapper.h"
#include "translator.h"
//#include "stdio.h"
//#include <sys/time.h>

/* A helper structure for picking pages in desired colors */
struct _page_picker_s
{
	int		picked;		/* Number of pages already picked */
	int		needed;		/* Max number of pages needed in this color */
	void	**pages;	/* Container of pages already picked */
};

/* A temporary list of virtual memory spaces, used to store intermediate
 * malloc-ed memory regions during page picking. */
#define VM_LIST_NODE_SIZE	512
struct _vm_list_node_s
{
	int		count;		/* Current number of vm regions in this node */
	int		max;		/* Max number of virtual spaces in this node */
	void	**mem;		/* The list of vm regions */
	size_t	*size;		/* Size of each vm region */
	struct _vm_list_node_s *next;
};

struct _vm_list_s
{
	struct _vm_list_node_s	*head;
};

int _ULCC_HIDDEN _nr_aligned_pages(const unsigned long *start, const unsigned long *end, const int n)
{
	int nr_pages = 0;
	int i, delta;

	for(i = 0; i < n; i++) {
		delta = (ULCC_ALIGN_LOWER(end[i]) - ULCC_ALIGN_HIGHER(start[i])) / ULCC_PAGE_BYTES;
		nr_pages += delta < 0 ? 0 : delta;
	}

	return nr_pages;
}

/* Note: if no colors are set in cacheregn, then all pages will be considered
 * to need remapping */
int _ULCC_HIDDEN _pages_to_be_remapped(
		const unsigned long *start,
		const unsigned long *end,
		const int n,
		const cc_cacheregn_t *regn,
		char *do_remap)
{
	int nr_pages = 0, c_pages;
	cc_uint64_t *pfnbuf = NULL;
	char *colors_inc = NULL;
	int i, j;

	/* Get the colors specified in the cache region */
	colors_inc = malloc(sizeof(*colors_inc) * ULCC_NR_CACHE_COLORS);
	if(!colors_inc)
		return -1;
	for(i = 0; i < ULCC_NR_CACHE_COLORS; i++)
		if(ULCC_TST_COLOR_BIT(regn, i))
			colors_inc[i] = 1;
		else
			colors_inc[i] = 0;

	/* Allocate page frame number buffer for address translation  */
	c_pages = 0;
	for(i = 0; i < n; i++) {
		nr_pages = (ULCC_ALIGN_LOWER(end[i]) - ULCC_ALIGN_HIGHER(start[i])) / ULCC_PAGE_BYTES;
		if(nr_pages > c_pages)
			c_pages = nr_pages;
	}
	pfnbuf = malloc(sizeof(*pfnbuf) * c_pages);
	if(!pfnbuf) {
		_ULCC_PERROR("malloc for pfnbuf failed in _pages_to_be_remapped");
		free(colors_inc);
		return -1;
	}

	/* Pages unpresent or whose colors are not included in desired colors need
	 * to be remapped */
	c_pages = 0; /* c_pages will equal the number of pages to be remapped */
	for(i = 0; i < n; i++) {
		nr_pages = (ULCC_ALIGN_LOWER(end[i]) - ULCC_ALIGN_HIGHER(start[i])) / ULCC_PAGE_BYTES;
		if(nr_pages <= 0)
			continue;

		if(cc_addr_translate(pfnbuf, ULCC_ALIGN_HIGHER(start[i]), nr_pages) < 0) {
			free(pfnbuf);
			free(colors_inc);
			return -1;
		}

		for(j = 0; j < nr_pages; j++) {
			if(cc_pfn_present(pfnbuf[j]) && colors_inc[cc_pfn_color(pfnbuf[j])])
				do_remap[j] = 0; /* need no remapping */
			else {
				do_remap[j] = 1; /* need remapping */
				c_pages++;
			}
		}

		do_remap += nr_pages;
	}

	free(pfnbuf);
	free(colors_inc);
	return c_pages;
}

struct _vm_list_node_s _ULCC_HIDDEN *_vm_list_node_new()
{
	struct _vm_list_node_s *p;

	p = malloc(sizeof(*p));
	if(!p)
		return NULL;

	p->mem = malloc(sizeof(void *) * VM_LIST_NODE_SIZE);
	if(!p->mem) {
		free(p);
		_ULCC_PERROR("malloc failed for mem in _vm_list_node_new()");
		return NULL;
	}

	p->size = malloc(sizeof(size_t) * VM_LIST_NODE_SIZE);
	if(!p->size) {
		free(p->mem);
		free(p);
		_ULCC_PERROR("malloc failed for size in _vm_list_node_new()");
		return NULL;
	}

	p->count = 0;
	p->max = VM_LIST_NODE_SIZE;
	p->next = NULL;

	return p;
}

void _ULCC_HIDDEN _vm_list_node_free(struct _vm_list_node_s *node)
{
	int i;

	for(i = 0; i < node->count; i++)
		cc_free_aligned(node->mem[i], node->size[i]);
	free(node->size);
	free(node->mem);
	free(node);
}

struct _vm_list_s _ULCC_HIDDEN *_vm_list_new()
{
	struct _vm_list_s *p;

	p = malloc(sizeof(*p));
	if(!p)
		return NULL;

	p->head = _vm_list_node_new();
	if(!p->head) {
		free(p);
		return NULL;
	}

	return p;
}

int _ULCC_HIDDEN _vm_list_add(struct _vm_list_s *list, void *m, size_t size)
{
	int ret = 0;

	if(list->head->count < list->head->max) {
		list->head->mem[list->head->count] = m;
		list->head->size[list->head->count++] = size;
	}
	else {
		struct _vm_list_node_s *p = _vm_list_node_new();
		if(!p)
			ret = -1;
		else {
			p->next = list->head;
			p->mem[p->count] = m;
			p->size[p->count++] = size;
			list->head = p;
		}
	}

	return ret;
}

void _ULCC_HIDDEN _vm_list_free(struct _vm_list_s *list)
{
	struct _vm_list_node_s *p, *q;

	p = list->head;
	while(p) {
		q = p->next;
		_vm_list_node_free(p);
		p = q;
	}

	free(list);
}

/* Create and initialize an array of page picker structures */
struct _page_picker_s _ULCC_HIDDEN *_page_picker_new(
		const int c_pages,
		const int c_pages_per_color,
		const cc_cacheregn_t *regn,
		const int maporder)
{
	struct _page_picker_s *p;
	int i;

	p = malloc(sizeof(*p) * ULCC_NR_CACHE_COLORS);
	if(!p)
		return NULL;

	/* Initialize page picker structures */
	for(i = 0; i < ULCC_NR_CACHE_COLORS; i++) {
		p[i].picked = 0;

		/* For CC_MAPORDER_ARB, max indicates whether this color is
		 * requested; otherwise, max is the maximum	number of pages needed
		 * in this color. */
		if(ULCC_TST_COLOR_BIT(regn, i)) {
			p[i].needed = c_pages_per_color;

			/* For CC_MAPORDER_ARB, only p[0].pages is needed, so set pages to
			 * NULL here temporarily; otherwise, p[i].pages is the container
			 * for all pages to be picked up in this color. */
			if(maporder == CC_MAPORDER_ARB)
				p[i].pages = NULL;
			else {
				p[i].pages = malloc(sizeof(void *) * p[i].needed);
				if(!p[i].pages)
					break;
			}
		}
		else {
			p[i].needed = 0;
			p[i].pages = NULL;
		}
	}

	/* If initialization for non-arb map orders failed, delete all previous
	 * allocations */
	if(i < ULCC_NR_CACHE_COLORS) {
		while(i >= 0) {
			if(p[i].pages)
				free(p[i].pages);
			i--;
		}

		free(p);
		p = NULL;
	}
	/* Otherwise, set up page pickup container for CC_MAPORDER_ARB */
	else if(maporder == CC_MAPORDER_ARB) {
		p[0].pages = malloc(sizeof(void *) * c_pages);
		if(!p[0].pages) {
			free(p);
			p = NULL;
		}
	}

	return p;
}

/* Destroy a page picker structure */
void _ULCC_HIDDEN _page_picker_free(struct _page_picker_s *picker)
{
	int i;

	for(i = 0; i < ULCC_NR_CACHE_COLORS; i++)
		if(picker[i].pages)
			free(picker[i].pages);

	free(picker);
}

#define MAX_PAGES_PER_LOOP		4096
#define MIN_PAGES_PER_LOOP		256

/* FIXME: there must be a bette way! */
int _ULCC_HIDDEN max_pick_loops(const int c_pages, const int c_colors)
{
	return (c_pages * ULCC_NR_CACHE_COLORS / (MIN_PAGES_PER_LOOP/* * c_colors*/) + 10240);
}

int _ULCC_HIDDEN pages_next_loop(const int pages_left)
{
	int c_pages;

	c_pages = ULCC_MIN(pages_left, MAX_PAGES_PER_LOOP);
	c_pages = ULCC_MAX(c_pages, MIN_PAGES_PER_LOOP);

	return c_pages;
}

int _ULCC_HIDDEN _pick_pages(
		struct _page_picker_s *picker,
		struct _vm_list_s *vml,
		const int c_pages,
		const int c_colors,
		const int c_pages_per_color,
		const int maporder)
{
	int c_pages_picked = 0, c_pages_this_loop;
	int i, i_loop, n_max_loop;
	cc_uint64_t *pfnbuf = NULL;
	void *mem;
	int pfcolor, ret = 0;

	pfnbuf = malloc(sizeof(*pfnbuf) * MAX_PAGES_PER_LOOP);
	if(!pfnbuf) {
		_ULCC_PERROR("failed to allocate memory for pfnbuf in _pick_pages\n");
		ret = -1;
		goto finish;
	}

	i_loop = 0;
	n_max_loop = max_pick_loops(c_pages, c_colors);

	/* Begin to allocate and pick pages */
	while(c_pages_picked < c_pages && i_loop < n_max_loop) {
		c_pages_this_loop = pages_next_loop(c_pages - c_pages_picked);

		/* Allocate virtual memory and enforce physical page allocation by
		 * writing to virtual pages */
		mem = cc_malloc_aligned(ULCC_PAGE_BYTES * c_pages_this_loop);
		if(!mem) {
			ret = -1;
			goto finish;
		}
		for(i = 0; i < c_pages_this_loop; i++)
			*(char *)(mem + i * ULCC_PAGE_BYTES) = 'x';
			/* mem_lock() ?? Without mem_lock, pages allocated may be swapped
			 * out in the case of memory pressure, but locking may cause
			 * memory pressure at the same time. */

		if(_vm_list_add(vml, mem, ULCC_PAGE_BYTES * c_pages_this_loop) < 0) {
			/* not added to the list; so free it here */
			cc_free_aligned(mem, ULCC_PAGE_BYTES * c_pages_this_loop);
			ret = -1;
			goto finish;
		}

		if(cc_addr_translate(pfnbuf, (unsigned long)mem, c_pages_this_loop) < 0) {
			ret = -1;
			goto finish;
		}

		/* Check and pick out pages in desired colors */
		if(maporder == CC_MAPORDER_ARB) {
			for(i = 0; i < c_pages_this_loop; i++) {
				pfcolor = cc_pfn_color(pfnbuf[i]);
				/* If present and its color is among what we need */
				if(cc_pfn_present(pfnbuf[i]) && picker[pfcolor].needed > 0) {
					picker[0].pages[c_pages_picked] = mem + i * ULCC_PAGE_BYTES;
					picker[pfcolor].picked++;
					if(++c_pages_picked >= c_pages)
						break;
				}
			}
		}
		else {
			for(i = 0; i < c_pages_this_loop; i++) {
				pfcolor = cc_pfn_color(pfnbuf[i]);
				/* If present, its color is among what we need, and we still
				 * need more pages in this color */
				if(cc_pfn_present(pfnbuf[i]) &&
				   picker[pfcolor].picked < picker[pfcolor].needed) {
					picker[pfcolor].pages[picker[pfcolor].picked++] =
							mem + i * ULCC_PAGE_BYTES;
					if(++c_pages_picked >= c_pages)
						break;
				}
			}
		}

		i_loop++;
	} /* while */

	if(c_pages_picked < c_pages) {
		_ULCC_ERROR("failed to pick out enough pages within max loops\n");
		ret = -1;
	}

finish:
	if(pfnbuf)
		free(pfnbuf);

	return ret;
}

int _ULCC_HIDDEN _remap_pages_seq(
		struct _page_picker_s *picker,
		const unsigned long *start,
		const unsigned long *end,
		const int n,
		char *do_remap,
		const int movedata)
{
	void *remap_from, *remap_to, *remap_to_end;
	int c_colors = 0, i_remap = 0, i, idr;
	int *index = NULL;
	int ret = 0;

	index = malloc(sizeof(*index) * ULCC_NR_CACHE_COLORS);
	if(!index) {
		_ULCC_PERROR("failed to malloc for index array in _remap_pages_seq");
		ret = -1;
		goto finish;
	}

	/* Compute the number of colors in this request and build index array */
	for(i = 0; i < ULCC_NR_CACHE_COLORS; i++)
		if(picker[i].needed > 0)
			index[c_colors++] = i;
	_ULCC_ASSERT(c_colors > 0);

	/* For each data region, do the remapping */
	for(idr = 0, i = 0; idr < n; idr++) {
		remap_to = (void *)ULCC_ALIGN_HIGHER(start[idr]);
		remap_to_end = (void *)ULCC_ALIGN_LOWER(end[idr]);

		while(remap_to < remap_to_end) {
			/* If this page does not need to be remapped, skip it */
			if(!do_remap[i_remap++]) {
				remap_to += ULCC_PAGE_BYTES;
				continue;
			}

			/* Get the next picked page to remap from.
			 * Infinite looping is guaranteed not to happen as long as the total
			 * amount of picked pages is not fewer than required, which is true
			 * after _pick_pages successfully returned. */
			while(picker[index[i]].picked <= 0)
				i = (i + 1) % c_colors;

			/* Select a page to remap from.
			 * A possible problem with remapping from tail to head is that the
			 * number of continuous physical pages in the virtual memory area
			 * being remapped to will decrease. Consider to remap from head to
			 * tail. */
			remap_from = picker[index[i]].pages[--picker[index[i]].picked];

			/* Copy data before remapping if required so */
			if(movedata)
				memcpy(remap_from, remap_to, ULCC_PAGE_BYTES);

			/* Remap the picked physical page to user data page; this is one
			 * of the key hacks to user-level cache control */
			if(mremap(remap_from, ULCC_PAGE_BYTES, ULCC_PAGE_BYTES,
			   MREMAP_MAYMOVE | MREMAP_FIXED, remap_to) == MAP_FAILED) {
				_ULCC_PERROR("mremap failed in _remap_pages_seq");
				ret = -1;
				goto finish;
			}

			/* Repair the page hole caused by the above remapping; this is one
			 * of the key hacks to user-level cache control */
			if(mmap(remap_from, ULCC_PAGE_BYTES, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, 0, 0) != remap_from) {
				_ULCC_PERROR("mmap failed in _remap_pages_seq");
				ret = -1;
				goto finish;
			}

			remap_to += ULCC_PAGE_BYTES;
			i = (i + 1) % c_colors;
		} /* while */
	} /* for each data region */

finish:
	if(index)
		free(index);

	return ret;
}

int _ULCC_HIDDEN _remap_pages_rand(
		struct _page_picker_s *picker,
		const unsigned long *start,
		const unsigned long *end,
		const int n,
		char *do_remap,
		const int movedata)
{
	void *remap_to, *remap_to_end, *remap_from;
	int c_colors = 0, i_remap = 0, i, idr;
	int *index = NULL;
	unsigned int rand_seed;
	int ret = 0;

	index = malloc(sizeof(*index) * ULCC_NR_CACHE_COLORS);
	if(!index) {
		_ULCC_PERROR("failed to malloc for index array in _remap_pages_seq");
		ret = -1;
		goto finish;
	}

	/* Compute the number of colors in this request and build index array */
	for(i = 0; i < ULCC_NR_CACHE_COLORS; i++)
		if(picker[i].needed > 0)
			index[c_colors++] = i;
	_ULCC_ASSERT(c_colors > 0);

	rand_seed = time(NULL);

	/* For each data region, do the remapping */
	for(idr = 0, i = 0; idr < n; idr++) {
		remap_to = (void *)ULCC_ALIGN_HIGHER(start[idr]);
		remap_to_end = (void *)ULCC_ALIGN_LOWER(end[idr]);

		while(remap_to < remap_to_end) {
			if(!do_remap[i_remap++]) {
				remap_to += ULCC_PAGE_BYTES;
				continue;
			}

			/* Get the next picked page to remap from */
			i = (i + rand_r(&rand_seed)) % c_colors;
			while(picker[index[i]].picked == 0)
				i = (i + 1) % c_colors;

			remap_from = picker[index[i]].pages[--picker[index[i]].picked];

			/* Copy data before remapping if required so */
			if(movedata)
				memcpy(remap_from, remap_to, ULCC_PAGE_BYTES);

			/* Remap the picked physical page to user data page */
			if(mremap(remap_from, ULCC_PAGE_BYTES, ULCC_PAGE_BYTES,
			   MREMAP_MAYMOVE | MREMAP_FIXED, remap_to) == MAP_FAILED) {
				_ULCC_PERROR("mremap failed in _remap_pages_rand");
				ret = -1;
				goto finish;
			}

			/* Repair the page hole caused by the above remapping */
			if(mmap(remap_from, ULCC_PAGE_BYTES, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, 0, 0) != remap_from) {
				_ULCC_PERROR("mmap failed in _remap_pages_rand");
				ret = -1;
				goto finish;
			}

			remap_to += ULCC_PAGE_BYTES;
		} /* while */
	} /* for each data region */

finish:
	if(index)
		free(index);

	return ret;
}

int _ULCC_HIDDEN _remap_pages_arb(
		struct _page_picker_s *picker,
		const unsigned long *start,
		const unsigned long *end,
		const int n,
		char *do_remap,
		const int movedata)
{
	void *remap_to, *remap_to_end, *remap_from;
	int idr, i_remap = 0, i_picked = 0;
	int ret = 0;

	for(idr = 0; idr < n; idr++) {
		remap_to = (void *)ULCC_ALIGN_HIGHER(start[idr]);
		remap_to_end = (void *)ULCC_ALIGN_LOWER(end[idr]);

		while(remap_to < remap_to_end) {
			if(!do_remap[i_remap++]) {
				remap_to += ULCC_PAGE_BYTES;
				continue;
			}

			remap_from = picker[0].pages[i_picked++];

			if(movedata)
				memcpy(remap_from, remap_to, ULCC_PAGE_BYTES);

			/* Remap the picked physical page to user data region */
			if(mremap(remap_from, ULCC_PAGE_BYTES, ULCC_PAGE_BYTES,
			   MREMAP_MAYMOVE | MREMAP_FIXED, remap_to) == MAP_FAILED) {
				_ULCC_PERROR("mremap failed in _remap_pages_arb");
				ret = -1;
				goto finish;
			}

			/* Repair the page hole caused by the above remapping */
			if(mmap(remap_from, ULCC_PAGE_BYTES, PROT_READ | PROT_WRITE,
			   MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, 0, 0) != remap_from) {
				_ULCC_PERROR("mmap failed in _remap_pages_arb");
				ret = -1;
				goto finish;
			}

			remap_to += ULCC_PAGE_BYTES;
		}
	} /* For each data region */

finish:
	return ret;
}

/* Remap user data regions to picked physical pages */
int _ULCC_HIDDEN _remap_pages(
		struct _page_picker_s *picker,
		const unsigned long *start,
		const unsigned long *end,
		const int n,
		char *do_remap,
		int flags)
{
	int maporder, movedata;
	int ret = 0;

	maporder = flags & CC_MASK_MAPORDER;
	if((flags & CC_MASK_MOVE) == CC_ALLOC_MOVE)
		movedata = 1;
	else
		movedata = 0;

	/* Sequential mapping */
	switch(maporder) {
	case CC_MAPORDER_SEQ:
		ret = _remap_pages_seq(picker, start, end, n, do_remap, movedata);
		break;
	/* Random mapping */
	case CC_MAPORDER_RAND:
		ret = _remap_pages_rand(picker, start, end, n, do_remap, movedata);
		break;
	/* Arbitrary mapping */
	case CC_MAPORDER_ARB:
		ret = _remap_pages_arb(picker, start, end, n, do_remap, movedata);
		break;
	/* Unknown mapping */
	default:
		ret = -1;
		break;
	}

	return ret;
}

int cc_remap(const unsigned long *start, const unsigned long *end, const int n,
			 const cc_cacheregn_t *regn, const int flags)
{
	int c_pages, c_colors, c_pages_per_color;
	struct _page_picker_s *picker = NULL;
	struct _vm_list_s *vml = NULL;
	char *do_remap = NULL;
	int ret, maporder;

	maporder = flags & CC_MASK_MAPORDER;
	c_colors = cc_cacheregn_cnt(regn);
	if(c_colors <= 0) {
		ret = -1;
		goto finish;
	}

	c_pages = _nr_aligned_pages(start, end, n);
	if(!c_pages) {
		_ULCC_ERROR("no aligned pages to be remapped in the data regions");
		ret = -1;
		goto finish;
	}

	do_remap = malloc(sizeof(*do_remap) * c_pages);
	if(!do_remap) {
		_ULCC_PERROR("failed to malloc for pfnbuf");
		ret = -1;
		goto finish;
	}

	/* Get the number of aligned pages that need to be remapped; pages that
	 * are already in desired region do not need to be remapped. */
	c_pages = _pages_to_be_remapped(start, end, n, regn, do_remap);
	if(c_pages < 0) {
		_ULCC_ERROR("failed to get which pages to be remapped");
		ret = -1;
		goto finish;
	}
	else if(c_pages == 0) {	/* no pages need to be remapped */
		ret = 0;
		goto finish;
	}

	c_pages_per_color = (c_pages % c_colors) ? (c_pages / c_colors + 1) :
			(c_pages / c_colors);

	vml = _vm_list_new();
	if(!vml) {
		ret = -1;
		goto finish;
	}

	picker = _page_picker_new(c_pages, c_pages_per_color, regn, maporder);
	if(!picker) {
		ret = -1;
		goto finish;
	}

	/* Pick pages in desired colors; and if successful, remap data regions to
	 * picked physical pages */
	if(_pick_pages(picker, vml, c_pages, c_colors, c_pages_per_color, maporder))
		ret = -1;
	else
		ret = _remap_pages(picker, start, end, n, do_remap, flags);

finish:
	if(picker)
		_page_picker_free(picker);
	if(vml)
		_vm_list_free(vml);
	if(do_remap)
		free(do_remap);

	return ret;
}

/* Allocate page-aligned memory; size must be multiple of ULCC_PAGE_BYTES */
void *cc_malloc_aligned(size_t size)
{
	void *mem;

	if(size <= 0 || size % ULCC_PAGE_BYTES)
		return NULL;

	mem =  mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if(mem == MAP_FAILED) {
		_ULCC_PERROR("mmap failed to aligned malloc");
		return NULL;
	}

	return mem;
}

void cc_free_aligned(void *mem, size_t size)
{
	munmap(mem, size);
}

/* Set or clear a set of colors, [low, high], in a cache region */
void cc_cacheregn_set(cc_cacheregn_t *regn, int low, int high, const int set)
{
	int i;

	if(low < 0)
		low = 0;
	if(high >= ULCC_NR_CACHE_COLORS)
		high = ULCC_NR_CACHE_COLORS - 1;

	if(set)
		for(i = low; i <= high; i++)
			ULCC_SET_COLOR_BIT(regn, i);
	else
		for(i = low; i <= high; i++)
			ULCC_CLR_COLOR_BIT(regn, i);
}

/* Clear the colors in a cache region */
void cc_cacheregn_clr(cc_cacheregn_t *regn)
{
	cc_cacheregn_set(regn, 0, ULCC_NR_CACHE_COLORS - 1, 0);
}

/* Count the number of colors set in a cache region */
int cc_cacheregn_cnt(const cc_cacheregn_t *regn)
{
	int i, c_colors = 0;

	for(i = 0; i < ULCC_NR_CACHE_COLORS; i++)
		if(ULCC_TST_COLOR_BIT(regn, i))
			c_colors++;

	return c_colors;
}
