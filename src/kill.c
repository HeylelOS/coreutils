#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <limits.h>
#include <ctype.h>
#include <err.h>

/*
 * The following macro speculates that sizeof pid_t == sizeof unsigned int,
 * this might be considered dangerous, however its always better than an
 * unexpected overflow/underflow when dealing with pids.
 */
#define KILL_PID_MAX ((long)((unsigned)-1 >> 1))
#define KILL_PID_MIN ((long)(~KILL_PID_MAX))

struct kill_args {
	int signalnumber;
};

/**
 * Only the following list of signals is
 * defined as mandatory by the standard
 */
static const struct signalpair {
	const int signalnumber;
	const char signalname[7];
} signals[] = {
	{ 0,         "0"      },
	{ SIGABRT,   "ABRT"   },
	{ SIGALRM,   "ALRM"   },
	{ SIGBUS,    "BUS"    },
	{ SIGCHLD,   "CHLD"   },
	{ SIGCONT,   "CONT"   },
	{ SIGFPE,    "FPE"    },
	{ SIGHUP,    "HUP"    },
	{ SIGILL,    "ILL"    },
	{ SIGINT,    "INT"    },
	{ SIGKILL,   "KILL"   },
	{ SIGPIPE,   "PIPE"   },
	{ SIGQUIT,   "QUIT"   },
	{ SIGSEGV,   "SEGV"   },
	{ SIGSTOP,   "STOP"   },
	{ SIGTERM,   "TERM"   },
	{ SIGTSTP,   "TSTP"   },
	{ SIGTTIN,   "TTIN"   },
	{ SIGTTOU,   "TTOU"   },
	{ SIGUSR1,   "USR1"   },
	{ SIGUSR2,   "USR2"   },
#ifndef __APPLE__
	{ SIGPOLL,   "POLL"   },
#endif
	{ SIGPROF,   "PROF"   },
	{ SIGSYS,    "SYS"    },
	{ SIGTRAP,   "TRAP"   },
	{ SIGURG,    "URG"    },
	{ SIGVTALRM, "VTALRM" },
	{ SIGXCPU,   "XCPU"   },
	{ SIGXFSZ,   "XFSZ"   }
};

static int
kill_signal_number(const char *signalname) {
	const struct signalpair *current,
		* const signalsend = signals + sizeof(signals) / sizeof(*signals);

	if(strncasecmp("SIG", signalname, 3) == 0) {
		signalname += 3;
	}

	for(current = signals;
		current != signalsend && strcasecmp(current->signalname, signalname) != 0;
		current++);

	if(current == signalsend) {
		errx(1, "Unknown signal name: %s", signalname);
	}

	return current->signalnumber;
}

static const char *
kill_signal_name(int signalnumber) {
	const struct signalpair *current,
		* const signalsend = signals + sizeof(signals) / sizeof(*signals);

	for(current = signals;
		current != signalsend && current->signalnumber != signalnumber;
		current++);

	return current != signalsend ? current->signalname : NULL;

	if(current == signalsend) {
		errx(1, "Unrecognized signal number: %d", signalnumber);
	}

	return current->signalname;
}

/**
 * The following function exists because the standard specifies
 * an undefined behavior for specifying other signal numbers than
 * the following list. Moreover, no values are bound to the signals in the API,
 * which means that using raw signal numbers is not portable anyway.
 */
static int
kill_signal_defined_number(const char *signalnumber) {
	static const struct signalpair definedvalues[] = {
		{ 0,       "0"  },
		{ SIGHUP,  "1"  },
		{ SIGINT,  "2"  },
		{ SIGQUIT, "3"  },
		{ SIGABRT, "6"  },
		{ SIGKILL, "9"  },
		{ SIGALRM, "14" },
		{ SIGTERM, "15" },
	};
	const struct signalpair *current,
		* const definedvaluesend = definedvalues
			+ sizeof(definedvalues) / sizeof(*definedvalues);

	for(current = definedvalues;
		current != definedvaluesend && strcmp(current->signalname, signalnumber) != 0;
		current++);

	if(current == definedvaluesend) {
		errx(1, "Invalid signal number: %s", signalnumber);
	}

	return current->signalnumber;
}

static void
kill_print_exit_status(const char *exitstatus) {
	char *exitstatusend;
	const long lstatus = strtol(exitstatus, &exitstatusend, 10);

	if(*exitstatusend == '\0'
		&& lstatus <= INT_MAX && lstatus >= INT_MIN) {
		puts(kill_signal_name((int)(WIFSIGNALED(lstatus)
			? WTERMSIG(lstatus) : lstatus)));
	} else {
		errx(1, "Invalid non-int exit status: %s", exitstatus);
	}
}

static void
kill_print_signals(void) {
	const struct signalpair *current,
		* const signalsend = signals + sizeof(signals) / sizeof(*signals);

	for(current = signals + 1; current != signalsend - 1; current++) {
		printf("%s ", current->signalname);
	}

	puts(current->signalname);
}

static void
kill_usage(const char *killname) {
	fprintf(stderr, "usage: %s -s signal_name pid...\n"
		"       %s -l [exit_status]\n",
		killname, killname);
	exit(1);
}

static struct kill_args
kill_parse_args(int argc, char **argv) {
	struct kill_args args = { .signalnumber = SIGTERM };

	if(optind < argc
		&& argv[optind] != NULL
		&& *argv[optind] == '-'
		&& strcmp(argv[optind], "-") != 0
		&& strcmp(argv[optind], "--") != 0) {
		optopt = argv[optind][1];

		switch(optopt) {
		case 's':
			if(argv[optind][2] != '\0') {
				optarg = argv[optind] + 2;
			} else if(++optind < argc && argv[optind] != NULL) {
				optarg = argv[optind];
			} else {
				warnx("option requires an argument -- %c", optopt);
				kill_usage(*argv);
			}

			args.signalnumber = kill_signal_number(optarg);
			break;
		case 'l':
			if(argv[optind][2] != '\0') {
				optarg = argv[optind] + 2;
			} else if(++optind < argc && argv[optind] != NULL) {
				optarg = argv[optind];
			} else {
				kill_print_signals();
				exit(0);
			}

			kill_print_exit_status(optarg);
			exit(0);
		default:
			if(isdigit(optopt)) {
				args.signalnumber = kill_signal_defined_number(argv[optind] + 1);
				break;
			} else if(isalpha(optopt)) {
				args.signalnumber = kill_signal_number(argv[optind] + 1);
				break;
			} else {
				warnx("Unknown argument -%c", optopt);
				kill_usage(*argv);
			}
		}

		optind++;
	}

	if(optind < argc && argv[optind] != NULL
		&& strcmp(argv[optind], "--") == 0) {
		optind++;
	}

	if(argc - optind < 1) {
		warnx("Not enough arguments");
		kill_usage(*argv);
	}

	return args;
}

int
main(int argc,
	char **argv) {
	const struct kill_args args = kill_parse_args(argc, argv);
	char **argpos = argv + optind, ** const argend = argv + argc;
	int retval = 0;

	while(argpos != argend) {
		char * const spid = *argpos, *endspid;
		const long lpid = strtol(spid, &endspid, 10);

		if(*endspid == '\0' && lpid <= KILL_PID_MAX && lpid >= KILL_PID_MIN) {
			if(kill((pid_t)lpid, args.signalnumber) == -1) {
				warn("kill %ld", lpid);
				retval++;
			}
		} else {
			warnx("Invalid pid: %s", spid);
			retval++;
		}

		argpos++;
	}

	return retval;
}

