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
		perror(echoname);
		exit(1);
	}

	bufferpos -= writeval;
}

static void
echo_pushchar(char c) {
	*bufferpos = c;
	bufferpos += 1;
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
			echo_flush();
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
		} else
#endif
		if(**argpos == '\0') {
			echo_pushchar(' ');
			argpos += 1;
#ifdef ECHO_XSI
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
#else /* Not ECHO_XSI */
		} else { /* if(**argpos == '\0') */
#endif
					echo_pushchar(**argpos);

				*argpos += 1;
#ifdef ECHO_XSI
			}
#endif
		}
	}

	if(bufferpos == buffer && trailing != '\0') {
		bufferpos += 1;
	}

	if(bufferpos != buffer) {
		bufferpos[-1] = trailing;
		echo_flush();
	}

	return 0;
}
