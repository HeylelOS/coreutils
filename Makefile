CC=clang
CFLAGS=-g -pedantic -Wall -fPIC -I./include
BUILDDIRS=build/ build/lib build/bin
CORENAME=core
CORELIBRARY=build/lib/lib$(CORENAME).so
LDFLAGS=-l$(CORENAME) -L./build/lib

ASA=build/bin/asa
BASENAME=build/bin/basename
CAT=build/bin/cat
CAL=build/bin/cal
CHMOD=build/bin/chmod
ECHO=build/bin/echo
UNIQ=build/bin/uniq
UNLINK=build/bin/unlink

.PHONY: all clean

all: $(BUILDDIRS) $(CORELIBRARY)\
	$(ASA) $(BASENAME) $(CAT) $(CAL)\
	$(CHMOD) $(ECHO) $(UNIQ) $(UNLINK)

clean:
	rm -rf build/

$(BUILDDIRS):
	mkdir $@

$(CORELIBRARY): src/core_fs.c src/core_io.c include/heylel/core.h
	$(CC) $(CFLAGS) -shared -o $@ src/core_fs.c src/core_io.c

$(ASA): src/asa.c
	$(CC) $(CFLAGS) -o $@ $^

$(BASENAME): src/basename.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CAT): src/cat.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(CAL): src/cal.c
	$(CC) $(CFLAGS) -o $@ $^

$(CHMOD): src/chmod.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(ECHO): src/echo.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	$(CC) $(CFLAGS) -D ECHO_XSI -o $@-xsi $^ $(LDFLAGS)

$(UNIQ): src/uniq.c
	$(CC) $(CFLAGS) -o $@ $^

$(UNLINK): src/unlink.c
	$(CC) $(CFLAGS) -o $@ $^
