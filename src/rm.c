#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <limits.h>
#include <dirent.h>
#include <libgen.h>
#include <errno.h>
#include <err.h>

#include "core_fs.h"
#include "core_io.h"

struct rm_args {
	unsigned recursive : 1;
	unsigned force : 1;
	unsigned interactive : 1;
};

static bool
rm_confirm(int dirfd, const char *file, const char *path,
	const char *message, const struct rm_args args) {

	return (args.force == 1
			|| (args.interactive == 0
				&& (faccessat(dirfd, file, W_OK, AT_SYMLINK_NOFOLLOW) == 0
					|| !isatty(STDIN_FILENO))))
		|| io_prompt_confirm("%s '%s'? ", message, path);
}

static int
rm_at(int dirfd, const char *file,
	const char *path, struct stat *statp,
	const struct rm_args args) {
	int retval;

	if((retval = fstatat(dirfd, file, statp, AT_SYMLINK_NOFOLLOW)) == 0) {
		if(!S_ISDIR(statp->st_mode)
			&& rm_confirm(dirfd, file, path, "Do you want to remove file", args)
			&& (retval = unlinkat(dirfd, file, 0)) == -1) {

			warn("unlink %s", path);
		}
	} else if(args.force == 0
		|| (errno != ENOENT && errno != ENOTDIR)) {
		warn("stat %s", path);
	}

	return retval;
}

static int
rm_recursion_next(struct fs_recursion *recursion) {
	struct dirent *entry;
	(void)fs_recursion_next;

	do {
		errno = 0;
		entry = readdir(recursion->dirp);
	} while(entry != NULL && fs_is_dot_or_dot_dot(entry->d_name));

	if(entry != NULL) {
		while(stpncpy(recursion->name, entry->d_name,
			recursion->pathend - recursion->name) >= recursion->pathend - 1) {
			size_t length = recursion->name - recursion->path,
				capacity = recursion->pathend - recursion->path;
			char *newpath = realloc(recursion->path, capacity * 2);

			if(newpath != NULL) {
				recursion->path = newpath;
				recursion->name = newpath + length;
				recursion->pathend = newpath + capacity * 2;
			} else {
				return -1;
			}
		}
	} else if(errno != 0) {
		warn("Unable to read entry");
		return -1;
	} else {
		*recursion->name = '\0';
	}

	return 0;
}

static int
rm_recursion_pop(struct fs_recursion *recursion, int *error, const struct rm_args args) {
	int retval = -1;
	(void)fs_recursion_pop;

	if(recursion->count != 0) {
		DIR *newdirp;

		recursion->name[-1] = '\0';
		do {
			recursion->name--;
		} while(recursion->name[-1] != '/');

		newdirp = fs_opendirat(dirfd(recursion->dirp), "..", recursion->flags);

		if(newdirp != NULL) {
			struct dirent *entry;
			long i = 0;

			recursion->count--;
			if(rm_confirm(dirfd(newdirp), recursion->name, recursion->path, "Do you want to remove directory", args)
				&& unlinkat(dirfd(newdirp), recursion->name, AT_REMOVEDIR) == -1) {

				warn("rmdir %s", recursion->path);
				recursion->locations[recursion->count]++;
				*error = -1;
			}

			while(i < recursion->locations[recursion->count]
				&& (entry = readdir(newdirp)) != NULL) {
				if(!fs_is_dot_or_dot_dot(entry->d_name)) {
					i++;
				}
			}

			closedir(recursion->dirp);
			recursion->dirp = newdirp;

			retval = 0;
		}

		*recursion->name = '\0';
	}

	return retval;
}

static int
rm_hierarchy(const char *directory, const struct rm_args args) {
	struct fs_recursion recursion;
	int retval = 0;

	if(rm_confirm(AT_FDCWD,
					directory, directory,
					"Do you want to enter directory", args)) {
		if(fs_recursion_init(&recursion, directory, 256, false) == 0) {
			do {
				if(rm_recursion_next(&recursion) == 0 && *recursion.name != '\0'
					&& rm_confirm(dirfd(recursion.dirp),
						recursion.name, recursion.path,
						"Do you want to enter directory", args)) {
					do {
						struct stat st;

						if(rm_at(dirfd(recursion.dirp), recursion.name,
							recursion.path, &st, args) == -1
							|| (S_ISDIR(st.st_mode)
								&& fs_recursion_push(&recursion) == -1)) {
							recursion.locations[recursion.count]++;
							retval = -1;
						}
					} while(rm_recursion_next(&recursion) == 0 && *recursion.name != '\0');
				} else {
					recursion.locations[recursion.count]++;
				}
			} while(rm_recursion_pop(&recursion, &retval, args) == 0);

			recursion.name[-1] = '\0';
			if(rm_confirm(AT_FDCWD, directory, directory,
				"Do you want to remove directory", args)
				&& rmdir(directory) == -1) {

				warn("rmdir %s", directory);
				retval = -1;
			}

			fs_recursion_deinit(&recursion);
		} else {
			retval = -1;
		}
	}

	return retval;
}

static void
rm_usage(const char *rmname) {
	fprintf(stderr, "usage: %s [-iRr] file...\n"
		"       %s -f [-iRr] [file...]\n",
		rmname, rmname);
	exit(1);
}

static struct rm_args
rm_parse_args(int argc, char **argv) {
	struct rm_args args = { 0, 0, 0 };
	int c;

	while((c = getopt(argc, argv, "fiRr")) != -1) {
		switch(c) {
		case 'f':
			args.force = 1;
			break;
		case 'i':
			args.interactive = 1;
			break;
		case 'R':
		case 'r':
			args.recursive = 1;
			break;
		default:
			warnx("Unknown argument -%c", optopt);
			rm_usage(*argv);
		}
	}

	if(argc - optind < 1 - args.force) {
		warnx("Not enough arguments");
		rm_usage(*argv);
	}

	return args;
}

int
main(int argc,
	char **argv) {
	const struct rm_args args = rm_parse_args(argc, argv);
	char **argpos = argv + optind, ** const argend = argv + argc;
	int retval = 0;

	while(argpos != argend) {
		const char *base = basename(*argpos);
		char *file = *argpos;
		struct stat st;

		if(!fs_is_dot_or_dot_dot(base) && *base != '/') {
			if(rm_at(AT_FDCWD, file, file, &st, args) == 0
				&& S_ISDIR(st.st_mode)) {

				if(args.recursive == 1) {
					if(rm_hierarchy(file, args) != 0) {
						retval = 1;
					}
				} else {
					warnx("-R or -r not specified, won't remove %s", file);
					retval = 1;
				}
			}
		} else {
			warnx("Invalid operand %s", file);
			retval = 1;
		}

		argpos++;
	}

	return retval;
}

