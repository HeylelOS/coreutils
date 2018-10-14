CC=clang
CFLAGS=-O3 -pedantic -Wall
BUILDDIRS=build/ build/bin
CAT=build/bin/cat

.PHONY: all clean

all: $(BUILDDIRS) $(CAT)

clean:
	rm -rf build/

$(BUILDDIRS):
	mkdir $@

$(CAT): src/cat.c
	$(CC) $(CFLAGS) -o $@ $^

