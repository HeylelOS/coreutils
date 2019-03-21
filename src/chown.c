#ifndef __APPLE__
#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500
#endif
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

static const char *chownname;

static const char *newowner;
static uid_t newuid;

static const char *newgroup;
static gid_t newgid;

static int
chown_change_follow(const char *path) {
	uid_t owner = newuid;
	gid_t group;

	if(newgroup == NULL) {
		struct stat st;

		if(stat(path, &st) == -1) {
			fprintf(stderr, "error: %s stat %s: %s\n",
				chownname, path, strerror(errno));
			exit(1);
		}

		group = st.st_gid;
	} else {
		group = newgid;
	}

	int retval = chown(path, owner, group);
	if(retval == -1) {
		fprintf(stderr, "error: %s chown %s: %s\n",
			chownname, path, strerror(errno));
	}

	return retval;
}

static int
chown_change_nofollow(const char *path) {
	uid_t owner = newuid;
	gid_t group;

	if(newgroup == NULL) {
		struct stat st;

		if(lstat(path, &st) == -1) {
			fprintf(stderr, "error: %s lstat %s: %s\n",
				chownname, path, strerror(errno));
			exit(1);
		}

		group = st.st_gid;
	} else {
		group = newgid;
	}

	int retval = lchown(path, owner, group);
	if(retval == -1) {
		fprintf(stderr, "error: %s lchown %s: %s\n",
			chownname, path, strerror(errno));
	}

	return retval;
}

static int
chown_ftw_follow(const char *path,
	const struct stat *st,
	int flag,
	struct FTW *ftw) {

	switch(flag) {
	case FTW_F:
	case FTW_D:
	case FTW_SL:
	case FTW_NS:
		return chown_change_follow(path);
	case FTW_DNR:
		fprintf(stderr, "error: %s: Unable to read directory %s\n",
			chownname, path);
		/* falltrough */
	default:
		return -1;
	}
}

static int
chown_ftw_nofollow(const char *path,
	const struct stat *st,
	int flag,
	struct FTW *ftw) {

	switch(flag) {
	case FTW_F:
	case FTW_D:
	case FTW_SL:
	case FTW_NS:
		return chown_change_nofollow(path);
	case FTW_DNR:
		fprintf(stderr, "error: %s: Unable to read directory %s\n",
			chownname, path);
		/* falltrough */
	default:
		return -1;
	}
}

static int
chown_ftw_follow_head(const char *path,
	const struct stat *st,
	int flag,
	struct FTW *ftw) {

	if(ftw->level == 0) {
		return chown_ftw_follow(path, st, flag, ftw);
	} else {
		return chown_ftw_nofollow(path, st, flag, ftw);
	}
}

static int
chown_change_recursive_follow(const char *path) {

	return nftw(path,
		chown_ftw_follow,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT),
		0);
}

static int
chown_change_recursive_nofollow(const char *path) {

	return nftw(path,
		chown_ftw_nofollow,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT),
		FTW_PHYS);
}

static int
chown_change_recursive_follow_head(const char *path) {

	return nftw(path,
		chown_ftw_follow_head,
		fs_fdlimit(HEYLEL_FDLIMIT_DEFAULT),
		FTW_PHYS);
}

static void
chown_usage(void) {
	fprintf(stderr, "usage: %s [-h] owner[:group] file...\n"
		"       %s -R [-H|-L|-P] owner[:group] file...\n",
		chownname, chownname);
	exit(1);
}

static void
chown_assign(char *ownergroup) {
	struct passwd *pwd;

	newowner = strsep(&ownergroup, ":");
	newgroup = strsep(&ownergroup, ":");

	errno = 0;
	if((pwd = getpwnam(newowner)) == NULL) {
		if(errno == 0) {
			/* No entry found, treat as numeric uid */
			char *end;
			unsigned long luid = strtoul(newowner, &end, 10);

			if(*newowner == '\0' || *end != '\0') {
				fprintf(stderr, "error: %s: Unable to infer uid from '%s'\n",
					chownname, newowner);
				exit(1);
			} else {
				/* I sadly didn't find a meaningful  way
				to determine the uid limit of the system,
				so we may be subject to overflows */
				newuid = luid;
			}
		} else {
			fprintf(stderr, "error: %s getpwnam: %s\n",
				chownname, strerror(errno));
			exit(1);
		}
	} else {
		newuid = pwd->pw_uid;
	}

	if(newgroup != NULL) {
		struct group *grp;

		errno = 0;
		if((grp = getgrnam(newgroup)) == NULL) {
			if(errno == 0) {
				/* No entry found, treat as numeric gid */
				char *end;
				unsigned long lgid = strtoul(newgroup, &end, 10);

				if(*newgroup == '\0' || *end != '\0') {
					fprintf(stderr, "error: %s: Unable to infer gid from '%s'\n",
						chownname, newowner);
					exit(1);
				} else {
					/* Same as uid */
					newgid = lgid;
				}
			} else {
				fprintf(stderr, "error: %s getgrnam: %s\n",
					chownname, strerror(errno));
				exit(1);
			}
		} else {
			newgid = grp->gr_gid;
		}
	}
}

int
main(int argc,
	char **argv) {
	int (*chown_change)(const char *) = chown_change_follow;
	char ** const argend = argv + argc;
	char **argpos;
	chownname = *argv;

	if(argc == 1) {
		chown_usage();
	}

	int c;
	if(strcmp(argv[1], "-R") == 0) {
		chown_change = chown_change_recursive_nofollow;
		argpos = argv + (optind = 2);

		while((c = getopt(argc, argv, "HLP")) != -1) {
			switch(c) {
			case 'H':
				chown_change = chown_change_recursive_follow_head; break;
			case 'L':
				chown_change = chown_change_recursive_follow; break;
			case 'P':
				chown_change = chown_change_recursive_nofollow; break;
			default:
				chown_usage();
			}
		}
	} else while((c = getopt(argc, argv, "h")) != -1) {
		if(c == 'h') {
			chown_change = chown_change_nofollow;
		} else {
			chown_usage();
		}
	}

	argpos = argv + optind;
	if(argpos + 2 >= argend) {
		chown_usage();
	}

	chown_assign(*argpos);
	argpos += 1;

	int retval = 0;

	while(argpos != argend) {

		if(chown_change(*argpos) == -1) {
			retval = 1;
		}

		argpos += 1;
	}

	return retval;
}

