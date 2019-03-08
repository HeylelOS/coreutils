#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <errno.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "core_fs.h"

static const char *chgrpname;

static const char *newgroup;
static gid_t newgid;

static int
chgrp_change_follow(const char *path) {
	gid_t group = newgid;
	struct stat st;

	if(stat(path, &st) == -1) {
		fprintf(stderr, "error: %s stat %s: %s\n",
			chgrpname, path, strerror(errno));
		exit(1);
	}

	int retval = chown(path, st.st_uid, group);
	if(retval == -1) {
		fprintf(stderr, "error: %s chown %s: %s\n",
			chgrpname, path, strerror(errno));
	}

	return retval;
}

static int
chgrp_change_nofollow(const char *path) {
	gid_t group = newgid;
	struct stat st;

	if(lstat(path, &st) == -1) {
		fprintf(stderr, "error: %s lstat %s: %s\n",
			chgrpname, path, strerror(errno));
		exit(1);
	}

	int retval = lchown(path, st.st_uid, group);
	if(retval == -1) {
		fprintf(stderr, "error: %s lchown %s: %s\n",
			chgrpname, path, strerror(errno));
	}

	return retval;
}

static int
chgrp_ftw_follow(const char *path,
	const struct stat *st,
	int flag,
	struct FTW *ftw) {

	switch(flag) {
	case FTW_F:
	case FTW_D:
	case FTW_SL:
	case FTW_NS:
		return chgrp_change_follow(path);
	case FTW_DNR:
		fprintf(stderr, "error: %s: Unable to read directory %s\n",
			chgrpname, path);
		/* falltrough */
	default:
		return -1;
	}
}

static int
chgrp_ftw_nofollow(const char *path,
	const struct stat *st,
	int flag,
	struct FTW *ftw) {

	switch(flag) {
	case FTW_F:
	case FTW_D:
	case FTW_SL:
	case FTW_NS:
		return chgrp_change_nofollow(path);
	case FTW_DNR:
		fprintf(stderr, "error: %s: Unable to read directory %s\n",
			chgrpname, path);
		/* falltrough */
	default:
		return -1;
	}
}

static int
chgrp_ftw_follow_head(const char *path,
	const struct stat *st,
	int flag,
	struct FTW *ftw) {

	if(ftw->level == 0) {
		return chgrp_ftw_follow(path, st, flag, ftw);
	} else {
		return chgrp_ftw_nofollow(path, st, flag, ftw);
	}
}

static int
chgrp_change_recursive_follow(const char *path) {

	return nftw(path,
		chgrp_ftw_follow,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT),
		0);
}

static int
chgrp_change_recursive_nofollow(const char *path) {

	return nftw(path,
		chgrp_ftw_nofollow,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT),
		FTW_PHYS);
}

static int
chgrp_change_recursive_follow_head(const char *path) {

	return nftw(path,
		chgrp_ftw_follow_head,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT),
		FTW_PHYS);
}

static void
chgrp_usage(void) {
	fprintf(stderr, "usage: %s [-h] owner[:group] file...\n"
		"       %s -R [-H|-L|-P] owner[:group] file...\n",
		chgrpname, chgrpname);
	exit(1);
}

static void
chgrp_assign(void) {
	struct group *grp;

	errno = 0;
	if((grp = getgrnam(newgroup)) == NULL) {
		if(errno == 0) {
			/* No entry found, treat as numeric gid */
			char *end;
			unsigned long lgid = strtoul(newgroup, &end, 10);

			if(*newgroup == '\0' || *end != '\0') {
				fprintf(stderr, "error: %s: Unable to infer gid from '%s'\n",
					chgrpname, newgroup);
				exit(1);
			} else {
				/* Same as uid */
				newgid = lgid;
			}
		} else {
			fprintf(stderr, "error: %s getgrnam: %s\n",
				chgrpname, strerror(errno));
			exit(1);
		}
	} else {
		newgid = grp->gr_gid;
	}
}

int
main(int argc,
	char **argv) {
	char **argpos = argv + 1;
	char ** const argend = argv + argc;
	int (*chgrp_change)(const char *) = chgrp_change_follow;
	chgrpname = *argv;

	if(argc < 3) {
		chgrp_usage();
	}

	if(strcmp(*argpos, "-R") == 0) {
		chgrp_change = chgrp_change_recursive_nofollow;
		argpos += 1;

		while(argpos != argend) {
			if(strcmp(*argpos, "-H") == 0) {
				chgrp_change = chgrp_change_recursive_follow_head;
			} else if(strcmp(*argpos, "-L") == 0) {
				chgrp_change = chgrp_change_recursive_follow;
			} else if(strcmp(*argpos, "-P") == 0) {
				chgrp_change = chgrp_change_recursive_nofollow;
			} else {
				break;
			}
			argpos += 1;
		}
	} else if(strcmp(*argpos, "-h") == 0) {
		chgrp_change = chgrp_change_nofollow;
		argpos += 1;
	}

	if(argpos + 1 >= argend) {
		chgrp_usage();
	}

	newgroup = *argpos;
	chgrp_assign();
	argpos += 1;

	int retval = 0;

	while(argpos != argend) {

		if(chgrp_change(*argpos) == -1) {
			retval = 1;
		}

		argpos += 1;
	}

	return retval;
}

