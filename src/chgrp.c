#include <stdio.h>
#include <unistd.h>
#include <grp.h>
#include <errno.h>
#include <err.h>

#include "core_fs.h"

static int
chgrp_change(const char *file, const struct stat *statp, gid_t gid) {
	int retval;

	if(S_ISLNK(statp->st_mode)) {
		if((retval = lchown(file, statp->st_uid, gid)) == -1) {
			warn("lchown %s", file);
		}
	} else {
		if((retval = chown(file, statp->st_uid, gid)) == -1) {
			warn("chown %s", file);
		} else if((retval = chmod(file, statp->st_mode & (S_ISVTX | S_IRWXA))) == -1) {
			warn("chmod %s", file);
		}
	}

	return retval;
}

static int
chgrp_change_follow(const char *file, gid_t gid, bool follows) {
	struct stat st;
	int retval;

	if(follows ? (retval = -stat(file, &st)) == 0
		: (retval = -lstat(file, &st)) == 0) {
		retval = -chgrp_change(file, &st, gid);
	} else {
		warn("stat %s", file);
	}

	return retval;
}

static int
chgrp_change_hierarchy(const char *file, gid_t gid,
	bool follows, bool traversal) {
	struct stat st;
	int retval;

	if((retval = -stat(file, &st)) == 0) {
		if(!follows) {
			struct stat lst;

			if((retval = -lstat(file, &lst)) == 0) {
				retval = -chgrp_change(file, &lst, gid);
			} else {
				warn("lstat %s", file);
			}
		} else {
			retval = -chgrp_change(file, &st, gid);
		}

		if(S_ISDIR(st.st_mode)) {
			struct fs_recursion recursion;
			char buffer[PATH_MAX], * const bufferend = buffer + sizeof(buffer);

			if(fs_recursion_init(&recursion,
				buffer, stpncpy(buffer, file, sizeof(buffer)), bufferend) == 0) {

				while(!fs_recursion_is_empty(&recursion)) {
					struct dirent *entry;

					while((entry = readdir(fs_recursion_peak(&recursion))) != NULL) {
						if(fs_recursion_is_valid(entry->d_name)) {
							if(entry->d_type == DT_DIR) {
								if(fs_recursion_push(&recursion, entry->d_name) == 0) {
									retval += -chgrp_change_follow(buffer, gid, traversal);
								} else {
									warnx("Unable to explore directory %s", buffer);
									retval++;
								}
							} else {
								char *pathend = fs_recursion_path_end(&recursion);

								if(stpncpy(pathend, entry->d_name, bufferend - pathend) < bufferend) {
									retval += -chgrp_change_follow(buffer, gid, traversal);
								} else {
									warnx("Unable to chgrp %s: Path too long", buffer);
									retval++;
								}
							}
						}
					}

					fs_recursion_pop(&recursion);
				}

				fs_recursion_deinit(&recursion);
			} else {
				warnx("Unable to explore hierarchy of %s", file);
			}
		}
	} else {
		warn("stat %s", file);
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

			if(args.recursive == 1) {
				retval += chgrp_change_hierarchy(file, gid, args.follows == 1, args.followstraversal == 1);
			} else {
				retval += chgrp_change_follow(file, gid, args.follows == 1);
			}

			argpos++;
		}
	} else {
		chgrp_usage(*argv);
	}

	return retval;
}

