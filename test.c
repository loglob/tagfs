#define _POSIX_C_SOURCE 200809L
//#include "hashmap_test.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#define UNUSED __attribute__ ((unused))

int main(UNUSED int argc, UNUSED char **argv)
{
	srand(clock());
	size_t t = sizeof(tests) / sizeof(test_t);
	size_t total = t;
	int fail = 0;

	#ifdef PARAM_T
	size_t pt = sizeof(ptests) / sizeof(ptest_t);
	size_t fc = sizeof(factories) / sizeof(factory_t);

	total += pt;

	if(!pt && fc)
		printf("[WARN] Found factories but no parameterized tests!\n");
	else if(!fc && pt)
		printf("[WARN] Found parameterized tests but no factories!\n");
	#endif

	if(total == 0)
	{
		printf("No tests found!\n");
		return EXIT_SUCCESS;
	}

	for (size_t i = 0; i < t; i++)
	{
		size_t num = i + 1;
		int c = tests[i]();

		if(c)
		{
			printf("Failed test #%zu with code %u\n", num, c);
			fail = c;
		}
		else
			printf("Finished test %zu/%zu\n", num, total);
	}
	
	#ifdef PARAM_T
	for (size_t i = 0; i < pt; i++)
	{
		size_t num = i + t + 1;

		for (size_t j = 0; j < fc; j++)
		{
			PARAM_T v = factories[j].create();
			int c = ptests[i](v);
			factories[j].destroy(v);

			if(c)
			{
				printf("Failed test #%zu with factory %zu with code %u\n", num, j+1, c);
				fail = c;
			}
			else
				printf("Finished test %zu/%zu with factory %zu/%zu\n", num, total, j + 1, fc);

		}
		
	}
	#endif
	 
	return fail;
}