#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#define UNIQ_MIN(a, b) ((a) < (b) ? (a) : (b))

#define UNIQ_SWAP(type, a, b) \
do {\
	type c = (a);\
	(a) = (b);\
	(b) = c;\
} while(false)

char *uniqname;
FILE *in, *out;
static void (*uniq_print_occurrences)(char *, size_t);
static size_t skipfields;
static size_t skipchars;

static void
uniq_print_default(char *string,
	size_t occurrences) {

	if(occurrences != 0) {
		if(string == NULL) {
			putchar('\n');
		} else {
			puts(string);
		}
	}
}

static void
uniq_print_count(char *string,
	size_t occurrences) {

	if(occurrences != 0) {
		printf("%ld %s\n", occurrences, string == NULL ? "" : string);
	}
}

static void
uniq_print_repeated(char *string,
	size_t occurrences) {

	if(occurrences > 1) {
		if(string == NULL) {
			putchar('\n');
		} else {
			puts(string);
		}
	}
}

static void
uniq_print_not_repeated(char *string,
	size_t occurrences) {

	if(occurrences == 1) {
		if(string == NULL) {
			putchar('\n');
		} else {
			puts(string);
		}
	}
}

/**
 * Well, that's awful, but that's the exact standard
 */
static int
uniq_parse_args(int argc,
	char **argv) {
	char **argpos = argv + 1;
	char ** const argend = argv + argc;
	if(argpos != argend
		&& (**argpos == '-' || **argpos == '+')
		&& (*argpos)[1] != '\0'
		&& (*argpos)[2] == '\0') {
		switch((*argpos)[1]) {
		case 'c':
			uniq_print_occurrences = uniq_print_count;
			argpos += 1;
			break;
		case 'd':
			uniq_print_occurrences = uniq_print_repeated;
			argpos += 1;
			break;
		case 'u':
			uniq_print_occurrences = uniq_print_not_repeated;
			argpos += 1;
			break;
		default:
			break;
		}
	}

	if(argpos + 1 < argend
		&& (strcmp(*argpos, "-f") == 0
			|| strcmp(*argpos, "+f") == 0)) {
		errno = 0;
		char *endptr;
		skipfields = strtoul(argpos[1], &endptr, 10);
		if(errno != 0
			|| argpos[1][0] == '\0'
			|| *endptr != '\0') {
			fprintf(stderr, "error: %s %s: Must be a decimal numeric\n", uniqname, argpos[1]);
			exit(1);
		}

		argpos += 2;
	}

	if(argpos + 1 < argend
		&& (strcmp(*argpos, "-s") == 0
			|| strcmp(*argpos, "+s") == 0)) {
		errno = 0;
		char *endptr;
		skipchars = strtoul(argpos[1], &endptr, 10);
		if(errno != 0
			|| argpos[1][0] == '\0'
			|| *endptr != '\0') {
			fprintf(stderr, "error: %s %s: Must be a decimal numeric\n", uniqname, argpos[1]);
			exit(1);
		}

		argpos += 2;
	}

	if(argpos != argend) {
		if(strcmp(*argpos, "-") != 0) {
			if((in = fopen(*argpos, "r")) == NULL) {
				fprintf(stderr, "error: %s %s: %s\n",
					uniqname, *argpos, strerror(errno));
				exit(1);
			}
		}
		argpos += 1;

		if(argpos != argend) {
			if(strcmp(*argpos, "-") != 0) {
				if((out = fopen(*argpos, "w")) == NULL) {
					fprintf(stderr, "error: %s %s: %s\n",
						uniqname, *argpos, strerror(errno));
					exit(1);
				}
			}
			argpos += 1;
		}
	}

	if(argpos == argend) {
		return 0;
	} else {
		fprintf(stderr, "error: %s: Expected end of arguments, "
			"found '%s'\n", uniqname, *argpos);
		return -1;
	}
}

static void
uniq_init(int argc,
	char **argv) {
	uniqname = *argv;
	in = stdin;
	out = stdout;
	uniq_print_occurrences = uniq_print_default;
	/* static values initialized at zero */

	if(uniq_parse_args(argc, argv) != 0) {
		fprintf(stderr, "usage: %s [-d|-c|-u] [-f fields] "
			"[-s char] [input_file [output_file]]\n", uniqname);
		exit(1);
	}
}

static bool
uniq_equals(const char *str1, size_t len1,
	const char *str2, size_t len2) {

	return (len1 == 0 && len2 == 0)
		|| (len1 == len2
			&& memcmp(str1, str2, len1 + 1) == 0);
}

int
main(int argc,
	char **argv) {
	char *line = NULL, *previous = NULL;
	size_t linecapacity = 0, previouscapacity = 0;
	ssize_t linelen, previouslen;
	size_t occurrences = 0;
	size_t truncshift = 0;
	uniq_init(argc, argv);

	while((linelen = getline(&line, &linecapacity, in)) > 0) {
		/* Cut EOL */
		linelen -= 1;
		line[linelen] = '\0';

		size_t linetruncshift = UNIQ_MIN(skipchars, linelen);

		if(uniq_equals(line + linetruncshift,
			linelen - linetruncshift,
			previous + truncshift,
			previouslen - truncshift)) {

			occurrences += 1;
		} else {
			uniq_print_occurrences(previous, occurrences);
			occurrences = 1;
		}

		UNIQ_SWAP(char *, line, previous);
		UNIQ_SWAP(size_t, linecapacity, previouscapacity);
		UNIQ_SWAP(ssize_t, linelen, previouslen);
		UNIQ_SWAP(size_t, linetruncshift, truncshift);
	}

	uniq_print_occurrences(previous, occurrences);
	free(line);
	free(previous);

	return 0;
}

