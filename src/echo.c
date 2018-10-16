#include <stdio.h>
#include <unistd.h>
#include <alloca.h>

#include <heylel/core.h>

static char *buffer;
static char *bufferpos;
static char *bufferend;

static void
echo_flush(void) {
	write(STDOUT_FILENO, buffer, bufferpos - buffer);
	bufferpos = buffer;
}

/**
 * The standard defines that non XSI compliant echo
 * shall not take any argument or modify input strings
 * that's why we also implement an echo-xsi version
 */
int
main(int argc,
	char ** const argv) {
	size_t outsize = io_blocksize(STDOUT_FILENO);
	buffer = alloca(outsize);
	bufferpos = buffer;
	bufferend = buffer + outsize;
	char **argpos = argv + 1;
	char **argend = argv + argc;

	while(argpos != argend) {
		if(bufferpos == bufferend) {
			echo_flush();
		}

		if(**argpos == '\0') {
			*bufferpos = ' ';
			argpos += 1;
		} else {
			*bufferpos = **argpos;
			*argpos += 1;
		}

		bufferpos += 1;
	}

	if(bufferpos != buffer) {
		bufferpos[-1] = '\n';
	} else {
		*bufferpos = '\n';
		bufferpos += 1;
	}

	echo_flush();

	return 0;
}
