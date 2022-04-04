// unit testing in C
#pragma once
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>

typedef void (*test_t)(void);

//extern const test_t tests[];

#ifdef PARAM_T
typedef struct {
	void (*destroy)(PARAM_T);
	PARAM_T (*create)(void);
} factory_t;

typedef void (*ptest_t)(PARAM_T);

extern const factory_t factories[];
extern const ptest_t ptests[];
#endif

#define faile() failc(errno)
#define failc(x) fail("%s\n", strerror(x))
#define fail(...) testError(__func__, __FILE__, __LINE__, __VA_ARGS__)

#define assertMsg(expr, ...) if(!(expr)) fail(__VA_ARGS__);

// Encapsulates information to produce a proper error message in _eprintf
static struct testInfo {
	int testNumber;
	int factoryNumber;
#ifdef PARAM_T
	PARAM_T value;
	factory_t factory;
#endif
} testInfo = {};

/* handles test errors */
void testError(const char *func, const char *file, unsigned long line, const char *fmt, ...)
{
	va_list va;
	va_start(va, fmt);

#ifdef PARAM_T
	if(testInfo.factoryNumber)
	{
		fprintf(stderr, "FAILED test #%u '%s' with factory %u (at %s:%lu): ",
			testInfo.testNumber, func, testInfo.factoryNumber, file, line);

		testInfo.factory.destroy(testInfo.value);
	}
	else
#endif
		fprintf(stderr, "FAILED test #%u '%s' (at %s:%lu): ", testInfo.testNumber, func, file, line);

	vfprintf(stderr, fmt, va);

	exit(EXIT_FAILURE);
}