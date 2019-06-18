#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int
main(int argc,
	char **argv) {

	while(getopt(argc, argv, "") != -1);

	if(argc - optind < 3) {
		char * const dir = dirname(argv[optind]);

		if(argc - optind == 2) {
			char * const suffix = argv[optind + 1];
			char * const dirsuffix = dir + strlen(dir) - strlen(suffix);

			if(dirsuffix > dir) {
				if(strcmp(dirsuffix, suffix) == 0) {
					*dirsuffix = '\0';
				}
			}
		}

		puts(dir);

		return 0;
	} else {
		fprintf(stderr, "usage: %s string [suffix]\n", *argv);
		return 1;
	}
}

