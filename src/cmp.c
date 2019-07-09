#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>

#include "core_io.h"

#define CMP_MAX(a, b) ((a) > (b) ? (a) : (b))

static enum {
	CMP_FORMAT_DEFAULT,
	CMP_FORMAT_OCTAL,
	CMP_FORMAT_NONE
} cmpformat;

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
		return open(path, O_RDONLY);
	}
}

static size_t
cmp_compare(const char *buffer1, const char *buffer2,
	size_t size, size_t *count, size_t *line) {
	const char * const begin = buffer1,
		* const end = begin + size;

	while(buffer1 < end
		&& *buffer1 == *buffer2) {
		if(*buffer1 == '\n') {
			++*line;
		}

		buffer1++;
		buffer2++;
	}

	*count += buffer1 - begin;

	return end - buffer1;
}

static int
cmp(const char *file1,
	size_t blksize1,
	const char *file2,
	size_t blksize2) {
	/* Fetch fds and bigger block size */
	const int fd1 = cmp_fd(file1);
	const int fd2 = cmp_fd(file2);
	const size_t blksize = CMP_MAX(blksize1, blksize2);
	int retval = 0;

	/* If one of the file is invalid, error */
	if(fd1 == -1) {
		err(2, "open %s", file1);
	} else if(fd2 == -1) {
		err(2, "open %s", file2);
	} else {
		char * const buffer1 = alloca(blksize);
		char * const buffer2 = alloca(blksize);
		ssize_t readval1, readval2;
		size_t bytenumber = 1, linenumber = 1,
			size, missing;

		while((readval1 = io_read_all(fd1, buffer1, blksize)) >= 0
			&& (readval2 = io_read_all(fd2, buffer2, blksize)) >= 0
			&& readval1 == readval2 && readval1 != blksize
			&& (missing = cmp_compare(buffer1, buffer2,
				(size = blksize - readval1), &bytenumber, &linenumber)) == 0) {
		}

		if(readval1 < 0) {
			err(2, "read %s", file1);
		} else if(readval2 < 0) {
			err(2, "read %s", file2);
		}

		if(readval1 != readval2) {
			const char *shortestfile;
			size_t readval;

			if(readval1 > readval2) {
				shortestfile = file1;
				readval = readval1;
			} else {
				shortestfile = file2;
				readval = readval2;
			}

			if((missing = cmp_compare(buffer1, buffer2,
				(size = blksize - readval), &bytenumber, &linenumber)) == 0
				&& cmpformat != CMP_FORMAT_NONE) {
				fprintf(stderr, "cmp: EOF on %s\n", shortestfile);
			}

			retval = -1;
		}

		if(missing != 0) {

			switch(cmpformat) {
			case CMP_FORMAT_DEFAULT:
				printf("%s %s differ: char %lu, line %lu\n", file1, file2, bytenumber, linenumber);
				break;
			case CMP_FORMAT_OCTAL:
				printf("%lu %o %o\n", bytenumber, buffer1[size - missing], buffer2[size - missing]);
				break;
			default: /* CMP_FORMAT_NONE */
				break;
			}

			retval = -1;
		}
	}

#ifdef FULL_CLEANUP
	close(fd1);
	close(fd2);
#endif

	return retval;
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
			cmpformat = CMP_FORMAT_OCTAL;
			break;
		case 's':
			cmpformat = CMP_FORMAT_NONE;
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
			|| (st1.st_ino != st2.st_ino || st1.st_dev != st2.st_dev)) {

			if(cmp(file1, st1.st_blksize,
				file2, st2.st_blksize) == -1) {
				return 1;
			}
		}
	}

	return 0;
}

