#include <stdio.h>
#include <unistd.h>

int
main(int argc,
	char **argv) {
	const char *tty;

	while(getopt(argc, argv, "") != -1);

	/**
	 * Note on return values:
	 * The standard considers 3 return values, 0 on success, 1 if not a tty,
	 * and >1 if an error occured. However, ttyname() shall fail with error codes
	 * specifying only a non terminal output, that's why we only return 1 on "error".
	 */

	if((tty = ttyname(STDIN_FILENO)) != NULL) {
		puts(tty);
		return 0;
	} else {
		puts("not a tty");
		return 1;
	}
}

