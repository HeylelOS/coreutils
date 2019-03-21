#include <stdio.h>
#include <string.h>

static int
asa_map(char **linep, size_t *linecapp, FILE *filep) {
	ssize_t length;
	int retval = 0;

	while((length = getline(linep, linecapp, filep)) > 0) {
		char *line = *linep;
		line[length - 1] = '\0';

		switch(**linep) {
		case '1':
			putchar('\n');
		case '0':
			putchar('\n');
		case '+':
		case ' ':
			line += 1;
			break;
		default:
			break;
		}

		if(puts(line) == EOF) {
			retval = -1;
		}
	}

	if(length < 0) {
		retval = -1;
	}

	return retval;
}

/**
 * I don't even know why I implement this one...
 */
int
main(int argc,
	char **argv) {
	char *line = NULL;
	size_t linecap = 0;

	if(argc > 1) {
		char **argpos = argv + 1;
		char ** const argend = argv + argc;
		int retval = 0;

		while(argpos != argend) {
			FILE *filep;

			if(strcmp("-", *argpos) != 0) {
				filep = fopen(*argpos, "r");
			} else {
				filep = stdin;
			}

			if(asa_map(&line, &linecap, filep) == -1) {
				retval = 1;
			}

			fclose(filep);
			++argpos;
		}

		return retval;
	} else {
		return asa_map(&line, &linecap, stdin) == -1 ?
			1 : 0;
	}
}

