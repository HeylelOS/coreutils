#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <err.h>

#include "core_fs.h"

#define isoctal(c) ((c) >= '0' && (c) <= '7')

#define isperm(c) ((c) == 'r'\
				|| (c) == 'w'\
				|| (c) == 'x'\
				|| (c) == 'X'\
				|| (c) == 's'\
				|| (c) == 't')

#define isop(c) ((c) == '+'\
				|| (c) == '-'\
				|| (c) == '=')

#define ispermcopy(c) ((c) == 'u'\
				|| (c) == 'g'\
				|| (c) == 'o')

#define iswho(c) (ispermcopy((c))\
				|| (c) == 'a')

static mode_t
chmod_mask_who(char who) {
	switch(who) {
	case 'u':
		return S_IRWXU | S_ISUID;
	case 'g':
		return S_IRWXG | S_ISGID;
	case 'o':
		return S_IRWXO;
	default: /* a */
		return S_IRWXA | S_ISUID | S_ISGID;
	}
}

static mode_t
chmod_mask_permcopy(char copy, mode_t source) {
	mode_t mask = source;

	switch(copy) {
	case 'u':
		mask &= S_IRWXU;
		return mask | mask >> 3 | mask >> 6;
	case 'g':
		mask &= S_IRWXG;
		return mask | mask << 3 | mask >> 3;
	default: /* o */
		mask &= S_IRWXO;
		return mask | mask << 3 | mask << 6;
	}
}

static mode_t
chmod_mask_perm(char who, mode_t mode, bool isdir) {
	switch(who) {
	case 'r':
		return S_IRALL;
	case 'w':
		return S_IWALL;
	case 'x':
		return S_IXALL;
	case 'X':
		return (isdir || (mode & S_IXALL) != 0) ? S_IXALL : 0;
	case 's':
		return S_ISUID | S_ISGID;
	default: /* t */
		return S_ISVTX;
	}
}

static mode_t
chmod_mask_op(mode_t mode, mode_t whomask, mode_t permmask, char op) {
	mode_t mask = whomask & permmask;

	switch(op) {
	case '+':
		return mode | mask;
	case '-':
		return mode & ~mask;
	default: /* = */
		return (mode & ~(whomask & S_IRWXA)) | permmask;
	}
}

static const char *
chmod_mode_clause_apply(const char *clause,
	mode_t *modep, mode_t cmask,
	bool isdir) {
	mode_t parsed = *modep;

	do {
		mode_t whomask = 0;
		while(iswho(*clause)) {
			whomask |= chmod_mask_who(*clause);
			clause++;
		}

		if(isop(*clause)) {
			do {
				const char op = *clause;
				mode_t permmask = 0;
				clause++;

				if(ispermcopy(*clause)) {
					permmask = chmod_mask_permcopy(*clause, parsed);
					clause++;
				} else if(isperm(*clause)) {
					do {
						permmask |= chmod_mask_perm(*clause, *modep, isdir);
						clause++;
					} while(isperm(*clause));
				} else {
					break;
				}

				if(permmask != 0) {
					parsed = chmod_mask_op(parsed,
						whomask == 0 ? ~cmask : whomask,
						permmask, op);
				}
			} while(isop(*clause));
		} else {
			break;
		}
	} while(*clause == ','
		&& *(clause++) != '\0');

	*modep = parsed;

	return clause;
}

static int
chmod_mode_apply(const char *modeexp,
	mode_t *modep, mode_t cmask, bool isdir) {
	const char *modeexpend = modeexp;

	if(isoctal(*modeexp)) {
		mode_t parsed = 0;

		do {
			parsed = (parsed << 3) + (*modeexp - '0');
			modeexpend++;
		} while(isoctal(*modeexpend));
		*modep = parsed;
	} else {

		while(*(modeexpend = chmod_mode_clause_apply(modeexpend,
			modep, cmask, isdir)) == ',') {
			modeexpend++;
		}
	}

	return *modeexpend;
}

static int
chmod_change(const char *file, struct stat *statp,
	const char *modeexp, mode_t cmask) {
	int retval;

	if((retval = stat(file, statp)) == 0) {
		mode_t mode = statp->st_mode & (S_ISALL | S_IRWXA);
		int c;

		if((c = chmod_mode_apply(modeexp, &mode, cmask, S_ISDIR(statp->st_mode))) == 0) {
			if((retval = chmod(file, mode)) == -1) {
				warn("chmod %s", file);
			}
		} else {
			warnx("Unable to parse mode '%s', stopped at '%c'", modeexp, c);
			retval = -1;
		}
	} else {
		warn("stat %s", file);
	}

	return retval;
}

static void
chmod_usage(const char *chmodname) {

	fprintf(stderr, "usage: %s [-R] mode file...\n", chmodname);
	exit(1);
}

int
main(int argc,
	char **argv) {
	bool recursive = false;
	int retval = 0;
	int c;

	while((c = getopt(argc, argv, "R")) != -1) {
		if(c == 'R') {
			recursive = true;
		} else {
			warnx("Unknown option -%c", c);
			chmod_usage(*argv);
		}
	}

	if(argc - optind > 1) {
		char **argpos = argv + optind + 1, ** const argend = argv + argc;
		const char *modeexp = argv[optind];
		mode_t cmask = umask(0);

		while(argpos != argend) {
			const char *file = *argpos;
			struct stat st;

			if(chmod_change(file, &st, modeexp, cmask) == 0
				&& S_ISDIR(st.st_mode) && recursive) {
				struct fs_recursion recursion;

				if(fs_recursion_init(&recursion, file, 256, true) == 0) {
					do {
						while(fs_recursion_next(&recursion) == 0 && *recursion.name != '\0') {
							if(chmod_change(recursion.path, &st, modeexp, cmask) == 0) {
								if(S_ISDIR(st.st_mode)) {
									fs_recursion_push(&recursion);
								}
							} else {
								retval = 1;
							}
						}
					} while(fs_recursion_pop(&recursion) == 0);

					fs_recursion_deinit(&recursion);
				} else {
					retval = 1;
				}
			}

			argpos++;
		}
	} else {
		chmod_usage(*argv);
	}

	return retval;
}

