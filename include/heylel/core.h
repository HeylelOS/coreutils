#ifndef HEYLEL_CORE_H
#define HEYLEL_CORE_H

#include <sys/types.h>
#include <sys/stat.h>

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
 * This function dumps fdsrc to fddest,
 * io_dump_to tries to read at blksrc pace and write at blkdest pace.
 * The buffer is stack-allocated, so beware the size you give!
 * @param fdsrc Valid source filedescriptor to dump
 * @param blksrc Best size of the buffer when reading
 * @param fddest Valid destination fildescriptor
 * @param blkdest Best size of the buffer for writing
 * @return 0 on success, -1 on failure on read, 1 on failure on write, errno set on error
 */
ssize_t
io_dump_to(int fdsrc, size_t blksrc,
	int fddest, size_t blkdest);

/**
 * This function flushes fdsrc to fddest.
 * io_flush_to tries to read and write at blksrc pace.
 * The buffer is stack-allocated, so beware the size you give!
 * @param fdsrc Valid source filedescriptor to dump
 * @param blksrc Best size of the buffer when reading
 * @param fddest Valid destination fildescriptor
 * @return 0 on success, -1 on failure on read, 1 on failure on write, errno set on error
 */
ssize_t
io_flush_to(int fdsrc,
	int fddest,
	size_t blksize);

/* FS Function */

/**
 * Finds the beginning and the end of the basename
 * @param path the path in which we get the basename, non-null
 * @param begin pointer where the beginning of the basename is returned
 * @param end pointer where the end of the basename is returned
 * @return 0 on success, -1 on error
 */
ssize_t
fs_basename(const char *path,
	const char **pbegin,
	const char **pend);

/**
 * Parses mask as specified in the standard
 * and returns a pointer to the last valid character
 * @param mask The string to parse
 * @param isdir Act like we target a directory
 * @param mode Pointer to the mask to parse
 * @return A pointer to the last valid character
 */
const char *
fs_parsemask(const char *mask,
	int isdir,
	mode_t *mode);

/* HEYLEL_CORE_H */
#endif
