#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

char *uniqname;
FILE *in, *out;
static void (*uniq_print_occurrences)(char *, size_t);

static void
uniq_print_default(char *line,
	size_t occurrences) {

	if(occurrences != 0) {
		if(line == NULL) {
			putchar('\n');
		} else {
			puts(line);
		}
	}
}

static void
uniq_print_count(char *line,
	size_t occurrences) {

	if(occurrences != 0) {
		printf("%ld %s\n", occurrences, line == NULL ? "" : line);
	}
}

static void
uniq_print_repeated(char *line,
	size_t occurrences) {

	if(occurrences > 1) {
		if(line == NULL) {
			putchar('\n');
		} else {
			puts(line);
		}
	}
}

static void
uniq_print_not_repeated(char *line,
	size_t occurrences) {

	if(occurrences == 1) {
		if(line == NULL) {
			putchar('\n');
		} else {
			puts(line);
		}
	}
}

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

	if(argpos != argend) {
		if((in = fopen(*argpos, "r")) == NULL) {
			fprintf(stderr, "error: %s %s: %s\n", uniqname, *argpos, strerror(errno));
			exit(1);
		}
		argpos += 1;

		if(argpos != argend) {
			if((out = fopen(*argpos, "w")) == NULL) {
				fprintf(stderr, "error: %s %s: %s\n", uniqname, *argpos, strerror(errno));
				exit(1);
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

	if(uniq_parse_args(argc, argv) != 0) {
		fprintf(stderr, "usage: %s [-d|-c|-u] [-f fields] "
				"[-s char] [input_file [output_file]]\n", uniqname);
		exit(1);
	}
}

static char *
uniq_strdup(const char *str, size_t n) {
	char *dup = malloc(n);
	return strcpy(dup, str);
}

static bool
uniq_strequals(const char *str1, ssize_t len1,
	const char *str2, ssize_t len2) {

	if(str1 == NULL) {
		return str2 == NULL
			|| len2 == 0;
	} else if(str2 == NULL) {
		return len1 == 0;
	} else {
		return len1 == len2
			&& strcmp(str1, str2) == 0;
	}
}

int
main(int argc,
	char **argv) {
	char *line = NULL;
	size_t capacity = 0;
	ssize_t length;

	char *current = NULL;
	ssize_t clength = 0;
	size_t occurrences = 0;
	uniq_init(argc, argv);

	while((length = getline(&line, &capacity, in)) > 0) {
		/* Cut EOL */
		length -= 1;
		line[length] = '\0';

		if(uniq_strequals(line, length, current, clength)) {
			occurrences += 1;
		} else {
			uniq_print_occurrences(current, occurrences);
			free(current);
			current = uniq_strdup(line, length);
			clength = length;
			occurrences = 1;
		}
	}

	uniq_print_occurrences(current, occurrences);
	/* free(current); */

	return 0;
}

