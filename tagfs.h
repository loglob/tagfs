/* tagfs.h: actually implements the tagfs.
	The real path is set as working directory*/
#pragma once
#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#define _GNU_SOURCE 1
// /usr/include/fuse/fuse_common.h:33:2: error: #error Please add -D_FILE_OFFSET_BITS=64 to your compile flags!
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26

#ifndef DEBUG
	#define NDEBUG
#endif

#include "tagdb.h"
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <fuse/fuse.h>
#include <dirent.h>
#include <time.h>
#include <assert.h>

#pragma region Macros

#define CONTEXT ((tagfs_context_t*)fuse_get_context()->private_data)
#define TDB (CONTEXT->tdb)
#define TAGFS_NEG_CHAR '-'
#define lprintf(...) fprintf(CONTEXT->log, __VA_ARGS__)
#define lflush() fflush(CONTEXT->log)

#ifdef DEBUG
#define dbprintf(...) lprintf(__VA_ARGS__), lflush()
#else
#define dbprintf(...) ;
#endif

#pragma endregion

#pragma region Types

typedef struct
{
	/* The tag database */
	tagdb_t *tdb;
	/* The real directory file descriptor */
	int dirfd;
	/* The real directory stream */
	DIR *dir;
	/* The log file */
	FILE *log;
	/* The stat of the underlying real directory */
	struct stat realStat;
} tagfs_context_t;

enum tagfs_flags
{
	// Accepts file entries
	TFS_FILE = 1,
	// Accepts tag entries
	TFS_TAG = 2, 
	// Accepts files or tags
	TFS_ANY = TFS_FILE | TFS_TAG,
	// Checks for real files and creates an entry for them. Only valid if TFS_FILE is set.
	TFS_MKFILE = 4,
	// Checks for tagnames prefixed with '.'. Only valid if TFS_TAG is set.
	TFS_CHKDOT = 8,
	// If MKFILE is specified, doesn't actually create the file. Returns NULL and sets errno to 0 instead.
	TFS_NOCREAT = 16,
	// Looks for any kind of entry, checks tags prefixed with . and existing files
	TFS_CHKALL = TFS_FILE | TFS_TAG | TFS_MKFILE | TFS_CHKDOT | TFS_NOCREAT,
	// If TFS_TAG is specified, check for tags prefixed with '-'
	TFS_CHKNEG = 32,
};

#pragma endregion

#pragma region Internal Functions

bool specialDir(const char *path)
{
	if(*path == '/')
		path++;

	return !*path || !strcmp(path, ".") || !strcmp(path, "..");
}

bool tdbFile(const char *path)
{
	if(*path == '/')
		path++;

	return strncmp(path, ".tagdb", 6) == 0;
}

static char *split(const char *_path, const char **_fname)
{
	if(*_path == '/')
		_path++;

	//assert(*_path != '/');
	const char *fname = strrchr(_path, '/');

	if(_fname)
		*_fname = fname ? fname + 1 : _path;

	if(fname && fname != _path)
	{
		size_t pl = (size_t)fname - (size_t)_path;
		char *path = malloc(pl + 1);

		if(path)
		{
			memcpy(path, _path, pl);
			path[pl] = 0;
		}

		return path;
	}
	else
		return calloc(1,1);
}


/* Attempts to retrieve a tagdb entry. flags must contain TFS_FILE, TFS_TAG or both.
	Filters out tagdb files and special dirs. */
static inline tagdb_entry_t *tagfs_get(const char *name, enum tagfs_flags flags)
{
	#define fail(eno) { errno=eno; return NULL; }
	//dbprintf("GET: %s; %u\n", name, flags);

	if(!(flags & (TFS_FILE | TFS_TAG)))
	{
	//	dbprintf("GET rejected because neither file nor tag are given\n");
		errno = EINVAL;
		return NULL;
	}

	tagdb_entry_t *e = tdb_get(CONTEXT->tdb, name);

	if(e)
	{
	//	dbprintf("GET found entry\n");

		if(e->kind == TDB_FILE_ENTRY)
		{
			if(flags & TFS_FILE)
				return e;
			else
				fail(ENOTDIR)
		}
		else
		{
			assert(e->kind == TDB_TAG_ENTRY);

			if(flags & TFS_TAG)
				return e;
			else
				fail(EISDIR)
		}
	}

	if((flags & TFS_CHKDOT) && (flags & TFS_TAG) && *name == '.' && (e = tdb_get(CONTEXT->tdb, name + 1)))
	{
	//	dbprintf("GET found dottag\n");

		if(e->kind == TDB_TAG_ENTRY)
			return e;
	}
	else if((flags & TFS_CHKNEG) && (flags & TFS_TAG) && *name == '-' && (e = tdb_get(CONTEXT->tdb, name + 1)))
	{
	//	dbprintf("GET found -tag\n");

		if(e->kind == TDB_TAG_ENTRY)
			return e;
	}

	if((flags & TFS_MKFILE) && (flags & TFS_FILE) && !tdbFile(name) && !specialDir(name) && !faccessat(CONTEXT->dirfd, name, F_OK, AT_SYMLINK_NOFOLLOW))
	{
	//	dbprintf("GET found existing file\n");

		if(flags & TFS_NOCREAT)
			fail(0);
		
		return tdb_ins(CONTEXT->tdb, name, TDB_FILE_ENTRY);
	}
	
	//dbprintf("GET fail\n");
	fail(ENOENT);
	#undef fail
}

/* Evaluates the given path as a tagdb query.
	Populates pos and neg with the positive and negative matching masks.
	path gets partly overwritten.
	Exits early with ENOENT on queries that contain a tag and its negation.
	Returns false and sets errno on failure.
	Returns true  on success. */
static bool tagfs_query(char *path, bitarr_t pos, bitarr_t neg)
{
	char *sav = NULL;
	
	for(char *tok = strtok_r(path, "/", &sav); tok; tok = strtok_r(NULL, "/", &sav))
	{
		bool mod = true;

		if(*tok == TAGFS_NEG_CHAR)
		{
			mod = false;
			tok++;
		}
		
		// Check for dot-prefixed tags only if the - prefix isn't used
		tagdb_entry_t *e = tagfs_get(tok, TFS_TAG | (mod ? TFS_CHKDOT : 0));

		if(!e)
			return false;

		// Check for impossible queries
		if(bitarr_get(mod ? neg : pos, e->tagId))
		{
			errno = ENOENT;
			return false;
		}

		bitarr_set(mod ? pos : neg, e->tagId, true);
	}

	return true;
}

/* Determines if the given path, without filename, is a valid tag query.
	if _fname isn't NULL, sets it to the filename of the path.
	returns true on success.
	returns false and sets errno on failure. */
static bool tagfs_validQuery(const char *_path, const char **_fname)
{
	const char *fname;
	char *path = split(_path, &fname);

	if(!path)
		return false;

	if(fname != _path)
	{
		bitarr_t pos = bitarr_new(TDB->tagCap);
		bitarr_t neg = bitarr_new(TDB->tagCap);

		if(pos && neg)
			tagfs_query(path, pos, neg);

		free(path);
		free(pos);
		free(neg);

		if(errno)
			return false;
	}
	else
		free(path);

	if(_fname)
		*_fname = fname;

	return true;
}

/* Determines if the given path resolves to a valid tag or file.
	Checks if all tags listed in the path are valid and if the file matches the query.
	Doesn't create new tdb entries.
	Finds negated tags.
	if _entry isn't NULL, and the path resolves to a tagdb entry, stores the entry in _entry.
	if _fname isn't NULL, stores the filename in fname.
	Returns the kind of entry found.
	Returns 0 and sets errno on failure. */
static tagdb_entrykind_t tagfs_resolve(const char *_path, tagdb_entry_t **_entry, const char **_fname)
{
	#define ERR(eno) { errno = eno; goto err; }
	//dbprintf("RESOLVE: %s\n", _path);

	tagdb_t *tdb = TDB;
	errno = 0;
	bitarr_t pos = NULL, neg = NULL;
	const char *fname;
	char *path = split(_path, &fname);

	// Check if path contains query
	if(*path)
	{
		pos = bitarr_new(tdb->tagCap);
		neg = bitarr_new(tdb->tagCap);

		if(path && pos && neg)
			tagfs_query(path, pos, neg);
		
		free(path);
		
		if(errno)
		{
			free(neg);
			free(pos);
			return 0;
		}
	}
	else
		free(path);

	if(_fname)
		*_fname = fname;

	if(specialDir(fname))
		return TDB_TAG_ENTRY;
	if(tdbFile(fname))
	{
		errno = ENOENT;
		return TDB_EMPTY_ENTRY;	
	}
	
	tagdb_entry_t *entry = tagfs_get(fname, TFS_CHKALL | TFS_CHKNEG);

	if(entry)
	{
	//	dbprintf("Found %s entry\n", tagdb_entrykind_names[entry->kind]);

		if(entry->kind == TDB_FILE_ENTRY && pos && !bitarr_match(entry->fileTags, tdb->tagCap, pos, neg))
			ERR(ENOENT)

		if(_entry)
			*_entry = entry;
	}
	else if(errno)
		goto err;
	else
	{
	//	dbprintf("Found existing file\n");
		
		if(pos && bitarr_any(pos, tdb->tagCap, true))
			ERR(ENOENT)
	}

	err:
	free(pos);
	free(neg);

	return errno ? TDB_EMPTY_ENTRY : entry ? entry->kind : TDB_FILE_ENTRY;
	#undef ERR
}

/* Determines if the givne entry name exists.
	Sets errno to 0, regardless of output. */
inline static bool tagfs_exists(const char *entry)
{
	errno = 0;

	if(tagfs_get(entry, TFS_CHKALL) || !errno)
		return true;

	errno = 0;
	return false;
}


#pragma endregion

#pragma region Implementation

int tagfs_readdir(const char *_path, void *buf, fuse_fill_dir_t filler, UNUSED off_t offset, UNUSED struct fuse_file_info *fi)
{
	dbprintf("READDIR: %s\n", _path);
	#define ERR(eno) { errno = eno; goto err; }
	errno = 0;
	tagfs_context_t *context = CONTEXT;
	tagdb_t *tdb = context->tdb;

	char *path = strdup(_path);
	bitarr_t positive = bitarr_new(tdb->tagCap);
	bitarr_t negative = bitarr_new(tdb->tagCap);
	// Contains all tags that have at least one listed file
	bitarr_t dirmask = bitarr_new(tdb->tagCap);
	
	if(!path || !positive || !negative || !dirmask)
		ERR(ENOMEM)

	if(!tagfs_query(path, positive, negative))
		goto err;
	
	int anyP = bitarr_any(positive, tdb->tagCap, true);
	struct dirent *ent;


	// iterate over existing real files
	while((ent = readdir(context->dir)))
	{
		// filter out the .tagdb file
		if(tdbFile(ent->d_name))
			continue;
		
		tagdb_entry_t *entry = tdb_get(tdb, ent->d_name);

		if(entry)
		{
			assert(entry->kind == TDB_FILE_ENTRY);

			if(!bitarr_match(entry->fileTags, tdb->tagCap, positive, negative))
				continue;

			bitarr_eqor(dirmask, tdb->tagCap, entry->fileTags);
		}
		else if(anyP)
			continue;

		struct stat s;
		if(filler(buf, ent->d_name, fstatat(context->dirfd, ent->d_name, &s, AT_SYMLINK_NOFOLLOW) ? NULL : &s, 0))
			ERR(ENOMEM)	
	}

	rewinddir(context->dir);

	TDB_FORALL(TDB, name, entry, {
		if(entry->kind != TDB_TAG_ENTRY)
			continue;
		if((anyP && bitarr_get(positive, entry->tagId)) || bitarr_get(negative, entry->tagId))
			continue;
		if(bitarr_get(dirmask, entry->tagId))
		{
			if(filler(buf, name, &context->realStat, 0))
				ERR(ENOMEM)
		}
		else
		{ // put the tag as a dotfile
			size_t nl = strlen(name);
			char *dname = malloc(nl + 2);

			if(!dname)
				goto err;

			*dname = '.';
			memcpy(dname + 1, name, nl + 1);
			int f = filler(buf, dname, NULL, 0);
			free(dname);

			if(f)
				ERR(ENOMEM)
		}

		#ifdef LIST_NEGATED_TAGS
		size_t nl = strlen(name);
		char *nname = malloc(nl + 2);

		if(!nname)
			goto err;

		*nname = '-';
		memcpy(nname + 1, name, nl + 1);
		int f = filler(buf, nname, NULL, 0);
		free(nname);
		
		if(f)
			ERR(ENOMEM)
		#endif
	})

	err:
	free(path);
	free(positive);
	free(negative);
	free(dirmask);

	return -errno;
	#undef ERR
}

int tagfs_getattr(const char *path, struct stat *_stat)
{
	dbprintf("GETATTR: %s\n", path);

	errno = 0;
	tagfs_context_t *context = CONTEXT;
	const char *fname;

	if(path[0] == '/' && !path[1])
		goto gotRoot;

	switch(tagfs_resolve(path, NULL, &fname))
	{
		case TDB_TAG_ENTRY:
			// TODO: proper access times
			gotRoot:
			dbprintf("GETATTR found tag\n");
			*_stat = context->realStat;

			return 0;
		break;

		case TDB_FILE_ENTRY:
			dbprintf("GETATTR found file\n");
			fstatat(CONTEXT->dirfd, fname, _stat, AT_EMPTY_PATH | AT_SYMLINK_NOFOLLOW);
		break;
	}

	dbprintf("GETATTR exits with %d (%s)\n", errno, strerror(errno));

	return -errno;
}

int tagfs_mknod(const char *_path, mode_t mode, dev_t dev)
{
	dbprintf("MKNOD: %s\n", _path);
	errno = 0;
	tagdb_t *tdb = TDB;
	const char *fname;
	char *path = split(_path, &fname);
	
	if(!path)
		return -ENOMEM;
	if(tagfs_get(fname, TFS_CHKALL) || !errno || tdbFile(fname))
		return -EEXIST;
	if(fname[0] == TAGFS_NEG_CHAR)
		return -EINVAL;
	
	tagdb_entry_t *e = NULL;

	// given path contains a query
	if(*path)
	{
		errno = 0;
		bitarr_t positive = bitarr_new(tdb->tagCap);
		bitarr_t negative = bitarr_new(tdb->tagCap);

		if(!positive || !negative)
			goto err;

		if(!tagfs_query(path, positive, negative))
			goto err;

		e = tdb_ins(tdb, fname, TDB_FILE_ENTRY);

		if(!e)
			goto err;

		bitarr_copy(e->fileTags, tdb->tagCap, positive);
		
		err:
		free(path);
		free(positive);
		free(negative);

		if(errno)
			return -errno;
	}
	else
		free(path);

	errno = 0;
	if(mknodat(CONTEXT->dirfd, fname, mode, dev) && e)
		tdb_rmE(TDB, e);

	return -errno;
}

int tagfs_mkdir(const char *_path, mode_t mode)
{
	dbprintf("MKDIR: %s\n", _path);
	errno = 0;
	tagdb_t *tdb = TDB;
	const char *fname;

	if((mode & 0777) != (CONTEXT->realStat.st_mode & 0777))
		return -ENOTSUP;
	if(!tagfs_validQuery(_path, &fname))
		return -errno;
	if(tagfs_get(fname, TFS_CHKALL) || !errno || tdbFile(fname) || specialDir(fname))
		return -EEXIST;
	if(fname[0] == TAGFS_NEG_CHAR)
		return -EINVAL;
		
	return tdb_ins(tdb, fname, TDB_TAG_ENTRY) ? 0 : -errno;
}

int tagfs_utimens(const char *_path, const struct timespec tv[2])
{
	#define ERR(eno) { return -eno; }
	dbprintf("UTIMENS: %s\n", _path);

	tagfs_context_t *context = CONTEXT;
	errno = 0;

	const char *fname;
	tagdb_entrykind_t k = tagfs_resolve(_path, NULL, &fname);
	
	if(k == TDB_FILE_ENTRY)
	{
		if(utimensat(context->dirfd, fname, tv, AT_SYMLINK_NOFOLLOW))
			return -errno;
	}
	else if(k == TDB_TAG_ENTRY)
		return -ENOTSUP;

	return -errno;
	#undef ERR
}

int tagfs_open(const char *path, struct fuse_file_info *ffi)
{
	errno = 0;
	const char *fname;
	tagdb_entrykind_t kind = tagfs_resolve(path, NULL, &fname);

	if(!kind)
		return -errno;
	if(kind != TDB_FILE_ENTRY)
		return -EISDIR;

	int fd = openat(CONTEXT->dirfd, fname, ffi->flags);
	
	if(fd < 0)
		return -errno;

	ffi->fh = fd;
	return 0;
}

int tagfs_release(const char *path, struct fuse_file_info *ffi)
{
	dbprintf("RELEASE: %s\n", path);
	close(ffi->fh);
	return 0;
}

int tagfs_read(UNUSED const char *path, char *buf, size_t len, off_t offset, struct fuse_file_info *ffi)
{
	return pread(ffi->fh, buf, len, offset);
}

int tagfs_write(UNUSED const char *_path, const char *buf, size_t len, off_t offset, struct fuse_file_info *ffi)
{
	return pwrite(ffi->fh, buf, len, offset);
}

int tagfs_truncate(const char *_path, off_t len)
{
	dbprintf("TRUNCATE: %s\n", _path);
	const char *fname;
	tagdb_entrykind_t kind = tagfs_resolve(_path, NULL, &fname);

	if(!kind)
		return -errno;
	if(kind != TDB_FILE_ENTRY)
		return -EISDIR;
	
	int fd = openat(CONTEXT->dirfd, fname, O_WRONLY);

	if(fd == -1)
		return -errno;

	if(ftruncate(fd, len))
	{
		close(fd);
		return -errno;
	}

	return 0;
}

int tagfs_unlink(const char *_path)
{
	dbprintf("UNLINK: %s\n", _path);
	tagdb_entry_t *entry = NULL;
	const char *fname;
	tagdb_entrykind_t kind = tagfs_resolve(_path, &entry, &fname);

	if(!kind)
		return -errno;
	if(entry)
		tdb_rmE(TDB, entry);
	if((kind == TDB_FILE_ENTRY) && unlinkat(CONTEXT->dirfd, fname, 0))
		return -errno;

	return 0;
}

int tagfs_rmdir(const char *_path)
{
	dbprintf("RMDIR: %s\n", _path);
	tagdb_entry_t *entry = NULL;
	const char *fname;
	tagdb_entrykind_t kind = tagfs_resolve(_path, &entry, &fname);

	if(!kind)
		return -errno;
	if(kind != TDB_TAG_ENTRY)
		return -ENOTDIR;
	if(entry)
		tdb_rmE(TDB, entry);

	return 0;
}

int tagfs_rename(const char *path, const char *npath)
{
	dbprintf("RENAME: %s -> %s\n", path, npath);
	#define ERR(eno) { errno = eno; goto err; }
	errno = 0;
	tagdb_entry_t *entry = NULL;
	const char *ofname;
	tagdb_entrykind_t kind = tagfs_resolve(path, &entry, &ofname);
	tagdb_t *tdb = TDB;

	if(!kind)
		return -errno;
	
	const char *nfname;
	char *query = split(npath, &nfname);
	bitarr_t pos = bitarr_new(tdb->tagCap);
	bitarr_t neg = bitarr_new(tdb->tagCap);

	if(!query || !pos || !neg)
		goto err;

	if(!tagfs_query(query, pos, neg))
		goto err;

	if(kind == TDB_FILE_ENTRY)
	{
		tagdb_entry_t *e = entry;

		if(!e && bitarr_any(pos, tdb->tagCap, true))
		{
			e = tdb_ins(tdb, nfname, TDB_FILE_ENTRY);
			
			if(!e)
				goto err;
		}

		if(e)
		{
		#ifdef RELATIVE_RENAME	
			if(bitarr_all(pos, tdb->tagCap, false) && bitarr_all(neg, tdb->tagCap, false))
				bitarr_fill(e->fileTags, tdb->tagCap, false);
			else
				bitarr_merge(e->fileTags, tdb->tagCap, pos, neg);
		#else
			bitarr_copy(e->fileTags, tdb->tagCap, pos);
		#endif
		}
	}
	
	if(strcmp(nfname, ofname))
	{
		if(kind == TDB_FILE_ENTRY && renameat(CONTEXT->dirfd, ofname, CONTEXT->dirfd, nfname))
			return -errno;
		if(entry && tdb_rename(tdb, entry, nfname))
			return -errno;
	}

	err:
	free(query);
	free(pos);
	free(neg);

	return -errno;
	#undef ERR
}

void *tagfs_init(struct fuse_conn_info *conn)
{
	char tbuf[201];
	time_t t = time(NULL);
	struct tm lt;
	localtime_r(&t, &lt);

	if(!strftime(tbuf, 200, "%c", &lt)) // the locale is weird or empty, use a fallback
		strftime(tbuf, 200, "%F %T", &lt); // this will fail by the year 10^185, be sure to fix it by then

	long pos = ftell(CONTEXT->log);

	if(pos && pos != -1)
	{ // print a separator for easier browsing of the log file
		for (int i = 0; i < 80; i++)
			fputc('=', CONTEXT->log);
		
		fputc('\n', CONTEXT->log);
	}

	lprintf("tagfs started at %s\nFuse protovol V%u.%u\n", tbuf, conn->proto_major, conn->proto_minor);
	dbprintf("Debugging printouts enabled.\n");
	lflush();

	return CONTEXT;
}

void tagfs_destroy(void *_context)
{
	tagfs_context_t *c = (tagfs_context_t*)_context;

	fprintf(c->log, "tagfs exiting.\n");

	tdb_flush(c->tdb, c->log);
	fflush(c->log);


//	fclose(c->log);
//	closedir(c->dir);
//	tdb_destroy(c->tdb);
//	free(c);
}

#pragma endregion