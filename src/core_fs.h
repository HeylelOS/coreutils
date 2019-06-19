
#ifndef HEYLEL_CORE_FS_H
#define HEYLEL_CORE_FS_H

#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>

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
	DIR **directories;
	size_t capacity;
	size_t count;

	char *buffer;
	const char *bufferend;
	char *current;
};

static int
fs_recursion_init(struct fs_recursion *recursion,
	char *buffer, const char *bufferend, char *pathend) {

	if(pathend < bufferend) {
		recursion->capacity = 16;
		recursion->directories = malloc(sizeof(*recursion->directories) * recursion->capacity);

		if(recursion->directories != NULL) {
			recursion->buffer = buffer;
			recursion->bufferend = bufferend;
			recursion->current = pathend;

			if(recursion->buffer < recursion->current) {
				DIR *dirp = opendir(recursion->buffer);

				if(dirp != NULL) {
					recursion->directories[0] = dirp;
					recursion->count = 1;

					if(recursion->current[-1] != '/') {
						*recursion->current = '/';
						recursion->current++;
					}
				} else {
					free(recursion->directories);
					return -1;
				}
			} else {
				recursion->count = 0;
			}
		} else {
			return -2;
		}
	} else {
		return -3;
	}

	return 0;
}

static void
fs_recursion_deinit(struct fs_recursion *recursion) {

	free(recursion->directories);
}

static inline bool
fs_recursion_is_empty(const struct fs_recursion *recursion) {

	return recursion->count == 0;
}

static inline char *
fs_recursion_path_end(const struct fs_recursion *recursion) {

	return recursion->current;
}

static inline DIR *
fs_recursion_peak(const struct fs_recursion *recursion) {

	return recursion->directories[recursion->count - 1];
}

static int
fs_recursion_push(struct fs_recursion *recursion, const char *name) {
	char *nameend = stpncpy(recursion->current, name,
		recursion->bufferend - recursion->current);

	if(nameend < recursion->bufferend - 1) {
		DIR *dirp = opendir(recursion->buffer);

		if(dirp != NULL) {
			if(recursion->count == recursion->capacity - 1) {
				DIR **newdirectories = realloc(recursion->directories,
					sizeof(*recursion->directories) * recursion->capacity * 2);

				if(newdirectories == NULL) {
					closedir(dirp);
					return -1;
				}

				recursion->directories = newdirectories;
				recursion->capacity *= 2;
			}

			*nameend = '/';
			recursion->current = nameend + 1;
			recursion->directories[recursion->count] = dirp;
			recursion->count++;

			return 0;
		}

		return -2;
	}

	return -3;
}

static void
fs_recursion_pop(struct fs_recursion *recursion) {

	recursion->count--;
	closedir(recursion->directories[recursion->count]);

	do {
		recursion->current--;
	} while(recursion->current != recursion->buffer
		&& recursion->current[-1] != '/');
}

static inline bool
fs_recursion_is_valid(const char *name) {

	return name[0] != '.' || (name[1] != '\0' && (name[1] != '.' || name[2] != '\0'));
}

/* HEYLEL_CORE_FS_H */
#endif
