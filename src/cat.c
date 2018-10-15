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

static void
error(const char *errormsg,
	const char *filename) {

	fprintf(stderr, "%s: %s %s: %s\n",
		catname, errormsg, filename, strerror(errno));

	exit(1);
}

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
 * This function ensures every bytes are written
 * @param fd The filedescriptor to write to
 * @param buffer The buffer to write
 * @param count How many bytes to write
 * @return 0 on success, -1 on error, with errno set.
 */
static ssize_t
full_write(int fd,
	const char *buffer,
	size_t count) {
	ssize_t writeval;

	while((writeval = write(fd, buffer, count)) < count
		&& writeval != -1) {
		count -= writeval;
		buffer += writeval;
	}

	return writeval == -1 ? -1 : 0;
}

/**
 * This function dumps a filedescriptor on stdout
 * @param fd A filedescriptor, valid or invalid, to dump
 * @param filename Associated filename of the filedescriptor
 */
static void
fd_dump(int fd,
	const char *filename) {
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
					error("Unable to read", filename);
				}

				position += readval;
			} while(position < end && readval != 0);

			if(full_write(STDOUT_FILENO, buffer, position - buffer) == -1) {
				error("Unable to write", filename);
			}

			position = buffer;
		} while(readval != 0);
	} else {
		error("Invalid file", filename);
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
		fd_dump(STDIN_FILENO, "-");
	} else {
		while(iterator != end) {
			if(strcmp(*iterator, "-") == 0) {
				fd_dump(STDIN_FILENO, "-");
			} else {
				int fd = open(*iterator, O_RDONLY);

				fd_dump(fd, *iterator);
				close(fd);
			}

			iterator += 1;
		}
	}

	exit(0);
}

