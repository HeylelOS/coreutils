CC=clang
CFLAGS=-O3 -pedantic -Wall -fPIC -I./include
BUILDDIRS=build/ build/lib build/bin
CORENAME=core
CORELIBRARY=build/lib/lib$(CORENAME).so
LDFLAGS=-l$(CORENAME) -L./build/lib

BASENAME=build/bin/basename
CAT=build/bin/cat
ECHO=build/bin/echo
UNLINK=build/bin/unlink

.PHONY: all clean

all: $(BUILDDIRS) $(CORELIBRARY) $(BASENAME) $(CAT)\
	$(ECHO) $(UNLINK)

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

$(ECHO): src/echo.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	$(CC) $(CFLAGS) -D ECHO_XSI -o $@-xsi $^ $(LDFLAGS)

$(UNLINK): src/unlink.c
	$(CC) $(CFLAGS) -o $@ $^
