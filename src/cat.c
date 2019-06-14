#include <fcntl.h>
#include <string.h>
#include <err.h>

#include "core_io.h"

static int
cat_flush(int fd, const char *filename, size_t outsize) {

	switch(io_flush_to(fd, STDOUT_FILENO, outsize)) {
	case -1:
		warn("read %s", filename);
		return -1;
	case 1:
		warn("write %s", filename);
		return -1;
	default:
		return 0;
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
	const size_t outsize = io_blocksize(STDOUT_FILENO);
	char ** argpos, ** const argend = argv + argc;
	int retval = 0;
	char c;

	while((c = getopt(argc, argv, "u")) != -1) {
		/* There is already no delay between read and writes in our case */
		if(c == '?') {
			return 1;
		}
	}
	argpos = argv + optind;

	if(argpos == argend) {
		cat_flush(STDIN_FILENO, "-", outsize);
	} else {
		while(argpos != argend) {
			if(strcmp(*argpos, "-") == 0) {
				if(cat_flush(STDIN_FILENO, "-", outsize) == -1) {
					retval = 1;
				}
			} else {
				int fd = open(*argpos, O_RDONLY);

				if(fd != -1) {
					if(cat_flush(fd, *argpos, outsize) == -1) {
						retval = 1;
					}
					close(fd);
				} else {
					warn("open %s", *argpos);
					retval = 1;
				}
			}

			argpos += 1;
		}
	}

	return retval;
}

