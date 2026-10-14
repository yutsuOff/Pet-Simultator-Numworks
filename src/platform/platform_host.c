/* PC backend: renders into a memory framebuffer and replays a key script.
 *
 *   petsim-host [--out DIR] [--rich] "SCRIPT"
 *
 * SCRIPT is a space-separated list of tokens, one per frame:
 *   L R U D OK BACK HOME SHIFT TOOLBOX 0-9   press the key for one frame (then release one frame)
 *   S+R / S+L                                 shift + right/left
 *   W<n>                                       wait n frames
 *   SHOT:name                                  save DIR/name.ppm
 * Time is simulated (33 ms per frame) so runs are reproducible. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform.h"

#define FRAME_MS 33

static uint16_t framebuffer[SCREEN_W * SCREEN_H];
static uint32_t now_ms;
static uint32_t rng_state = 0x12345678;
static char **tokens;
static int token_count, token_index;
static int wait_frames;
static uint64_t current_keys;
static bool release_frame;
static const char *out_dir = "output/screens";
static bool rich;

extern int app_main(void);

static void save_ppm(const char *name) {
  char path[512];
  snprintf(path, sizeof(path), "%s/%s.ppm", out_dir, name);
  FILE *f = fopen(path, "wb");
  if (!f) {
    fprintf(stderr, "cannot write %s\n", path);
    return;
  }
  fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_H);
  for (int i = 0; i < SCREEN_W * SCREEN_H; i++) {
    uint16_t c = framebuffer[i];
    unsigned char rgb[3] = {(unsigned char)(((c >> 11) & 31) * 255 / 31),
                            (unsigned char)(((c >> 5) & 63) * 255 / 63),
                            (unsigned char)((c & 31) * 255 / 31)};
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
  printf("screenshot %s\n", path);
}

static uint64_t key_for_token(const char *t) {
  static const struct { const char *name; int key; } names[] = {
    {"L", KEY_LEFT}, {"R", KEY_RIGHT}, {"U", KEY_UP}, {"D", KEY_DOWN},
    {"OK", KEY_OK}, {"EXE", KEY_EXE}, {"BACK", KEY_BACK}, {"HOME", KEY_HOME},
    {"SHIFT", KEY_SHIFT}, {"TOOLBOX", KEY_TOOLBOX},
    {"0", KEY_0}, {"1", KEY_1}, {"2", KEY_2}, {"3", KEY_3}, {"4", KEY_4},
    {"5", KEY_5}, {"6", KEY_6}, {"7", KEY_7}, {"8", KEY_8}, {"9", KEY_9},
  };
  if (strncmp(t, "S+", 2) == 0) {
    return (1ULL << KEY_SHIFT) | key_for_token(t + 2);
  }
  for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
    if (strcmp(t, names[i].name) == 0) {
      return 1ULL << names[i].key;
    }
  }
  fprintf(stderr, "unknown script token '%s'\n", t);
  exit(2);
}

void platform_init(void) {}

void platform_push_rect(int x, int y, int w, int h, const uint16_t *pixels) {
  for (int j = 0; j < h; j++) {
    memcpy(&framebuffer[(y + j) * SCREEN_W + x], &pixels[j * w], (size_t)w * sizeof(uint16_t));
  }
}

uint64_t platform_keys(void) { return current_keys; }

bool platform_debug_rich(void) { return rich; }

uint32_t platform_millis(void) { return now_ms; }

uint32_t platform_random(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

bool platform_end_frame(void) {
  now_ms += FRAME_MS;
  if (release_frame) {
    current_keys = 0;
    release_frame = false;
    return true;
  }
  if (wait_frames > 0) {
    wait_frames--;
    return true;
  }
  while (token_index < token_count) {
    const char *t = tokens[token_index++];
    if (strncmp(t, "SHOT:", 5) == 0) {
      save_ppm(t + 5);
      continue;
    }
    if (t[0] == 'W' && t[1] >= '0' && t[1] <= '9') {
      wait_frames = atoi(t + 1);
      return true;
    }
    current_keys = key_for_token(t);
    release_frame = true;
    return true;
  }
  return false;
}

int main(int argc, char **argv) {
  static char *storage[4096];
  int i = 1;
  for (; i < argc && strncmp(argv[i], "--", 2) == 0; i++) {
    if (strcmp(argv[i], "--rich") == 0) rich = true;
    else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) out_dir = argv[++i];
  }
  for (; i < argc; i++) {
    char *copy = strdup(argv[i]);
    for (char *t = strtok(copy, " \n\t"); t && token_count < 4096; t = strtok(NULL, " \n\t")) {
      storage[token_count++] = t;
    }
  }
  tokens = storage;
  return app_main();
}
