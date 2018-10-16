#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <alloca.h>
#ifdef ECHO_XSI
#include <stdbool.h>
#else
#include <string.h>
#endif

#include <heylel/core.h>

static char *buffer;
static char *bufferpos;
static char *bufferend;
static char *echoname;

static void
echo_flush(void) {
	ssize_t writeval = write(STDOUT_FILENO, buffer, bufferpos - buffer);
	if(writeval <= 0) {
		if(writeval == -1) {
			perror(echoname);
		}
		exit(1);
	}
	bufferpos -= writeval;
}

/**
 * The standard defines that non XSI compliant echo
 * shall not take any argument or modify input strings
 * that's why we also implement an echo-xsi version
 */
int
main(int argc,
	char **argv) {
	size_t outsize = io_blocksize(STDOUT_FILENO);
	buffer = alloca(outsize);
	bufferpos = buffer;
	bufferend = buffer + outsize;
	echoname = *argv;
	char **argpos = argv + 1;
	char **argend = argv + argc;
#ifdef ECHO_XSI
#define trailing '\n'
#else
	char trailing = '\n';
	if(argc > 1
		&& strcmp(*argpos, "-n") == 0) {
		/* Like a majority of unix-like systems,
		 * we won't write trailing newline on -n */
		trailing = '\0';
		argpos += 1;
	}
#endif

	while(argpos != argend) {
		if(bufferpos == bufferend) {
			echo_flush();
		}

		switch(**argpos) {
		case '\0':
			*bufferpos = ' ';
			argpos += 1;
			break;
		default:
			*bufferpos = **argpos;
			*argpos += 1;
			break;
		}

		bufferpos += 1;
	}

#ifndef ECHO_XSI
	if(trailing != '\0') {
#endif
		if(bufferpos != buffer) {
			bufferpos[-1] = trailing;
		} else {
			*bufferpos = trailing;
			bufferpos += 1;
		}
#ifndef ECHO_XSI
	} else if(bufferpos != buffer) {
		bufferpos -= 1;
	}
#endif

	echo_flush();

	return 0;
}
