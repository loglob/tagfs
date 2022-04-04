#include "bitarr.h"
#define PARAM_T bitarr_t
#include "test.h"
#include <string.h>
#include <errno.h>

#define BASE_LEN 64

void testnext1()
{
	word w = 0b1110111;

	int v;
	#define next(f, i) assertMsg((v = bitarr_next(&w, f, 16, false)) == i, "testnext 1: from %d: Got %d, expected %d\n", f, v, i)

	next(0, 3)
	next(3, 3)
	next(4, 7)
	next(7, 7)
	next(8, 8)
	next(16, -1)

	#undef next
}

void testnext2()
{
	word w = 0b000100010001000;

	int v;
	#define next(f, i) assertMsg((v = bitarr_next(&w, f, 16, true)) == i, "testnext 2: from %d: Got %d, expected %d\n", f, v, i)

	next(0, 3)
	next(3, 3)
	next(4, 7)
	next(7, 7)
	next(8, 11)

	#undef next
}

void testnext3()
{
	word w = UINT64_MAX;

	int v;
	#define next(f, i) assertMsg((v = bitarr_next(&w, f, 16, false)) == i, "testnext 3: from %d: Got %d, expected %d\n", f, v, i)

	next(0, -1)
	next(10, -1)

	#undef next
}

void testResize()
{
	bitarr_t arr = bitarr_new(BASE_LEN);
	assertMsg(bitarr_count(arr, BASE_LEN, true) == 0, "newly allocated bitarray contains 1s\n");
	bitarr_fill(arr, 0, BASE_LEN, true);

	arr = bitarr_resize(arr, BASE_LEN, BASE_LEN / 2);
	assertMsg(arr, "bitarr_resize() shrinking failed: %s\n", strerror(errno));
	assertMsg(bitarr_count(arr, BASE_LEN / 2, false) == 0, "resize changed value\n")

	arr = bitarr_resize(arr, BASE_LEN / 2, BASE_LEN);
	assertMsg(arr, "bitarr_resize() growing failed: %s\n", strerror(errno));
	assertMsg(bitarr_count(arr, BASE_LEN, true) == BASE_LEN / 2, "resized portion of bitarray contains 1s\n");

	bitarr_destroy(arr);
}

void testSimple(bitarr_t arr)
{
	assertMsg(bitarr_get(arr, 5) == 0, "init failure!")
	bitarr_set(arr, 5, 0);
	assertMsg(bitarr_get(arr, 5) == 0, "set failure!")

	bitarr_set(arr, 5, 1);
	assertMsg(bitarr_get(arr, 5) == 1, "set failure!")
	bitarr_set(arr, 5, 1);
	assertMsg(bitarr_get(arr, 5) == 1, "set failure!")

	bitarr_set(arr, 5, 0);
	assertMsg(bitarr_get(arr, 5) == 0, "set failure!")
}

bitarr_t newBitarr()
{
	return bitarr_new(BASE_LEN);
}

const test_t tests[] = { testnext1, testnext2, testnext3, testResize };
const ptest_t ptests[] = { testSimple };
const factory_t factories[] = { (factory_t){ bitarr_destroy, newBitarr } };