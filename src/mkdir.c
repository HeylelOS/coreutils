#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <err.h>

#define S_IRALL (S_IRUSR | S_IRGRP | S_IROTH)
#define S_IWALL (S_IWUSR | S_IWGRP | S_IWOTH)
#define S_IXALL (S_IXUSR | S_IXGRP | S_IXOTH)
#define S_IRWXA (S_IRWXU | S_IRWXG | S_IRWXO)

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

struct mkdir_args {
	mode_t mode;
};

static mode_t
mkdir_mask_who(char who) {
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
mkdir_mask_permcopy(char copy, mode_t source) {
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
mkdir_mask_perm(char who, mode_t mode) {
	switch(who) {
	case 'r':
		return S_IRALL;
	case 'w':
		return S_IWALL;
	case 'x':
	case 'X':
		return S_IXALL;
	case 's':
		return S_ISUID | S_ISGID;
	default: /* t */
		return S_ISVTX;
	}
}

static mode_t
mkdir_mask_op(mode_t mode, mode_t whomask, mode_t permmask, char op) {
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
mkdir_mode_clause_apply(const char *clause,
	mode_t *modep, mode_t cmask) {
	mode_t parsed = *modep;

	do {
		mode_t whomask = 0;
		while(iswho(*clause)) {
			whomask |= mkdir_mask_who(*clause);
			clause++;
		}

		if(isop(*clause)) {
			do {
				const char op = *clause;
				mode_t permmask = 0;
				clause++;

				if(ispermcopy(*clause)) {
					permmask = mkdir_mask_permcopy(*clause, parsed);
					clause++;
				} else if(isperm(*clause)) {
					do {
						permmask |= mkdir_mask_perm(*clause, *modep);
						clause++;
					} while(isperm(*clause));
				} else {
					break;
				}

				if(permmask != 0) {
					parsed = mkdir_mask_op(parsed,
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
mkdir_mode_apply(const char *modeexp,
	mode_t *modep, mode_t cmask) {
	const char *modeexpend = modeexp;

	if(isoctal(*modeexp)) {
		mode_t parsed = 0;

		do {
			parsed = (parsed << 3) + (*modeexp - '0');
			modeexpend++;
		} while(isoctal(*modeexpend));
		*modep = parsed;
	} else {
		while(*(modeexpend = mkdir_mode_clause_apply(modeexpend,
			modep, cmask)) == ',') {
			modeexpend++;
		}
	}

	return *modeexpend;
}

static void
mkdir_usage(const char *mkdirname) {
	fprintf(stderr, "usage: %s [-p] [-m mode] file...\n", mkdirname);
	exit(1);
}

static struct mkdir_args
mkdir_parse_args(int argc, char **argv) {
	const mode_t cmask = umask(0);
	struct mkdir_args args = {
		.mode = S_IRWXA & ~cmask
	};
	int c;

	while((c = getopt(argc, argv, ":pm:")) != -1) {
		switch(c) {
		case 'p':
			/* TODO */
			break;
		case 'm':
			if((c = mkdir_mode_apply(optarg, &args.mode, cmask)) != 0) {
				warnx("Unable to parse mode '%s', stopped at '%c'", optarg, c);
			}
			break;
		case ':':
			warnx("-%c: Missing argument", optopt);
			mkdir_usage(*argv);
		default:
			warnx("Unknown argument -%c", c);
			mkdir_usage(*argv);
		}
	}

	if(argc == optind) {
		warnx("Not enough arguments");
		mkdir_usage(*argv);
	}

	return args;
}

int
main(int argc,
	char **argv) {
	const struct mkdir_args args = mkdir_parse_args(argc, argv);
	char **argpos = argv + optind, ** const argend = argv + argc;
	int retval = 0;

	while(argpos != argend) {
		char *file = *argpos;

		if(mkdir(file, args.mode) == -1) {
			warn("mkdir %s", file);
			retval++;
		}

		argpos++;
	}

	return retval;
}

