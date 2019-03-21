#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>

#define UNIQ_MIN(a, b) ((a) < (b) ? (a) : (b))

#define UNIQ_SWAP(type, a, b) \
do {\
	type c = (a);\
	(a) = (b);\
	(b) = c;\
} while(false)

static char *uniqname;
static FILE *in, *out;
/**
 * The following belongs to a class of function which prints
 * the output depending on its occurrences
 */
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
 * We only miss adding the '+' delimiter, must make a custom getopt
 */
static int
uniq_parse_args(int argc,
	char **argv) {
	int retval = 0;
	int c;

	while(retval == 0
		&& (c = getopt(argc, argv, ":cdf:s:u")) != -1) {
		switch(c) {
		case 'c':
			uniq_print_occurrences = uniq_print_count; break;
		case 'd':
			uniq_print_occurrences = uniq_print_repeated; break;
		case 'f': {
			char *endptr;
			skipfields = strtoul(optarg, &endptr, 10);
			if(*endptr != '\0') {
				fprintf(stderr, "error: %s %s: Must be a decimal numeric\n", uniqname, optarg);
				exit(1);
			}
		} break;
		case 's': {
			char *endptr;
			skipchars = strtoul(optarg, &endptr, 10);
			if(*endptr != '\0') {
				fprintf(stderr, "error: %s %s: Must be a decimal numeric\n", uniqname, optarg);
				exit(1);
			}
		} break;
		case 'u':
			uniq_print_occurrences = uniq_print_not_repeated; break;
		case ':':
			fprintf(stderr, "error: %s -%c: Missing argument\n", uniqname, optopt);
			/* fallthrough */
		default:
			optind = argc;
			break;
		}
	}

	if(optind + 2 == argc) {
		if(strcmp(argv[optind], "-") != 0) {
			if((in = fopen(argv[optind], "r")) == NULL) {
				fprintf(stderr, "error: %s %s: input %s\n",
					uniqname, argv[optind], strerror(errno));
				exit(1);
			}
		}

		optind += 1;
		if(strcmp(argv[optind], "-") != 0) {
			if((out = fopen(argv[optind], "w")) == NULL) {
				fprintf(stderr, "error: %s %s: output %s\n",
					uniqname, argv[optind], strerror(errno));
				exit(1);
			}
		}
	} else {
		retval = -1;
	}

	return retval;
}

/**
 * Initializes global values and checks for
 * usage printing
 */
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

/**
 * Indicates the position of the truncation
 * in string
 * @param string The string to truncate
 * @param length Length of string
 * @return Truncation index of string, <= length
 */
static size_t
uniq_truncation(const char *string, size_t length) {
	const char * const begin = string;
	const char * const end = string + length;
	size_t skipped = skipfields;

	while(string != end
		&& skipped != 0) {
		while(string != end
			&& isblank(*string)) {
			string += 1;
		}
 		while(string != end
			&& !isblank(*string)) {
			string += 1;
		}
 		skipped -= 1;
	}

	return string - begin + UNIQ_MIN(skipchars, end - string);
}

/**
 * Returns whether str1 and str2 are equals,
 * accepts NULL strings through their length
 * @param str1 First string
 * @param len1 Length of str1
 * @param str2 Second string
 * @param len2 Length of str2
 * @return Whether str1 equals str2 or not
 */
static bool
uniq_equals(const char *str1, size_t len1,
	const char *str2, size_t len2) {

	return (len1 == 0 && len2 == 0)
		|| (len1 == len2
			&& memcmp(str1, str2, len1) == 0);
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

		size_t linetruncshift = uniq_truncation(line, linelen);

		if(uniq_equals(line + linetruncshift,
			linelen - linetruncshift,
			previous + truncshift,
			previouslen - truncshift)) {

			occurrences += 1;
		} else {
			uniq_print_occurrences(previous, occurrences);
			occurrences = 1;

			truncshift = linetruncshift;
			UNIQ_SWAP(char *, line, previous);
			UNIQ_SWAP(size_t, linecapacity, previouscapacity);
			UNIQ_SWAP(ssize_t, linelen, previouslen);
		}
	}

	uniq_print_occurrences(previous, occurrences);

#ifdef FULL_CLEANUP
	free(line);
	free(previous);
#endif

	return 0;
}

