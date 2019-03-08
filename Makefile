CC=clang
CFLAGS=-g -pedantic -Wall -fPIC
BUILDDIRS=build/ build/bin

ASA=build/bin/asa
BASENAME=build/bin/basename
CAT=build/bin/cat
CAL=build/bin/cal
CHGRP=build/bin/chgrp
CHMOD=build/bin/chmod
CHOWN=build/bin/chown
CKSUM=build/bin/cksum
CMP=build/bin/cmp
CP=build/bin/cp
ECHO=build/bin/echo
UNIQ=build/bin/uniq
UNLINK=build/bin/unlink

.PHONY: all clean

all: $(BUILDDIRS)\
	$(ASA) $(BASENAME) $(CAT) $(CAL)\
	$(CHGRP) $(CHMOD) $(CHOWN) $(CKSUM)\
	$(CMP) $(CP) $(ECHO) $(UNIQ) $(UNLINK)

clean:
	rm -rf build/

$(BUILDDIRS):
	mkdir $@

$(ASA): src/asa.c
	$(CC) $(CFLAGS) -o $@ $^

$(BASENAME): src/basename.c
	$(CC) $(CFLAGS) -o $@ $^

$(CAT): src/cat.c
	$(CC) $(CFLAGS) -o $@ $^

$(CAL): src/cal.c
	$(CC) $(CFLAGS) -o $@ $^

$(CHGRP): src/chgrp.c
	$(CC) $(CFLAGS) -o $@ $^

$(CHMOD): src/chmod.c
	$(CC) $(CFLAGS) -o $@ $^

$(CHOWN): src/chown.c
	$(CC) $(CFLAGS) -o $@ $^

$(CKSUM): src/cksum.c
	$(CC) $(CFLAGS) -o $@ $^

$(CMP): src/cmp.c
	$(CC) $(CFLAGS) -o $@ $^

$(CP): src/cp.c
	$(CC) $(CFLAGS) -o $@ $^

$(ECHO): src/echo.c
	$(CC) $(CFLAGS) -o $@ $^
	$(CC) $(CFLAGS) -D ECHO_XSI -o $@-xsi $^

$(UNIQ): src/uniq.c
	$(CC) $(CFLAGS) -o $@ $^

$(UNLINK): src/unlink.c
	$(CC) $(CFLAGS) -o $@ $^
