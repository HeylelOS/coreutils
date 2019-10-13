#ifndef GETOPTPLUS_H
#define GETOPTPLUS_H

#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <err.h>

static int
getoptplus(int argc, char * const argv[], const char *optstring) {
	static int optcur = 1;
	int retval = -1;
	const char *opt;

	if(optind >= argc
		|| argv[optind] == NULL
		|| (*argv[optind] != '-' && *argv[optind] != '+')
		|| strcmp(argv[optind], "-") == 0) {
		goto getoptplus_end1;
	}

	if(strcmp(argv[optind], "--") == 0) {
		goto getoptplus_end2;
	}

	optarg = NULL;
	optopt = argv[optind][optcur];
	opt = strchr(optstring, optopt);

	if(opt != NULL && isalnum(*opt)) {
		retval = optopt;

		if(opt[1] == ':') {
			if(argv[optind][optcur + 1] == '\0') {
				optind += 2;
				optcur = 1;

				if(optind <= argc) {
					optarg = argv[optind - 1];
				} else {
					if(*optstring == ':') {
						retval = ':';
					} else {
						retval = '?';
						if(opterr != 0) {
							warnx("option requires an argument -- %c", optopt);
						}
					}
				}
				goto getoptplus_end0;
			} else {
				optarg = argv[optind] + optcur + 1;
				goto getoptplus_end2;
			}
		}
	} else {
		retval = '?';

		if(opterr != 0 && *optstring != ':') {
			warnx("illegal option -- %c", optopt);
		}
	}

	if(argv[optind][++optcur] != '\0') {
		goto getoptplus_end0;
	}

getoptplus_end2:
	optind++;
getoptplus_end1:
	optcur = 1;
getoptplus_end0:
	return retval;
}

/* GETOPTPLUS_H */
#endif
