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

/* Copy-on-write support */
#ifdef __APPLE__
#include <sys/clonefile.h>
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

#include "core_io.h"

static const char *cpname;
static struct {
	unsigned recursive : 1;
	unsigned force : 1;
	unsigned interactive : 1;
	unsigned duplicate : 1;
	unsigned nofollowsources : 1;
	unsigned followtraversal : 1;
} cpargs;
static int cpfdlimit;
static char cpdestfile[PATH_MAX];
static char *cptargetend;

#define CP_IS_RECURSIVE()      (cpargs.recursive == 1)
#define CP_FOLLOWS_SOURCES()   (cpargs.nofollowsources == 0)
#define CP_FOLLOWS_TRAVERSAL() (cpargs.followtraversal != 0)

static void
cp_usage(void) {
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
		cp_usage();
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
			cp_usage();
		}
	}

	if(optind + 2 > argc) {
		cp_usage();
	}
}

static int
cp_dest_from(const char *source) {
	int retval = 0;

	if(*source != '\0') {
		strncpy(cptargetend, source,
			cpdestfile + sizeof(cpdestfile) - cptargetend);

		if(cpdestfile[sizeof(cpdestfile) - 1] != '\0') {
			fprintf(stderr, "error: %s: dest_file path too long\n",
				cpname);
			retval = -1;
		}
	} else {
		fprintf(stderr, "error: %s: empty source path invalid\n",
			cpname);
		retval = -1;
	}

	return retval;
}

static int
cp_copy(const char *sourcefile,
	const struct stat *sourcefilestatp,
	const struct stat *destfilestatp);

static int
cp_copy_ftw_fn(const char *sourcefile,
	const struct stat *sourcefilestatp,
	int flags, struct FTW *ftw) {

	if(cp_dest_from(sourcefile) == 0) {
		struct stat deststat;

		cp_copy(sourcefile, sourcefilestatp,
			stat(cpdestfile, &deststat) == 0 ?
				&deststat : NULL);
	}

	return 0;
}

static int
cp_symlink_cow(const char *sourcefile) {
#ifdef __APPLE__
	return clonefile(sourcefile, cpdestfile, CLONE_NOFOLLOW);
#else
	return -1;
#endif
}

static int
cp_regfile_cow(const char *sourcefile,
	const struct stat *sourcefilestatp) {
#ifdef __APPLE__
	return clonefile(sourcefile, cpdestfile, 0);
#elif defined(__linux__)
	int fdsrc = open(sourcefile, O_RDONLY);
	int retval = 0;

	if(fdsrc >= 0) {
		int fddest = open(cpdestfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			sourcefilestatp->st_mode);

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
cp_regfile(const char *sourcefile,
	const struct stat *sourcefilestatp) {
	int fdsrc = open(sourcefile, O_RDONLY);
	int retval = 0;

	if(fdsrc >= 0) {
		int fddest = open(cpdestfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			sourcefilestatp->st_mode);

		if(fddest >= 0) {
			switch(io_flush_to(fdsrc, fddest,
				sourcefilestatp->st_blksize)) {
			case -1:
				fprintf(stderr, "error: %s: %s to %s read failed: %s\n",
					cpname, sourcefile, cpdestfile, strerror(errno));
				retval = -1;
				break;
			case 1:
				fprintf(stderr, "error: %s: %s to %s write failed: %s\n",
					cpname, sourcefile, cpdestfile, strerror(errno));
				retval = -1;
				break;
			default:
				break;
			}

			close(fddest);
		} else {
			fprintf(stderr, "error: %s: couldn't open destination file %s: %s\n",
				cpname, cpdestfile, strerror(errno));
			retval = -1;
		}

		close(fdsrc);
	} else {
		fprintf(stderr, "error: %s: couldn't open source file %s: %s\n",
			cpname, sourcefile, strerror(errno));
		retval = -1;
	}

	return retval;
}

static int
cp_copy(const char *sourcefile,
	const struct stat *sourcefilestatp,
	const struct stat *destfilestatp) {
	static bool inrecursion;
	int retval = 0;

	do {
		if(destfilestatp != NULL
			&& sourcefilestatp->st_ino == destfilestatp->st_ino) {
			fprintf(stderr, "error: %s: %s and %s are identical (not copied)\n",
				cpname, sourcefile, cpdestfile);
			retval = 1;
			break;
		}

		switch(sourcefilestatp->st_mode & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
			if(mknod(cpdestfile, sourcefilestatp->st_mode, sourcefilestatp->st_dev) == -1) {
				fprintf(stderr, "error: %s: %s unable to copy to %s device: %s\n",
					cpname, sourcefile, cpdestfile, strerror(errno));
				retval = 1;
			}
			break;
		case S_IFIFO:
			if(mkfifo(cpdestfile, sourcefilestatp->st_mode) == -1) {
				fprintf(stderr, "error: %s: %s unable to copy to %s fifo: %s\n",
					cpname, sourcefile, cpdestfile, strerror(errno));
				retval = 1;
			}
			break;
		case S_IFLNK:
			if(!CP_FOLLOWS_SOURCES()
				|| (inrecursion && !CP_FOLLOWS_TRAVERSAL())) {
				if(cp_symlink_cow(sourcefile) == -1
					&& symlink(sourcefile, cpdestfile) == -1) {
					retval = 1;
				}
				break;
			}
			/* fallthrough */
		case S_IFREG:
			if(cp_regfile_cow(sourcefile, sourcefilestatp) == -1
				&& cp_regfile(sourcefile, sourcefilestatp) == -1) {
				retval = 1;
			}
			break;
		case S_IFDIR:
			if(!CP_IS_RECURSIVE()) {
				fprintf(stderr, "error: %s: %s copy of directory authorized only with -R specified\n",
					cpname, sourcefile);
				retval = 1;
			} else if(inrecursion) {
				if(mkdir(cpdestfile, sourcefilestatp->st_mode) == -1) {
					fprintf(stderr, "error: %s: unable to create directory %s: %s\n",
						cpname, cpdestfile, strerror(errno));
					retval = 1;
				}
			} else {
				inrecursion = true;
				if(nftw(sourcefile, cp_copy_ftw_fn,
					cpfdlimit,
					CP_FOLLOWS_TRAVERSAL() ? 0 : FTW_PHYS) == -1) {
					fprintf(stderr, "error: %s: %s copy of directory authorized only with -R specified\n",
						cpname, sourcefile);
					retval = 1;
				}
				inrecursion = false;
			}
			break;
		default:
			fprintf(stderr, "error: %s: %s has unsupported file type\n",
				cpname, sourcefile);
			retval = 1;
			break;
		}
	} while(false);

	return retval;
}

static void
cp_set_fdlimit(void) {

	if((cpfdlimit = sysconf(_SC_OPEN_MAX)) == -1) {
		cpfdlimit = 1024;
	}
}

static const char *
cp_source_base(const char *sourcefile) {
	const char *sourcebase = sourcefile;

	if(*sourcefile != '\0') {
		const char *current = sourcefile;

		while(*current != '\0') {
			if(*current == '/'
				&& current[1] != '\0'
				&& current[1] != '/') {
				sourcebase = current + 1;
			}
			current += 1;
		}
	}

	return sourcebase;
}

static int
cp_copy_argument(const char *sourcefile,
	const struct stat *destfilestatp) {
	struct stat sourcefilestat;
	int retval = 0;

	if(stat(sourcefile, &sourcefilestat) == 0) {
		retval = cp_copy(sourcefile, &sourcefilestat, destfilestatp);
	} else {
		fprintf(stderr, "error: %s %s: %s\n",
			cpname, sourcefile, strerror(errno));
		retval = 1;
	}

	return retval;
}

int
main(int argc,
	char **argv) {
	cpname = *argv;
	cp_set_fdlimit();
	cp_parse_args(argc, argv);

	struct stat targetstat;
	char **sourcefiles = argv + optind;
	char *target = argv[argc - 1];
	int retval = 0;

	cptargetend = stpncpy(cpdestfile, target, sizeof(cpdestfile));

	if(cptargetend == cpdestfile) {
		fprintf(stderr, "error: %s %s: empty target path invalid\n",
			cpname, target);
		retval = 1;
	} else if(cptargetend - cpdestfile > sizeof(cpdestfile) - 1) {
		fprintf(stderr, "error: %s %s: target path too long\n",
			cpname, target);
		retval = 1;
	} else if(stat(target, &targetstat) == 0) {
		if(S_ISDIR(targetstat.st_mode)) {
			/* Standards specifies append ONE slash if one not already here */
			if(cptargetend - cpdestfile <= sizeof(cpdestfile) - 2
				&& cptargetend[-1] != '/') {
				*cptargetend = '/';
				cptargetend += 1;
			}

			while(*sourcefiles != target) {
				if(cp_dest_from(cp_source_base(*sourcefiles)) == 0) {
					struct stat deststat;

					retval += cp_copy_argument(*sourcefiles,
						stat(cpdestfile, &deststat) == 0 ?
							&deststat : NULL);
				} else {
					retval += 1;
				}

				sourcefiles += 1;
			}
		} else if(argc - optind == 2) {
			retval = cp_copy_argument(*sourcefiles, &targetstat);
		} else {
			cp_usage();
		}
	} else if(errno == ENOENT) {
		if(argc - optind == 2
			&& cptargetend[-1] != '/') {
			retval = cp_copy_argument(*sourcefiles, NULL);
		} else {
			cp_usage();
		}
	} else {
		fprintf(stderr, "error: %s: Couldn't resolve target: %s\n",
			cpname, strerror(errno));
		retval = 1;
	}

	return retval;
}

