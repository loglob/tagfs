/* futil.h: Implements helper functions for some file operations */
#ifndef _FUTIL_H
#define _FUTIL_H
#include <stdio.h>
#include "dynbuf.h"

/* Reads a string with escaped newlines and backslashes from a file. Returns NULL and sets errno on malloc failure. */
char *readfield(FILE *f);
/* Writes a string to a file with escaped newlines and backslashes. Returns false and sets errno on failure. */
bool writefield(FILE *f, const char *str);

char *readfield(FILE *f)
{
	dynbuf_t buf = {};
	int c;
	bool esc = false;
	
	while(c = fgetc(f), c != EOF && c)
	{
		if(esc)
		{
			if(c != '\\' && c != '\n')
			{
				if(!dynbuf_ins(&buf, '\\'))
					goto err;
			}

			if(!dynbuf_ins(&buf, c))
				goto err;

			esc = false;
		}
		else
		{
			if(c == '\\')
				esc = true;
			else if(c == '\n')
				break;
			else
			{
				if(!dynbuf_ins(&buf, c))
					goto err;
			}
		}
	}

	// either EOF or \0 ended the field before matching character
	if(esc)
	{
		if(!dynbuf_ins(&buf, '\\'))
			goto err;
	}

	return dynbuf_end(&buf);

	err:
	dynbuf_free(&buf);

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