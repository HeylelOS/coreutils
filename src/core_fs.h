
#ifndef HEYLEL_CORE_FS_H
#define HEYLEL_CORE_FS_H

#include <sys/stat.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <dirent.h>

#ifndef HEYLEL_UNUSED
#define HEYLEL_UNUSED __attribute__((unused))
#endif

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
	} while(recursion->current[-1] != '/');
}

static inline bool
fs_recursion_is_valid(const char *name) {

	return name[0] != '.' || (name[1] != '\0' && (name[1] != '.' || name[2] != '\0'));
}

/**
 * The following macros complete the mode mask
 * set to include multiple useful selections
 */
#define S_IRALL (S_IRUSR | S_IRGRP | S_IROTH)
#define S_IWALL (S_IWUSR | S_IWGRP | S_IWOTH)
#define S_IXALL (S_IXUSR | S_IXGRP | S_IXOTH)
#define S_IRWXA (S_IRWXU | S_IRWXG | S_IRWXO)
#define S_ISALL (S_ISUID | S_ISGID | S_ISVTX)

#define isperm(c) ((c) == 'r'\
				|| (c) == 'w'\
				|| (c) == 'x'\
				|| (c) == 'X'\
				|| (c) == 's'\
				|| (c) == 't')

#define isop(c) ((c) == '+'\
				|| (c) == '-'\
				|| (c) == '=')

#define ispermcopy(c) ((c) == 'u'\
				|| (c) == 'g'\
				|| (c) == 'o')

#define iswho(c) (ispermcopy((c))\
				|| (c) == 'a')

static mode_t
fs_mask_who(char who) {
	switch(who) {
	case 'u':
		return S_IRWXU | S_ISUID;
	case 'g':
		return S_IRWXG | S_ISGID;
	case 'o':
		return S_IRWXO;
	default: /* a */
		return S_IRWXA | S_ISUID | S_ISGID;
	}
}

static mode_t
fs_mask_permcopy(char copy,
	mode_t source) {
	switch(copy) {
	case 'u': {
		mode_t mask = source & S_IRWXU;
		return mask | mask >> 3 | mask >> 6;
	}
	case 'g': {
		mode_t mask = source & S_IRWXG;
		return mask | mask << 3 | mask >> 3;
	}
	default: { /* o */
		mode_t mask = source & S_IRWXO;
		return mask | mask << 3 | mask << 6;
	}
	}
}

static mode_t
fs_mask_perm(char who,
	mode_t mode,
	int isdir) {
	switch(who) {
	case 'r':
		return S_IRALL;
	case 'w':
		return S_IWALL;
	case 'x':
		return S_IXALL;
	case 'X': {
		mode_t xmode = mode & S_IXALL;
		if(isdir != 0
			|| xmode != 0) {
			return xmode;
		} else {
			return 0;
		}
	}
	case 's':
		return S_ISUID | S_ISGID;
	default: /* t */
		return S_ISVTX;
	}
}

static mode_t
fs_mask_op(mode_t mode,
	mode_t whomask,
	mode_t permmask,
	char op) {
	mode_t mask = whomask & permmask;

	switch(op) {
	case '+':
		return mode | mask;
	case '-':
		return mode & ~mask;
	default: /* = */
		return (mode & ~(whomask & S_IRWXA)) | permmask;
	}
}

/**
 * Parses expression as specified in the standard
 * and returns a pointer to the last valid character
 * @param expression The string to parse
 * @param mode mode to modify
 * @param cmask Consider it the cmask of the process
 * @param isdir Act like we target a directory
 * @return A pointer to the last valid character of expression
 */
static const char * HEYLEL_UNUSED
fs_parsemode(const char *expression,
	mode_t *mode,
	mode_t cmask,
	int isdir) {
	mode_t parsed = *mode;

	do {
		mode_t whomask = 0;
		while(iswho(*expression)) {
			whomask |= fs_mask_who(*expression);
			expression++;
		}

		if(isop(*expression)) {
			do {
				const char op = *expression;
				mode_t permmask = 0;

				expression++;
				if(ispermcopy(*expression)) {
					permmask = fs_mask_permcopy(*expression, parsed);
					expression++;
				} else if(isperm(*expression)) {
					do {
						permmask |= fs_mask_perm(*expression, *mode, isdir);
						expression++;
					} while(isperm(*expression));
				} else {
					break;
				}

				if(permmask != 0) {
					if(whomask == 0) {
						whomask = ~cmask;
					}
					parsed = fs_mask_op(parsed, whomask, permmask, op);
				}
			} while(isop(*expression));
		} else {
			break;
		}
	} while(*expression == ','
		&& *(expression++) != '\0');

	*mode = parsed;

	return expression;
}

/* HEYLEL_CORE_FS_H */
#endif
