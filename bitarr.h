/* Implements an expandable int->bool bit array. */
#pragma once

#include <stdbool.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef WORD
	typedef uint64_t word;
	#define WORD 64
	#define WORD_MAX UINT64_MAX
	#ifdef PRIu64
	#define PRIW PRIu64 
	#else
	#define PRIW "ul"
	#endif
#endif

#pragma region Types
typedef word *bitarr_t;
#pragma endregion

#pragma region Internal Functions
static inline size_t _bitarr_size(size_t len)
{
	return (len / 64) + ((len % 64) ? 1 : 0);
}
#pragma endregion

#pragma region Interface
/* Resizes the given array from the old to the new size.
	New bits are initialized to 0. */
bitarr_t bitarr_resize(bitarr_t arr, size_t oldLen, size_t newLen);
/* Initializes a new bitarray with the given length.
	Every bit is set to 0. */
bitarr_t bitarr_new(size_t len);
/* Reads the bit at the given index. */
bool bitarr_get(bitarr_t arr, size_t index);
/* Sets the bit at the given index to the given value. */
void bitarr_set(bitarr_t arr, size_t index, bool value);
/* Deallocates the given bit array */
void bitarr_destroy(bitarr_t arr);
/* Counts the total number of bits in the bitarray of the given length with the givne value. */
size_t bitarr_count(bitarr_t arr, size_t len, bool val);
/* Finds the lowest bit index >= start in the array with the given value.
	The given length is absolute.
	Returns -1 if the value doesn't exist */
size_t bitarr_next(const bitarr_t arr, size_t start, size_t len, bool val);
/* Determines if any bit in the array is of the given value. */
bool bitarr_any(const bitarr_t arr, size_t len, bool val);
/* Determines if every bit in the array is of the given value. */
bool bitarr_all(const bitarr_t arr, size_t len, bool val);
/* Determines if, for every 1 in pos, arr is 1 at that position and, for every 1 in neg, arr is 0 at that position.
	pos or neg may be NULL, in which case they are ignored. */
bool bitarr_match(const bitarr_t arr, size_t len, const bitarr_t pos, const bitarr_t neg);
/* Sets arr to the bitwise or of arr and r. */
void bitarr_eqor(bitarr_t arr, size_t len, const bitarr_t r);
/* Determines if, at any index, l and r are both 1. */
bool bitarr_anyAnd(bitarr_t l, size_t len, bitarr_t r);
/* Copies src to dest */
void bitarr_copy(bitarr_t dest, size_t len, const bitarr_t src);
#pragma endregion

#pragma region Macros
/* Iterates over all indexes in arr for which arr[i] == val. i cannot be a full declaration, only an identifier. */
#define bitarr_forall(arr, len, i, val) for (size_t i = bitarr_next(arr, 0, len, val); i != (size_t)-1; i = bitarr_next(arr, i + 1, len, val))
#pragma endregion

#pragma region Implementation
bitarr_t bitarr_resize(bitarr_t arr, size_t oldLen, size_t newLen)
{
	size_t newSiz = _bitarr_size(newLen);
	size_t oldSiz = _bitarr_size(oldLen);

	if(oldSiz == newSiz)
		return arr;

	bitarr_t newArr = realloc(arr, newSiz * 8);

	if(!newArr)
		return NULL;

	for (size_t i = oldSiz; i < newSiz; i++)
		newArr[i] = 0;
	
	return newArr;
}

bitarr_t bitarr_new(size_t len)
{
	return calloc(_bitarr_size(len), sizeof(word));
}

bool bitarr_get(bitarr_t arr, size_t index)
{
	return (bool)(arr[index / 64] & (1 << (index % 64)));
}

void bitarr_set(bitarr_t arr, size_t index, bool value)
{
	if(value)
		arr[index / 64] |= (1 << (index % 64));
	else
		arr[index / 64] &= ~(1 << (index % 64));
}

void bitarr_destroy(bitarr_t arr)
{
	free(arr);
}

size_t bitarr_count(bitarr_t arr, size_t len, bool val)
{
	const word skip = val ? 0 : UINT64_MAX;
	const word all = val ? UINT64_MAX : 0;
	size_t siz = _bitarr_size(len);
	size_t c = 0;

	for (size_t i = 0; i < siz; i++)
	{
		word cur = arr[i];

		// Avoid bitwise iteration if possible
		if(cur == skip)
			continue;
		if(i < (len / WORD) && cur == all)
		{
			c += WORD;
			continue;
		}
		
		size_t wl = ((i + 1 == siz) && (len % 64)) ? (len % 64) : WORD;

		for (size_t j = 0; j < wl; j++)
		{
			if(cur & 1)
				c++;

			cur >>= 1;
		}
	}
	
	return c;
}

size_t bitarr_next(const bitarr_t arr, size_t start, size_t len, bool val)
{
	size_t z = _bitarr_size(len);
	word skip = val ? 0 : UINT64_MAX;
	size_t p = start;

	for (size_t w = start / WORD; w < z; w++)
	{
		if(arr[w] == skip)
			continue;

		word cur = arr[w];

		if(start % WORD)
		{
			cur >>= start % WORD;
			start = 0;
		}
		
		for (; p < len; cur >>= 1, p++)
		{
			if((cur & 1) == val)
				return p;
		}
	}

	return -1;
}

bool bitarr_any(const bitarr_t arr, size_t len, bool val)
{
	return bitarr_next(arr, 0, len, val) != (size_t)-1;
}

bool bitarr_all(const bitarr_t arr, size_t len, bool val)
{
	word want = val ? WORD_MAX : 0;

	for (size_t w = 0; w < len / WORD; w++)
	{
		if(arr[w] != want)
			return false;
	}

	size_t rest = len % WORD;

	if(!rest)
		return true;

	word mask = WORD_MAX >> (WORD - rest);

	return (arr[len / WORD] & mask) == (want & mask);
}

bool bitarr_match(const bitarr_t arr, size_t len, const bitarr_t pos, const bitarr_t neg)
{
	if(pos)
	{
		bitarr_forall(pos, len, i, true)
		{
			if(!bitarr_get(arr, i))
				return false;
		}
	}

	if(neg)
	{
		bitarr_forall(neg, len, i, true)
		{
			if(bitarr_get(arr, i))
				return false;
		}
	}
	
	return true;
}

void bitarr_eqor(bitarr_t arr, size_t len, const bitarr_t r)
{
	for (size_t w = 0; w < len / WORD; w++)
		arr[w] |= r[w];
	
	// Handles last word
	if(len % WORD)
		arr[len/WORD] |= r[len/WORD] & (WORD_MAX >> (WORD - (len % WORD)));
}

bool bitarr_anyAnd(bitarr_t l, size_t len, bitarr_t r)
{
	for (size_t w = 0; w < len / WORD; w++)
	{
		if(l[w] & r[w])
			return true;
	}
	
	// Handles last word
	if(len % WORD && (l[len/WORD] & r[len/WORD] & (WORD_MAX >> (WORD - (len % WORD)))))
		return true;

	return false;
}

void bitarr_copy(bitarr_t dest, size_t len, const bitarr_t src)
{
	size_t z = _bitarr_size(len);

	for (size_t w = 0; w < z; w++)
		dest[w] = src[w];
}

/* Sets every bit in arr to 1 is pos is 1, 0 if neg is 1 and to the value of arr otherwise */
void bitarr_merge(bitarr_t arr, size_t len, const bitarr_t pos, const bitarr_t neg)
{
	size_t z = _bitarr_size(len);

	for (size_t w = 0; w < z; w++)
	{
		arr[w] |= pos[w];
		arr[w] &= ~neg[w];
	}	
}

void bitarr_fill(bitarr_t arr, size_t len, bool value)
{
	size_t z = _bitarr_size(len);
	word fill = value ? WORD_MAX : 0;

	for (size_t w = 0; w < z; w++)
		arr[w] = fill;
}
#pragma endregion