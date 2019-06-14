#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>

#include "core_io.h"

static int
cmp_stat(const char *path,
	struct stat *st) {
	int retval;

	if(strcmp(path, "-") == 0) {
		retval = fstat(STDIN_FILENO, st);
	} else {
		retval = stat(path, st);
	}

	if(retval == -1) {
		warn("stat %s", path);
	}

	return retval;
}

static int
cmp_fd(const char *path) {

	if(strcmp(path, "-") == 0) {
		return STDIN_FILENO;
	} else {
		return open(path, O_RDONLY);
	}
}

static void
cmp(const char *file1,
	size_t blksize1,
	const char *file2,
	size_t blksize2) {
	/* Fetch fds and bigger block size */
	const int fd1 = cmp_fd(file1);
	const int fd2 = cmp_fd(file2);
	const size_t blksize = blksize1 > blksize2 ? blksize1 : blksize2;

	/* If one of the file is invalid, error */
	if(fd1 == -1) {
		err(2, "open %s", file1);
	} else if(fd2 == -1) {
		err(2, "open %s", file2);
	} else {
		char * const buffer1 = alloca(blksize);
		char * const buffer2 = alloca(blksize);
		ssize_t readval1, readval2;

		while((readval1 = io_read_all(fd1, buffer1, blksize)) >= 0
			&& (readval2 = io_read_all(fd2, buffer2, blksize)) >= 0
			&& readval1 == readval2
			&& memcmp(buffer1, buffer2, blksize - readval1) == 0) {
		}

		if(readval1 < 0) {
			err(2, "read %s", file1);
		} else if(readval2 < 0) {
			err(2, "read %s", file2);
		} else if(readval1 != readval2) {
		}
	}

#ifdef FULL_CLEANUP
	close(fd1);
	close(fd2);
#endif
}

static void
cmp_usage(const char *cmpname) {

	fprintf(stderr, "usage %s [-l|-s] file1 file2\n",
		cmpname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	int c;

	while((c = getopt(argc, argv, "ls")) != -1) {
		switch(c) {
		case 'l':
			break;
		case 's':
			break;
		default:
			cmp_usage(*argv);
		}
	}

	if(optind + 2 != argc) {
		cmp_usage(*argv);
	}

	const char *file1 = argv[optind];
	const char *file2 = argv[optind + 1];
	/* If their names are equal, it's the same file */
	if(strcmp(file1, file2) != 0) {
		struct stat st1 = { .st_blksize = 512 };
		struct stat st2 = { .st_blksize = 512 };

		/* If inodes are identical its the same
		file, the standard says it should be
		an "undefined behavior" when we check for
		this, we choose to say they're equal */

		int statval1 = cmp_stat(file1, &st1);
		int statval2 = cmp_stat(file2, &st2);

		if(statval1 == -1 || statval2 == -1
			|| st1.st_ino != st2.st_ino) {

			cmp(file1, st1.st_blksize,
				file2, st2.st_blksize);
		}
	}

	return 0;
}

