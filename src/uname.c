#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/utsname.h>
#include <err.h>

struct uname_args {
	unsigned machine : 1;
	unsigned nodename : 1;
	unsigned release : 1;
	unsigned sysname : 1;
	unsigned version : 1;
};

static bool
uname_print(const char *symbol, bool shouldwrite, bool previouswritten) {

	if(shouldwrite) {
		if(previouswritten) {
			putchar(' ');
		}

		fputs(symbol, stdout);
		previouswritten = true;
	}

	return previouswritten;
}

static void
uname_usage(const char *unamename) {
	fprintf(stderr, "usage: %s [-amnrsv]\n", unamename);
	exit(1);
}

static struct uname_args
uname_parse_args(int argc, char **argv) {
	struct uname_args args = { 0, 0, 0, 0, 0 };
	int c;

	while((c = getopt(argc, argv, "amnrsv")) != -1) {
		switch(c) {
		case 'a':
			args.machine = 1;
			args.nodename = 1;
			args.release = 1;
			args.sysname = 1;
			args.version = 1;
			break;
		case 'm':
			args.machine = 1;
			break;
		case 'n':
			args.nodename = 1;
			break;
		case 'r':
			args.release = 1;
			break;
		case 's':
			args.sysname = 1;
			break;
		case 'v':
			args.version = 1;
			break;
		default:
			warnx("Unknown argument -%c", optopt);
			uname_usage(*argv);
		}
	}

	if(args.machine == 0 && args.nodename == 0 && args.release == 0
		&& args.sysname == 0 && args.version == 0) {
		args.sysname = 1;
	}

	return args;
}

int
main(int argc,
	char **argv) {
	const struct uname_args args = uname_parse_args(argc, argv);
	struct utsname utsname;

	if(uname(&utsname) == 0) {
		bool printed = false;

		printed = uname_print(utsname.sysname, args.sysname == 1, printed);
		printed = uname_print(utsname.nodename, args.nodename == 1, printed);
		printed = uname_print(utsname.release, args.release == 1, printed);
		printed = uname_print(utsname.version, args.version == 1, printed);
		printed = uname_print(utsname.machine, args.machine == 1, printed);
		putchar('\n');

		return 0;
	} else {
		err(1, "uname");
	}
}

