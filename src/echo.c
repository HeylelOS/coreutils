#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <alloca.h>
#ifndef ECHO_XSI
#include <string.h>
#endif

#include <heylel/core.h>

static char *buffer;	/**< Buffer used for IO */
static char *bufferpos;	/**< Current writing position in buffer */
static char *bufferend;	/**< last position of buffer + 1 */
static char *echoname;	/**< Name of the program */

/* Writes as possible of the buffer as possible
 * @param flushall Should we write everything?
 * @return No return on error, prints error message
 */ 
static void
echo_flush(bool flushall) {
	ssize_t writeval = flushall ?
		io_write_all(STDOUT_FILENO, buffer, bufferpos - buffer)
		: write(STDOUT_FILENO, buffer, bufferpos - buffer);

	if(writeval < 0
		|| (!flushall && writeval == 0)) {
		perror(echoname);
		exit(1);
	}

	bufferpos -= writeval;
}

/**
 * push char on buffer, the buffer must not
 * be filled
 * @param c The character to push
 */
static void
echo_pushchar(char c) {
	*bufferpos = c;
	bufferpos += 1;
}

/**
 * The standard defines that XSI compliant echo
 * shall not take one argument and may expand input strings
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
	char trailing = '\n';
#ifdef ECHO_XSI
#define isoctal(n)	((n) >= 48 && (n) <= 55)
	bool escaping = false;
	int octal = -1;
#else /* Not ECHO_XSI */
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
			echo_flush(false);
		}
#ifdef ECHO_XSI
		if(octal != -1) {
			if(isoctal(**argpos)) {
				octal <<= 3;
				octal += **argpos - '0';
				*argpos += 1;
			} else {
				echo_pushchar((char)(octal & 0xFF));
				octal = -1;
			}
		} else if(**argpos == '\0') {
			echo_pushchar(' ');
			argpos += 1;
			escaping = false;
		} else {
			if(escaping && **argpos == 'c') {
				trailing = '\0';
				bufferpos += 1;
				break;
			} else if(escaping && **argpos == '0') {
				octal = 0;
				escaping = false;
			} else {
				if(**argpos == '\\' && !escaping) {
					escaping = true;
				} else if(escaping) {
					switch(**argpos) {
					case 'a': echo_pushchar('\a'); break;
					case 'b': echo_pushchar('\b'); break;
					case 'f': echo_pushchar('\f'); break;
					case 'r': echo_pushchar('\r'); break;
					case 'n': echo_pushchar('\n'); break;
					case 't': echo_pushchar('\t'); break;
					case 'v': echo_pushchar('\v'); break;
					default: echo_pushchar(**argpos); break; /* '\' will be wrote here */
					}
					escaping = false;
				} else
				echo_pushchar(**argpos);
				*argpos += 1;
			}
		}
#else /* Not ECHO_XSI */
		if(**argpos == '\0') {
			echo_pushchar(' ');
			argpos += 1;
		} else {
			echo_pushchar(**argpos);
			*argpos += 1;
		}
#endif
	}

	if(bufferpos == buffer && trailing != '\0') {
		bufferpos += 1;
	}

	if(bufferpos != buffer) {
		bufferpos[-1] = trailing;
		echo_flush(true);
	}

	return 0;
}
