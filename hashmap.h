/* Implements a string->void* hashmap. */
#pragma once
// from package libssl-dev
#include <openssl/md5.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#ifndef HVAL_T
#error "hashmap.h requires you to #define HVAL_T to the hashmap value type"
#endif

#pragma region Types
struct hmap_digest
{
	uint64_t primary, secondary;
	size_t keyLen;
};

struct hmap_entry
{
	HVAL_T data;
	char *key;
	struct hmap_digest digest;
};

struct hmap
{
	size_t len;
	struct hmap_entry *entries;
};

typedef struct hmap *hmap_t;

#pragma endregion

#pragma region Interface Definition
/* Retrieves an item from the hmap.
	Returns a pointer to the value, or NULL if it isn't in the hmap.
	The pointer remains valid until the -ins or -put functions are used to create a new entry. */
HVAL_T *hmap_get(hmap_t map, const char *key);

/* Inserts a value into the hashmap, only if it doesn't already exist.
	Returns the value for an existant entry, or the allocated given value.
	Returns NULL if the key didn't already exist and insertion failed.
	The pointer remains valid until hmap_ins or hmap_put are used to create a new entry. */
HVAL_T *hmap_ins(hmap_t map, const char *key, HVAL_T value);

/* Sets the given key's value in the hashmap.
	Creates a new entry if the key doesn't have one, overrides existant entries.
	Returns the entries' value.
	Returns NULL if the key didn't already exist and insertion failed.
	The pointer remains valid until hmap_ins or hmap_put are used to create a new entry. */
HVAL_T *hmap_put(hmap_t map, const char *key, HVAL_T value);

/* Retrieves the key for the given hmap value.
	The value must have been returned by a hmap function. */
const char *hmap_key(HVAL_T *value);

/* Tries to insert an item into the hmap.
	Does not override already existant entries.
	If p isn't NULL, stores a pointer to the value in it.
	Returns 1 if the value was inserted.
	Returns 0 if the key already existed.
	Returns -1 and sets errno on failure. */
int hmap_tryIns(hmap_t map, const char *key, HVAL_T val, HVAL_T **p);

/* Insert an item into the hmap.
	Overrides already existant entries.
	Returns 1 if the entry didn't exist.
	If p isn't NULL, stores a pointer to the value in it.
	Returns 0 if an existant value was overriden.
	Returns -1 and sets errno on failure. */
int hmap_tryPut(hmap_t map, const char *key, HVAL_T val, HVAL_T **p);

/* Deletes an entry from the hmap.
	Returns true if the delection was succesful.
	Returns false if no entry was found, */
bool hmap_del(hmap_t map, const char *key);

/* Deletes the entry with the given value from the hmap.
	The pointer must have been returned by a hmap function. */
void hmap_delVal(HVAL_T *val);

/* Deallocates all resources used by the hmap. */
void hmap_destroy(hmap_t hmap);

/* Allocates a new hmap. */
hmap_t hmap_new();

#pragma endregion

#pragma region Macros

#define _HMAP_FORALL_I(map, keyVar, valueVar, body, index) for (size_t index = 0; index < (map)->len; index++) { \
		valueVar = &((map)->entries[index].data); \
		keyVar = (map)->entries[index].key; \
		if((map)->entries[index].key) \
			{body} \
	} \

/* Iterates over all elements in the map using the given variable declarations and loop body. */
#define HMAP_FORALL(map, keyVar, valueVar, body) _HMAP_FORALL_I(map, keyVar, valueVar, body, __ind_ ## __COUNTER__)

#pragma endregion

#pragma region Internal Functions
/* Calculates the digest of the given key */
static struct hmap_digest _hmap_hash(const char *key)
{
	// The MD5 digest should be 128 Bits long
	_Static_assert(MD5_DIGEST_LENGTH == sizeof(uint64_t) * 2, "Either MD5 or uint64 is defined badly");

	uint64_t hash[2];
	size_t len = strlen(key);

	MD5((const unsigned char*)key, len, (unsigned char*)hash);

	return (struct hmap_digest){ hash[0], hash[1], len };
}

/* Determines if the entry is valid and matches the given digest and key. */
static bool _hmap_matches(struct hmap_entry e, struct hmap_digest h, const char *key)
{
	return e.key
		&& e.digest.primary == h.primary
		&& e.digest.secondary == h.secondary
		&& e.digest.keyLen == h.keyLen
		&& (memcmp(e.key, key, h.keyLen) == 0);
}

/* Finds the matching entry for the given digest and key. */
static struct hmap_entry *_hmap_get(hmap_t map, struct hmap_digest hash, const char *key)
{
	if(_hmap_matches(map->entries[hash.primary % map->len], hash, key))
		return &map->entries[hash.primary % map->len];
	
	if(_hmap_matches(map->entries[hash.secondary % map->len], hash, key))
		return &map->entries[hash.secondary % map->len];

	return NULL;
}

/* Attempts to relocate the entry at position cur.
	Returns true and updates map on success.
	Returns false on failure. */
static bool _hmap_move(hmap_t map, size_t cur, size_t tries)
{
	// Detect loops/full maps
	if(tries >= map->len)
		return false;

	// Determine the new position for the relocated entry
	size_t new = map->entries[cur].digest.secondary % map->len;

	if(new == cur)
		new = map->entries[cur].digest.primary % map->len;

	if(new == cur)
		// The entry's secondary position is the same as the primary
		return false;

	// Clear the new position, if needed
	if(map->entries[new].key && !_hmap_move(map, new, tries + 1))
		return false;

	map->entries[new] = map->entries[cur];
	map->entries[cur] = (struct hmap_entry){};

	return true;
}

/* Attempts to insert the new entry into the map without changing its size.
	Returns a pointer to the new entry on success.
	Returns NULL on failure. */
static struct hmap_entry *_hmap_put(hmap_t map, struct hmap_entry e)
{
	#define insat(i) { map->entries[i] = e; return &map->entries[i]; }
	#define empty(i) if(!map->entries[i].key) insat(i)
	#define cuckoo(i) if(_hmap_move(map, i, 0)) insat(i)
	#define try(f) f(e.digest.primary % map->len) f(e.digest.secondary % map->len)
	
	try(empty)
	try(cuckoo)

	return NULL;

	#undef insat
	#undef empty
	#undef cuckoo
	#undef try
}

/* Attempts to resize the hashmap and reinsert the old entries and the new entry e.
	Returns 0 on success, 1 on benign error (the size doesn't yield a valid hashmap) and -1 on malloc failure.
	If p isn't NULL and the operation succeeds, stores a pointer to the new entry for e in *p. */
static int _hmap_resize(hmap_t map, size_t newsize, struct hmap_entry e, struct hmap_entry **p)
{
	//printf("Resizing from %zu to %zu\n", map->len, newsize);
	struct hmap newmap = (struct hmap){ .len = newsize, .entries = calloc(newsize, sizeof(struct hmap_entry)) };

	if(!newmap.entries)
		return -1;

	struct hmap_entry *cur;

	for (size_t i = 0; i <= map->len; i++)
	{
		cur = _hmap_put(&newmap, (i == map->len) ? e : map->entries[i]);

		if(!cur)
		{
			//printf("Failed at %s->%zu\n", cur.key, cur.data);
			free(newmap.entries);
			return 1;
		}
	}

	if(p)
		*p = cur;

	free(map->entries);
	*map = newmap;

	return 0;
}


/* Attempts to insert a new entry into the map, possibly changing its size.
	Copies the key to construct a hmap_entry object.
	Returns the entry of the entry of the givne value and key.
	Returns NULL on malloc failure. */
static struct hmap_entry *_hmap_ins(hmap_t map, HVAL_T data, struct hmap_digest hash, const char *_key)
{
	char *key = malloc(hash.keyLen + 1);

	if(!key)
		return NULL;
	
	memcpy(key, _key, hash.keyLen + 1);
	struct hmap_entry e = (struct hmap_entry){ .data = data, .key = key, .digest = hash };
	struct hmap_entry *p = _hmap_put(map, e);

	if(p)
		return p;
	
	// naive resizing
	for (int newsize = map->len * 2; ; newsize++)
	{
		switch(_hmap_resize(map, newsize, e, &p))
		{
			case 0:
				return p;

			case -1:
				free(key);
				return NULL;
		}
	}
}

inline static struct hmap_entry *_hmap_entry(HVAL_T *val)
{
	// Uses data being the the first field.
	return (struct hmap_entry *)val;
}

static void _hmap_del(struct hmap_entry *e)
{
	if(e)
	{
		free(e->key);
		e->key = NULL;
	}
}

#pragma endregion

#pragma region Interface Implementation

HVAL_T *hmap_get(hmap_t map, const char *key)
{
	struct hmap_entry *e = _hmap_get(map, _hmap_hash(key), key);

	return e ? &e->data : NULL;
}

HVAL_T *hmap_ins(hmap_t map, const char *_key, HVAL_T data)
{
	struct hmap_digest d = _hmap_hash(_key);
	struct hmap_entry *ex = _hmap_get(map, d, _key);

	if(ex) // Entry already exists
		return &ex->data;
	
	ex = _hmap_ins(map, data, d, _key);

	return ex ? &ex->data : NULL;
}

HVAL_T *hmap_put(hmap_t map, const char *_key, HVAL_T data)
{
	struct hmap_digest d = _hmap_hash(_key);
	struct hmap_entry *ex = _hmap_get(map, d, _key);

	if(ex) // Entry already exists
	{
		ex->data = data;
		return &ex->data;
	}
	
	ex = _hmap_ins(map, data, d, _key);
	
	return ex ? &ex->data : NULL;
}


int hmap_tryIns(hmap_t map, const char *key, HVAL_T value, HVAL_T **p)
{
	struct hmap_digest hash = _hmap_hash(key);
	struct hmap_entry *e = _hmap_get(map, hash, key);

	if(e)
	{
		if(p)
			*p = &e->data;

		return 0;
	}

	e = _hmap_ins(map, value, hash, key);

	if(e && p)
		*p = &e->data;

	return e ? 1 : -1;
}

int hmap_tryPut(hmap_t map, const char *key, HVAL_T value, HVAL_T **p)
{
	struct hmap_digest hash = _hmap_hash(key);
	struct hmap_entry *e = _hmap_get(map, hash, key);

	if(e)
		e->data = value;

	return e ? 0 : _hmap_ins(map, value, hash, key) ? 1 : -1;

	if(e)
	{
		if(p)
			*p = &e->data;

		e->data = value;
		return 0;
	}

	e = _hmap_ins(map, value, hash, key);

	if(e && p)
		*p = &e->data;

	return e ? 1 : -1;
}


const char *hmap_key(HVAL_T *value)
{
	return _hmap_entry(value)->key;
}

bool hmap_del(hmap_t map, const char *key)
{
	struct hmap_entry *e = _hmap_get(map, _hmap_hash(key), key);

	_hmap_del(e);

	return (bool)e;
}

void hmap_delVal(HVAL_T *val)
{
	_hmap_del(_hmap_entry(val));
}

void hmap_destroy(hmap_t map)
{
	for (size_t i = 0; i < map->len; i++)
	{
		if(map->entries[i].key)
			free(map->entries[i].key);
	}

	free(map->entries);
	free(map);
}

hmap_t hmap_new()
{
	hmap_t map = malloc(sizeof(struct hmap));

	if(map)
	{
		map->len = 10;
		map->entries = calloc(10, sizeof(struct hmap_entry));

		if(!map->entries)
		{
			free(map);
			return NULL;
		}
	}

	return map;
}

#pragma endregion