#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#include "core_io.h"

static const char *cpname;
static struct {
	bool recursive;
	bool force;
	bool interactive;
	bool duplicate;
	enum {
		SYMLINKS_NOFOLLOW	= 0,
		SYMLINKS_FOLLOWARGS = 1,
		SYMLINKS_FOLLOWALL  = 3
	} symlinks;
} cpargs;

static void
cp_usage(void) {
	fprintf(stderr, "usage: %s [-Pfip] source_file target_file\n"
		"       %s [-Pfip] source_file... target\n"
		"       %s -R [-H|-L|-P] [-fip] source_file... target\n",
		cpname, cpname, cpname);
	exit(1);
}

static void
cp_parse_args(int argc,
	char **argv) {
	int c;

	if(argc == 1) {
		cp_usage();
	}

	while((c = getopt(argc, argv, "RLHPfip")) != -1) {
		switch(c) {
		case 'R':
			cpargs.recursive = true; break;
		case 'P':
			cpargs.symlinks = SYMLINKS_NOFOLLOW; break;
		case 'f':
			cpargs.force = true; break;
		case 'i':
			cpargs.interactive = true; break;
		case 'p':
			cpargs.duplicate = true; break;
		case 'H':
			if(cpargs.recursive)
				cpargs.symlinks = SYMLINKS_FOLLOWARGS;
			/* fallthrough */
		case 'L':
			if(cpargs.recursive)
				cpargs.symlinks = SYMLINKS_FOLLOWALL;
			/* fallthrough */
		default:
			cp_usage();
		}
	}

	if(optind + 2 >= argc) {
		cp_usage();
	}
}

int
main(int argc,
	char **argv) {
	cpname = *argv;
	cp_parse_args(argc, argv);

	return 0;
}

