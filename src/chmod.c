#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <err.h>

#include "core_fs.h"

static int chmodretval;
static const char *modeexp;
static mode_t cmask;

static int
chmod_apply(const char *path,
	const struct stat *st) {
	mode_t mode = st->st_mode & (S_ISALL | S_IRWXA);
	int isdir = (S_IFMT & mode) == S_IFDIR ? 1 : 0;
	const char *last = fs_parsemode(modeexp,
		&mode, cmask, isdir);
	int retval = 0;

	if(*last != '\0') {
		warnx("Unable to parse mode '%s', stopped at '%c'", modeexp, *last);
		retval = -1;
	} else if((retval = chmod(path, mode)) == -1) {
		warn("chmod %s", path);
	}

	return retval;
}

static int
chmod_change_default(const char *path) {
	struct stat st;

	if(stat(path, &st) == -1) {
		warn("stat %s", path);
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
		if(chmod_apply(path, st) == -1) {
			chmodretval = 1;
		}
		break;
	case FTW_NS:
		warn("stat %s", path);
		chmodretval = 1;
		break;
	case FTW_DNR:
		warnx("Unable to read directory %s", path);
		chmodretval = 1;
		break;
	default:
		break;
	}

	return 0;
}

static int
chmod_change_recursive(const char *path) {

	return ftw(path, chmod_ftw,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT));
}

static void
chmod_usage(const char *chmodname) {

	fprintf(stderr, "usage: %s [-R] mode file...\n", chmodname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	int (*chmod_change)(const char *) = chmod_change_default;
	char **argpos, ** const argend = argv + argc;

	int c;
	while((c = getopt(argc, argv, "R")) != -1) {
		if(c == 'R') {
			chmod_change = chmod_change_recursive;
		} else {
			chmod_usage(*argv);
		}
	}

	argpos = argv + optind;
	if(argpos + 2 >= argend) {
		chmod_usage(*argv);
	}

	cmask = umask(S_IRWXA);
	modeexp = *argpos;
	argpos += 1;

	while(argpos != argend) {
		if(chmod_change(*argpos) == -1) {
			chmodretval = 1;
		}

		argpos += 1;
	}

	return chmodretval;
}

