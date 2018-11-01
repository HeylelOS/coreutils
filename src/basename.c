#include <stdio.h>
#include <string.h>

#include "core_fs.h"

int
main(int argc,
	char **argv) {
	const char *begin, *end;

	if(argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s string [suffix]\n", *argv);
		return 1;
	}

	fs_basename(argv[1], &begin, &end);

	size_t basenamelen = end - begin;
	char *basename = strndup(begin, basenamelen);

	if(argc == 3) {
		char *suffix = argv[2];
		size_t suffixlen = strlen(suffix);
		char *basesuffix = basename + basenamelen - suffixlen;

		if(basesuffix > basename) {
			if(strcmp(basesuffix, suffix) == 0) {
				*basesuffix = '\0';
			}
		}
	}

	puts(basename);

	/* free(basename); */

	return 0;
}

