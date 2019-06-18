#include <stdio.h>
#include <unistd.h>
#include <err.h>

int
main(int argc,
	char **argv) {

	while(getopt(argc, argv, "") != -1);

	if(argc - optind == 1) {
		if(unlink(argv[optind]) == -1) {
			err(1, "unlink %s", argv[optind]);
		}

		return 0;
	} else {
		fprintf(stderr, "usage: %s file\n", *argv);
		return 1;
	}
}

