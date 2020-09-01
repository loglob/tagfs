CC = c99 -Wall -Wextra -Wno-unknown-pragmas
CFLAGS = -lcrypto -lexplain
TEST_H := $(wildcard ./*_test.h)
TESTS := $(patsubst ./%.h,./%,$(TEST_H))
GRINDS := $(patsubst ./%.h,./%_grind,$(TEST_H))
DEFS := -DRELATIVE_RENAME

tagfs-debug: tagfs.c tagfs.h tagdb.h hashmap.h bitarr.h futil.h
	$(CC) -g $(DEFS) -DMALLOC_CHECK_ -DDEBUG -DTRACE "$<" ${CFLAGS} -lfuse -o "$@"

tagfs: tagfs.c tagdb.h hashmap.h bitarr.h futil.h
	$(CC) $(DEFS) "$<" -o "$@" -lfuse ${CFLAGS}

remount: umount mount

mount: tagfs-debug
	ulimit -c unlimited
	./tagfs-debug -l log dir -o nonempty

grind: tagfs-debug
	ulimit -c unlimited
	valgrind --leak-check=full ./tagfs-debug -l log dir -o nonempty

gremount: umount grind

umount:
	fusermount -u dir

%_test.o: %_test.h test.c test.h %.h
	$(CC) -include $< test.c -o $@ ${CFLAGS}

%_test: %_test.o
	./$^
	rm $^

testall: ${TESTS}

%_grind: %.o
	valgrind ./$^
	rm $^

grindall: ${GRINDS}
