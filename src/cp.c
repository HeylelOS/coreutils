#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
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

static mode_t cpumask;

#define CP_IS_RECURSIVE()      (cpargs.recursive == 1)
#define CP_FORCES()            (cpargs.force == 1)
#define CP_INTERACTS()         (cpargs.interactive == 1)
#define CP_DUPLICATES()        (cpargs.duplicate == 1)
#define CP_FOLLOWS_SOURCES()   (cpargs.nofollowsources == 0)
#define CP_FOLLOWS_TRAVERSAL() (cpargs.followtraversal != 0)

/* Standards specifies append ONE slash if one not already here on paths */
#define CP_ENSURE_SLASH(ptr) if((ptr)[-1] != '/') { *(ptr) = '/'; (ptr)++; }
#define CP_UMASK(mode) ((mode) & ~cpumask)

static int
cp_symlink_cow(const char *sourcefile, const char *destfile) {
#ifdef __APPLE__
	return clonefile(sourcefile, destfile, CLONE_NOFOLLOW);
#elif defined(__linux__)
	/* No known linux API to COW symlinks */
	return -1;
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
	int fddest;
	int retval = 0;

	if(access(destfile, F_OK) == 0) {
		if(CP_INTERACTS()) {
			char *line = NULL;
			size_t capacity = 0;
			char c;

			fprintf(stderr, "Do you want to overwrite '%s' with '%s'? ", destfile, sourcefile);
			if(getline(&line, &capacity, stdin) == -1) {
				warn("Unable to read interactive prompt");
				retval = 1;
			} else {
				c = *line;
			}

			free(line);
			if(retval == 1
				|| (c != 'y' && c != 'Y')) {
				return retval;
			}
		}

		fddest = open(destfile, O_WRONLY | O_TRUNC);

		if(fddest == -1 && CP_FORCES()) {
			if(unlink(destfile) == 0) {
				fddest = open(destfile,
					O_WRONLY | O_TRUNC | O_CREAT,
					statp->st_mode);
			} else {
				warn("Unable to unlink %s", destfile);
			}
		}
	} else {
		fddest = open(destfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			statp->st_mode);
	}

	if(fddest >= 0) {
		int fdsrc = open(sourcefile, O_RDONLY);

		if(fdsrc >= 0) {
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

			close(fdsrc);
		} else {
			warn("Couldn't open source file %s", sourcefile);
			retval = -1;
		}

		close(fddest);
	} else {
		warn("Couldn't open destination file %s", destfile);
		retval = -1;
	}

	return retval;
}

static int
cp_copy(const char *sourcefile, const struct stat *sourcestatp,
	const char *destfile, const struct stat *deststatp);

static int
cp_directory(const char *sourcefile, const char *destfile) {
	size_t sourcelen = strlen(sourcefile), destlen = strlen(destfile);
	char *source = malloc(PATH_MAX), *dest = malloc(PATH_MAX);
	int retval = 0;

	if(source != NULL && dest != NULL) {
		char *sourceend = stpncpy(source, sourcefile, sourcelen),
			*destend = stpncpy(dest, destfile, destlen);
		DIR *dirp;

		CP_ENSURE_SLASH(sourceend);
		CP_ENSURE_SLASH(destend);

		if((dirp = opendir(sourcefile)) != NULL) {
			struct dirent *entry;

			while((entry = readdir(dirp)) != NULL) {
				if(strcmp(".", entry->d_name) != 0
					&& strcmp("..", entry->d_name) != 0) {
					struct stat sourcestat;
					const size_t length = strlen(entry->d_name);

					if(length >= source + PATH_MAX - sourceend
						|| length >= dest + PATH_MAX - destend) {
						warnx("Filename too long in %s", sourcefile);
						retval++;
						continue;
					}

					strncpy(sourceend, entry->d_name, length + 1);
					strncpy(destend, entry->d_name, length + 1);

					if(CP_FOLLOWS_TRAVERSAL() ? stat(source, &sourcestat) == 0
						: lstat(source, &sourcestat) == 0) {
						struct stat deststat;

						retval += cp_copy(source, &sourcestat,
							dest, stat(dest, &deststat) == 0 ? &deststat : NULL);
					} else {
						warn("Unable to stat tree file %s", source);
						retval++;
					}
				}
			}

			closedir(dirp);
		} else {
			warn("Unable to copy directory %s", sourcefile);
			retval = 1;
		}
	}

	free(source);
	free(dest);

	return retval;
}

static int
cp_copy(const char *sourcefile, const struct stat *sourcestatp,
	const char *destfile, const struct stat *deststatp) {
	int error = 0, treeerrors = 0;

	if(deststatp != NULL
		&& sourcestatp->st_ino == deststatp->st_ino) {
		warnx("%s and %s are identical (not copied)", sourcefile, destfile);
		error = 1;
	} else {
		switch(sourcestatp->st_mode & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
			if(mknod(destfile, CP_UMASK(sourcestatp->st_mode & 0777), sourcestatp->st_dev) == -1) {
				warn("%s unable to copy to %s as device", sourcefile, destfile);
				error = 1;
			}
			break;
		case S_IFIFO:
			if(mkfifo(destfile, CP_UMASK(sourcestatp->st_mode & 0777)) == -1) {
				warn("%s unable to copy to %s as fifo", sourcefile, destfile);
				error = 1;
			}
			break;
		case S_IFLNK:
			if(cp_symlink_cow(sourcefile, destfile) == -1
				&& symlink(sourcefile, destfile) == -1) {
				error = 1;
			}
			break;
		case S_IFREG:
			if(cp_regfile_cow(sourcefile, destfile, sourcestatp) == -1
				&& cp_regfile(sourcefile, destfile, sourcestatp) == -1) {
				error = 1;
			}
			break;
		case S_IFDIR:
			if(CP_IS_RECURSIVE()) {
				if(mkdir(destfile, CP_UMASK(sourcestatp->st_mode & 0777) | S_IRWXU) == 0) {
					treeerrors = cp_directory(sourcefile, destfile);
					if(chmod(destfile, CP_UMASK(sourcestatp->st_mode & 0777)) == -1) {
						warn("Unable to keep %s permissions' for %s", sourcefile, destfile);
						error = 1;
					}
				} else {
					warn("Unable to create directory %s", destfile);
					error = 1;
				}
			} else {
				warnx("Missing -R to copy directories: %s to %s", sourcefile, destfile);
				error = 1;
			}
			break;
		default:
			warnx("%s has unsupported file type", sourcefile);
			error = 1;
			break;
		}
	}

	if(CP_DUPLICATES() && error == 0) {
		struct timespec times[2] = { sourcestatp->st_atim, sourcestatp->st_mtim };

		if(utimensat(AT_FDCWD, destfile, times,
			(sourcestatp->st_mode & S_IFMT) == S_IFLNK ? AT_SYMLINK_NOFOLLOW : 0) == -1) {
			warn("Unable to duplicate time properties of %s for %s", sourcefile, destfile);
			error = 1;
		}

		int destumask = 06777;
		if(chown(destfile, sourcestatp->st_uid, sourcestatp->st_gid) == -1) {
			warn("Unable to duplicate owners of %s for %s", sourcefile, destfile);
			destumask = 0777;
			error = 1;
		}

		if(chmod(destfile, CP_UMASK(sourcestatp->st_mode & destumask)) == -1) {
			warn("Unable to duplicate permissions of %s for %s", sourcefile, destfile);
			error = 1;
		}
	}

	return error + treeerrors;
}

static int
cp_copy_argument(const char *sourcefile, char *destfile,
	struct stat *deststatp) {
	struct stat sourcestat;

	if(CP_FOLLOWS_SOURCES() ? stat(sourcefile, &sourcestat) == 0
		: lstat(sourcefile, &sourcestat) == 0) {
		return cp_copy(sourcefile, &sourcestat, destfile, deststatp);
	} else {
		warn("Unable to stat source file %s", sourcefile);
		return 1;
	}

	return 0;
}

static void
cp_usage(const char *cpname) {
	fprintf(stderr, "usage: %s [-Pfip] source_file target_file\n"
		"       %s [-Pfip] source_file... target\n"
		"       %s -R [-H|-L|-P] [-fip] source_file... target\n",
		cpname, cpname, cpname);
	exit(1);
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

			CP_ENSURE_SLASH(targetend);

			while(sourcefiles != sourcefilesend) {
				char *sourcefile = *sourcefiles;

				if(stpncpy(targetend, basename(sourcefile), targetendcapacity) < target + sizeof(target)) {
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

	/* The standard asks specific handling (see when creating a directory) for
	permissions so we manually handle copies with the CP_UMASK macro which applies our umask */
	cpumask = umask(0);
	if(CP_DUPLICATES()) {
		cpumask = 0;
	}
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

