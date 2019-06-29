#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <limits.h>
#include <errno.h>
#include <err.h>

/* Copy-on-write support */
#ifdef __APPLE__
#include <sys/clonefile.h>
#elif defined(__linux__)
#include <sys/ioctl.h>
#include <linux/fs.h>
#endif

#include "core_fs.h"
#include "core_io.h"

#ifdef __APPLE__
#define st_atim st_atimespec
#define st_mtim st_mtimespec
#endif

#define CP_PERMS(mode, args) ((mode) & S_IRWXA & ~(args).cmask)

struct cp_args {
	unsigned recursive : 1;
	unsigned force : 1;
	unsigned interactive : 1;
	unsigned duplicate : 1;
	unsigned followssources : 1;
	unsigned followstraversal : 1;
	mode_t cmask;
};

struct cp_recursion {
	struct fs_recursion source;
	struct {
		char *path;
		char *name;
		const char *pathend;
	} dest;
	mode_t *sourcemodes;
};

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
	const struct stat *statp, const struct cp_args args) {
#ifdef __APPLE__
	return clonefile(sourcefile, destfile, 0);
#elif defined(__linux__)
	int fdsrc = open(sourcefile, O_RDONLY);
	int retval = 0;

	if(fdsrc >= 0) {
		int fddest = open(destfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			CP_PERMS(statp->st_mode, args));

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
	const struct stat *statp, const struct cp_args args) {
	int fddest;
	int retval = 0;

	if(access(destfile, F_OK) == 0) {
		if(args.interactive == 1
			&& !io_prompt_confirm("Do you want to overwrite '%s' with '%s'? ",
				destfile, sourcefile)) {
			return 0;
		}

		fddest = open(destfile, O_WRONLY | O_TRUNC);

		if(fddest == -1 && args.force == 1) {
			if(unlink(destfile) == 0) {
				fddest = open(destfile,
					O_WRONLY | O_TRUNC | O_CREAT,
					CP_PERMS(statp->st_mode, args));
			} else {
				warn("Unable to unlink %s", destfile);
			}
		}
	} else {
		fddest = open(destfile,
			O_WRONLY | O_TRUNC | O_CREAT,
			CP_PERMS(statp->st_mode, args));
	}

	if(fddest >= 0) {
		int fdsrc = open(sourcefile, O_RDONLY);

		if(fdsrc >= 0) {
			switch(io_flush_to(fdsrc, fddest,
				statp->st_blksize)) {
			case -1:
				warn("'%s' to '%s' read failed", sourcefile, destfile);
				retval = -1;
				break;
			case 1:
				warn("'%s' to '%s' write failed", sourcefile, destfile);
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
	const char *destfile, const struct stat *deststatp,
	const struct cp_args args) {
	int retval = 0;

	if(deststatp != NULL
		&& sourcestatp->st_ino == deststatp->st_ino) {
		warnx("'%s' and '%s' are identical (not copied)", sourcefile, destfile);
		retval = -1;
	} else {
		switch(sourcestatp->st_mode & S_IFMT) {
		case S_IFBLK:
		case S_IFCHR:
			if((retval = mknod(destfile, CP_PERMS(sourcestatp->st_mode, args), sourcestatp->st_dev)) == -1) {
				warn("'%s' unable to copy to '%s' as device", sourcefile, destfile);
			}
			break;
		case S_IFIFO:
			if((retval = mkfifo(destfile, CP_PERMS(sourcestatp->st_mode, args))) == -1) {
				warn("'%s' unable to copy to '%s' as fifo", sourcefile, destfile);
			}
			break;
		case S_IFLNK:
			if(cp_symlink_cow(sourcefile, destfile) == -1) {
				retval = symlink(sourcefile, destfile);
			}
			break;
		case S_IFREG:
			if(cp_regfile_cow(sourcefile, destfile, sourcestatp, args) == -1) {
				retval = cp_regfile(sourcefile, destfile, sourcestatp, args);
			}
			break;
		case S_IFDIR:
			if(args.recursive == 1) {
				if((retval = mkdir(destfile, CP_PERMS(sourcestatp->st_mode, args) | S_IRWXU)) == -1) {
					warn("Unable to create directory %s", destfile);
				}
			} else {
				warnx("-R not specified, won't copy '%s' to '%s'", sourcefile, destfile);
				retval = 1;
			}
			break;
		default:
			warnx("%s has unsupported file type", sourcefile);
			retval = -1;
			break;
		}
	}

	if(args.duplicate == 1 && retval == 0) {
		struct timespec times[2] = { sourcestatp->st_atim, sourcestatp->st_mtim };

		if(utimensat(AT_FDCWD, destfile, times,
			S_ISLNK(sourcestatp->st_mode) ? AT_SYMLINK_NOFOLLOW : 0) == -1) {
			warn("Unable to duplicate time properties of '%s' for '%s'", sourcefile, destfile);
			retval = -1;
		}

		mode_t destmask = S_ISALL | S_IRWXA;
		if(chown(destfile, sourcestatp->st_uid, sourcestatp->st_gid) == -1) {
			warn("Unable to duplicate owners of '%s' for '%s'", sourcefile, destfile);
			destmask ^= S_ISUID | S_ISGID;
			retval = -1;
		}

		if(!S_ISDIR(sourcestatp->st_mode) /* Directories handled later after their full traversal, see cp_recursive_pop() */
			&& chmod(destfile, sourcestatp->st_mode & destmask & ~args.cmask) == -1) {
			warn("Unable to duplicate permissions of '%s' for '%s'", sourcefile, destfile);
			retval = -1;
		}
	}

	return retval;
}

/*
static int
cp_recursion_init(struct cp_recursion *recursion, const struct stat *sourcestatp,
	char *sourcefile, char *sourcefilenul, const char *sourcefileend,
	char *destfile, char *destfilenul, const char *destfileend) {

	if(destfilenul < destfileend - 1
		&& fs_recursion_init(&recursion->source, sourcefile, sourcefilenul, sourcefileend) == 0) {
		recursion->destfile = destfile;
		recursion->destfilenul = CP_ENSURE_SLASH(destfilenul);
		recursion->destfileend = destfileend;

		if((recursion->stats = malloc(sizeof(*recursion->stats) * recursion->source.capacity)) != NULL) {
			*recursion->stats = *sourcestatp;
			return 0;
		}

		fs_recursion_deinit(&recursion->source);
	}

	return -1;
}

static void
cp_recursion_deinit(struct cp_recursion *recursion) {
	fs_recursion_deinit(&recursion->source);

	free(recursion->stats);
}

static inline bool
cp_recursion_is_empty(const struct cp_recursion *recursion) {

	return fs_recursion_is_empty(&recursion->source);
}

static inline bool
cp_recursion_is_valid(struct cp_recursion *recursion, const char *name) {

	return fs_recursion_is_valid(name)
		&& stpncpy(recursion->source.buffernul, name,
			recursion->source.bufferend - recursion->source.buffernul) < recursion->source.bufferend
		&& stpncpy(recursion->destfilenul, name, recursion->destfileend - recursion->destfilenul) < recursion->destfileend - 1;
}

static inline DIR *
cp_recursion_peak(const struct cp_recursion *recursion, struct stat *deststatp) {

	*deststatp = recursion->stats[recursion->source.count - 1];

	return fs_recursion_peak(&recursion->source);
}

static int
cp_recursion_push(struct cp_recursion *recursion, const char *name,
	const struct stat *statp) {
	const char *oldbuffernul = recursion->source.buffernul;

	if(recursion->source.count == recursion->source.capacity) {
		struct stat *newstats = realloc(recursion->stats,
			sizeof(*recursion->stats) * recursion->source.capacity * 2);

		if(newstats == NULL) {
			return -1;
		}

		recursion->stats = newstats;
	}

	if(fs_recursion_push(&recursion->source, name) == 0) {
		recursion->destfilenul += recursion->source.buffernul - oldbuffernul;
		recursion->destfilenul[-1] = '/';
		recursion->stats[recursion->source.count - 1] = *statp;

		return 0;
	}

	return -1;
}

static int
cp_recursion_pop(struct cp_recursion *recursion, const struct cp_args args) {

	*recursion->source.buffernul = '\0';
	*recursion->destfilenul = '\0';
	fs_recursion_pop(&recursion->source);
	do {
		recursion->destfilenul--;
	} while(recursion->destfilenul != recursion->destfile
		&& recursion->destfilenul[-1] != '/');

	// POSIX leaves undefined behaviour on bits other than S_IRWXA for mkdir, creat... Don't forget to leave sticky bit when chmod. Note also this function is only called when the directory has been succesfully created, so we can leave set-user/group-ID safely in the mask
	mode_t destmask = (args.duplicate == 1 ? S_ISALL : S_ISVTX) | S_IRWXA;
	if(chmod(recursion->destfile, recursion->stats[recursion->source.count].st_mode & destmask & ~args.cmask) == -1) {
		warn("Unable to keep '%s' permissions' for '%s'", recursion->source.buffer, recursion->destfile);
		return -1;
	}

	return 0;
}

static int
cp_copy_argument(const char *sourcefile,
	char *destfile, char *destfilenul, const char *destfileend,
	const struct stat *deststatp, const struct cp_args args) {
	struct stat sourcestat;
	int retval = 0;

	if(args.followssources == 1 ? stat(sourcefile, &sourcestat) == 0
		: lstat(sourcefile, &sourcestat) == 0) {
		if((retval = cp_copy(sourcefile, &sourcestat, destfile, deststatp, args)) == 0
			&& S_ISDIR(sourcestat.st_mode)) {
			char buffer[PATH_MAX], * const bufferend = buffer + sizeof(buffer);
			char *buffernul = stpncpy(buffer, sourcefile, bufferend - buffer);
			struct cp_recursion recursion;

			if(cp_recursion_init(&recursion, &sourcestat,
				buffer, buffernul, bufferend,
				destfile, destfilenul, destfileend) == 0) {

				while(!cp_recursion_is_empty(&recursion)) {
					struct dirent *entry;

					while((entry = readdir(cp_recursion_peak(&recursion, &sourcestat))) != NULL) {

						if(cp_recursion_is_valid(&recursion, entry->d_name)) {
							if(args.followstraversal == 1 ? stat(buffer, &sourcestat) == 0
								: lstat(buffer, &sourcestat) == 0) {

								if(cp_copy(buffer, &sourcestat, destfile, NULL, args) == 0) {
									if(S_ISDIR(sourcestat.st_mode)
										&& cp_recursion_push(&recursion, entry->d_name, &sourcestat) == -1) {
										warnx("Unable to explore directory %s", buffer);
										retval++;
									}
								} else {
									retval++;
								}
							} else {
								warn("Unable to stat traversal file %s", destfile);
								retval++;
							}
						}
					}

					retval += -cp_recursion_pop(&recursion, args);
				}

				cp_recursion_deinit(&recursion);
			} else {
				warnx("Unable to explore hierarchy of %s", sourcefile);
				retval++;
			}
		}
	} else {
		warn("Unable to stat source file %s", sourcefile);
		retval = 1;
	}

	return retval;
}
*/
static int
cp_recursion_init(struct cp_recursion *recursion,
	const char *source, const struct stat *sourcestatp,
	const char *dest,
	size_t size, bool follows) {

	if(fs_recursion_init(&recursion->source, source, size, follows) == -1) {
		goto cp_recursion_init_err0;
	}

	if((recursion->dest.path = malloc(size)) == NULL) {
		goto cp_recursion_init_err1;
	}

	recursion->dest.pathend = recursion->dest.path + size;

	if((recursion->dest.name = stpncpy(recursion->dest.path, dest, size)) >= recursion->dest.pathend - 1
		|| (recursion->sourcemodes = malloc(sizeof(*recursion->sourcemodes) * recursion->source.capacity)) == NULL) {
		goto cp_recursion_init_err2;
	}

	if(recursion->dest.name[-1] != '/') {
		*recursion->dest.name = '/';
		recursion->dest.name++;
	}

	*recursion->sourcemodes = sourcestatp->st_mode;

	return 0;

cp_recursion_init_err2:
	free(recursion->dest.path);
cp_recursion_init_err1:
	fs_recursion_deinit(&recursion->source);
cp_recursion_init_err0:
	return -1;
}

static void
cp_recursion_deinit(struct cp_recursion *recursion) {

	free(recursion->sourcemodes);
	free(recursion->dest.path);
	fs_recursion_deinit(&recursion->source);
}

static inline int
cp_recursion_next(struct cp_recursion *recursion) {

	if(fs_recursion_next(&recursion->source) == 0) {
		while(stpncpy(recursion->dest.name, recursion->source.name,
			recursion->dest.pathend - recursion->dest.name) >= recursion->dest.pathend - 1) {
			size_t length = recursion->dest.name - recursion->dest.path,
				capacity = recursion->dest.pathend - recursion->dest.path;
			char *newpath = realloc(recursion->dest.path, capacity * 2);

			if(newpath != NULL) {
				recursion->dest.path = newpath;
				recursion->dest.name = newpath + length;
				recursion->dest.pathend = newpath + capacity * 2;
			} else {
				return -1;
			}
		}

		return 0;
	}

	return -1;
}

static int
cp_recursion_push(struct cp_recursion *recursion, mode_t sourcemode) {

	if(recursion->source.count == recursion->source.capacity) {
		mode_t *newsourcemodes = realloc(recursion->sourcemodes,
			sizeof(*recursion->sourcemodes) * recursion->source.capacity * 2);

		if(newsourcemodes == NULL) {
			recursion->sourcemodes = newsourcemodes;
		} else {
			return -1;
		}
	}

	if(fs_recursion_push(&recursion->source) == 0) {
		while(*recursion->dest.name != '\0') {
			recursion->dest.name++;
		}
		*recursion->dest.name = '/';
		recursion->dest.name++;

		return 0;
	}

	return -1;
}

static int
cp_recursion_pop(struct cp_recursion *recursion, const struct cp_args args) {
	/* POSIX leaves undefined behaviour on bits other than S_IRWXA for mkdir, creat... Don't forget to leave sticky bit when chmod.
	Note also this is only called when the directory has been succesfully created, so we can leave set-user/group-ID safely in the mask */
	mode_t destmask = (args.duplicate == 1 ? S_ISALL : S_ISVTX) | S_IRWXA;

	if(chmod(recursion->dest.path, recursion->sourcemodes[recursion->source.count - 1] & destmask & ~args.cmask) == -1) {
		warn("Unable to set '%s' permissions' for '%s'", recursion->source.path, recursion->dest.path);
		return -1;
	}

	if(fs_recursion_pop(&recursion->source) == 0) {
		do {
			recursion->dest.name--;
		} while(recursion->dest.name[-1] != '/');
		*recursion->dest.name = '\0';

		return 0;
	}

	return -1;
}

static int
cp_copy_argument(const char *sourcefile,
	char *destfile, const struct stat *deststatp,
	const struct cp_args args) {
	struct stat sourcestat;
	int retval = 0;

	if(args.followssources == 1 ? stat(sourcefile, &sourcestat) == 0
		: lstat(sourcefile, &sourcestat) == 0) {
		if(cp_copy(sourcefile, &sourcestat, destfile, deststatp, args) == 0
			&& S_ISDIR(sourcestat.st_mode)) {
			struct cp_recursion recursion;

			if(cp_recursion_init(&recursion, sourcefile, &sourcestat,
				destfile, 256, args.followstraversal == 1) == 0) {
				do {
					while(cp_recursion_next(&recursion) == 0 && recursion.source.name != '\0') {
						if(cp_copy(recursion.source.path, &sourcestat,
								recursion.dest.path, NULL, args) == -1
							|| (S_ISDIR(sourcestat.st_mode)
								&& cp_recursion_push(&recursion, sourcestat.st_mode) == -1)) {
							retval++;
						}
					}
				} while(cp_recursion_pop(&recursion, args) == 0);

				cp_recursion_deinit(&recursion);
			} else {
				retval++;
			}
		} else {
			retval++;
		}
	} else {
		warn("Unable to stat source file %s", sourcefile);
		retval = 1;
	}

	return retval;
}

static void
cp_usage(const char *cpname) {
	fprintf(stderr, "usage: %s [-Pfip] source_file target_file\n"
		"       %s [-Pfip] source_file... target\n"
		"       %s -R [-H|-L|-P] [-fip] source_file... target\n",
		cpname, cpname, cpname);
	exit(1);
}

static struct cp_args
cp_parse_args(int argc, char **argv) {
	struct cp_args args;
	int c;

	while((c = getopt(argc, argv, "RLHPfip")) != -1) {
		switch(c) {
		case 'R':
			args.recursive = 1;
			break;
		case 'P':
			args.followssources = 0;
			args.followstraversal = 0;
			break;
		case 'f':
			args.force = 1;
			break;
		case 'i':
			args.interactive = 1;
			break;
		case 'p':
			args.duplicate = 1;
			break;
		case 'H':
			if(args.recursive == 1) {
				args.followssources = 1;
				args.followstraversal = 0;
				break;
			}
			/* fallthrough */
		case 'L':
			if(args.recursive == 1) {
				args.followssources = 1;
				args.followstraversal = 1;
				break;
			}
			/* fallthrough */
		default:
			warnx("Unknown argument -%c", c);
			cp_usage(*argv);
		}
	}

	if(argc - optind < 2) {
		warnx("Not enough arguments");
		cp_usage(*argv);
	}

	args.cmask = umask(0);
	if(args.duplicate == 1) {
		args.cmask = 0;
	}

	return args;
}

int
main(int argc,
	char **argv) {
	const struct cp_args args = cp_parse_args(argc, argv);
	char **argpos = argv + optind, ** const argend = argv + argc - 1;
	char target[PATH_MAX], * const targetend = target + sizeof(target);
	char *targetname = stpncpy(target, argv[argc - 1], sizeof(target));
	struct stat targetstat;
	int retval = 0;

	if(*target == '\0') {
		errx(1, "Empty target path invalid");
	} else if(targetname >= targetend - 1) { /* -1 for the slash if we need it */
		errx(1, "Target path too long");
	}

	if(stat(target, &targetstat) == 0) {
		if(S_ISDIR(targetstat.st_mode)) { /* Synopsis 2 & 3 */
			/* Standard specifies append ONE slash if one not already here on paths */
			if(targetname[-1] != '/') {
				*targetname = '/';
				++targetname;
			}

			while(argpos != argend) {
				char *sourcefile = *argpos;

				if(stpncpy(targetname, basename(sourcefile), targetend - targetname) < targetend) {
					retval += cp_copy_argument(sourcefile, target,
						lstat(target, &targetstat) == 0 ? &targetstat : NULL,
						args);
				} else {
					warnx("Destination path too long for source file %s", sourcefile);
					retval++;
				}

				argpos++;
			}
		} else if(argc - optind == 2) { /* Synopsis 1 */
			retval = cp_copy_argument(*argpos, target, &targetstat, args);
		} else {
			warnx("%s is not a directory", target);
			cp_usage(*argv);
		}
	} else if(errno == ENOENT || errno == ENOTDIR) {
		if(argc - optind == 2) { /* Synopsis 1 */
			retval = cp_copy_argument(*argpos, target, NULL, args);
		} else {
			warnx("%s doesn't exist and multiple source files were provided", target);
			cp_usage(*argv);
		}
	} else {
		err(1, "Unable to stat target %s", target);
	}

	return retval;
}

