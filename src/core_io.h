
#ifndef HEYLEL_CORE_IO_H
#define HEYLEL_CORE_IO_H

#include <unistd.h>
#include <sys/stat.h>
#include <alloca.h>

#ifndef HEYLEL_UNUSED
#define HEYLEL_UNUSED __attribute__((unused))
#endif

/* IO Function */

/**
 * This function returns the best suiting block size
 * for intensive IO concerning fd, or 512 else.
 * @param fd The filedescriptor for which the buffer is selected.
 * @return The block size
 */
static size_t HEYLEL_UNUSED
io_blocksize(int fd) {
	struct stat stat;
	size_t blksize;

	if(fstat(fd, &stat) == 0) {
		blksize = stat.st_blksize;
	} else {
		blksize = 512; /* Arbitrary unix buffer size */
	}

	return blksize;
}

/**
 * This function ensures every bytes are read
 * @param fd The filedescriptor to read from
 * @param buffer The buffer where we read
 * @param count How many bytes we should read
 * @return 0 on success, number of bytes missing on end of file,
 * negation of number of bytes missing on error, with errno set.
 */
static ssize_t HEYLEL_UNUSED
io_read_all(int fd,
	char *buffer,
	size_t count) {
	ssize_t readval;

	while(count != 0
		&& (readval = read(fd, buffer, count)) > 0) {
		count -= readval;
		buffer += readval;
	}

	return readval == -1 ? -count : count;
}

/**
 * This function ensures every bytes are written
 * @param fd The filedescriptor to write to
 * @param buffer The buffer to write
 * @param count How many bytes to write
 * @return 0 on success, -1 on error, with errno set.
 */
static ssize_t HEYLEL_UNUSED
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

/**
 * This function dumps fdsrc to fddest,
 * io_dump_to tries to read at blksrc pace and write at blkdest pace.
 * The buffer is stack-allocated, so beware the size you give!
 * @param fdsrc Valid source filedescriptor to dump
 * @param blksrc Best size of the buffer when reading
 * @param fddest Valid destination file descriptor
 * @param blkdest Best size of the buffer for writing
 * @return 0 on success, -1 on failure on read, 1 on failure on write, errno set on error
 */
static ssize_t HEYLEL_UNUSED
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

/**
 * This function flushes fdsrc to fddest.
 * io_flush_to tries to read and write at blksrc pace.
 * The buffer is stack-allocated, so beware the size you give!
 * @param fdsrc Valid source filedescriptor to dump
 * @param blksrc Best size of the buffer when reading
 * @param fddest Valid destination fildescriptor
 * @return 0 on success, -1 on failure on read, 1 on failure on write, errno set on error
 */
static ssize_t HEYLEL_UNUSED
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

/* HEYLEL_CORE_IO_H */
#endif
