#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

int
main(int argc,
	char **argv) {

	while(getopt(argc, argv, "") != -1);

	if(argc - optind == 1) {
		char * const dir = dirname(argv[optind]);

		puts(dir);

		return 0;
	} else {
		fprintf(stderr, "usage: %s string\n", *argv);
		return 1;
	}
}

