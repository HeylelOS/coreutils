#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ftw.h>
#include <errno.h>

#include <heylel/core.h>

static char *chmodname;
static mode_t cmask;
static const char *modeexp;

static int
chmod_apply(const char *path,
	const struct stat *st) {
	mode_t mode = st->st_mode & (S_ISALL | S_IRWXA);
	int isdir = (S_IFMT & mode) == S_IFDIR ?
		1 : 0;
	const char *last = fs_parsemode(modeexp,
		&mode, cmask, isdir);

	if(*last != '\0') {
		fprintf(stderr, "error: %s: Unable to parse mode, stopped at '%c'\n",
			chmodname, *last);
		exit(1);
	}

	return chmod(path, mode);

	int retval;
	if((retval = chmod(path, mode)) == -1) {
		fprintf(stderr, "error: %s chmod %s: %s\n",
			chmodname, path, strerror(errno));
	}

	return retval;
}

static int
chmod_change_default(const char *path) {
	struct stat st;

	if(stat(path, &st) == -1) {
		fprintf(stderr, "error: %s stat: %s\n", chmodname, strerror(errno));
		return -1;
	}

	return chmod_apply(path, &st);
}

static int
chmod_ftw(const char *path,
	const struct stat *st,
	int flag) {

	switch(flag) {
	case FTW_F:
	case FTW_D:
	case FTW_SL:
		return chmod_apply(path, st);
	case FTW_NS:
		fprintf(stderr, "error: %s stat: %s\n",
			chmodname, strerror(errno));
		return -1;
	case FTW_DNR:
		fprintf(stderr, "error: %s: Unable to read directory %s\n",
			chmodname, path);
		return -1;
	default:
		return -1;
	}
}

static int
chmod_change_recursive(const char *path) {
	return ftw(path, chmod_ftw, 1);
}

static void
chmod_usage(void) {
	fprintf(stderr, "usage: %s [-R] mode file...\n", chmodname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	char **argpos = argv + 1;
	char ** const argend = argv + argc;
	int (*chmod_change)(const char *) = chmod_change_default;
	chmodname = *argv;

	if(argc < 3) {
		chmod_usage();
	}

	if(strcmp(argv[1], "-R") == 0) {
		if(argc < 4) {
			chmod_usage();
		}
		argpos += 1;
		chmod_change = chmod_change_recursive;
	}

	int retval = 0;
	cmask = umask(S_IRWXA);
	modeexp = *argpos;
	argpos += 1;

	while(argpos != argend) {
		if(chmod_change(*argpos) == -1) {
			retval = 1;
		}

		argpos += 1;
	}

	return retval;
}

