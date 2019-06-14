#include <stdio.h>
#include <string.h>

#include "core_fs.h"

int
main(int argc,
	char **argv) {

	if(argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s string [suffix]\n", *argv);
		return 1;
	}

	const char *begin, *end;
	fs_basename(argv[1], &begin, &end);

	const size_t basenamelen = end - begin;
	char * const basename = (char *)begin;

	if(argc == 3) {
		char * const suffix = argv[2];
		const size_t suffixlen = strlen(suffix);
		char * const basesuffix = basename + basenamelen - suffixlen;

		if(basesuffix > basename) {
			if(strcmp(basesuffix, suffix) == 0) {
				*basesuffix = '\0';
			}
		}
	}

	puts(basename);

	return 0;
}

