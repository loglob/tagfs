/* tagdb.h: Handles interacting with a tag database */
#pragma once

#include "bitarr.h"
#include "futil.h"
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>

#pragma region Types
const char * const tagdb_entrykind_names[] = { "empty", "tag", "file" };

typedef enum
{
	// Used as intermediate value. Should never appear in a valid tagdb.
	TDB_EMPTY_ENTRY = 0,
	// A tag entry
	TDB_TAG_ENTRY,
	// A file entry
	TDB_FILE_ENTRY,
} tagdb_entrykind_t;

typedef struct
{
	tagdb_entrykind_t kind;
	
	union
	{
		// Only valid if kind==TDB_TAG_ENTRY, in 0..tagCap
		size_t tagId;
		// Only valid if kind==TDB_FILE_ENTRY, has length of at least tagCap
		bitarr_t fileTags;
	};
} tagdb_entry_t;

#define HVAL_T tagdb_entry_t
#include "hashmap.h"

typedef struct
{
	/* Maps entry names to tagdb_entry_t structures */
	hmap_t map;
	/* Length of tagCap. Stores a 1 for a used and 0 for a free tagId */
	bitarr_t tagIds;
	/* Upper limit on tag IDs */
	size_t tagCap;
	/* The underlying file stream */
	FILE *file;
} tagdb_t;

#pragma endregion

#pragma region Interface Declaration
/* Opens the given filename as tagdb.
	The file stream is used internally after the call and is managed by the tagdb.
	Returns NULL and prints an error message on IO or malloc error. */
tagdb_t *tdb_open(FILE *f);
/* Serializes the tagdb to the stream given with tdb_open.
	Returns false and prints an error message to the given error stream on IO error, true on success. */
bool tdb_flush(tagdb_t *tdb, FILE *log);
/* Releases all resources of the given tagdb. */
void tdb_destroy(tagdb_t *tdb);

/* Retrieves the entry with the given name from the tagdb.
	Check the kind field to tell what kind of entry was returned.
	Returns NULL if no entry is found */
tagdb_entry_t *tdb_get(tagdb_t *tdb, const char *entryName);
/* Creates a new entry of the given kind.
	Returns NULL on malloc failure.
	If an entry already exists, returns it.
	Note that an existing entry may not have the given kind. */
tagdb_entry_t *tdb_ins(tagdb_t *tdb, const char *entryName, tagdb_entrykind_t k);
/* Tries inserting a new entry of the given kind.
	if _entry isn't null, sets it to the found/inserted entry.
	Returns 1 if the value was inserted.
	Returns 0 if the key already existed.
	Returns -1 and sets errno on failure. */
int tdb_tryIns(tagdb_t *tdb, const char *entryName, tagdb_entrykind_t k, tagdb_entry_t **_entry);

/* Removes the entry with the given name from the tagdb.
	Returns true if the entry existed, false otherwise. */
bool tdb_rm(tagdb_t *tdb, const char *entryName);
/* Deletes the entry from the tagdb. */
void tdb_rmE(tagdb_t *tdb, tagdb_entry_t *entry);

/* Gets the entry name of the given entry */
const char *tdb_entryName(tagdb_entry_t *entry);
/* Renames the given entry to the given key.
	Returns 1 if the new key already existed.
	Returns 0 if the entry was successfully moved.
	Returns -1 and sets errno on error. */
int tdb_rename(tagdb_t *tdb, tagdb_entry_t *entry, const char *key);

/* Gets the value for the given tagId in the given file entry */
bool tdb_entry_get(tagdb_entry_t *fileEntry, size_t tagId);
/* Sets the value for the given tagId in the given file entry */
void tdb_entry_set(tagdb_entry_t *fileEntry, size_t tagId, bool value);

#pragma endregion

#pragma region Macros
#define UNUSED __attribute__ ((unused))

/* Iterates over all entries in tdb.
	tdb must be tagdb_t*.
	Declares name as a const char * to the name of the entry.
	Declares entry as tagdb_entry_t* to the current entry.
	Continue and break work as expected. */
#define TDB_FORALL(tdb, name, entry, body) HMAP_FORALL((tdb)->map, const char *name, tagdb_entry_t *entry, body)

/* Iterates over all tags in the given file entry.
	tdb must be tagdb_t*.
	file must be tagdb_entry_t* with kind equal to TDB_FILE_ENTRY.
	Declares tagname as a const char * to the name of the tag.
	Declares entry as tagdb_entry_t* to the current tag.
	Continue and break work as expected. */
#define TDB_FILE_FORALL(tdb, file, tagname, tag, body) TDB_FORALL(tdb, tagname, tag, { \
	if(tag->kind == TDB_TAG_ENTRY && bitarr_get(file->fileTags, tag->tagId)) \
		body \
})

/* Iterates over all files marked with the given tag.
	tdb must be tagdb_t*.
	tag must be tagdb_entry_t* with kind equal to TDB_TAG_ENTRY.
	Declares filename as a const char * to the name of the file.
	Declares entry as tagdb_entry_t* to the current file.
	Continue and break work as expected. */
#define TDB_TAG_FORALL(tdb, tag, filename, file, body) TDB_FORALL(tdb, filename, file, { \
	if(file->kind == TDB_FILE_ENTRY && bitarr_get(file->fileTags, tag->tagId)) \
		body \
})

/* Asserts that an entry is valid */
#define assertEntry(e) assert(!e \
	|| (e->kind == TDB_FILE_ENTRY && bitarr_match(tdb->tagIds, tdb->tagCap, e->fileTags, NULL)) \
	|| (e->kind == TDB_TAG_ENTRY && e->tagId < tdb->tagCap && bitarr_get(tdb->tagIds, e->tagId)))


#pragma endregion

#pragma region Internal Functions
/* Finalizes the given entry. Allocates file tag array or finds a free tagID. */
bool _tdb_mkentry(tagdb_t *tdb, tagdb_entry_t *e, tagdb_entrykind_t k)
{
	assert(k == TDB_FILE_ENTRY || k == TDB_TAG_ENTRY);
	
	if(k == TDB_FILE_ENTRY)
	{
		if(!(e->fileTags = bitarr_new(tdb->tagCap)))
			return false;
	}
	else
	{
		size_t freeId = bitarr_next(tdb->tagIds, 0, tdb->tagCap, false);

		if(freeId == (size_t)-1)
		{ // Need to expand every file entry's bitarray
			size_t newCap = tdb->tagCap * 2;
			
			HMAP_FORALL(tdb->map, UNUSED const char *key, tagdb_entry_t *fe, {
				if(fe->kind != TDB_FILE_ENTRY)
					continue;

				bitarr_t nb = bitarr_resize(fe->fileTags, tdb->tagCap, newCap);
				
				if(!nb)
					return false;

				fe->fileTags = nb;
			})

			bitarr_t ntb = bitarr_resize(tdb->tagIds, tdb->tagCap, newCap);

			if(!ntb)
				return false;

			freeId = tdb->tagCap;
			tdb->tagIds = ntb;
			tdb->tagCap = newCap;
		}

		bitarr_set(tdb->tagIds, freeId, true);
		e->tagId = freeId;
	}

	e->kind = k;
	return true;
}
#pragma endregion

#pragma region Implementation

tagdb_entry_t *tdb_get(tagdb_t *tdb, const char *entryName)
{
	tagdb_entry_t *e = hmap_get(tdb->map, entryName);
	assertEntry(e);
	return e;
}

tagdb_entry_t *tdb_ins(tagdb_t *tdb, const char *entryName, tagdb_entrykind_t k)
{
	tagdb_entry_t *e = NULL;

	if(hmap_tryIns(tdb->map, entryName, (tagdb_entry_t){ .kind = TDB_EMPTY_ENTRY }, &e) == 1 && !_tdb_mkentry(tdb, e, k))
		goto fail;

	assertEntry(e);

	return e;

	fail:
	hmap_delVal(e);
	return NULL;
}

int tdb_tryIns(tagdb_t *tdb, const char *entryName, tagdb_entrykind_t k, tagdb_entry_t **_entry)
{
	tagdb_entry_t *entry;
	int c = hmap_tryIns(tdb->map, entryName, (tagdb_entry_t){ .kind = TDB_EMPTY_ENTRY }, &entry);

	if(_entry)
		*_entry = entry;

	if(c == 1 && !_tdb_mkentry(tdb, entry, k))
	{
		hmap_delVal(entry);
		return -1;
	}

	return c;
}

bool tdb_rm(tagdb_t *tdb, const char *entryName)
{
	tagdb_entry_t *e = hmap_get(tdb->map, entryName);

	assertEntry(e);

	if(e)
		tdb_rmE(tdb, e);

	return (bool)e;
}

void tdb_rmE(tagdb_t *tdb, tagdb_entry_t *entry)
{
	// TODO: Shrink tagCap back down

	if(entry->kind == TDB_FILE_ENTRY)
		free(entry->fileTags);
	else
		bitarr_set(tdb->tagIds, entry->tagId, false);

	hmap_delVal(entry);
}

const char *tdb_entryName(tagdb_entry_t *entry)
{
	return hmap_key(entry);
}

bool tdb_entry_get(tagdb_entry_t *fileEntry, size_t tagId)
{
	return bitarr_get(fileEntry->fileTags, tagId);
}

void tdb_entry_set(tagdb_entry_t *fileEntry, size_t tagId, bool value)
{
	bitarr_set(fileEntry->fileTags, tagId, value);
}

int tdb_rename(tagdb_t *tdb, tagdb_entry_t *entry, const char *key)
{
	if(hmap_get(tdb->map, key))
		return 1;

	tagdb_entry_t e = *entry;
	hmap_delVal(entry);

	return hmap_ins(tdb->map, key, e) ? 0 : -1;
}

void tdb_destroy(tagdb_t *tdb)
{
	if(tdb)
	{
		fclose(tdb->file);
		hmap_destroy(tdb->map);
		bitarr_destroy(tdb->tagIds);
		free(tdb);
	}
}

tagdb_t *tdb_open(FILE *f)
{
	#define ERRPE(msg) { perror(msg); goto err; }
	tagdb_t *tdb = (tagdb_t*)malloc(sizeof(tagdb_t));

	if(!tdb)
	{
		perror("Malloc failure");
		return NULL;
	}

	tdb->file = f;
	tdb->map = hmap_new();
	tdb->tagCap = 16;
	tdb->tagIds = bitarr_new(16);

	if(!tdb->map || !tdb->tagIds)
		ERRPE("Malloc failure")

	do
	{
		char *tagName = readfield(f);

		if(!tagName)
			ERRPE("Cannot read field")
		if(!*tagName)
		{
			free(tagName);
			continue;
		}

		tagdb_entry_t *tag;
		int c = tdb_tryIns(tdb, tagName, TDB_TAG_ENTRY, &tag);
		// tag may get invalidated by file insertion.
		size_t tagId = tag->tagId;

		if(c == -1)
			ERRPE("Cannot insert tag")
		else if(c == 0)
			fprintf(stderr, "Tag '%s' present twice - merging definitions\n", tagName);

		for (;;)
		{
			char *fileName = readfield(f);

			if(!fileName)
			{
				free(tagName);
				ERRPE("Cannot read field")
			}
			if(!*fileName)
			{
				free(fileName);
				break;
			}

			tagdb_entry_t *file = tdb_ins(tdb, fileName, TDB_FILE_ENTRY);

			if(!file)
			{
				free(fileName);
				free(tagName);
				ERRPE("Cannot insert file")
			}

			if(bitarr_get(file->fileTags, tagId))
				fprintf(stderr, "Relationship %s->%s present twice - ignoring duplicate definition\n", tagName, fileName);
			else
				bitarr_set(file->fileTags, tagId, true);
			
			free(fileName);
		}
		
		free(tagName);
	} while(!feof(f));

	clearerr(tdb->file);

	return tdb;
	err:
	tdb_destroy(tdb);
	return NULL;
}

bool tdb_flush(tagdb_t *tdb, FILE *log)
{
	// Does it need to be truncated?
	bool s = !ftruncate(fileno(tdb->file), 0);
	rewind(tdb->file);

	#define WRITE(str) if(!writefield(tdb->file, str)) { \
			fprintf(log, "IO error: %s\n", strerror(errno)); \
			s = false; \
		}
	
	TDB_FORALL(tdb, tagname, tag, {
		if(tag->kind != TDB_TAG_ENTRY)
			continue;
			
		WRITE(tagname)

		TDB_TAG_FORALL(tdb, tag, filename, file, {
			WRITE(filename)
		})

		putc('\n', tdb->file);
	});

	fflush(tdb->file);
	rewind(tdb->file);
	
	return s;
	#undef WRITE
}

#pragma endregion