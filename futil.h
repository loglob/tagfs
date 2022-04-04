/* futil.h: Implements helper functions for some file operations */
#ifndef _FUTIL_H
#define _FUTIL_H
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

/* Reads a string with escaped newlines and backslashes from a file. Returns NULL and sets errno on malloc failure. */
char *readfield(FILE *f);
/* Writes a string to a file with escaped newlines and backslashes. Returns false and sets errno on failure. */
bool writefield(FILE *f, const char *str);

char *readfield(FILE *f)
{
	char *buf = NULL;
	size_t len;
	FILE *bf = open_memstream(&buf, &len);

	if(!bf)
		goto _err;

	int c;
	bool esc = false;

	while(c = fgetc(f), c != EOF && c)
	{
		if(esc)
		{
			if(c != '\\' && c != '\n' && fputc('\\', bf) == EOF)
				goto err;
			if(fputc(c, bf) == EOF)
				goto err;

			esc = false;
		}
		else
		{
			if(c == '\\')
				esc = true;
			else if(c == '\n')
				break;
			else if(fputc(c, bf) == EOF)
				goto err;
		}
	}

	// either EOF or \0 ended the field before matching character
	if(esc && fputc('\\', bf) == EOF)
		goto err;

	if(fclose(bf))
		goto _err;

	return buf;

	err:
	fclose(bf);
	_err:
	free(buf);

	return NULL;
}

bool writefield(FILE *f, const char *str)
{
	#define _PUTC(chr) { if(fputc(chr, f) == -1) return false; }
	char c;

	while((c = *str++))
	{
		if(c == '\\' || c == '\n')
			_PUTC('\\');

		_PUTC(c);
	}

	_PUTC('\n');

	return true;
	#undef _PUTC
}

#endif