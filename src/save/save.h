#ifndef SAVE_SAVE_H
#define SAVE_SAVE_H

/* Game save in Epsilon's storage, as a Python script "petsim.py" (visible in the
 * Python app, so it can be backed up like any script). Its content is a comment:
 *
 *   # Pet Sim : sauvegarde (ne pas modifier)
 *   #<game state, binary + CRC32, in base64>
 *
 * The state is written again (in place) whenever it changed, at most every few
 * seconds, and when leaving the game. */

#include <stdbool.h>
#include <stdint.h>
#include "storage.h"

#define SAVE_NAME "petsim.py"
#define SAVE_VERSION 2
#define SAVE_TEXT_CAPACITY 1024 /* fixed record size, so it is always rewritten in place */

typedef enum {
  SAVE_NO_STORAGE,  /* Epsilon's storage not found: saving disabled */
  SAVE_NEW_GAME,    /* storage found, no save yet */
  SAVE_LOADED,      /* the game was restored */
  SAVE_UNREADABLE,  /* petsim.py exists but is damaged or edited: saving disabled */
  SAVE_TOO_NEW,     /* written by a newer version of the game: saving disabled */
} save_status_t;

/* Text of the save for the current game state (NUL-terminated, < SAVE_TEXT_CAPACITY). */
int save_encode(char *text);
/* Restores the game from a save text. Returns false (game untouched) if invalid;
 * *too_new is set when the version is newer than this game. */
bool save_decode(const char *text, size_t size, bool *too_new);

/* Finds the storage and loads the save. Call once after game_init(). */
save_status_t save_init(void);
save_status_t save_status(void);
/* Writes the game if it changed since the last write. Returns the result. */
storage_result_t save_now(void);
/* Calls save_now() at most every SAVE_PERIOD_MS. */
void save_tick(uint32_t now_ms);
/* Diagnostic: where the storage is and how full. */
bool save_storage_info(uint32_t *address, long *used, long *size);
storage_result_t save_last_result(void);

#endif
