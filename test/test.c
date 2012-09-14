#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "../src/ulcc.h"

#define TDIFF(t1, t2) (((t2).tv_sec + ((double)((t2).tv_usec))/1000000) - \
	((t1).tv_sec + ((double)((t1).tv_usec))/1000000))

int main()
{
	struct timeval t1, t2;
	cc_cacheregn_t region;
	int size, size_held;
	int nr_colors, size_per_color;
	unsigned long start, end;
	char *data, a;
	int i, j;

	/* size of data is made 1MB larger than the LLC size */
	size = cc_llc_size();
	size += 1024 * 1024;

	data = malloc(size); /* you can also use malloc() simply */
	if(!data) {
		fprintf(stderr, "failed to allocate data of %d bytes\n", size);
		return -1;
	}

	/* The data is now initialized */
	for(i = 0; i < size; i += ULCC_PAGE_BYTES)
		data[i] = 'x';

	/* Now timing data access without ULCC support */
	gettimeofday(&t1, NULL);

	for(i = 0; i < 1000; i++)
		for(j = 0; j < size; j += 64)
			a = data[j];

	gettimeofday(&t2, NULL);
	printf("Access time without ULCC support: %lf s\n", TDIFF(t1, t2));

	/* Allocate a cache space to hold part of the data in LLC */
	size_per_color = cc_llc_size() / cc_nr_cache_colors();
	size_held = cc_llc_size() - 3 * size_per_color; /* leave 3 colors of space */
	nr_colors = size_held / size_per_color + 1;

	cc_cacheregn_clr(&region);
	cc_cacheregn_set(&region, 0, nr_colors - 1, 1);

	/* The core: remap the first size_held bytes of data to the cache space just
	 * allocated, and then remap the rest data to the small cache space left.
	 * The purpose is to keep the first size_held bytes of data in LLC, while
	 * preventing the rest of data from evicting data in the first part */
	start = (unsigned long)data;
	end = (unsigned long)(data + size_held);

	if(cc_remap(&start, &end, 1, &region, CC_ALLOC_NOMOVE | CC_MAPORDER_SEQ)) {
		fprintf(stderr, "failed to remap data 1\n");
		free(data);
		return -1;
	}

	cc_cacheregn_clr(&region);
	cc_cacheregn_set(&region, cc_nr_cache_colors() - 1, cc_nr_cache_colors() - 1, 1);
	start = (unsigned long)(data + size_held);
	end = (unsigned long)(data + size);

	if(cc_remap(&start, &end, 1, &region, CC_ALLOC_NOMOVE | CC_MAPORDER_SEQ)) {
		fprintf(stderr, "failed to remap data 2\n");
		free(data);
		return -1;
	}

	/* Now timing data access with ULCC support */
	gettimeofday(&t1, NULL);

	for(i = 0; i < 1000; i++)
		for(j = 0; j < size; j += 64)
			a = data[j];

	gettimeofday(&t2, NULL);
	printf("Access time with ULCC support: %lf s\n", TDIFF(t1, t2));

	free(data);
	return 0;
}
