CC=musl-clang
CFLAGS=-O3 -Wall -fPIC
LDFLAGS=

BUILDDIR=./build

.PHONY: all install clean

all: build/bin/asa build/bin/basename build/bin/cal build/bin/cat build/bin/chgrp build/bin/chmod build/bin/chown build/bin/cksum build/bin/cmp build/bin/cp build/bin/dirname build/bin/echo build/bin/echo-xsi build/bin/false build/bin/ln build/bin/rm build/bin/true build/bin/uniq build/bin/unlink 

clean:
	rm -rf $(BUILDDIR)

$(BUILDDIR):
	mkdir -p $@/bin
build/bin/asa: src/asa.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/basename: src/basename.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/cal: src/cal.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/cat: src/cat.c src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/chgrp: src/chgrp.c src/core_fs.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/chmod: src/chmod.c src/core_fs.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/chown: src/chown.c src/core_fs.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/cksum: src/cksum.c src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/cmp: src/cmp.c src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/cp: src/cp.c src/core_fs.h src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/dirname: src/dirname.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/echo: src/echo.c src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/echo-xsi: src/echo.c src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) -D ECHO_XSI -o $@ $<

build/bin/false: src/false.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/ln: src/ln.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/rm: src/rm.c src/core_fs.h src/core_io.h $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/true: src/true.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/uniq: src/uniq.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

build/bin/unlink: src/unlink.c $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<

