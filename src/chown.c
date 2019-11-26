#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <err.h>

#include "core_fs.h"

struct chown_args {
	unsigned recursive : 1;
	unsigned follows : 1;
	unsigned followstraversal : 1;
};

static int
chown_at(int dirfd, const char *name, const char *path,
	struct stat *statp, uid_t uid, gid_t *gidp, bool follows) {
	int flags = follows ? 0 : AT_SYMLINK_NOFOLLOW;
	int retval;

	if((retval = fstatat(dirfd, name, statp, flags)) == 0) {
		if((retval = fchownat(dirfd, name, uid,
				gidp == NULL ? statp->st_gid : *gidp, flags)) == 0) {
			if((retval = fchmodat(dirfd, name,
					statp->st_mode & (S_ISVTX | S_IRWXA), flags)) == -1) {
				warn("chmod %s", path);
			}
		} else {
			warn("chown %s", path);
		}
	} else {
		warn("stat %s", path);
	}

	return retval;
}

static int
chown_uid_gid(char *ownergroup, uid_t *uidp, gid_t *gidp) {
	const char *owner = strsep(&ownergroup, ":");
	const char *group = strsep(&ownergroup, ":");
	struct passwd *pwd;

	errno = 0;
	if((pwd = getpwnam(owner)) == NULL) {
		if(errno == 0) {
			/* No entry found, treat as numeric uid */
			char *end;
			unsigned long luid = strtoul(owner, &end, 10);

			if(*owner == '\0' || *end != '\0') {
				errx(1, "Unable to infer uid from '%s'", owner);
			} else {
				/* I sadly didn't find a meaningful  way
				to determine the uid limit of the system,
				so we may be subject to overflows */
				*uidp = luid;
			}
		} else {
			err(1, "getpwnam");
		}
	} else {
		*uidp = pwd->pw_uid;
	}

	if(group != NULL) {
		struct group *grp;

		errno = 0;
		if((grp = getgrnam(group)) == NULL) {
			if(errno == 0) {
				/* No entry found, treat as numeric gid */
				char *end;
				unsigned long lgid = strtoul(group, &end, 10);

				if(*group == '\0' || *end != '\0') {
					errx(1, "Unable to infer gid from '%s'", group);
				} else {
					/* Same as uid */
					*gidp = lgid;
				}
			} else {
				err(1, "getgrnam");
			}
		} else {
			*gidp = grp->gr_gid;
		}

		return 1;
	}

	return 0;
}

static void
chown_usage(const char *chownname) {
	fprintf(stderr, "usage: %s [-h] group file...\n"
		"       %s -R [-H|-L|-P] group file...\n",
		chownname, chownname);
	exit(1);
}

static struct chown_args
chown_parse_args(int argc, char **argv) {
	struct chown_args args = { 0, 0, 0 };
	int c;

	while((c = getopt(argc, argv, "hRHLP")) != -1) {
		switch(c) {
		case 'h':
			args.follows = 0;
			break;
		case 'R':
			args.recursive = 1;
			break;
		case 'H':
			if(args.recursive == 1) {
				args.follows = 1;
				args.followstraversal = 0;
				break;
			} else {
				warnx("-H specified before -R");
			}
			/* fallthrough */
		case 'L':
			if(args.recursive == 1) {
				args.follows = 1;
				args.followstraversal = 1;
				break;
			} else {
				warnx("-L specified before -R");
			}
			/* fallthrough */
		case 'P':
			if(args.recursive == 1) {
				args.follows = 0;
				args.followstraversal = 0;
				break;
			} else {
				warnx("-P specified before -R");
			}
			/* fallthrough */
		default:
			chown_usage(*argv);
			break;
		}
	}

	if(argc - optind < 2) {
		chown_usage(*argv);
	}

	return args;
}

static inline bool
chown_recur(const char *file, struct stat *statp, int *error,
	const struct chown_args args) {

	if(args.follows == 0 && S_ISLNK(statp->st_mode)
		&& stat(file, statp) == -1) {
		warn("stat %s", file);
		*error = 1;
	}

	return S_ISDIR(statp->st_mode) && args.recursive == 1;
}

int
main(int argc,
	char **argv) {
	const struct chown_args args = chown_parse_args(argc, argv);
	char **argpos = argv + optind + 1, ** const argend = argv + argc;
	gid_t gid, *gidp = NULL;
	uid_t uid;
	int retval = 0;

	umask(0); /* When clearing set-user-ID and set-group-ID bits */

	if(chown_uid_gid(argv[optind], &uid, &gid) == 1) {
		gidp = &gid;
	}

	while(argpos != argend) {
		const char *file = *argpos;
		struct stat st;

		/* chown operand */
		if(chown_at(AT_FDCWD, file, file, &st, uid, gidp, args.follows == 1) == 0) {
			/* Get destination stats if it didn't follow a symlink,
			enter recursion if it's a directory and -R specified */
			if(chown_recur(file, &st, &retval, args)) {
				struct fs_recursion recursion;

				if(fs_recursion_init(&recursion, file, 256, args.followstraversal == 1) == 0) {
					do {
						while(fs_recursion_next(&recursion) == 0 && *recursion.name != '\0') {
							if(chown_at(dirfd(recursion.dirp), recursion.name, recursion.path,
									&st, uid, gidp, args.followstraversal == 1) == 0
								&& S_ISDIR(st.st_mode)) {
								fs_recursion_push(&recursion);
							} else {
								retval = 1;
							}
						}
					} while(fs_recursion_pop(&recursion) == 0);

					fs_recursion_deinit(&recursion);
				} else {
					retval = 1;
				}
			}
		} else {
			retval = 1;
		}

		argpos++;
	}

	return retval;
}

