#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>

#include "getoptplus.h"

#define UNIQ_MIN(a, b) ((a) < (b) ? (a) : (b))

#define UNIQ_SWAP(type, a, b) \
do {\
	type c = (a);\
	(a) = (b);\
	(b) = c;\
} while(false)

struct uniq_args {
	FILE *in;
	FILE *out;
	/**
	 * The following belongs to a class of function which prints
	 * the output depending on its occurrences
	 */
	void (*print_occurrences)(FILE *, char *, size_t);
	size_t skipfields;
	size_t skipchars;
};

static void
uniq_print_default(FILE *out, char *string, size_t occurrences) {

	if(occurrences != 0) {
		if(string != NULL) {
			fputs(string, out);
		}
		fputc('\n', out);
	}
}

static void
uniq_print_count(FILE *out, char *string, size_t occurrences) {

	if(occurrences != 0) {
		fprintf(out, "%ld %s\n", occurrences, string == NULL ? "" : string);
	}
}

static void
uniq_print_repeated(FILE *out, char *string, size_t occurrences) {

	if(occurrences > 1) {
		if(string != NULL) {
			fputs(string, out);
		}
		fputc('\n', out);
	}
}

static void
uniq_print_not_repeated(FILE *out, char *string, size_t occurrences) {

	if(occurrences == 1) {
		if(string != NULL) {
			fputs(string, out);
		}
		fputc('\n', out);
	}
}

static void
uniq_usage(const char *uniqname) {
	fprintf(stderr, "usage: %s [-d|-c|-u] [-f fields] [-s char] [input_file [output_file]]\n", uniqname);
	exit(1);
}

static struct uniq_args
uniq_parse_args(int argc, char **argv) {
	struct uniq_args args = {
		.in = stdin,
		.out = stdout,
		.print_occurrences = uniq_print_default,
		.skipfields = 0,
		.skipchars = 0
	};
	char *endptr;
	int c;

	while((c = getoptplus(argc, argv, ":cdf:s:u")) != -1) {
		switch(c) {
		case 'c':
			args.print_occurrences = uniq_print_count;
			break;
		case 'd':
			args.print_occurrences = uniq_print_repeated;
			break;
		case 'f':
			args.skipfields = strtoul(optarg, &endptr, 10);
			if(*endptr != '\0') {
				warnx("%s: Ignored fields must be a decimal numeric", optarg);
				uniq_usage(*argv);
			}
			break;
		case 's':
			args.skipchars = strtoul(optarg, &endptr, 10);
			if(*endptr != '\0') {
				warnx("%s: Ignored chars must be a decimal numeric", optarg);
				uniq_usage(*argv);
			}
			break;
		case 'u':
			args.print_occurrences = uniq_print_not_repeated;
			break;
		case ':':
			warnx("-%c: Missing argument", optopt);
			uniq_usage(*argv);
		default:
			warnx("Unknown argument -%c", c);
			uniq_usage(*argv);
		}
	}

	switch(argc - optind) {
	case 2:
		if(strcmp(argv[optind + 1], "-") != 0
			&& (args.out = fopen(argv[optind + 1], "w")) == NULL) {
			err(1, "output %s\n", argv[optind + 1]);
		}
		/* fallthrough */
	case 1:
		if(strcmp(argv[optind], "-") != 0
			&& (args.in = fopen(argv[optind], "r")) == NULL) {
			err(1, "input %s\n", argv[optind]);
		}
	case 0:
		break;
	default:
		uniq_usage(*argv);
	}

	return args;
}

/**
 * Indicates the position of the truncation
 * in string
 * @param string The string to truncate
 * @param length Length of string
 * @return Truncation index of string, <= length
 */
static inline size_t
uniq_truncation(const char *string, size_t length,
	size_t skipfields, size_t skipchars) {
	const char * const begin = string;
	const char * const end = string + length;

	while(string != end
		&& skipfields != 0) {
		while(string != end
			&& isblank(*string)) {
			string += 1;
		}
 		while(string != end
			&& !isblank(*string)) {
			string += 1;
		}
 		skipfields -= 1;
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
static inline bool
uniq_equals(const char *str1, size_t len1,
	const char *str2, size_t len2) {

	return (len1 == 0 && len2 == 0)
		|| (len1 == len2
			&& memcmp(str1, str2, len1) == 0);
}

int
main(int argc,
	char **argv) {
	const struct uniq_args args = uniq_parse_args(argc, argv);
	char *line = NULL, *previous = NULL;
	size_t linecapacity = 0, previouscapacity = 0;
	ssize_t linelen, previouslen;
	size_t occurrences = 0;
	size_t truncshift = 0;

	while((linelen = getline(&line, &linecapacity, args.in)) > 0) {
		if(linelen != 0 && line[linelen - 1] == '\n') {
			line[--linelen] = '\0';
		}

		size_t linetruncshift = uniq_truncation(line, linelen,
			args.skipfields, args.skipchars);

		if(uniq_equals(line + linetruncshift,
			linelen - linetruncshift,
			previous + truncshift,
			previouslen - truncshift)) {

			occurrences += 1;
		} else {
			args.print_occurrences(args.out, previous, occurrences);
			occurrences = 1;

			truncshift = linetruncshift;
			UNIQ_SWAP(char *, line, previous);
			UNIQ_SWAP(size_t, linecapacity, previouscapacity);
			UNIQ_SWAP(ssize_t, linelen, previouslen);
		}
	}

	args.print_occurrences(args.out, previous, occurrences);

#ifdef FULL_CLEANUP
	free(line);
	free(previous);
#endif

	return 0;
}

