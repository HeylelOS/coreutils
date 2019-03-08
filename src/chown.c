#define _DEFAULT_SOURCE
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

static char *chownname;

static char *newowner;
static uid_t newuid;

static char *newgroup;
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

	int retval;
	if((retval = chown(path, owner, group)) == -1) {
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

	int retval;
	if((retval = lchown(path, owner, group)) == -1) {
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
		1, 0);
}

static int
chown_change_recursive_nofollow(const char *path) {

	return nftw(path,
		chown_ftw_nofollow,
		1, FTW_PHYS);
}

static int
chown_change_recursive_follow_head(const char *path) {

	return nftw(path,
		chown_ftw_follow_head,
		1, FTW_PHYS);
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
	char **argpos = argv + 1;
	char ** const argend = argv + argc;
	int (*chown_change)(const char *) = chown_change_follow;
	chownname = *argv;

	if(argc < 3) {
		chown_usage();
	}

	if(strcmp(*argpos, "-R") == 0) {
		chown_change = chown_change_recursive_nofollow;
		argpos += 1;

		while(argpos != argend) {
			if(strcmp(*argpos, "-H") == 0) {
				chown_change = chown_change_recursive_follow_head;
			} else if(strcmp(*argpos, "-L") == 0) {
				chown_change = chown_change_recursive_follow;
			} else if(strcmp(*argpos, "-P") == 0) {
				chown_change = chown_change_recursive_nofollow;
			} else {
				break;
			}
			argpos += 1;
		}
	} else if(strcmp(*argpos, "-h") == 0) {
		chown_change = chown_change_nofollow;
		argpos += 1;
	}

	if(argpos + 1 >= argend) {
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

