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
	char **argend = argv + argc;
	char **argpos = argv + 1;

	if(argpos != argend
		&& **argpos == '-') {
		char *arg = *argpos + 1;

		if(*arg == 'R' && arg[1] == '\0') {
			cpargs.recursive = true;
			argpos += 1;

			if(argpos != argend
				&& **argpos == '-') {
				char *arg = *argpos + 1;

				if(*arg != '\0' && arg[1] == '\0') {
					switch(*arg) {
					case 'H':
						cpargs.symlinks = SYMLINKS_FOLLOWARGS;
						break;
					case 'L':
						cpargs.symlinks = SYMLINKS_FOLLOWALL;
						break;
					case 'P':
						cpargs.symlinks = SYMLINKS_NOFOLLOW;
						break;
					default:
						cp_usage();
					}

					argpos += 1;
				}
			}
		}
	}

	if(argpos != argend
		&& **argpos == '-') {
		char *arg = *argpos + 1;

		while(*arg != '\0') {
			switch(*arg) {
			case 'f':
				cpargs.force = true;
				break;
			case 'i':
				cpargs.interactive = true;
				break;
			case 'p':
				cpargs.duplicate = true;
				break;
			case 'P':
				if(!cpargs.recursive) {
					cpargs.symlinks = SYMLINKS_NOFOLLOW;
					break;
				}
				/* fallthrough */
			default:
				cp_usage();
			}

			arg += 1;
		}

		argpos += 1;
	}

	if(argend - argpos < 2) {
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

