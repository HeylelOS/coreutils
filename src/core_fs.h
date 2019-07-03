#ifndef HEYLEL_CORE_FS_H
#define HEYLEL_CORE_FS_H

#ifdef __GLIBC__
#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

/**
 * The following macros complete the mode mask
 * set to include multiple useful selections
 */
#define S_IRALL (S_IRUSR | S_IRGRP | S_IROTH)
#define S_IWALL (S_IWUSR | S_IWGRP | S_IWOTH)
#define S_IXALL (S_IXUSR | S_IXGRP | S_IXOTH)
#define S_IRWXA (S_IRWXU | S_IRWXG | S_IRWXO)
#define S_ISALL (S_ISUID | S_ISGID | S_ISVTX)

struct fs_recursion {
	DIR *dirp;
	int flags;

	long *locations;
	size_t count;
	size_t capacity;

	char *path;
	char *name;
	const char *pathend;
};

static inline bool
fs_is_dot_or_dot_dot(const char *name) {

	return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

static DIR *
fs_opendirat(int dirfd, const char *path, int flags) {
	int newdirfd = openat(dirfd, path, flags);
	DIR *newdirp = NULL;

	if(newdirfd >= 0) {
		newdirp = fdopendir(newdirfd);

		if(newdirp == NULL) {
			warn("Unable to explore %s", path);
			close(newdirfd);
		}
	} else {
		warn("Unable to open %s", path);
	}

	return newdirp;
}

static int
fs_recursion_init(struct fs_recursion *recursion,
	const char *directory, size_t size, bool follows) {

	if((recursion->dirp = fs_opendirat(AT_FDCWD, directory, O_DIRECTORY | O_NOFOLLOW)) == NULL) {
		goto fs_recursion_init_err0;
	}

	if((recursion->path = malloc(size)) == NULL) {
		goto fs_recursion_init_err1;
	}

	recursion->flags = O_DIRECTORY | (follows ? 0 : O_NOFOLLOW);
	recursion->pathend = recursion->path + size;
	recursion->count = 0;
	recursion->capacity = 16;

	if((recursion->name = stpncpy(recursion->path, directory, size)) >= recursion->pathend - 1
		|| (recursion->locations = malloc(sizeof(*recursion->locations) * recursion->capacity)) == NULL) {
		goto fs_recursion_init_err2;
	}

	*recursion->locations = 0;
	if(recursion->name[-1] != '/') {
		*recursion->name = '/';
		recursion->name++;
	}

	return 0;

fs_recursion_init_err2:
	free(recursion->path);
fs_recursion_init_err1:
	closedir(recursion->dirp);
fs_recursion_init_err0:
	return -1;
}

static void
fs_recursion_deinit(struct fs_recursion *recursion) {

	closedir(recursion->dirp);
	free(recursion->path);
	free(recursion->locations);
}

static int
fs_recursion_next(struct fs_recursion *recursion) {
	struct dirent *entry;

	do {
		errno = 0;
		entry = readdir(recursion->dirp);
		recursion->locations[recursion->count]++;
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
fs_recursion_push(struct fs_recursion *recursion) {
	DIR *newdirp = fs_opendirat(dirfd(recursion->dirp),
		recursion->name, recursion->flags);

	if(newdirp != NULL) {
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
		recursion->count++;
		recursion->locations[recursion->count] = 0;

		while(*recursion->name != '\0') {
			recursion->name++;
		}
		*recursion->name = '/';
		recursion->name++;
	} else {
		return -1;
	}

	return 0;
}

static int
fs_recursion_pop(struct fs_recursion *recursion) {

	if(recursion->count != 0) {
		DIR *newdirp;

		do {
			recursion->name--;
		} while(recursion->name[-1] != '/');
		*recursion->name = '\0';

		if((recursion->flags & O_NOFOLLOW) == 0) {
			newdirp = fs_opendirat(AT_FDCWD, recursion->path, recursion->flags);
		} else {
			newdirp = fs_opendirat(dirfd(recursion->dirp), "..", recursion->flags);
		}

		if(newdirp != NULL) {
			struct dirent *entry;
			long i = 0;

			recursion->count--;

			while(i < recursion->locations[recursion->count]
				&& (entry = readdir(newdirp)) != NULL) {
				if(!fs_is_dot_or_dot_dot(entry->d_name)) {
					i++;
				}
			}

			closedir(recursion->dirp);
			recursion->dirp = newdirp;

			return 0;
		}
	}

	return -1;
}

/* HEYLEL_CORE_FS_H */
#endif
