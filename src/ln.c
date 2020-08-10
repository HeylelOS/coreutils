#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <libgen.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>

struct ln_args {
	unsigned force : 1;
	unsigned symbolic : 1;
	int flags;
};

static int
ln_link_argument(const char *sourcefile,
	const char *targetfile, bool targetexists,
	const struct ln_args args) {
	int retval;

	if(targetexists) {
		if(args.force == 0) {
			warnx("%s exists and -f not specified", targetfile);
			return -1;
		}

		if(strcmp(targetfile, sourcefile) == 0) {
			/* Not totally accurate to check if the destination path
			names the same directory entry as the current source_file */
			warnx("%s %s name the same file", sourcefile, targetfile);
			return -1;
		}

		if(unlink(targetfile) == -1) {
			warn("Unable to unlink %s", targetfile);
			return -1;
		}
	}

	if(args.symbolic == 1
		&& (retval = symlink(targetfile, sourcefile)) == 1) {
		warn("Unable to create symlink %s to %s", targetfile, sourcefile);
	} else if((retval = linkat(AT_FDCWD, sourcefile, AT_FDCWD, targetfile, args.flags)) == 1) {
		warn("Unable to link %s to %s", targetfile, sourcefile);
	}

	return retval;
}

static void
ln_usage(const char *lnname) {
	fprintf(stderr, "usage: %s [-fs] [-L|-P] source_file target_file\n"
		"       %s [-fs] [-L|-P] source_file... target_dir\n",
		lnname, lnname);
	exit(1);
}

struct ln_args
ln_parse_args(int argc, char **argv) {
	struct ln_args args = { 0, 0, 0 };
	int c;

	while((c = getopt(argc, argv, "fsLP")) != -1) {
		switch(c) {
		case 'f':
			args.force = 1;
			break;
		case 's':
			args.symbolic = 1;
			break;
		case 'L':
			args.flags = AT_SYMLINK_FOLLOW;
			break;
		case 'P':
			args.flags = 0;
			break;
		default:
			warnx("Unknown argument -%c", optopt);
			ln_usage(*argv);
		}
	}

	if(argc - optind < 2) {
		warnx("Not enough arguments");
		ln_usage(*argv);
	}

	return args;
}

int
main(int argc,
	char **argv) {
	const struct ln_args args = ln_parse_args(argc, argv);
	char **argpos = argv + optind, ** const argend = argv + argc - 1;
	char target[PATH_MAX], * const targetend = target + sizeof(target);
	char *targetname = stpncpy(target, *argend, sizeof(target));
	struct stat targetstat;
	int retval = 0;

	if(*target == '\0') {
		errx(1, "Empty target path invalid");
	} else if(targetname >= targetend - 1) { /* -1 for the slash if we need it */
		errx(1, "Target path too long");
	}

	if(stat(target, &targetstat) == 0) {
		if(S_ISDIR(targetstat.st_mode)) { /* Synopsis 2 */
			if(targetname[-1] != '/') {
				*targetname = '/';
				++targetname;
			}

			while(argpos != argend) {
				char *sourcefile = *argpos;

				if(stpncpy(targetname, basename(sourcefile), targetend - targetname) < targetend) {
					if(ln_link_argument(*argpos, target, access(target, F_OK) == 0, args) != 0) {
						retval = 1;
					}
				} else {
					warnx("Destination path too long for source file %s", sourcefile);
					retval = 1;
				}

				argpos++;
			}
		} else if(argc - optind == 2) { /* Synopsis 1 */
			if(ln_link_argument(*argpos, target, access(target, F_OK) == 0, args)) {
				retval = 1;
			}
		} else {
			warnx("%s is not a directory", target);
			ln_usage(*argv);
		}
	} else if(errno == ENOENT || errno == ENOTDIR) {
		if(argc - optind == 2) { /* Synopsis 1 */
			if(ln_link_argument(*argpos, target, false, args) != 0) {
				retval = 1;
			}
		} else {
			warnx("%s doesn't exist and multiple source files were provided", target);
			ln_usage(*argv);
		}
	} else {
		err(1, "Unable to stat target %s", target);
	}

	return retval;
}

