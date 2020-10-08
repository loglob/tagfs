// unit testing in C
#pragma once
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>

typedef int (*test_t)(void);

//extern const test_t tests[];

#ifdef PARAM_T
typedef struct {
	void (*destroy)(PARAM_T);
	PARAM_T (*create)(void);
} factory_t;

typedef int (*ptest_t)(PARAM_T);

extern const factory_t factories[];
extern const ptest_t ptests[];
#endif

#define fail(...) failx(1, __VA_ARGS__)
#define faile() failc(errno)
#define failx(x, ...) (fprintf(stderr, "Test %s failed at %s:%u: ", __func__, __FILE__, __LINE__), _eprintf(__VA_ARGS__), x)
#define failc(x) (_eprintf("Test %s dailed at %s:%u: %s\n", __func__, __FILE__, __LINE__, strerror(x)), x)

#define assertMsg(expr, ...) if(!(expr)) return fail(__VA_ARGS__);

/* Declares as proper function for easyier debugging. */
void _eprintf(const char *str, ...)
{
	va_list va;
	va_start(va, str);
	vfprintf(stderr, str, va);
}