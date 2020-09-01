// unit testing in C
#pragma once
#include <errno.h>
#include <stdio.h>

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

#define assert(expr, ...) if(!(expr)) return fail(__VA_ARGS__);

#define fail(...) failx(1, __VA_ARGS__)
#define faile() failc(errno)
#define failx(x, ...) (fprintf(stderr, __VA_ARGS__), x)
#define failc(x) (fprintf(stderr, "%s\n", strerror(x)), x)