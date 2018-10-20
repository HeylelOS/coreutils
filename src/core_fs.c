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

const char *
fs_parsemask(const char *mask,
	int isdir,
	mode_t *mode) {
	mode_t parsed = *mode;

	do {
		char * const whobegin = mask;
		while(iswho(*mask)) {
			mask += 1;
		}
		char * const whoend = mask;

		if(isop(*mask)) {
			do {
				mask += 1;
				if(ispermcopy(*mask)) {
					mask += 1;
				} else if(isperm(*mask)) {
					do {
						mask += 1;
					} while(isperm(*mask));
				} else {
					break;
				}
			} while(isop(*mask));
		} else {
			break;
		}
	} while(*mask == ',');

	*mode = parsed;

	return mask;
}

