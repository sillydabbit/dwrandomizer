
#ifndef _SPOILER_H_
#define _SPOILER_H_

#include "dwr_types.h"

/**
 * Builds the spoiler log for a randomized rom.
 *
 * The caller owns the returned buffer and must free() it. The log is built as
 * a string rather than written straight to a file so that builds without a
 * filesystem (the web and electron versions) can use it too.
 *
 * @param rom The rom struct, after all randomization is complete
 * @param crc The rom checksum, for the header
 * @return A newly allocated string, or NULL if it could not be allocated
 */
char *dwr_spoiler(dw_rom *rom, uint64_t crc);

/**
 * Writes the spoiler log for a randomized rom to a file.
 *
 * @param rom The rom struct, after all randomization is complete
 * @param crc The rom checksum, for the header
 * @param path The file to write
 * @return A boolean indicating success
 */
BOOL dwr_write_spoiler(dw_rom *rom, uint64_t crc, const char *path);

#endif
