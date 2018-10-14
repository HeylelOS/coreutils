#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <alloca.h>

static char *catname; /**< Name of the program */
static blksize_t outsize; /**< Best suiting IO block size for stdout */

/**
 * This function returns the best suiting block size
 * for intensive IO concerning fd.
 * @param fd The filedescriptor for which the buffer is selected.
 * @return The block size
 */
static blksize_t
io_blksize(int fd) {
	struct stat stat;
	blksize_t blksize;

	if(fstat(fd, &stat) == 0) {
		blksize = stat.st_blksize;
	} else {
		blksize = 512; /* Arbitrary unix buffer size */
	}

	return blksize;
}

/**
 * This function dumps a filedescriptor on stdout
 * @param fd A filedescriptor, valid or invalid, to dump
 * @param filename Associated filename of the filedescriptor
 */
static void
cat_dump(int fd, char *filename) {
	if(fd >= 0) {
		blksize_t insize = io_blksize(fd);
		size_t buffersize = (insize > outsize ? insize : outsize);
		char * const buffer = alloca(buffersize);
		char *position = buffer, * const end = buffer + buffersize;
		ssize_t readval;

		do {
			do {
				size_t bufferleft = end - position;
				readval = read(fd, position,
					(bufferleft < insize ? bufferleft : insize));

				if(readval == -1) {
					fprintf(stderr, "%s: Unable to read \"%s\": %s\n",
						catname, filename, strerror(errno));
					exit(1);
				}

				position += readval;
			} while(position < end && readval != 0);

			if(write(STDOUT_FILENO, buffer, position - buffer) == -1) {
				fprintf(stderr, "%s: Unable to write \"%s\": %s\n",
					catname, filename, strerror(errno));
				exit(1);
			}

			position = buffer;
		} while(readval != 0);
	} else {
		fprintf(stderr, "%s: Invalid file %s: %s\n",
			catname, filename, strerror(errno));
		exit(1);
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
	outsize = io_blksize(STDOUT_FILENO);

	if(argc > 1
		&& strcmp(*iterator, "-u") == 0) {
		/* There is already no delay between read and writes in our case */
		iterator += 1;
	}

	if(iterator == end) {
		cat_dump(STDIN_FILENO, "-");
	} else {
		while(iterator != end) {
			if(strcmp(*iterator, "-") == 0) {
				cat_dump(STDIN_FILENO, "-");
			} else {
				int fd = open(*iterator, O_RDONLY);

				cat_dump(fd, *iterator);
				close(fd);
			}

			iterator += 1;
		}
	}

	exit(0);
}

