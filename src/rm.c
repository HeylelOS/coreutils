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

#include "core_io.h"

struct rm_args {
	unsigned recursive : 1;
	unsigned force : 1;
	unsigned interactive : 1;
};

struct rm_recursion {
	DIR *dirp;

	long *locations;
	size_t count;
	size_t capacity;

	char *path;
	char *pathnul;
	const char *pathend;
};

static inline bool
rm_is_dot_or_dot_dot(const char *name) {

	return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

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

static DIR *
rm_opendirat(DIR *dirp, const char *path) {
	int newdirfd = openat(dirfd(dirp), path, O_DIRECTORY | O_NOFOLLOW);
	DIR *newdirp = NULL;

	if(newdirfd >= 0) {
		newdirp = fdopendir(newdirfd);

		if(newdirp == NULL) {
			close(newdirfd);
		}
	} else {
		warn("Unable to explore %s", path);
	}

	return newdirp;
}

static int
rm_recursion_init(struct rm_recursion *recursion, const char *directory) {

	if((recursion->dirp = opendir(directory)) == NULL) {
		goto rm_recursion_init_err0;
	}

	if((recursion->path = malloc(PATH_MAX)) == NULL) {
		goto rm_recursion_init_err1;
	}

	recursion->pathend = recursion->path + PATH_MAX;
	recursion->count = 0;
	recursion->capacity = 16;

	if((recursion->pathnul = stpncpy(recursion->path, directory,
			recursion->pathend - recursion->path)) >= recursion->pathend - 1
		|| (recursion->locations = malloc(sizeof(*recursion->locations) * recursion->capacity)) == NULL) {
		goto rm_recursion_init_err2;
	}

	if(recursion->pathnul[-1] != '/') {
		*recursion->pathnul = '/';
		recursion->pathnul++;
	}

	return 0;

rm_recursion_init_err2:
	free(recursion->path);
rm_recursion_init_err1:
	closedir(recursion->dirp);
rm_recursion_init_err0:
	return -1;
}

static void
rm_recursion_deinit(struct rm_recursion *recursion) {

	closedir(recursion->dirp);
	free(recursion->path);
	free(recursion->locations);
}

static int
rm_recursion_next(struct rm_recursion *recursion, struct dirent **entryp) {
	errno = 0;

	do {
		*entryp = readdir(recursion->dirp);
	} while(*entryp != NULL && rm_is_dot_or_dot_dot((*entryp)->d_name));

	if(*entryp != NULL) {
		char *pathnul;

		while((pathnul = stpncpy(recursion->pathnul, (*entryp)->d_name,
			recursion->pathend - recursion->pathnul)) >= recursion->pathend - 1) {
			size_t length = recursion->pathnul - recursion->path,
				capacity = recursion->pathend - recursion->path;
			char *newpath = realloc(recursion->path, capacity * 2);

			if(newpath != NULL) {
				recursion->path = newpath;
				recursion->pathnul = newpath + length;
				recursion->pathend = newpath + capacity * 2;
			} else {
				return -1;
			}
		}

		*pathnul = '\0';
	} else if(errno != 0) {
		warn("Unable to read entry at %s", recursion->path);
		return -1;
	}

	return 0;
}

static int
rm_recursion_push(struct rm_recursion *recursion, const char *name) {
	long location = telldir(recursion->dirp);
	DIR *newdirp;

	if(location != -1
		&& (newdirp = rm_opendirat(recursion->dirp, name)) != NULL) {
		if(recursion->count == recursion->capacity) {
			long *newlocations = realloc(recursion->locations,
				sizeof(*recursion->locations) * recursion->capacity * 2);

			if(newlocations != NULL) {
				recursion->capacity *= 2;
				recursion->locations = newlocations;
			} else {
				closedir(newdirp);
				return -1;
			}
		}

		closedir(recursion->dirp);
		recursion->dirp = newdirp;
		recursion->locations[recursion->count] = location;
		recursion->count++;

		while(*recursion->pathnul != '\0') {
			recursion->pathnul++;
		}
		*recursion->pathnul = '/';
		recursion->pathnul++;
	} else {
		return -1;
	}

	return 0;
}

static int
rm_recursion_pop(struct rm_recursion *recursion, int *errors, const struct rm_args args) {
	DIR *newdirp = rm_opendirat(recursion->dirp, "..");
	int retval = -1;

	if(newdirp != NULL) {
		const char *base = basename(recursion->path);

		*recursion->pathnul = '\0';
		if(rm_confirm(dirfd(newdirp), base, recursion->path, "Do you want to remove directory", args)
			&& unlinkat(dirfd(newdirp), base, AT_REMOVEDIR) == -1) {
			warn("rmdir %s", recursion->path);
			++*errors;
		}

		if(recursion->count != 0) {
				recursion->count--;
				seekdir(newdirp, recursion->locations[recursion->count]);
				closedir(recursion->dirp);
				recursion->dirp = newdirp;

				do {
					recursion->pathnul--;
				} while(recursion->pathnul[-1] != '/');
				*recursion->pathnul = '\0';

				retval = 0;
		}
	}

	return retval;
}

static int
rm_hierarchy(const char *directory, const struct rm_args args) {
	struct rm_recursion recursion;
	struct stat st;
	int retval = 0;

	if(rm_recursion_init(&recursion, directory) == 0) {
		do {
			struct dirent *entry;

			while(rm_recursion_next(&recursion, &entry) == 0 && entry != NULL) {
				if(rm_at(dirfd(recursion.dirp), entry->d_name, recursion.path, &st, args) == 0
					&& S_ISDIR(st.st_mode)) {
					rm_recursion_push(&recursion, entry->d_name);
				}
			}
		} while(rm_recursion_pop(&recursion, &retval, args) == 0);

		rm_recursion_deinit(&recursion);
	} else {
		retval = 1;
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
			warnx("Unknown argument -%c", c);
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

		if(!rm_is_dot_or_dot_dot(base) && *base != '/') {
			if(rm_at(AT_FDCWD, file, file, &st, args) == 0
				&& S_ISDIR(st.st_mode)) {

				if(args.recursive == 1) {
					retval += rm_hierarchy(file, args);
				} else {
					warnx("-R or -r not specified, won't remove %s", file);
					retval++;
				}
			}
		} else {
			warnx("Invalid operand %s", file);
			retval++;
		}

		argpos++;
	}

	return retval;
}

