// config.h: Defined the available
#pragma once

/**
	Controls whether or not negated tags are shown by readdir (i.e. via ls).
	If the switch is not defined, a directory listing will never contain negated tags.
	If it is defined, a directory listing will contain the negation of every tag not already present in the query.
 */
#ifndef NO_LIST_NEGATED_TAGS
#define LIST_NEGATED_TAGS
#endif

/**
	Controls whether or not Trash files are blocked from being created.
	If the switch is not defined, tag creation functions as expected and
		deleting files using a file manager may yield unexpected results, potentially not actually deleting them.
	If it is defined, if mkdir is used to create a directory matching '.Trash*', a EINVAL is returned and no tag is created.
		Restoring files deleted using a file manager most likely won't work.
*/
#ifndef NO_BLOCK_TRASH_CREATION
#define BLOCK_TRASH_CREATION
#endif

/**
	Controls how the tags of a file are changed by renaming (i.e. via mv).
	If the switch is not defined, the file's tags are overwritten with the tags in the target path and negated tags are ignored.
	If it is defined, the default behaviour, as specified above, is used.
*/
#ifndef NO_RELATIVE_RENAME
#define RELATIVE_RENAME
#endif