#include "zones.h"

const zone_t zones[] = {
  {"Spawn", RGB(120, 200, 255), RGB(150, 225, 140), RGB(60, 170, 80)},
  {"Colorful Forest", RGB(255, 170, 220), RGB(170, 230, 160), RGB(220, 80, 170)},
  {"Castle", RGB(170, 185, 230), RGB(200, 200, 210), RGB(110, 110, 160)},
};
const int zone_count = sizeof(zones) / sizeof(zones[0]);
