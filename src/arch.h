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

#ifndef _ULCC_ARCH_H_
#define _ULCC_ARCH_H_

/****************************************************************************
 * EDIT information below to fit the parameters of your machine.
 * Currently these information are set statically by library programmers;
 * a better approach is to detect such parameters automatically in library
 * initializer. */

/* Last level cache size in KiB */
#define ULCC_CACHE_KB				(6 * 1024/* TODO: fill here */)
/* Last level cache associativity */
#define ULCC_CACHE_ASSOC			(48/* TODO: fill here */)

/* Number of bits for the size of a memory page: 4KiB should be pretty standard */
#define ULCC_PAGE_BITS				12

/* END OF EDIT
 ***************************************************************************/

#define ULCC_PAGE_BYTES			((unsigned long)1 << ULCC_PAGE_BITS)
#define ULCC_PAGE_KB			(ULCC_PAGE_BYTES / 1024)
#define ULCC_PAGE_OFFSET_MASK	(((unsigned long)1 << ULCC_PAGE_BITS) - 1)
#define ULCC_PAGE_IDX_MASK		(~ULCC_PAGE_OFFSET_MASK)
#define ULCC_PAGE_NBR(addr)		(((unsigned long)(addr)) >> ULCC_PAGE_BITS)

/* Number of cache colors */
#define ULCC_NR_CACHE_COLORS	(ULCC_CACHE_KB / ULCC_CACHE_ASSOC / ULCC_PAGE_KB)

#endif
