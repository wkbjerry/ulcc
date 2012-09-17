#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <pthread.h>
#include "../src/ulcc.h"

#define TDIFF(t1, t2) (((t2).tv_sec + ((double)((t2).tv_usec))/1000000) - \
	((t1).tv_sec + ((double)((t1).tv_usec))/1000000))

char *data1_start, *data1_end;
char *data2_start, *data2_end;
int ulcc_enabled = 0;

void *thread_test(void *param)
{
	long it = (long)param;
	int i;
	char *p, r;
	struct timeval t1, t2;

	if(it % 2) {
		gettimeofday(&t1, NULL);
		for(i = 0; i < 3000; i++) {
			for(p = data1_start; p < data1_end; p += 64)
				r = *p;
		}
		gettimeofday(&t2, NULL);
		printf("%s ULCC support: strong LLC locality - %.4lf s\n",
			ulcc_enabled ? "With" : "Without",
			TDIFF(t1, t2));
	}
	else {
		/*gettimeofday(&t1, NULL);*/
		for(i = 0; i < 3000; i++) {
			for(p = data2_start; p < data2_end; p += 64)
				r = *p;
		}
		/*gettimeofday(&t2, NULL);
		printf("%s ULCC support: weak LLC locality - %.4lf s\n",
			ulcc_enabled ? "With" : "Without",
			TDIFF(t1, t2));*/
	}

	return NULL;
}

int test()
{
	void *data1 = NULL, *data2 = NULL;
	cc_cacheregn_t regn;
	int size1, size2;
	pthread_t tid[2];
	int ret = 0;
	long i;
	char *p;

	/* Create a data region to be accessed with strong LLC locality */
	size1 = cc_llc_size() * 3 / 4;
	data1 = malloc(size1);
	if(!data1) {
		perror("failed to allocate memory for data1");
		ret = -1;
		goto finish;
	}
	data1_start = (char *)ULCC_ALIGN_HIGHER((unsigned long)data1);
	data1_end = (char *)ULCC_ALIGN_LOWER((unsigned long)(data1 + size1));

	/* Create a data region to be accessed with weak LLC locality */
	size2 = cc_llc_size() * 2;
	data2 = malloc(size2);
	if(!data2){
		perror("failed to allocate memory for data2");
		free(data1);
		ret = -1;
		goto finish;
	}
	data2_start = (char *)ULCC_ALIGN_HIGHER((unsigned long)data2);
	data2_end = (char *)ULCC_ALIGN_LOWER((unsigned long)(data2 + size2));

	/* Data initialization */
	for(p = data1_start; p < data1_end; p += ULCC_PAGE_BYTES)
		*p = 'x';
	for(p = data2_start; p < data2_end; p += ULCC_PAGE_BYTES)
		*p = 'x';

	printf("Start testing: improving the performance of a strong-locality loop"
		" co-running with a weak-locality loop\n");

	/* Do scanning without ULCC support */
	ulcc_enabled = 0;
	for(i = 0; i < 2; i++)
		if(pthread_create(&tid[i], NULL, thread_test, (void *)i)) {
			printf("failed to create thread %d!\n", i);
			exit(-1);
		}
	for(i = 0; i < 2; i++)
		pthread_join(tid[i], NULL);

	/* Remap data regions to use separate cache space */
	/* First, give strong-locality data enough LLC space */
	cc_cacheregn_clr(&regn);
	cc_cacheregn_set(&regn, 0, cc_nr_colors() - 4, 1);
	if(cc_remap((unsigned long *)&data1_start, (unsigned long *)&data1_end, 1,
			&regn, CC_ALLOC_NOMOVE | CC_MAPORDER_SEQ) < 0) {
		fprintf(stderr, "failed to remap data region 1\n");
		ret = -1;
		goto finish;
	}

	/* Then, constrain weak-locality data's LLC usage */
	cc_cacheregn_clr(&regn);
	cc_cacheregn_set(&regn, cc_nr_colors() - 2, cc_nr_colors() - 2, 1);
	if(cc_remap((unsigned long *)&data2_start, (unsigned long *)&data2_end, 1,
			&regn, CC_ALLOC_NOMOVE | CC_MAPORDER_SEQ) < 0) {
		fprintf(stderr, "failed to remap data region 2\n");
		ret = -1;
		goto finish;
	}

	/* Do scanning with ULCC support */
	ulcc_enabled = 1;
	for(i = 0; i < 2; i++)
		if(pthread_create(&tid[i], NULL, thread_test, (void *)i)) {
			printf("failed to create thread %d!\n", i);
			exit(-1);
		}
	for(i = 0; i < 2; i++)
		pthread_join(tid[i], NULL);

	printf("Testing finished\n");

finish:
	if(data1)
		free(data1);
	if(data2)
		free(data2);

	return ret;
}

int main()
{
	return test();
}
