#include "storage.h"
#include <string.h>

static unsigned rd16(const uint8_t *p) { return p[0] | (unsigned)p[1] << 8; }

static void wr16(uint8_t *p, unsigned v) {
  p[0] = (uint8_t)v;
  p[1] = (uint8_t)(v >> 8);
}

/* Walks the records. Returns the end marker offset or -1; *found / *found_size
 * describe the record named full_name (if full_name is not NULL). */
static long walk(const storage_t *s, const char *full_name, long *found, unsigned *found_size) {
  size_t name_len = full_name ? strlen(full_name) : 0;
  long o = 0;
  if (found) *found = -1;
  for (;;) {
    if ((size_t)o + 2 > s->size) return -1;
    unsigned size = rd16(s->buffer + o);
    if (size == 0) return o;
    if (size < 3 || (size_t)o + size + 2 > s->size) return -1;
    const uint8_t *name = s->buffer + o + 2;
    if (!memchr(name, 0, size - 2)) return -1; /* a full name always ends inside the record */
    if (full_name && found && *found < 0 && size >= name_len + 3 && memcmp(name, full_name, name_len + 1) == 0) {
      *found = o;
      *found_size = size;
    }
    o += size;
  }
}

long storage_used(const storage_t *s) { return walk(s, 0, 0, 0); }

const uint8_t *storage_find(const storage_t *s, const char *full_name, size_t *value_size) {
  long found;
  unsigned size;
  if (walk(s, full_name, &found, &size) < 0 || found < 0) return 0;
  size_t header = 2 + strlen(full_name) + 1;
  *value_size = size - header;
  return s->buffer + found + header;
}

storage_result_t storage_write(storage_t *s, const char *full_name, const void *value, size_t size) {
  long found;
  unsigned found_size = 0;
  long end = walk(s, full_name, &found, &found_size);
  if (end < 0) return STORAGE_CORRUPT;
  size_t header = 2 + strlen(full_name) + 1;
  size_t record = header + size;
  if (record > 0xFFFF) return STORAGE_FULL;
  long at;
  if (found >= 0 && found_size == record) {
    memcpy(s->buffer + found + header, value, size); /* in place */
    return STORAGE_OK;
  }
  if (found >= 0 && found + (long)found_size != end) return STORAGE_MOVED;
  at = found >= 0 ? found : end; /* last record, or a new one at the end */
  if ((size_t)at + record + 2 > s->size) return STORAGE_FULL;
  wr16(s->buffer + at, (unsigned)record);
  memcpy(s->buffer + at + 2, full_name, header - 2);
  memcpy(s->buffer + at + header, value, size);
  wr16(s->buffer + at + record, 0);
  return STORAGE_OK;
}
