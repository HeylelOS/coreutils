#include <stdio.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>
#include <err.h>

#include "core_fs.h"

static int
chown_change(const char *file, const struct stat *statp,
	uid_t uid, gid_t *gidp) {
	int retval;

	if(S_ISLNK(statp->st_mode)) {
		if((retval = lchown(file, uid, gidp == NULL ? statp->st_gid : *gidp)) == -1) {
			warn("lchown %s", file);
		}
	} else {
		if((retval = chown(file, uid, gidp == NULL ? statp->st_gid : *gidp)) == -1) {
			warn("chown %s", file);
		} else if((retval = chmod(file, statp->st_mode & (S_ISVTX | S_IRWXA))) == -1) {
			warn("chmod %s", file);
		}
	}

	return retval;
}

static int
chown_change_follow(const char *file,
	uid_t uid, gid_t *gidp, bool follows) {
	struct stat st;
	int retval;

	if(follows ? (retval = -stat(file, &st)) == 0
		: (retval = -lstat(file, &st)) == 0) {
		retval = -chown_change(file, &st, uid, gidp);
	} else {
		warn("stat %s", file);
	}

	return retval;
}

static int
chown_change_hierarchy(const char *file,
	uid_t uid, gid_t *gidp,
	bool follows, bool traversal) {
	struct stat st;
	int retval;

	if((retval = -stat(file, &st)) == 0) {
		if(!follows) {
			struct stat lst;

			if((retval = -lstat(file, &lst)) == 0) {
				retval = -chown_change(file, &lst, uid, gidp);
			} else {
				warn("lstat %s", file);
			}
		} else {
			retval = -chown_change(file, &st, uid, gidp);
		}

		if(S_ISDIR(st.st_mode)) {
			struct fs_recursion recursion;
			char path[PATH_MAX], * const pathend = path + sizeof(path);

			if(fs_recursion_init(&recursion,
				path, pathend, stpncpy(path, file, sizeof(path))) == 0) {

				while(!fs_recursion_is_empty(&recursion)) {
					struct dirent *entry;

					while((entry = readdir(fs_recursion_peak(&recursion))) != NULL) {
						if(fs_recursion_is_valid(entry->d_name)) {
							if(entry->d_type == DT_DIR) {
								if(fs_recursion_push(&recursion, entry->d_name) == 0) {
									retval += -chown_change_follow(path, uid, gidp, traversal);
								} else {
									warnx("Unable to explore directory %s", path);
									retval++;
								}
							} else {
								char *nameend = fs_recursion_path_end(&recursion);

								if(stpncpy(nameend, entry->d_name, pathend - nameend) < pathend) {
									retval += -chown_change_follow(path, uid, gidp, traversal);
								} else {
									warnx("Unable to chown %s: Path too long", path);
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
	fprintf(stderr, "usage: %s [-h] owner[:group] file...\n"
		"       %s -R [-H|-L|-P] owner[:group] file...\n",
		chownname, chownname);
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
			chown_usage(*argv);
			break;
		}
	}

	if(argc - optind > 1) {
		char **argpos = argv + optind + 1, ** const argend = argv + argc;
		uid_t uid;
		gid_t gid, *gidp = NULL;

		if(chown_uid_gid(argv[optind], &uid, &gid) == 1) {
			gidp = &gid;
		}
		umask(0); /* Clear the set-user id and set-group id bits */

		while(argpos != argend) {
			const char *file = *argpos;

			if(args.recursive == 1) {
				retval += chown_change_hierarchy(file, uid, gidp,
					args.follows == 1, args.followstraversal == 1);
			} else {
				retval += chown_change_follow(file, uid, gidp, args.follows == 1);
			}

			argpos++;
		}
	} else {
		chown_usage(*argv);
	}

	return retval;
}

