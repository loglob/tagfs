/* tagfs.c: handled initializing and interfacing with fuse. The actual functions are declared in tagfs.h */
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
};

/* Makes sure the loaded tagdb is valid and obeys all asserts.
	Returns true if the tagdb is clean, false and prints warnings, or exits immediatly on fatal error. */
bool tagfs_chk(tagfs_context_t *context)
{
	bool chk = true;
	#define ERR(...) { fprintf(stderr, "Tagdb invalid; " __VA_ARGS__); chk = false; continue; }


	TDB_FORALL(context->tdb, name, entry,{
		if(name[0] == TAGFS_NEG_CHAR)
			ERR("Entry name '%s' may not start with '%c' as it is reserved for negating tags\n", name, TAGFS_NEG_CHAR)
		if(strchr(name, '/'))
			ERR("Entry name '%s' may not contain '/'\n", name);

		bool exists = !faccessat(context->dirfd, name, F_OK, AT_SYMLINK_NOFOLLOW);

		if(entry->kind == TDB_FILE_ENTRY)
		{
			if(!exists)
				ERR("No file for entry '%s': %s\n", name, strerror(errno))
		}
		else
		{
			if(exists)
				ERR("Tag '%s' conflicts with existing file\n", name)
		}
	})

	struct dirent *ent;

	// iterate over existing real files
	while((ent = readdir(context->dir)))
	{
		if(ent->d_name[0] == TAGFS_NEG_CHAR)
			ERR("Real file '%s' may not start with '%c' as it is reserved for negating tags\n", ent->d_name, TAGFS_NEG_CHAR)
		if(ent->d_name[0] == '.' && tdb_get(context->tdb, ent->d_name + 1))
			ERR("Real file '%s' conflicts with tag '%s'\n", ent->d_name, ent->d_name + 1);
		if(ent->d_type == DT_DIR)
			ERR("Real file '%s' may not be a directory", ent->d_name);
	}

	rewinddir(context->dir);

	return chk;
	#undef ERR
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
			context->log = explain_fopen_or_die(*argv, "a");
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
	if(!tagfs_chk(context))
		goto fail;

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