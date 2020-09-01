#define _POSIX_C_SOURCE 200809L
#include "tagdb.h"
#include <stdio.h>
#include <ctype.h>

tagdb_t *tdb = NULL;

void help(char **args);
void get(char **args);
void tag(char **args);
void file(char **args);
void del(char **args);
void add(char **args);
void sub(char **args);
void list(char **args);

struct cmd
{
	const char *command;
	const char *description;
	void (*func)(char **);
};

struct cmd commands[] =
{
	{ "help", "prints this", help },
	{ "get", "Looks up the given token(s)", get },
	{ "tag", "creates new tag(s)", tag },
	{ "file", "creates new file(s)", file },
	{ "del", "deletes the token(s)", del },
	{ "add", "marks all the given files with all given tags", add },
	{ "sub", "removes all the given tags from all the given files", sub },
	{ "list", "lists all entries", list },
};

void help(char **args)
{
	printf("Help:\n");

	for (size_t i = 0; i < sizeof(commands)/sizeof(struct cmd); i++)
		printf("	%s	%s\n", commands[i].command, commands[i].description);
	
	printf("empty lines exit.\n");
}

void get(char **keys)
{
	char **tagnames = calloc(tdb->tagCount, sizeof(char*));

	if(!tagnames)
	{
		perror("malloc failure");
		return;
	}

	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_get(tdb, keys[i]);
		printf("%s: %s", keys[i], e ? tagdb_entrykind_names[e->kind] : "Doesn't exist");

		if(e)
		{
			bool any = false;

			if(e->kind == TDB_FILE_ENTRY)
			{
				TDB_FILE_FORALL(tdb, e, tagname, tag)
					if(!any)
						printf(" marked with:\n");
			
					printf("	%s (%zu)\n", tagname, tag->_data.tagId);

					any = true;
				TDB_FORALL_END

				if(!any)
					printf(" without any tags\n");
			}
			else if(e->kind == TDB_TAG_ENTRY)
			{
				TDB_TAG_FORALL(tdb, e, filename, file)
					if(!any)
						printf(" marking file(s):\n");

					printf("	%s\n", filename);

					any = true;
				TDB_FORALL_END

				if(!any)
					printf(" without any files\n");
			}
		}

	}
}

void tag(char **keys)
{
	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_ins(tdb, keys[i]);
		
		if(!e)
			perror("malloc failure");
		else if(e->kind)
			printf("%s: already exists\n", keys[i]);
		else if(!tdb_entry_new(tdb, e, TDB_TAG_ENTRY))
			perror("malloc failure");
	}
}

void file(char **keys)
{
	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_ins(tdb, keys[i]);
		
		if(!e)
			perror("malloc failure");
		else if(e->kind)
			printf("%s: already exists\n", keys[i]);
		else if(!tdb_entry_new(tdb, e, TDB_FILE_ENTRY))
			perror("malloc failure");
	}
}

void del(char **keys)
{
	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_get(tdb, keys[i]);

		if(!e)
			printf("%s: Doesn't exist\n", keys[i]);
		else
			tdb_rm(tdb, keys[i]);
	}
}

void add(char **keys)
{
	bitarr_t *bits = bitarr_new(tdb->tagCount);

	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_get(tdb, keys[i]);
		
		if(e && e->kind == TDB_TAG_ENTRY)
			bitarr_set(bits, e->_data.tagId, true);
	}

	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_get(tdb, keys[i]);
		
		if(!e)
			printf("%s: Doesn't exist\n", keys[i]);
		else if(e->kind == TDB_FILE_ENTRY)
			bitarr_add(e->_data.fileData, bits, tdb->tagCount);
	}

	free(bits);
}

void sub(char **keys)
{
	bitarr_t *bits = bitarr_new(tdb->tagCount);

	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_get(tdb, keys[i]);
		
		if(e && e->kind == TDB_TAG_ENTRY)
			bitarr_set(bits, e->_data.tagId, true);
	}

	for (size_t i = 0; keys[i]; i++)
	{
		tagdb_entry_t *e = tdb_get(tdb, keys[i]);
		
		if(!e)
			printf("%s: Doesn't exist\n", keys[i]);
		else if(e->kind == TDB_FILE_ENTRY)
			bitarr_sub(e->_data.fileData, bits, tdb->tagCount);
	}

	free(bits);
}

void list(char **keys)
{
	tagdb_entrykind_t kind = 4;

	if(keys[0])
	{
		if(!strcmp(keys[0], "tags"))
			kind = TDB_TAG_ENTRY;
		else if(!strcmp(keys[0], "files"))
			kind = TDB_FILE_ENTRY;
	}
	
	HMAP_FORALL(&tdb->map, name, _entry)
		tagdb_entry_t *entry = (tagdb_entry_t*)_entry;

		printf("'%s': %s\n", name, tagdb_entrykind_names[entry->kind]);
	TDB_FORALL_END
/*
	TDB_FORALL(tdb, name, entry)
		if(entry->kind & kind)
			printf("%s: %s\n", name, tagdb_entrykind_names[entry->kind]);
	TDB_FORALL_END */
}

char *stripend(char *str)
{
	size_t i = strlen(str);

	while(i--)
	{
		if(isspace(str[i]))
			str[i] = 0;
		else
			break;
	}

	return str;
}

void split(char *str, char **args)
{
	size_t a = 0;
	int w = 1;

	for (size_t i = 0; str[i]; i++)
	{
		if(isspace(str[i]))
		{
			str[i] = 0;
			w = 1;
		}
		else
		{
			if(w)
				args[a++] = &str[i];

			w = 0;
		}
	}

	args[a] = NULL;
}

int main(int argc, char **argv)
{
	if(argc < 2)
	{
		printf("No tagdb file given\n");
		return 1;
	}

	tdb = tdb_open(argv[1]);

	if(!tdb)
	{
		printf("Cannot open tagdb\n");

		return 1;
	}

	char ln[200];
	char *args[20];

	printf("tagdb loaded.\n");
	fflush(stdout);

	while(printf("> "), *stripend(fgets(ln, 200, stdin)))
	{
		split(ln, args);

		for (size_t i = 0; i < sizeof(commands)/sizeof(struct cmd); i++)
		{
			if(!strcmp(commands[i].command, args[0]))
			{
				commands[i].func(args + 1);
				goto found;
			}
		}

		printf("Unknown operation '%s'\n", args[0]);
		found:;
	}

	printf("Saving tagdb\n");
	tdb_flush(tdb);
}