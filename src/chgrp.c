#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include <errno.h>
#include <err.h>

#include "core_fs.h"

static int
chgrp_at(int dirfd, const char *name, const char *path,
	struct stat *statp, gid_t gid, bool follows) {
	int flags = follows ? 0 : AT_SYMLINK_NOFOLLOW;
	int retval;

	if((retval = fstatat(dirfd, name, statp, flags)) == 0) {
		if((retval = fchownat(dirfd, name, statp->st_uid, gid, flags)) == -1) {
			warn("chown %s", path);
		}
	} else {
		warn("stat %s", path);
	}

	return retval;
}

static int
chgrp_hierarchy(const char *directory, gid_t gid, bool follows) {
	struct fs_recursion recursion;
	int retval = 0;

	if(fs_recursion_init(&recursion, directory, 256, follows) == 0) {
		do {
			while(fs_recursion_next(&recursion) == 0 && *recursion.name != '\0') {
				struct stat st;

				if(chgrp_at(dirfd(recursion.dirp), recursion.name, recursion.path,
					&st, gid, follows) == 0) {
					if(S_ISDIR(st.st_mode)) {
						fs_recursion_push(&recursion);
					}
				} else {
					retval++;
				}
			}
		} while(fs_recursion_pop(&recursion) == 0);

		fs_recursion_deinit(&recursion);
	} else {
		retval = 1;
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

int
main(int argc,
	char **argv) {
	struct {
		unsigned recursive : 1;
		unsigned follows : 1;
		unsigned followstraversal : 1;
	} args = { 0, 1, 0 };
	int retval = 0;
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

	if(argc - optind > 1) {
		char **argpos = argv + optind + 1, ** const argend = argv + argc;
		gid_t gid = chgrp_gid(argv[optind]);
		umask(0); /* Clear the set-user id and set-group id bits */

		while(argpos != argend) {
			const char *file = *argpos;
			struct stat st;

			if(chgrp_at(AT_FDCWD, file, file, &st, gid, args.follows == 1) == 0) {
				if(S_ISDIR(st.st_mode) && args.recursive == 1) {
					retval += chgrp_hierarchy(file, gid, args.followstraversal == 1);
				}
			} else {
				retval++;
			}

			argpos++;
		}
	} else {
		chgrp_usage(*argv);
	}

	return retval;
}

