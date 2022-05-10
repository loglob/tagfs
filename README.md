# tagfs
A non-hierarchical filesystem using libfuse.
In a tagfs directory, directories represent boolean tags applied to files, rather than directories containing files.
All available files are shown in the root and can be filtered by entering the subdirectories.

For example, `/tag0/tag1` (starting at the tagfs root) retrieves all files that are marked with both tag0 and tag1.
Prepending a '-' negates a tag, selecting every file that is not marked with the tag.
Because of this, filenames may not start with a leading '-'.
A leading '.' is ignored for non-negated tag names.

Moving a file marks the file with every tag used in the target path.
Negated tags are removed and regular tags are added.
Moving a file to the directory root clears all its tags.

## building
Run `make tagfs` to build the tagfs executable.

### switches
To build manually, use tagfs.c as main file and link libcrypto, libfuse and libexplain.
Look at `config.h` for a list of available switches, which are all active by default.
To opt out, define `NO_<switch>` in your compile options, or edit the `config.h` file.

## usage
Run `tagfs <target path>` to mount a tagfs instance at the given path.
The directory may not contain directories before mounting.
Tagfs stores all its metadata in a `.tagdb` file in the target path.

Running `tagfs -l <log file> <target path>` uses the given log file to print debug info.
