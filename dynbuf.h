/* Implements a dynamically growing buffer for building strings. */
#ifndef _DYNBUF_H
#define _DYNBUF_H
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>

#define DYNBUF_GRAIN 64

typedef struct
{
	size_t len;
	char *buf;
} dynbuf_t;

/* Inserts the given character into the dynamic buffer.
	Returns true on success and false on malloc failure.
	The buffer remains usable on malloc failure (but future calls probably won't succeed unless you free some memory)*/
bool dynbuf_ins(dynbuf_t *buf, char c);
/* Closes the given buffer and returns a string signed with a null terminator.
	Returns NULL on malloc failure.
	On success, the buffer is no longer usable. */
char *dynbuf_end(dynbuf_t *buf);
/* Frees as resources used by the dynamic buffer. Does NOT free the given pointer. */
void dynbuf_free(dynbuf_t *buf);

bool dynbuf_ins(dynbuf_t *buf, char c)
{
	if(buf->len % DYNBUF_GRAIN == 0)
	{ // expand buffer
		char *newBuf = realloc(buf->buf, buf->len + DYNBUF_GRAIN);

		if(!newBuf)
			return false;
	
		buf->buf = newBuf;
	}

	buf->buf[buf->len++] = c;
	return true;
}

char *dynbuf_end(dynbuf_t *buf)
{
	char *str = realloc(buf->buf, buf->len + 1);

	if(!str)
	{
		free(buf->buf);
		return NULL;
	}

	str[buf->len] = 0;

	return str;
}

void dynbuf_free(dynbuf_t *buf)
{
	free(buf->buf);

	buf->len = 0;
	buf->buf = NULL;
}


#endif