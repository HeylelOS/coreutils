#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include <errno.h>
#include <err.h>

#include "core_fs.h"

struct chgrp_args {
	unsigned recursive : 1;
	unsigned follows : 1;
	unsigned followstraversal : 1;
};

static int
chgrp_at(int dirfd, const char *name, const char *path,
	struct stat *statp, gid_t gid, bool follows) {
	int flags = follows ? 0 : AT_SYMLINK_NOFOLLOW;
	int retval;

	if((retval = fstatat(dirfd, name, statp, flags)) == 0) {
		if((retval = fchownat(dirfd, name, statp->st_uid, gid, flags)) == 0) {
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

static gid_t
chgrp_gid(const char *group) {
	struct group *grp;
	gid_t gid;

	errno = 0;
	if((grp = getgrnam(group)) == NULL) {
		if(errno == 0) {
			/* No entry found, treat as numeric gid */
			char *end;
			unsigned long lgid = strtoul(group, &end, 10);

			if(*group == '\0' || *end != '\0') {
				errx(1, "Unable to infer gid from '%s'", group);
			} else {
				gid = lgid;
			}
		} else {
			err(1, "getgrnam");
		}
	} else {
		gid = grp->gr_gid;
	}

	return gid;
}

static void
chgrp_usage(const char *chgrpname) {
	fprintf(stderr, "usage: %s [-h] group file...\n"
		"       %s -R [-H|-L|-P] group file...\n",
		chgrpname, chgrpname);
	exit(1);
}

static struct chgrp_args
chgrp_parse_args(int argc, char **argv) {
	struct chgrp_args args = { 0, 0, 0 };
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
			chgrp_usage(*argv);
			break;
		}
	}

	if(argc - optind < 2) {
		chgrp_usage(*argv);
	}

	return args;
}

static inline bool
chgrp_recur(const char *file, struct stat *statp, int *errors,
	const struct chgrp_args args) {

	if(args.follows == 0 && S_ISLNK(statp->st_mode)
		&& stat(file, statp) == -1) {
		warn("stat %s", file);
		++*errors;
	}

	return S_ISDIR(statp->st_mode) && args.recursive == 1;
}

int
main(int argc,
	char **argv) {
	const struct chgrp_args args = chgrp_parse_args(argc, argv);
	char **argpos = argv + optind + 1, ** const argend = argv + argc;
	gid_t gid = chgrp_gid(argv[optind]);
	int retval = 0;

	umask(0); /* When clearing set-user-ID and set-group-ID bits */

	while(argpos != argend) {
		const char *file = *argpos;
		struct stat st;

		/* chgrp operand */
		if(chgrp_at(AT_FDCWD, file, file, &st, gid, args.follows == 1) == 0) {
			/* Get destination stats if it didn't follow a symlink,
			enter recursion if it's a directory and -R specified */
			if(chgrp_recur(file, &st, &retval, args)) {
				struct fs_recursion recursion;

				if(fs_recursion_init(&recursion, file, 256, args.followstraversal == 1) == 0) {
					do {
						while(fs_recursion_next(&recursion) == 0 && *recursion.name != '\0') {
							if(chgrp_at(dirfd(recursion.dirp), recursion.name, recursion.path,
									&st, gid, args.followstraversal == 1) == 0
								&& S_ISDIR(st.st_mode)) {
								fs_recursion_push(&recursion);
							} else {
								retval++;
							}
						}
					} while(fs_recursion_pop(&recursion) == 0);

					fs_recursion_deinit(&recursion);
				} else {
					retval++;
				}
			}
		} else {
			retval++;
		}

		argpos++;
	}

	return retval;
}

