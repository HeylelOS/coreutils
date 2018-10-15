#include <stdio.h>
#include <string.h>

#include <heylel/core.h>

int
main(int argc,
	char **argv) {
	const char *begin, *end;

	if(argc < 2 || argc > 3) {
		fprintf(stderr, "usage: %s string [suffix]\n", *argv);
		return 1;
	}

	fs_basename(argv[1], &begin, &end);

	char *basename = strndup(begin, end - begin);

	if(argc == 2) {
		puts(basename);
	} else {
		/* suffix case */
	}

	return 0;
}

