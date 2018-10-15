CC=clang
CFLAGS=-O3 -pedantic -Wall -fPIC -I./include
BUILDDIRS=build/ build/lib build/bin
CORENAME=core
CORELIBRARY=build/lib/lib$(CORENAME).so
LDFLAGS=-l$(CORENAME) -L./build/lib
BASENAME=build/bin/basename
CAT=build/bin/cat

.PHONY: all clean

all: $(BUILDDIRS) $(CORELIBRARY) $(BASENAME) $(CAT)

clean:
	rm -rf build/

$(BUILDDIRS):
	mkdir $@

$(CORELIBRARY): src/core_fs.c src/core_io.c include/heylel/core.h
	$(CC) $(CFLAGS) -shared -o $@ src/core_fs.c src/core_io.c

$(BASENAME): src/basename.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CAT): src/cat.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

