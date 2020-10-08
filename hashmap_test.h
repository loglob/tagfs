// unit testing, in C
#include <time.h>
#include <string.h>

#define HVAL_T int
#include "hashmap.h"
#define PARAM_T hmap_t
#include "test.h"

void testSimple(hmap_t map)
{
	if(!map)
		failc(ENOMEM);

	HMAP_FORALL(map, const char *key, int *value, {
		fail("Empty map contains at least 1 element: %s->%u", key, *value);
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
			fail("hmap_ins failure on key '%s'", key);
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
			fail("hmap_get failure on key '%s': Entry not found!\n", key);
		

		if(*d != data[i])
			fail("hmap_get failure on key '%s': Expected %u, got %u.\n", key, data[i], *d);
	}
}

void testRand(hmap_t map)
{
	if(!map)
		failc(ENOMEM);

	const char alphabet[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	int data[sizeof(alphabet)];
	bool has[sizeof(alphabet)] = {};
	unsigned char lastop[sizeof(alphabet)] = {};
	char key[] = "key_";

	const char *ops[] = {
		"put", "get", "ins",
		"tryPut", "tryIns", "del",
		"(none)"
	};

	// init lastops[] to (none)
	memset(lastop, 6, sizeof(alphabet));

	for (size_t j = 0; j < 20000; j++)
	{
		int i = rand() % sizeof(alphabet);
		key[3] = alphabet[i];
		int op = rand() % (sizeof(ops) / sizeof(*ops) - 1);
		

		#define sfail() fail("%s failure for key %s after %s\n", ops[op], key, ops[lastop[i]])
		#define mfail(msg) fail("%s failure for key %s after %s, %s\n", ops[op], key, ops[lastop[i]], msg)
		#define eqchk(supposed, actual) { if(supposed != actual) fail("%s failure for key %s after %s: Expected %u, got %u\n", ops[op], key, ops[lastop[i]], supposed, actual); }
		#define eqpchk(supposed, actual) { if(supposed != actual) fail("%s failure for key %s after %s: Expected %p(%u), got %p(%u)\n", ops[op], key, ops[lastop[i]], supposed, *supposed, actual, *actual); }
		#define errfail() mfail(strerror(errno))

		switch(op)
		{
			case 0:
			{
				int d = rand();
				int *v = hmap_put(map, key, d);

				if(!v)
					sfail();

				eqchk(d, *v)

				data[i] = d;
				has[i] = true;
			}
			break;

			case 1:
			{
				int *d = hmap_get(map, key);
				
				if(has[i] != (bool)d)
					mfail(has[i] ? "exptected a value" : "expected null");
				if(has[i])
					eqchk(data[i], *d)
			}
			break;

			case 2:
			{
				int _d = rand();
				int *d = hmap_ins(map, key, _d);
				
				if(!d)
					errfail();

				if(has[i])
					eqchk(data[i], *d)
				else
				{
					eqchk(_d, *d);
					data[i] = _d;
					has[i] = true;
				}
			}
			break;

			case 3:
			{
				int d = rand();
				int *p = NULL;
				int s = hmap_tryPut(map, key, d, &p); 

				if(s < 0)
					errfail();
				if(!p)
					mfail("returned NULL");

				eqchk(!has[i], s);
				eqchk(d, *p);
				
				data[i] = d;
				has[i] = true;
			}
			break;

			case 4:
			{
				int d = rand();
				int *p;
				int s = hmap_tryIns(map, key, d, &p); 
				
				if(s < 0)
					errfail();
				if(!p)
					mfail("returned NULL");
				
				eqchk(!has[i], s)
				if(has[i])
					eqchk(data[i], *p);
				if(!has[i])
				{
					eqchk(d, *p)
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
	
		lastop[i] = op;
	}
	
	HMAP_FORALL(map, const char *key, int *val, {
		char *_a = strchr(alphabet, key[3]);
		int i = _a ? ((size_t)_a - (size_t)alphabet) : sizeof(alphabet) - 1;
		int *d;

		if(!has[i])
			fail("Got value for key %s which shouldn't exist\n", key);
		if(*val != data[i])
			fail("Invalid value for key %s, expected %u, got %u.\n", key, data[i], *val);
		if(!(d = hmap_get(map, key)))
			fail("tryGet failure for key %s, expected 1; Entry is listed by HMAP_FORALL\n", key);
		if(d != val)
			fail("tryGet value for key %s inconsistend with HMAP_FORALL: Got %p(%u), expected %p(%u).\n", key, d,*d, val,*val);
	})
}

const test_t tests[] = {  };
const ptest_t ptests[] = { testSimple, testRand };
const factory_t factories[] = { (factory_t){ hmap_destroy, hmap_new } };