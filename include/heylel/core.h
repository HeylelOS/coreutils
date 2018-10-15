#ifndef HEYLEL_CORE_H
#define HEYLEL_CORE_H

#include <sys/types.h>

/* IO Function */

/**
 * This function returns the best suiting block size
 * for intensive IO concerning fd.
 * @param fd The filedescriptor for which the buffer is selected.
 * @return The block size
 */
size_t
io_blocksize(int fd);

/**
 * This function ensures every bytes are written
 * @param fd The filedescriptor to write to
 * @param buffer The buffer to write
 * @param count How many bytes to write
 * @return 0 on success, -1 on error, with errno set.
 */
ssize_t
io_write_all(int fd, const char *buffer, size_t count);

/**
 * This function dumps fdsrc to fddest, the buffer is stack-allocated,
 * so beware the size you give!
 * @param fdsrc Valid source filedescriptor to dump
 * @param blksrc Best size of the buffer when reading
 * @param fddest Valid destination fildescriptor
 * @param blkdest Best size of the buffer for writing
 * @return 0 on success, -1 on failure on read, 1 on failure on write
 */
int
io_dump_to(int fdsrc, size_t blksrc,
	int fddest, size_t blkdest);

/* HEYLEL_CORE_H */
#endif
