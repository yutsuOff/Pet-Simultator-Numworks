#ifndef SCREENS_ADMIN_H
#define SCREENS_ADMIN_H

/* Hidden admin menu (code: 2 8 1 2 ↑ ↑ ↓ ↓ ← → ← → OK), drawn over the content. */
#include <stdbool.h>
#include <stdint.h>
#include "screens.h"

/* Follows the keys pressed; returns true (and opens the menu) when the code is complete. */
bool admin_check_code(const input_t *in);
bool admin_is_open(void);
int admin_update(const input_t *in, uint32_t now);
void admin_draw(void);

#endif
