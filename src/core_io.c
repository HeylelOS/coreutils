#include <heylel/core.h>

#include <sys/stat.h>
#include <unistd.h>
#include <alloca.h>

size_t
io_blocksize(int fd) {
	struct stat stat;
	blksize_t blksize;

	if(fstat(fd, &stat) == 0) {
		blksize = stat.st_blksize;
	} else {
		blksize = 512; /* Arbitrary unix buffer size */
	}

	return blksize;
}

ssize_t
io_write_all(int fd,
	const char *buffer,
	size_t count) {
	ssize_t writeval;

	while((writeval = write(fd, buffer, count)) < count
		&& writeval != -1) {
		count -= writeval;
		buffer += writeval;
	}

	return writeval == -1 ? -1 : 0;
}

ssize_t
io_dump_to(int fdsrc, size_t blksrc,
	int fddest, size_t blkdest) {
	size_t buffersize = (blksrc > blkdest ? blksrc : blkdest);
	char * const buffer = alloca(buffersize);
	char *position = buffer, * const end = buffer + buffersize;
	ssize_t readval;

	do {
		do {
			size_t bufferleft = end - position;
			readval = read(fdsrc, position,
				(bufferleft < blksrc ? bufferleft : blksrc));

			position += readval;
		} while(readval > 0 && position < end);

		if(readval > 0
			&& io_write_all(fddest, buffer, position - buffer) == -1) {
			return 1;
		}

		position = buffer;
	} while(readval > 0);

	return readval;
}

ssize_t
io_flush_to(int fdsrc,
	int fddest,
	size_t blksize) {
	char * const buffer = alloca(blksize);
	ssize_t readval;

	while((readval = read(fdsrc, buffer, blksize)) > 0) {
		if(io_write_all(fddest, buffer, readval) == -1) {
			return 1;
		}
	}

	return readval;
}
