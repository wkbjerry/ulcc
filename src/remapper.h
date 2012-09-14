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

#ifndef _ULCC_REMAPPER_H_
#define _ULCC_REMAPPER_H_

#include "ulcc.h"

/* Cache region: a bitmap representation of cache colors */
typedef struct cc_cacheregn_s
{
	unsigned char color_map[(ULCC_NR_CACHE_COLORS + 8) / 8];
} cc_cacheregn_t;

/* set a bit in a color bitmap */
#define ULCC_SET_COLOR_BIT(regn, ibit)	\
	((regn)->color_map[(ibit)/8] |= \
	((unsigned char)((unsigned char) 1) << ((ibit) % 8)))
/* clear a bit in a color bitmap */
#define ULCC_CLR_COLOR_BIT(regn, ibit)	\
	((regn)->color_map[(ibit)/8] &= \
	~((unsigned char)(((unsigned char) 1) << ((ibit) % 8))))
/* test whether a bit in a color bitmap is 1 or 0 */
#define ULCC_TST_COLOR_BIT(regn, ibit)	\
	(((regn)->color_map[(ibit)/8] & \
	((unsigned char)(((unsigned char) 1) << ((ibit) % 8)))) >> ((ibit) % 8))

/* The low-level cache space allocation interface */
int _ULCC_EXPORT cc_remap(const unsigned long *start, const unsigned long *end,
	const int n, const cc_cacheregn_t *regn, const int flags);

/* Aligned memory allocation */
void _ULCC_EXPORT *cc_malloc_aligned(size_t size);
void _ULCC_EXPORT cc_free_aligned(void *mem, size_t size);

/* Operations on cache region */
void _ULCC_EXPORT cc_cacheregn_set(cc_cacheregn_t *regn, int low,
	int high, const int set);
void _ULCC_EXPORT cc_cacheregn_clr(cc_cacheregn_t *regn);
int _ULCC_EXPORT cc_cacheregn_cnt(const cc_cacheregn_t *regn);

#endif
