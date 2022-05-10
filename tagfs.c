/* tagfs.c: handled initializing and interfacing with fuse. The actual functions are declared in tagfs.h */
#include "config.h"
#include "tagfs.h"
#include <unistd.h>
#include <limits.h>
#include <libexplain/opendir.h>
#include <libexplain/dirfd.h>
#include <libexplain/malloc.h>
#include <libexplain/openat.h>
#include <libexplain/fopen.h>
#include <libexplain/fstat.h>
#include <libexplain/fdopen.h>
#include <libexplain/read.h>
#include <libexplain/write.h>
#include "explain_pthread_rwlock_init_or_die.h"

const char *usage =
	"Proper usage:\n"
	"	tagfs [-l|--log <log file>] <mount point> [FUSE arguments...]\n"
	"	tagfs [-q|--quiet] <mount point> [FUSE arguments...]\n";

static struct fuse_operations op =
{
	.readdir = tagfs_readdir,
	.init = tagfs_init,
	.getattr = tagfs_getattr,
	.mknod = tagfs_mknod,
	.destroy = tagfs_destroy,
	.mkdir = tagfs_mkdir,
	.utimens = tagfs_utimens,
	.open = tagfs_open,
	.read = tagfs_read,
	.write = tagfs_write,
	.truncate = tagfs_truncate,
	.unlink = tagfs_unlink,
	.rmdir = tagfs_rmdir,
	.rename = tagfs_rename,
	.release = tagfs_release,
	.fsync = tagfs_fsync,
	.getxattr = tagfs_getxattr,
	.setxattr = tagfs_setxattr,
	.listxattr = tagfs_listxattr,
};

/* Makes sure the loaded tagdb is valid and obeys all asserts.
	Returns 0 if the tdb is clean, 1 on recoverable error and -1 on irrecoverable error. */
int tagfs_chk(tagfs_context_t *context)
{
	int err = 0;
	#define IERR(...) { fprintf(stderr, "Tagdb invalid; " __VA_ARGS__); err = -1; continue; }

	TDB_FORALL(context->tdb, name, entry,{
		if(name[0] == TAGFS_NEG_CHAR)
			IERR("Entry name '%s' may not start with '%c' as it is reserved for negating tags\n", name, TAGFS_NEG_CHAR)
		if(name[0] == '.' && tdb_get(context->tdb, name + 1))
			IERR("Entry name '%s' conflicts with entry '%s'\n", name, name)
		if(strchr(name, '/'))
			IERR("Entry name '%s' may not contain '/'\n", name)
		if(entry->kind == TDB_TAG_ENTRY && !faccessat(context->dirfd, name, F_OK, AT_SYMLINK_NOFOLLOW))
			IERR("Tag '%s' conflicts with existing file\n", name)
	})

	struct dirent *ent;

	// iterate over existing real files
	while((ent = readdir(context->dir)))
	{
		if(specialDir(ent->d_name) || tdbFile(ent->d_name))
			continue;
		if(ent->d_name[0] == TAGFS_NEG_CHAR)
			IERR("Real file '%s' may not start with '%c' as it is reserved for negating tags\n", ent->d_name, TAGFS_NEG_CHAR)
		if(ent->d_name[0] == '.' && tdb_get(context->tdb, ent->d_name + 1))
			IERR("Real file '%s' conflicts with tag '%s'\n", ent->d_name, ent->d_name + 1);
		if(ent->d_type == DT_DIR)
			IERR("Real file '%s' may not be a directory", ent->d_name);
	}

	if(err)
		return err;

	// seek and remove nonexistant files
	rerun:
	TDB_FORALL(context->tdb, name, entry,{
		if(entry->kind == TDB_FILE_ENTRY && faccessat(context->dirfd, name, F_OK, AT_SYMLINK_NOFOLLOW))
		{
			fprintf(stderr, "No file for entry '%s': %s\nRemoving bad entry from TDB\n", name, strerror(errno));
			tdb_rmE(context->tdb, entry);
			err = 1;
			// the indices used by TDB_FORALL are no longer valid after hashmap update
			goto rerun;
		}
	})


	rewinddir(context->dir);

	return err;
	#undef IERR
}

#define printdie(...) printf(__VA_ARGS__), exit(EXIT_FAILURE)

int main(int argc, char **argv)
{
	tagfs_context_t *context = explain_malloc_or_die(sizeof(tagfs_context_t));

	// parse arguments
	{
		#define push() argc--,argv++
		push();

		if(argc && (!strcmp("-l", *argv) || !strcmp("--log", *argv)))
		{
			if(argc < 2)
				printdie("Invalid usage; missing logfile after %s\n%s", *argv, usage);

			push();
			if(strcmp(*argv, "-"))
				context->log = explain_fopen_or_die(*argv, "a");
			else
				context->log = stderr;
			push();
		}
		else
		{
			if(argc && (!strcmp("-q", *argv) || !strcmp("--quiet", *argv)))
				push();

			context->log = explain_fopen_or_die("/dev/null", "w");
		}

		if(!argc)
			printdie("Invalid usage; missing mount point\n%s", usage);

		#undef push
	}

	// init lock
	explain_pthread_rwlock_init_or_die(&context->lock, NULL);

	// open the base directory
	context->dir = explain_opendir_or_die(*argv);
	context->dirfd = explain_dirfd_or_die(context->dir);
	explain_fstat_or_die(context->dirfd, &context->realStat);

	context->tdb = tdb_open(
		explain_fdopen_or_die(
			explain_openat_or_die(context->dirfd, ".tagdb",
				O_RDWR | O_CREAT,
				S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH),
			"r+"));

	if(!context->tdb)
		goto fail;

	// final check of the tagdb and real directory
	int chk = tagfs_chk(context);

	if(chk == -1)
		goto fail;
	if(chk == 1)
	{
		char bakfile[400];
		time_t nowt;
		time(&nowt);
		struct tm now;
		localtime_r(&nowt, &now);
		int tries = 0;

		do
		{
			snprintf(bakfile, 400, tries ? ".tagdb.%04u-%02u-%02u (%u)" : ".tagdb.%04u-%02u-%02u",
				1900 + now.tm_year, 1+ now.tm_mon, now.tm_mday, tries);
			tries++;
		} while (!faccessat(context->dirfd, bakfile, F_OK, AT_SYMLINK_NOFOLLOW));

		printf("Creating backup of tagdb in '%s'\n", bakfile);
		int bakfd = explain_openat_or_die(context->dirfd, bakfile, O_WRONLY | O_CREAT | O_TRUNC, S_IWGRP | S_IWUSR | S_IRGRP | S_IRUSR | S_IROTH);
		int tdbfd = explain_openat_or_die(context->dirfd, ".tagdb", O_RDONLY, 0);

		for(;;)
		{
			char buf[4096];
			size_t r = explain_read_or_die(tdbfd, buf, sizeof(buf));
			explain_write_or_die(bakfd, buf, r);

			if(r < sizeof(buf))
				break;
		}

		close(bakfd);
		close(tdbfd);
	}

	fprintf(stderr, "Mounting at '%s'\n", *argv);

	char **fuseopts = explain_malloc_or_die((argc + 4) * sizeof(char*));

	fuseopts[0] = "tagfs";
	fuseopts[1] = *argv;
	fuseopts[2] = "-o";
	fuseopts[3] = "nonempty";
	fuseopts[argc+3] = NULL;

	for (int i = 1; i < argc; i++)
		fuseopts[i + 3] = argv[i];

	fuse_main(argc + 3, fuseopts, &op, context);

	fail:
	if(context)
	{
		if(context->tdb)
			tdb_destroy(context->tdb);

		if(context->dir)
			closedir(context->dir);

		if(context->log)
			fclose(context->log);

		free(context);
	}

	return EXIT_FAILURE;

	#undef ERR_PF
	#undef ERR_PE
}