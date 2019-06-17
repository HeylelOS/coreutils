#define _XOPEN_SOURCE 700
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>

/* Copy-on-write support */
#ifdef __APPLE__
#include <sys/clonefile.h>
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

#include "core_io.h"

static struct {
	unsigned recursive : 1;
	unsigned force : 1;
	unsigned interactive : 1;
	unsigned duplicate : 1;
	unsigned nofollowsources : 1;
	unsigned followtraversal : 1;
} cpargs;

#define CP_IS_RECURSIVE()      (cpargs.recursive == 1)
#define CP_FOLLOWS_SOURCES()   (cpargs.nofollowsources == 0)
#define CP_FOLLOWS_TRAVERSAL() (cpargs.followtraversal != 0)

static void
cp_usage(const char *cpname) {
	fprintf(stderr, "usage: %s [-Pfip] source_file target_file\n"
		"       %s [-Pfip] source_file... target\n"
		"       %s -R [-H|-L|-P] [-fip] source_file... target\n",
		cpname, cpname, cpname);
	exit(1);
}

static void
cp_parse_args(int argc,
	char **argv) {
	int c;

	if(argc == 1) {
		cp_usage(*argv);
	}

	while((c = getopt(argc, argv, "RLHPfip")) != -1) {
		switch(c) {
		case 'R':
			cpargs.recursive = 1;
			break;
		case 'P':
			cpargs.nofollowsources = 1;
			cpargs.followtraversal = 0;
			break;
		case 'f':
			cpargs.force = 1;
			break;
		case 'i':
			cpargs.interactive = 1;
			break;
		case 'p':
			cpargs.duplicate = 1;
			break;
		case 'H':
			if(CP_IS_RECURSIVE()) {
				cpargs.nofollowsources = 0;
				cpargs.followtraversal = 0;
				break;
			}
			/* fallthrough */
		case 'L':
			if(CP_IS_RECURSIVE()) {
				cpargs.nofollowsources = 0;
				cpargs.followtraversal = 1;
				break;
			}
			/* fallthrough */
		default:
			cp_usage(*argv);
		}
	}

	if(optind + 2 > argc) {
		cp_usage(*argv);
	}
}

static int
cp_symlink_cow(const char *sourcefile, const char *destfile) {
#ifdef __APPLE__
	return clonefile(sourcefile, destfile, CLONE_NOFOLLOW);
#else
	return -1;
#endif
}

static int
cp_regfile_cow(const char *sourcefile, const char *destfile,
	const struct stat *statp) {
#ifdef __APPLE__
	return clonefile(sourcefile, destfile, 0);
#elif defined(__linux__)
	int fdsrc = open(sourcefile, O_RDONLY);
	int retval = 0;

	if(fdsrc >= 0) {
		int fddest = open(destfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			statp->st_mode);

		if(fddest >= 0) {

			retval = ioctl(fddest, FICLONE, fdsrc);

			close(fddest);
		} else {
			retval = -1;
		}

		close(fdsrc);
	} else {
		retval = -1;
	}

	return retval;
#else
	return -1;
#endif
}

static int
cp_regfile(const char *sourcefile, const char *destfile,
	const struct stat *statp) {
	int fdsrc = open(sourcefile, O_RDONLY);
	int retval = 0;

	if(fdsrc >= 0) {
		int fddest = open(destfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			statp->st_mode);

		if(fddest >= 0) {
			switch(io_flush_to(fdsrc, fddest,
				statp->st_blksize)) {
			case -1:
				warn("%s to %s read failed", sourcefile, destfile);
				retval = -1;
				break;
			case 1:
				warn("%s to %s write failed", sourcefile, destfile);
				retval = -1;
				break;
			default:
				break;
			}

			close(fddest);
		} else {
			warn("Couldn't open destination file %s", destfile);
			retval = -1;
		}

		close(fdsrc);
	} else {
		warn("Couldn't open source file %s", sourcefile);
		retval = -1;
	}

	return retval;
}

static int
cp_copy(const char *sourcefile, const struct stat *sourcestatp,
	char *destfile, const struct stat *deststatp,
	bool traversal) {
	int retval = 0;

	if(deststatp != NULL
		&& sourcestatp->st_ino == deststatp->st_ino) {
		warnx("%s and %s are identical (not copied)", sourcefile, destfile);
		retval = 1;
	} else {
		switch(sourcestatp->st_mode & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
			if(mknod(destfile, sourcestatp->st_mode, sourcestatp->st_dev) == -1) {
				warn("%s unable to copy to %s as device", sourcefile, destfile);
				retval = 1;
			}
			break;
		case S_IFIFO:
			if(mkfifo(destfile, sourcestatp->st_mode) == -1) {
				warn("%s unable to copy to %s as fifo", sourcefile, destfile);
				retval = 1;
			}
			break;
		case S_IFLNK:
			if(traversal ? !CP_FOLLOWS_TRAVERSAL() : !CP_FOLLOWS_SOURCES()) {
				if(cp_symlink_cow(sourcefile, destfile) == -1
					&& symlink(sourcefile, destfile) == -1) {
					retval = 1;
				}
				break;
			}
			/* fallthrough */
		case S_IFREG:
			if(cp_regfile_cow(sourcefile, destfile, sourcestatp) == -1
				&& cp_regfile(sourcefile, destfile, sourcestatp) == -1) {
				retval = 1;
			}
			break;
		case S_IFDIR:
			if(!CP_IS_RECURSIVE()) {
				warnx("%s copy of directory authorized only with -R specified", sourcefile);
				retval = 1;
			} else {
				if(mkdir(destfile, sourcestatp->st_mode) == -1) {
					warn("Unable to create directory %s", destfile);
					retval = 1;
				}
			}
			break;
		default:
			warnx("%s has unsupported file type", sourcefile);
			retval = 1;
			break;
		}
	}

	return retval;
}

static const char *
cp_basename(const char *path) {
	const char *basename = path;

	if(*basename != '\0') {
		const char *current = basename;

		while(*current != '\0') {
			if(*current == '/'
				&& current[1] != '\0'
				&& current[1] != '/') {
				basename = current + 1;
			}
			current += 1;
		}
	}

	return basename;
}

static int
cp_copy_argument(const char *sourcefile, char *destfile,
	struct stat *deststatp) {
	struct stat sourcestat;

	if(stat(sourcefile, &sourcestat) == 0) {
		return cp_copy(sourcefile, &sourcestat, destfile, deststatp, false);
	} else {
		warn("Unable to stat source file %s", sourcefile);
		return 1;
	}

	return 0;
}

static int
cp(int argc,
	char **argv) {
	char target[PATH_MAX];
	char *targetend = stpncpy(target, argv[argc - 1], sizeof(target));
	char **sourcefiles = argv + optind;
	struct stat targetstat;
	int retval = 0;

	if(*target == '\0') {
		errx(1, "Empty target path invalid");
	} else if(target[sizeof(target) - 2] != '\0') { /* -2 for the slash if we need it */
		errx(1, "Target path too long");
	}

	if(stat(target, &targetstat) == 0) {
		if(S_ISDIR(targetstat.st_mode)) { /* Synopsis 2 & 3 */
			const size_t targetendcapacity = target + sizeof(target) - targetend;
			char ** const sourcefilesend = argv + argc - 1;

			/* Standards specifies append ONE slash if one not already here */
			if(targetend[-1] != '/') {
				*targetend = '/';
				targetend += 1;
			}

			while(sourcefiles != sourcefilesend) {
				const char *sourcefile = *sourcefiles;

				if(stpncpy(targetend, cp_basename(sourcefile), targetendcapacity) < target + sizeof(target)) {
					retval += cp_copy_argument(sourcefile, target, NULL);
				} else {
					warnx("Destination path too long for source file %s", sourcefile);
					retval++;
				}

				sourcefiles++;
			}
		} else if(argc - optind == 2) { /* Synopsis 1 */
			retval = cp_copy_argument(*sourcefiles, target, &targetstat);
		} else {
			warnx("%s is not a directory", target);
			cp_usage(*argv);
		}
	} else if(errno == ENOENT) {
		if(argc - optind == 2) { /* Synopsis 1 */
			retval = cp_copy_argument(*sourcefiles, target, NULL);
		} else {
			warnx("%s doesn't exist and multiple source files were provided", target);
			cp_usage(*argv);
		}
	} else {
		err(1, "Unable to stat target %s", target);
	}

	return 0;
}

int
main(int argc,
	char **argv) {
	int retval = 0;
	cp_parse_args(argc, argv);

	if(argc - optind >= 2) {
		retval = cp(argc, argv);
	} else {
		warnx("Not enough arguments");
		cp_usage(*argv);
	}

	return retval;
}

