
#ifndef HEYLEL_CORE_FS_H
#define HEYLEL_CORE_FS_H

#include <sys/stat.h>
#include <sys/resource.h>

#ifndef HEYLEL_UNUSED
#define HEYLEL_UNUSED __attribute__((unused))
#endif

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
			expression += 1;
		}

		if(isop(*expression)) {
			do {
				const char op = *expression;
				mode_t permmask = 0;

				expression += 1;
				if(ispermcopy(*expression)) {
					permmask = fs_mask_permcopy(*expression,
						parsed);
					expression += 1;
				} else if(isperm(*expression)) {
					do {
						permmask |= fs_mask_perm(*expression,
							*mode, isdir);
						expression += 1;
					} while(isperm(*expression));
				} else {
					break;
				}

				if(permmask != 0) {
					if(whomask == 0) {
						whomask = ~cmask;
					}
					parsed = fs_mask_op(parsed,
						whomask, permmask, op);
				}
			} while(isop(*expression));
		} else {
			break;
		}
	} while(*expression == ','
		&& *(expression += 1) != '\0');

	*mode = parsed;

	return expression;
}

#define HEYLEL_FDLIMIT_DEFAULT	1024
static int HEYLEL_UNUSED
fs_fdlimit(int deflimit) {
	struct rlimit rl;

	if(getrlimit(RLIMIT_NOFILE, &rl) == 0) {
		return rl.rlim_cur;
	} else {
		return deflimit;
	}
}

/* HEYLEL_CORE_FS_H */
#endif
