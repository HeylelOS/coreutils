#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include "core_io.h"

static const char *catname;
static size_t outsize;

static void
cat_error(const char *errormsg,
	const char *filename) {

	fprintf(stderr, "error: %s: %s %s: %s\n",
		catname, errormsg, filename, strerror(errno));

	exit(1);
}

static void
cat_flush(int fd, const char *filename) {

	switch(io_flush_to(fd, STDOUT_FILENO, outsize)) {
	case -1:
		cat_error("read", filename);
		/* noreturn */
	case 1:
		cat_error("write", filename);
		/* noreturn */
	default:
		break;
	}
}

/**
 * Implementation of cat utility as described in
 * The Open Group Base Specifications Issue 7, 2018 edition
 * IEEE Std 1003.1-2017 (Revision of IEEE Std 1003.1-2008).
 *
 * @return Standard specifies either 0 or >0 as return values.
 */
int
main(int argc,
	char **argv) {
	char ** const argend = argv + argc;
	char **argpos;
	char c;
	catname = *argv;
	outsize = io_blocksize(STDOUT_FILENO);

	while((c = getopt(argc, argv, "u")) != -1);
	/* There is already no delay between read and writes in our case */
	argpos = argv + optind;

	if(argpos == argend) {
		cat_flush(STDIN_FILENO, "-");
	} else {
		while(argpos != argend) {
			if(strcmp(*argpos, "-") == 0) {
				cat_flush(STDIN_FILENO, "-");
			} else {
				int fd = open(*argpos, O_RDONLY);

				if(fd != -1) {
					cat_flush(fd, *argpos);
					close(fd);
				} else {
					cat_error("open", *argpos);
				}
			}

			argpos += 1;
		}
	}

	return 0;
}

