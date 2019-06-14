#define _XOPEN_SOURCE 500
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <grp.h>
#include <ftw.h>
#include <err.h>

#include "core_fs.h"

static int chgrpretval;
static const char *newgroup;
static gid_t newgid;

static int
chgrp_change_follow(const char *path) {
	gid_t group = newgid;
	struct stat st;
	int retval;

	if((retval = stat(path, &st)) == -1) {
		warn("stat %s", path);
	} else if((retval = chown(path, st.st_uid, group)) == -1) {
		warn("chown %s", path);
	}

	return retval;
}

static int
chgrp_change_nofollow(const char *path) {
	gid_t group = newgid;
	struct stat st;
	int retval;

	if((retval = lstat(path, &st)) == -1) {
		warn("lstat %s", path);
	} else if((retval = lchown(path, st.st_uid, group)) == -1) {
		warn("lchown %s", path);
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
		if(chgrp_change_follow(path) == -1) {
			chgrpretval = 1;
		}
		break;
	case FTW_DNR:
		warnx("Unable to read directory %s", path);
		chgrpretval = 1;
		break;
	default:
		break;
	}

	return 0;
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
		if(chgrp_change_nofollow(path) == -1) {
			chgrpretval = 1;
		}
		break;
	case FTW_DNR:
		warnx("Unable to read directory %s", path);
		chgrpretval = 1;
		break;
	default:
		break;
	}

	return 0;
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
chgrp_usage(const char *chgrpname) {
	fprintf(stderr, "usage: %s [-h] owner[:group] file...\n"
		"       %s -R [-H|-L|-P] owner[:group] file...\n",
		chgrpname, chgrpname);
	exit(1);
}

static void
chgrp_assign(const char *newgrp) {
	struct group *grp;
	newgroup = newgrp;

	errno = 0;
	if((grp = getgrnam(newgroup)) == NULL) {
		if(errno == 0) {
			/* No entry found, treat as numeric gid */
			char *end;
			unsigned long lgid = strtoul(newgroup, &end, 10);

			if(*newgroup == '\0' || *end != '\0') {
				errx(1, "Unable to infer gid from '%s'", newgroup);
			} else {
				/* Same as uid */
				newgid = lgid;
			}
		} else {
			err(1, "getgrnam");
		}
	} else {
		newgid = grp->gr_gid;
	}
}

int
main(int argc,
	char **argv) {
	int (*chgrp_change)(const char *) = chgrp_change_follow;
	char ** const argend = argv + argc, **argpos;

	if(argc == 1) {
		chgrp_usage(*argv);
	}

	int c;
	if(strcmp(argv[1], "-R") == 0) {
		chgrp_change = chgrp_change_recursive_nofollow;
		argpos = argv + (optind = 2);

		while((c = getopt(argc, argv, "HLP")) != -1) {
			switch(c) {
			case 'H':
				chgrp_change = chgrp_change_recursive_follow_head;
				break;
			case 'L':
				chgrp_change = chgrp_change_recursive_follow;
				break;
			case 'P':
				chgrp_change = chgrp_change_recursive_nofollow;
				break;
			default:
				chgrp_usage(*argv);
				/* no return */
			}
		}
	} else while((c = getopt(argc, argv, "h")) != -1) {
		if(c == 'h') {
			chgrp_change = chgrp_change_nofollow;
		} else {
			chgrp_usage(*argv);
		}
	}

	argpos = argv + optind;
	if(argpos + 2 >= argend) {
		chgrp_usage(*argv);
	}

	chgrp_assign(*argpos);
	argpos += 1;

	while(argpos != argend) {

		if(chgrp_change(*argpos) == -1) {
			chgrpretval = 1;
		}

		argpos += 1;
	}

	return chgrpretval;
}

