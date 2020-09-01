#include "bitarr.h"
#define PARAM_T bitarr_t
#include "test.h"

#define BASE_LEN 64

int testnext1()
{
	word w = 0b1110111;

	int v;
	#define next(f, i) assert((v = bitarr_next(&w, f, 16, false)) == i, "testnext 1: from %d: Got %d, expected %d\n", f, v, i)
	
	next(0, 3)
	next(3, 3)
	next(4, 7)
	next(7, 7)
	next(8, 8)
	next(16, -1)

	return 0;
	#undef next
}

int testnext2()
{
	word w = 0b000100010001000;

	int v;
	#define next(f, i) assert((v = bitarr_next(&w, f, 16, true)) == i, "testnext 2: from %d: Got %d, expected %d\n", f, v, i)
	
	next(0, 3)
	next(3, 3)
	next(4, 7)
	next(7, 7)
	next(8, 11)

	return 0;
	#undef next
}

int testnext3()
{
	word w = UINT64_MAX;

	int v;
	#define next(f, i) assert((v = bitarr_next(&w, f, 16, false)) == i, "testnext 3: from %d: Got %d, expected %d\n", f, v, i)
	
	next(0, -1)
	next(10, -1)

	return 0;
	#undef next
}

int testSimple(bitarr_t arr)
{
	assert(bitarr_get(arr, 5) == 0, "init failure!")
	bitarr_set(arr, 5, 0);
	assert(bitarr_get(arr, 5) == 0, "set failure!")

	bitarr_set(arr, 5, 1);
	assert(bitarr_get(arr, 5) == 1, "set failure!")
	bitarr_set(arr, 5, 1);
	assert(bitarr_get(arr, 5) == 1, "set failure!")

	bitarr_set(arr, 5, 0);
	assert(bitarr_get(arr, 5) == 0, "set failure!")

	return 0;
}

bitarr_t newBitarr()
{
	return bitarr_new(BASE_LEN);
}

const test_t tests[] = { testnext1, testnext2, testnext3 };
const ptest_t ptests[] = { testSimple };
const factory_t factories[] = { (factory_t){ bitarr_destroy, newBitarr } };