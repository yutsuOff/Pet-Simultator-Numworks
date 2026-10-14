#ifndef GAME_ZONES_H
#define GAME_ZONES_H

#include "../gfx/gfx.h"

/* Provisional zone list for the M0 skeleton: the full 50 zones will be generated
 * from data/zones.csv once the content is validated. */
typedef struct {
  const char *name;
  color_t sky_top, sky_bottom; /* background gradient */
  color_t accent;              /* zone badge in the top bar */
} zone_t;

extern const zone_t zones[];
extern const int zone_count;

#endif
