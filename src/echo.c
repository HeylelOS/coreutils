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
/*	for(size_t i = 0; i < (bufferpos - buffer); ++i) {
		fprintf(stderr, "%p\n", (void*)(buffer + i));
	}
*/	if(writeval <= 0) {
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
#define isoctal(n)	((n) >= 48 && (n) <= 55)
	bool escaping = false;
	int octal = -1;
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

#ifdef ECHO_XSI
		if(octal != -1) {
			if(isoctal(**argpos)) {
				octal <<= 3;
				octal += **argpos - '0';
				*argpos += 1;
			} else {
				*bufferpos = (char)(octal & 0xFF);
				bufferpos += 1;
				octal = -1;
			}
		} else
#endif
		if(**argpos == '\0') {
			*bufferpos = ' ';
			bufferpos += 1;
			argpos += 1;
#ifdef ECHO_XSI
			escaping = false;
#endif
		} else {
#ifdef ECHO_XSI
			if(**argpos == '\\' && !escaping) {
				escaping = true;
				*argpos += 1;
			} else if(escaping && **argpos == 'c') {
				echo_flush();
				return 0;
			} else if(escaping && **argpos == '0') {
				octal = 0;
				escaping = false;
			} else {
				if(escaping) {
					switch(**argpos) {
					case 'a': *bufferpos = '\a'; break;
					case 'b': *bufferpos = '\b'; break;
					case 'f': *bufferpos = '\f'; break;
					case 'r': *bufferpos = '\r'; break;
					case 'n': *bufferpos = '\n'; break;
					case 't': *bufferpos = '\t'; break;
					case 'v': *bufferpos = '\v'; break;
					default: *bufferpos = **argpos; break; /* '\' will be wrote here */
					}
					escaping = false;
				} else {
#endif
					*bufferpos = **argpos;
#ifdef ECHO_XSI
				}
#endif
				bufferpos += 1;
				*argpos += 1;
#ifdef ECHO_XSI
			}
#endif
		}
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
