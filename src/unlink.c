#include <stdio.h>
#include <unistd.h>

int
main(int argc,
	char **argv) {

	if(argc != 2) {
		fprintf(stderr, "usage: %s file\n", *argv);
		return 1;
	}

	if(unlink(argv[1]) == -1) {
		perror(*argv);
		return 1;
	}

	return 0;
}

