#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <err.h>

#include "core_fs.h"

static int
chmod_change(const char *file, const struct stat *statp,
	const char *modeexp, mode_t cmask) {
	mode_t mode = statp->st_mode & (S_ISALL | S_IRWXA);
	const char *modeexpend = fs_parsemode(modeexp, &mode,
		cmask, S_ISDIR(statp->st_mode));
	int retval = -1;

	if(*modeexpend == '\0') {
		if((retval = chmod(file, mode)) == -1) {
			warn("chmod %s", file);
		}
	} else {
		warnx("Unable to parse mode '%s', stopped at '%c'", modeexp, *modeexpend);
	}

	return retval;
}

static int
chmod_change_argument(const char *file,
	const char *modeexp, mode_t cmask,
	bool recursive) {
	struct stat st;
	int retval;

	if((retval = -stat(file, &st)) == 0) {
		if((retval = -chmod_change(file, &st, modeexp, cmask)) == 0
			&& S_ISDIR(st.st_mode) && recursive) {
			struct fs_recursion recursion;
			char path[PATH_MAX], * const pathend = path + sizeof(path);

			if(fs_recursion_init(&recursion,
				path, pathend, stpncpy(path, file, sizeof(path))) == 0) {

				while(!fs_recursion_is_empty(&recursion)) {
					struct dirent *entry;

					while((entry = readdir(fs_recursion_peak(&recursion))) != NULL) {
						if(fs_recursion_is_valid(entry->d_name)) {

							if(stat(path, &st) == 0) {
								if(S_ISDIR(st.st_mode)) {
									if(fs_recursion_push(&recursion, entry->d_name) == 0) {
										retval += -chmod_change(path, &st, modeexp, cmask);
									} else {
										warnx("Unable to explore directory %s", path);
										retval++;
									}
								} else {
									char *nameend = fs_recursion_path_end(&recursion);

									if(stpncpy(nameend, entry->d_name, pathend - nameend) < pathend) {
										retval += -chmod_change(path, &st, modeexp, cmask);
									} else {
										warnx("Unable to chmod %s: Path too long", path);
										retval++;
									}
								}
							} else {
								warn("stat %s", file);
								retval++;
							}
						}
					}

					fs_recursion_pop(&recursion);
				}

				fs_recursion_deinit(&recursion);
			} else {
				warnx("Unable to explore hierarchy of %s", file);
				retval++;
			}
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

			retval += chmod_change_argument(file, modeexp, cmask, recursive);

			argpos++;
		}
	} else {
		chmod_usage(*argv);
	}

	return retval;
}

