CC=clang
CFLAGS=-O3 -pedantic -Wall -fPIC -I./include
BUILDDIRS=build/ build/lib build/bin
CORENAME=core
CORELIBRARY=build/lib/lib$(CORENAME).so
LDFLAGS=-l$(CORE) -L./build/lib
CAT=build/bin/cat

.PHONY: all clean

all: $(BUILDDIRS) $(CORELIBRARY) $(CAT)

clean:
	rm -rf build/

$(BUILDDIRS):
	mkdir $@

$(CORELIBRARY): src/core.c include/heylel/core.h
	$(CC) $(CFLAGS) -shared -o $@ $<

$(CAT): src/cat.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

