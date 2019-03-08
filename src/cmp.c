#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "core_io.h"

static const char *cmpname;

static int
cmp_stat(const char *path,
	struct stat *st) {

	if(strcmp(path, "-") == 0) {
		return fstat(STDIN_FILENO, st);
	} else {
		return stat(path, st);
	}
}

static int
cmp_fd(const char *path) {

	if(strcmp(path, "-") == 0) {
		return STDIN_FILENO;
	} else {
		int fd = open(path, O_RDONLY);

		if(fd == -1) {
			fprintf(stderr, "error: %s open %s: %s\n",
				cmpname, path, strerror(errno));
		}

		return fd;
	}
}

static int
cmp(const char *file1,
	size_t blksize1,
	const char *file2,
	size_t blksize2) {
	/* Fetch fds and bigger block size */
	const int fd1 = cmp_fd(file1);
	const int fd2 = cmp_fd(file2);
	const size_t blksize = blksize1 > blksize2 ?
		blksize1 : blksize2;
	/* 1 undetermined, 0 equals, -1 error/not equal */
	int cmpval = 1;

	/* If one of the file is invalid, error */
	if(fd1 == -1 || fd2 == -1) {
		cmpval = -1;
	} else {
		char * const buffer1 = alloca(blksize);
		char * const buffer2 = alloca(blksize);
		ssize_t readval1, readval2;

		/* While equals, fill buffers, check read sizes, and content */
		while(cmpval == 1) {
			cmpval = -1;

			/* If no error on reading */
			if((readval1 = io_read_all(fd1, buffer1, blksize)) < 0) {
				fprintf(stderr, "error: %s read %s: %s\n",
					cmpname, file1, strerror(errno));
			} else if((readval2 = io_read_all(fd2, buffer2, blksize)) < 0) {
				fprintf(stderr, "error: %s read %s: %s\n",
					cmpname, file2, strerror(errno));
			} else if(readval1 == readval2
				&& memcmp(buffer1, buffer2, blksize - readval1) == 0) {

				if(readval1 == 0) {
					/* Continue if not at end of file */
					cmpval = 1;
				} else {
					/* End of file, we're equals */
					cmpval = 0;
				}
			}
		}
	}

	/* close(fd1); */
	/* close(fd2); */

	return cmpval;
}

static void
cmp_usage(void) {
	fprintf(stderr, "usage %s [-l|-s] file1 file2\n",
		cmpname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	cmpname = *argv;

	if(argc < 3) {
		cmp_usage();
	}

	char **argpos = argv + 1;
	if(**argpos == '-') {
		if(strcmp(*argpos, "-l") == 0) {
			argpos += 1;
		} else if(strcmp(*argpos, "-s") == 0) {
			argpos += 1;
		}
	}

	if(argpos + 2 != argv + argc) {
		cmp_usage();
	}

	const char *file1 = argpos[0];
	const char *file2 = argpos[1];
	int retval = 0;

	/* If their names are equal, it's the same file */
	if(strcmp(file1, file2) != 0) {
		struct stat st1 = { .st_blksize = 512 };
		struct stat st2 = { .st_blksize = 512 };

		/* If inodes are identical its the same
		file, the standard says it should be
		an "undefined behavior" when we check for
		this, we choose to say they're equals */
		int statval1 = cmp_stat(file1, &st1);
		if(cmp_stat(file2, &st2) == 0
			&& statval1 == 0
			&& st1.st_ino != st2.st_ino) {

			/* "Streams" comparison */
			if(cmp(file1, st1.st_blksize,
				file2, st2.st_blksize) == -1) {
				retval = 1;
			}
		}
	}

	return retval;
}

