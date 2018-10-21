#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <heylel/core.h>

ssize_t
fs_basename(const char *path,
	const char **pbegin,
	const char **pend) {

	if(*path != '\0') {
		const char *end = path + strlen(path);

		while(end != path
			&& end[-1] == '/') {
			end -= 1;
		}

		const char *begin = end;

		while(begin != path
			&& begin[-1] != '/') {
			begin -= 1;
		}

		if(end == begin) {
			/* Only '/' on path case */
			end += 1;
		}

		*pbegin = begin;
		*pend = end;

		return 0;
	}

	return -1;
}

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

const char *
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

