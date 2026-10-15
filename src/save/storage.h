#ifndef SAVE_STORAGE_H
#define SAVE_STORAGE_H

/* Records in Epsilon's storage (where the Python scripts live), read from the
 * Epsilon sources (shared/ion/include/ion/storage/file_system.h):
 *
 *   | magic | record | record | ... | 0x0000 | free space ... | magic |
 *   record: | size (uint16 LE, whole record) | full name "petsim.py" \0 | value |
 *
 * EADK has no storage API: we write into Epsilon's RAM directly. To never break
 * Epsilon (it caches the address of the last record it used), existing records
 * are never moved: a record is either rewritten in place (same size), or it is
 * the last one, or it is appended after the last one. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STORAGE_MAGIC 0xEE0BDDBAu

typedef struct {
  uint8_t *buffer; /* first record (just after the magic header) */
  size_t size;     /* size of the record area */
} storage_t;

typedef enum {
  STORAGE_OK,
  STORAGE_CORRUPT,  /* the record chain does not make sense: never write */
  STORAGE_FULL,     /* not enough free space */
  STORAGE_MOVED,    /* the record exists with another size and is not the last one */
} storage_result_t;

/* Offset of the end marker (= bytes used by the records), or -1 if corrupt. */
long storage_used(const storage_t *s);
/* Value of the record, or NULL. */
const uint8_t *storage_find(const storage_t *s, const char *full_name, size_t *value_size);
storage_result_t storage_write(storage_t *s, const char *full_name, const void *value, size_t size);

#endif
