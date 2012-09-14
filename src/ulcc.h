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

#ifndef _ULCC_H_
#define _ULCC_H_

#include "arch.h"

#ifdef _ULCC_LIB
#define _ULCC_EXPORT	__attribute__ ((visibility ("default")))
#define _ULCC_HIDDEN	__attribute__ ((visibility ("hidden")))
#else
#define _ULCC_EXPORT
#define _ULCC_HIDDEN
#endif

#ifdef _ULCC_DBG
#include <stdio.h>
#define _ULCC_STRINGIFY(x)	#x
#define _ULCC_ASSERT(x)		if(!(x)) fprintf(stderr,\
			"_ULCC_ASSERT Error: the evaluation of (%s) returned false\n",\
			_ULCC_STRINGIFY(x))
#define _ULCC_ERROR(m)		fprintf(stderr, "ULCC_DBG_ERROR: " m)
#define _ULCC_PERROR(m)		perror("ULCC_DBG_ERROR: " m)
#else
#define _ULCC_ASSERT(x)
#define _ULCC_ERROR(m)
#define _ULCC_PERROR(m)
#endif

#define ULCC_MIN(x,y)	((x) < (y) ? (x) : (y))
#define ULCC_MAX(x,y)	((x) > (y) ? (x) : (y))

/* Page alignment macros
 */
#define ULCC_ALIGN_HIGHER(addr) \
		(((addr) & ULCC_PAGE_OFFSET_MASK) ?\
		(((addr) & ULCC_PAGE_IDX_MASK) + ULCC_PAGE_BYTES) :\
		(addr)\
	)
#define ULCC_ALIGN_LOWER(addr)	\
		(((addr) & ULCC_PAGE_OFFSET_MASK) ?\
		((addr) & ULCC_PAGE_IDX_MASK) :\
		(addr)\
	)

/* Constants for allocation flags */
#define CC_MASK_MOVE			0x0001	/* MOVE bits mask: bit 0 */
#define CC_ALLOC_MOVE			0x0000	/* Move data to new pages; default */
#define CC_ALLOC_NOMOVE			0x0001	/* No need to move data to new pages */

#define CC_MASK_MAPORDER		0x0006	/* MAP_ORDER bits mask: bit 1-2 */
#define CC_MAPORDER_SEQ			0x0000	/* Sequential mapping; default */
#define CC_MAPORDER_RAND		0x0002	/* Random mapping */
#define CC_MAPORDER_ARB			0x0004	/* Arbitrary mapping */

/* Portable data types */
#if __x86_64__
typedef long					cc_int64_t;
typedef unsigned long			cc_uint64_t;
#else
typedef long long				cc_int64_t;
typedef unsigned long long		cc_uint64_t;
#endif

/* Library interfaces */
static inline int cc_nr_cache_colors()
{
	return ULCC_NR_CACHE_COLORS;
}

static inline int cc_llc_size()
{
	return ULCC_CACHE_KB * 1024;
}
/* other interfaces in remapper.h */
#include "remapper.h"

#endif
