#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <heylel/core.h>

static char *chmodname;

static void
chmod_apply(const char *path,
	const char *modeexp,
	mode_t cmask) {
	struct stat st;
	mode_t mode;
	int isdir;

	if(stat(path, &st) == -1) {
		fprintf(stderr, "error: %s stat: %s\n", chmodname, strerror(errno));
		exit(1);
	}

	mode = st.st_mode;
	if((S_IFMT & mode) == S_IFDIR) {
		isdir = 1;
	} else {
		isdir = 0;
	}

	const char *last = fs_parsemode(modeexp, &mode, cmask, isdir);
	if(*last == '\0') {
		printf("%.7o -> %.7o\n", st.st_mode, mode);
	} else {
		fprintf(stderr, "error: %s: Unable to parse mode, stopped at '%c'\n",
			chmodname, *last);
		exit(1);
	}
}

static void
chmod_usage(void) {
	fprintf(stderr, "usage: %s [-R] mode file...\n", chmodname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	char ** const argend = argv + argc;
	char **argpos = argv + 1;
	chmodname = *argv;

	if(argc < 3) {
		chmod_usage();
	}

	if(strcmp(argv[1], "-R") == 0) {
		if(argc < 4) {
			chmod_usage();
		}
		argpos += 1;
		/* NEEDED */
	}

	const mode_t cmask = umask(S_IRWXA);
	const char * const modeexp = *argpos;
	argpos += 1;

	while(argpos != argend) {
		chmod_apply(*argpos, modeexp, cmask);
		argpos += 1;
	}

	return 0;
}

