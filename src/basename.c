#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int
main(int argc,
	char **argv) {

	while(getopt(argc, argv, "") != -1);

	if(argc != optind
		&& argc - optind < 3) {
		char * const base = basename(argv[optind]);

		if(argc - optind == 2) {
			char * const suffix = argv[optind + 1];
			char * const basesuffix = base + strlen(base) - strlen(suffix);

			if(basesuffix > base) {
				if(strcmp(basesuffix, suffix) == 0) {
					*basesuffix = '\0';
				}
			}
		}

		puts(base);

		return 0;
	} else {
		fprintf(stderr, "usage: %s string [suffix]\n", *argv);
		return 1;
	}
}

