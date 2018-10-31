#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <heylel/core.h>

static char *catname;
static size_t outsize;

static void
cat_error(const char *errormsg,
	const char *filename) {

	fprintf(stderr, "error %s: %s %s: %s\n",
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
	char **iterator = argv + 1;
	char **end = argv + argc;
	catname = *argv;
	outsize = io_blocksize(STDOUT_FILENO);

	if(argc > 1
		&& strcmp(*iterator, "-u") == 0) {
		/* There is already no delay between read and writes in our case */
		iterator += 1;
	}

	if(iterator == end) {
		cat_flush(STDIN_FILENO, "-");
	} else {
		while(iterator != end) {
			if(strcmp(*iterator, "-") == 0) {
				cat_flush(STDIN_FILENO, "-");
			} else {
				int fd = open(*iterator, O_RDONLY);

				if(fd != -1) {
					cat_flush(fd, *iterator);
					close(fd);
				} else {
					cat_error("open", *iterator);
				}
			}

			iterator += 1;
		}
	}

	return 0;
}

