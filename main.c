#include <stdio.h>

#include "l_memory_pool.h"

void test_memoryPool()
{
	ncount = 0;
	for (int i = 1; i < 1000; i++)
	{
		l_allocate(16);
	}
	printf("l_allocate malloc count:%d\n",ncount);
	ncount = 0;
	for (int i = 1; i < 1000; i++)
	{
		malloc_allocate(16);
	}
	printf("malloc_allocate count:%d\n", ncount);
}

int main()
{
	printf("test memeroyPool------------\n");
	test_memoryPool();

	return 0;
}