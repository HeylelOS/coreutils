#include <stdio.h>

#include "core_fs.h"

static void
rm_usage(const char *rmname) {
	fprintf(stderr, "usage: %s [-iRr] file...\n"
		"       %s -f [-iRr] [file...]\n",
		rmname, rmname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	int retval = 0;
	int c;

	while((c = getopt(argc, argv, "fiRr")) != -1) {
		switch(c) {
		default:
			rm_usage(*argv);
		}
	}

	return retval;
}

