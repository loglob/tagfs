// unit testing, in C
#include <time.h>
#include <string.h>

#define HVAL_T size_t
#include "hashmap.h"
#define PARAM_T hmap_t
#include "test.h"


int testSimple(hmap_t map)
{
	if(!map)
		return failc(ENOMEM);

	HMAP_FORALL(map, const char *key, size_t *value, {
		return fail("Empty map contains at least 1 element: %s->%zu", key, *value);
	})

	char key[] = "key_";

	const char alphabet[] = "0123456789ABCDEF";
	size_t data[sizeof(alphabet)];

	for (size_t i = 0; i < sizeof(alphabet); i++)
	{
		data[i] = rand();
		//data[i] = i;
		key[3] = alphabet[i];
	
		//printf("Inserting %s->%zu\n", key, data[i]);

		if(!hmap_ins(map, key, data[i]))
			return fail("hmap_ins failure on key '%s'", key);
	}

	/*
	HMAP_FORALL(map, const char *key, size_t value, {
		printf("%s->%zu\n", key, value);
	})*/
	
	for (size_t i = 0; i < sizeof(alphabet); i++)
	{
		key[3] = alphabet[i];
		size_t d;
		
		if(!hmap_tryGet(map, key, &d))
			return fail("hmap_get failure on key '%s'; Entry not found!\n", key);
		

		if(d != data[i])
			return fail("hmap_get failure on key '%s'; Expected %zu, got %zu.\n", key, data[i], d);
	}
	
	return 0;
}

int testRand(hmap_t map)
{
	if(!map)
		return failc(ENOMEM);

	const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	size_t data[sizeof(alphabet)];
	bool has[sizeof(alphabet)] = {};
	char key[] = "key_";

	for (size_t j = 0; j < 2000; j++)
	{
		int i = rand() % sizeof(alphabet);
		key[3] = alphabet[i];
		
		switch(rand() % 4)
		{
			case 0:
			{
				data[i] = rand();
				has[i] = true;

				if(!hmap_ins(map, key, data[i]))
					return fail("ins failure for key %s", key);
			}
			break;

			case 1:
			{
				size_t d;
				
				if(hmap_tryGet(map, key, &d) != has[i])
					return fail("tryGet failure for key %s, expected %d.\n", key, has[i]);

				if(has[i] && (d != data[i]))
					return fail("get failure for key %s, expected %zu, got %zu.", key, data[i], d);
			}
			break;

			case 2:
			{
				if(hmap_del(map, key) != has[i])
					return fail("del failure for key %s, expected %d.", key, has[i]);
				
				has[i] = false;
			}
			break;

			case 3:
			{
				size_t d = has[i] ? data[i] + 1 : (size_t)rand();

				int s = hmap_tryIns(map, key, d); 
				
				if(s < 0)
					return fail("tryIns failure for key %s.", key);
				if(!has[i] != s)
					return fail("Wrong tryIns return code. Expected %d, got %d.", !has[i], s);

				size_t *r = hmap_get(map, key);

				if(!r)
					return fail("tryIns; get broke assertions on key %s, expected %zu, got NULL.", key, data[i]);
				if(*r != d)
					return fail("tryIns; get broke assertions on key %s, expected %zu, got %zu.", key, data[i], *r);
			
				data[i] = d;
				has[i] = true;
			}
			break;
		}
	}
	
	HMAP_FORALL(map, const char *key, size_t *val, {
		char *_a = strchr(alphabet, key[3]);
		int i = _a ? ((size_t)_a - (size_t)alphabet) : sizeof(alphabet) - 1;
		size_t d;

		if(!has[i])
			return fail("Got value for key %s which shouldn't exist\n", key);
		if(*val != data[i])
			return fail("Invalid value for key %s, expected %zu, got %zu.\n", key, data[i], *val);
		if(!hmap_tryGet(map, key, (void*)&d))
			return fail("tryGet failure for key %s, expected 1; Entry is listed by HMAP_FORALL\n", key);
		if(d != *val)
			return fail("tryGet value for key %s inconsistend with HMAP_FORALL: Got %zu, expected %zu.\n", key, d, *val);
	})

	return 0;
}

const test_t tests[] = {  };
const ptest_t ptests[] = { testSimple, testRand };
const factory_t factories[] = { (factory_t){ hmap_destroy, hmap_new } };