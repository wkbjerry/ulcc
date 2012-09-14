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

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include "translator.h"

/* Translate virtual page addresses to physical page numbers.
 * The pagemap interface provided by linux kernel 2.6.25+ is used to get
 * the physical frame numbers of virtual memory pages.
 * See Documentation/vm/pagemap.txt within your kernel source tree for details
 * of the interface.
 */
int cc_addr_translate(cc_uint64_t *pfnbuf, const unsigned long vmp_start, const int n)
{
	char	fname[32];
	int		fid, i;

	sprintf(fname, "/proc/%d/pagemap", getpid());
	fid = open(fname, O_RDONLY);
	if(fid < 0) {
		_ULCC_PERROR("failed to open pagemap address translator");
		return -1;
	}

	if(lseek(fid, ULCC_PAGE_NBR(vmp_start) * 8, SEEK_SET) == (off_t)-1) {
		_ULCC_PERROR("failed to seek to translation start address");
		close(fid);
		return -1;
	}

	if(read(fid, pfnbuf, 8 * n) < 8 * n) {
		_ULCC_PERROR("failed to read pfn info");
		close(fid);
		return -1;
	}

	close(fid);

	for(i = 0; i < n; i++) {
		if(pfnbuf[i] & PAGEMAP_PAGE_PRESENT) {
			pfnbuf[i] &= PAGEMAP_MASK_PFN;
		}
		else {
			pfnbuf[i] = 0;
		}
	}

	return 0;
}
