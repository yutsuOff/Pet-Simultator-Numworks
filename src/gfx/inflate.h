#ifndef GFX_INFLATE_H
#define GFX_INFLATE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Decodes raw DEFLATE data (no zlib header). Returns the bytes written, or -1 if the data is
 * damaged or does not fit in out_size. */
int inflate_raw(const uint8_t *in, size_t in_size, uint8_t *out, size_t out_size);

#endif
