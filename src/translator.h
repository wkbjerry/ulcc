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

#ifndef _ULCC_TRANSLATOR_H_
#define _ULCC_TRANSLATOR_H_

#include "ulcc.h"

/* Pagemap interface masks */
#define PAGEMAP_MASK_PFN		(((cc_uint64_t)1 << 55) - 1)
#define PAGEMAP_PAGE_PRESENT	((cc_uint64_t)1 << 63)

/* Whether a page is present in memory */
#define cc_pfn_present(pfn)		((pfn) > 0)

/* The color of a physical page */
#define cc_pfn_color(pfn)		((pfn) % ULCC_NR_CACHE_COLORS)

/* Translate virtual page addresses to physical page numbers */
int _ULCC_HIDDEN cc_addr_translate(cc_uint64_t *pfnbuf, const unsigned long vmp_start, const int n);

#endif
