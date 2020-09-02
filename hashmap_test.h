// unit testing, in C
#include <time.h>
#include <string.h>

#define HVAL_T int
#include "hashmap.h"
#define PARAM_T hmap_t
#include "test.h"

int testSimple(hmap_t map)
{
	if(!map)
		return failc(ENOMEM);

	HMAP_FORALL(map, const char *key, int *value, {
		return fail("Empty map contains at least 1 element: %s->%u", key, *value);
	})

	char key[] = "key_";

	const char alphabet[] = "0123456789ABCDEF";
	int data[sizeof(alphabet)];

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
		int *d;
		
		if(!(d = hmap_get(map, key)))
			return fail("hmap_get failure on key '%s'; Entry not found!\n", key);
		

		if(*d != data[i])
			return fail("hmap_get failure on key '%s'; Expected %u, got %u.\n", key, data[i], *d);
	}
	
	return 0;
}

int testRand(hmap_t map)
{
	if(!map)
		return failc(ENOMEM);

	const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int data[sizeof(alphabet)];
	bool has[sizeof(alphabet)] = {};
	int lastop[sizeof(alphabet)] = {};
	char key[] = "key_";

	for (size_t j = 0; j < 2000; j++)
	{
		int i = rand() % sizeof(alphabet);
		key[3] = alphabet[i];
		int op;
		
		const char *ops[] = {
			"put", "get", "ins",
			"tryPut", "tryIns", "del"
		};

		#define sfail() fail("%s failure for key %s after %s\n", ops[op], key, ops[lastop[op]])
		#define mfail(msg) fail("%s failure for key %s after %s, %s\n", ops[op], key, ops[lastop[op]], msg)
		#define eqchk(supposed, actual) { if(supposed != actual) { return fail("%s failure for key %s after %s: Expected %u, got %u\n", ops[op], key, ops[lastop[op]], supposed, actual); }}
		#define eqpchk(supposed, actual) { if(supposed != actual) { return fail("%s failure for key %s after %s: Expected %p(%u), got %p(%u)\n", ops[op], key, ops[lastop[op]], supposed, *supposed, actual, *actual); } }
		#define errfail() mfail(strerror(errno))

		switch(op = rand() % (sizeof(ops) / sizeof(*ops)))
		{
			case 0:
			{
				data[i] = rand();
				has[i] = true;

				if(!hmap_put(map, key, data[i]))
					return sfail();
			}
			break;

			case 1:
			{
				int *d = hmap_get(map, key);
				
				if(has[i] != (bool)d)
					return mfail(has[i] ? "exptected a value" : "expected null");
				if(has[i])
					eqchk(data[i], *d)
			}
			break;

			case 2:
			{
				int _d = rand();
				int *d = hmap_ins(map, key, _d);
				
				if(!d)
					return errfail();

				eqchk(has[i] ? data[i] : _d, *d)
				if(!has[i]) data[i] = _d;
			}
			break;

			case 3:
			{
				int d = has[i] ? data[i] + 1 : (int)rand();
				int *p;
				int s = hmap_tryPut(map, key, d, &p); 

				if(s < 0)
					return errfail();

				eqchk(!has[i], s);
				eqchk(d, *p);
				
				data[i] = d;
				has[i] = true;
			}
			break;

			case 4:
			{
				int d = has[i] ? data[i] + 1 : (int)rand();
				int *p;
				int s = hmap_tryIns(map, key, d, &p); 
				
				if(s < 0)
					return errfail();
				
				eqchk(!has[i], s)
				eqchk(has[i] ? data[i] : d, *p)
				
				if(!has[i])
				{
					data[i] = d;
					has[i] = true;
				}	
			}
			break;

			case 5:
			{
				int d = hmap_del(map, key);

				eqchk(has[i], d);					
				
				has[i] = false;
			}
			break;

		}
	}
	
	HMAP_FORALL(map, const char *key, int *val, {
		char *_a = strchr(alphabet, key[3]);
		int i = _a ? ((size_t)_a - (size_t)alphabet) : sizeof(alphabet) - 1;
		int *d;

		if(!has[i])
			return fail("Got value for key %s which shouldn't exist\n", key);
		if(*val != data[i])
			return fail("Invalid value for key %s, expected %u, got %u.\n", key, data[i], *val);
		if(!(d = hmap_get(map, key)))
			return fail("tryGet failure for key %s, expected 1; Entry is listed by HMAP_FORALL\n", key);
		if(d != val)
			return fail("tryGet value for key %s inconsistend with HMAP_FORALL: Got %p(%u), expected %p(%u).\n", key, d,*d, val,*val);
	})

	return 0;
}

const test_t tests[] = {  };
const ptest_t ptests[] = { testSimple, testRand };
const factory_t factories[] = { (factory_t){ hmap_destroy, hmap_new } };