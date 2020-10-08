/* The makefile calls cc so that the effective first line is of the form
#include "*_test.h"
And includes the relevant test header and test.h */
#include "test.h"
#include <stdio.h>
#include <stdlib.h>
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
		// from test.h
		testInfo.testNumber = i + 1;
		
		tests[i]();
		printf("Finished test %zu/%zu\n", i + 1, total);
	}
	
	#ifdef PARAM_T

	for (size_t i = 0; i < pt; i++)
	{
		size_t num = i + t + 1;

		for (size_t j = 0; j < fc; j++)
		{
			PARAM_T v = factories[j].create();

			testInfo = (struct testInfo){
				.testNumber = num,
				.factoryNumber = j+1,
				.factory = factories[j],
				.value = v
			};

			ptests[i](v);
			factories[j].destroy(v);

			printf("Finished test %zu/%zu with factory %zu/%zu\n", num, total, j + 1, fc);

		}
		
	}
	#endif
	 
	return fail;
}