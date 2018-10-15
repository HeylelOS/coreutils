#include <string.h>

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


