# Pet Simulator - NumWorks Edition
#
#   make            -> output/petsim.nwa  (calculator app, needs arm-none-eabi-gcc + Node.js)
#   make run        -> installs the app on a calculator plugged in USB
#   make host       -> output/petsim-host (PC build, for tests and screenshots)
#   make screens    -> PNG screenshots of every screen in output/screens/
#   make web        -> output/web/petsim.html (the game in a browser, needs clang + wasi-libc)
#   make test       -> unit tests on PC
#   make balance    -> a bot plays the whole game: time per zone (output/balance.txt)
#   make assets     -> regenerates src/generated/ from data/, assets/ and tools/ (needs Python 3 + Pillow)
#   make wiki       -> downloads data/ and the images again from the PS99 wiki (needs network)

Q ?= @
BUILD_DIR = output
NWLINK = npx --yes -- nwlink@1.0.0
CC = arm-none-eabi-gcc
STRIP = arm-none-eabi-strip
HOST_CC ?= gcc

COMMON_SRC = $(filter-out src/platform/%,$(wildcard src/*.c src/*/*.c))
DEVICE_SRC = $(COMMON_SRC) src/platform/platform_eadk.c
HOST_SRC = $(COMMON_SRC) src/platform/platform_host.c

# --- Calculator build (same flags as NumWorks' official sample app) ---

EADK_CFLAGS := $(shell $(NWLINK) eadk-cflags-device 2>/dev/null)
CFLAGS = -std=c11 -Os -Wall -Wextra -Wno-unused-parameter $(EADK_CFLAGS)
CFLAGS += -fdata-sections -ffunction-sections
CFLAGS += -flto -fno-fat-lto-objects -fwhole-program -fvisibility=internal
LDFLAGS = -Wl,--relocatable -nostartfiles --specs=nano.specs
LDFLAGS += -Wl,-e,main -Wl,-u,eadk_app_name -Wl,-u,eadk_app_icon -Wl,-u,eadk_api_level
LDFLAGS += -Wl,--gc-sections -flinker-output=nolto-rel

DEVICE_OBJ = $(addprefix $(BUILD_DIR)/device/,$(DEVICE_SRC:.c=.o))

.PHONY: build
build: $(BUILD_DIR)/petsim.nwa

$(BUILD_DIR)/petsim.nwa: $(DEVICE_OBJ) $(BUILD_DIR)/device/icon.o
	@echo "LD      $@"
	$(Q) $(CC) $(CFLAGS) $(LDFLAGS) $^ -lm -o $@
	$(Q) $(STRIP) --strip-debug --strip-unneeded $@  # same app, smaller file (names and debug info)

$(BUILD_DIR)/device/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC      $<"
	$(Q) $(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# recompile an object when a header it includes changes
-include $(DEVICE_OBJ:.o=.d)

$(BUILD_DIR)/device/icon.o: assets/icon.png
	@mkdir -p $(dir $@)
	@echo "ICON    $<"
	$(Q) $(NWLINK) png-icon-o $< $@

.PHONY: run
run: $(BUILD_DIR)/petsim.nwa
	$(Q) $(NWLINK) install-nwa $<

.PHONY: check
check: $(BUILD_DIR)/petsim.bin

$(BUILD_DIR)/petsim.bin: $(BUILD_DIR)/petsim.nwa
	@echo "BIN     $@"
	$(Q) $(NWLINK) nwa-bin $< $@

# --- PC build ---

HOST_CFLAGS = -std=c11 -O2 -g -Wall -Wextra -Wno-unused-parameter

.PHONY: host
host: $(BUILD_DIR)/petsim-host

$(BUILD_DIR)/petsim-host: $(HOST_SRC) $(wildcard src/*.h src/*/*.h)
	@mkdir -p $(BUILD_DIR)
	@echo "HOSTCC  $@"
	$(Q) $(HOST_CC) $(HOST_CFLAGS) $(HOST_SRC) -lm -o $@

SCREENS_SCRIPT = SHOT:00_title OK W2 SHOT:01_farm OK OK OK W2 SHOT:02_farm_hit \
  3 W2 SHOT:03_eggs OK W10 SHOT:04_hatch_shake W24 SHOT:05_hatch_crack W30 SHOT:06_hatch_reveal \
  BACK 2 W2 SHOT:07_pets 1 W90 SHOT:08_farm_team D SHOT:09_tabs_focus R R R R SHOT:10_upgrades
HATCH_20 = $(foreach i,1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20,OK OK)
# Later game (--rich: starts with 20M coins): zones 2-9, then many eggs.
SCREENS_RICH = OK 4 D OK D OK D OK D OK D OK D OK D OK D OK D D W2 SHOT:11_zones \
  3 $(HATCH_20) W60 SHOT:12_hatch_rich BACK U U U U $(HATCH_20) BACK U U U $(HATCH_20) BACK \
  W60 SHOT:13_eggs_list 2 TOOLBOX W2 SHOT:14_pets_rich 1 W90 SHOT:15_farm_rich \
  6 R R R D SHOT:16_index \
  5 OK OK OK W60 SHOT:17_upgrades R R D OK OK W60 SHOT:18_upgrades_eggs \
  3 OK W14 SHOT:19_hatch3_shake W80 SHOT:20_hatch3_reveal BACK \
  2 U R R R OK W2 SHOT:21_recycle OK W2 SHOT:22_recycled L L OK W2 SHOT:23_pets_filter \
  7 W2 SHOT:24_quests 8 W2 SHOT:25_potions OK W2 SHOT:26_potion_used TOOLBOX R OK W2 SHOT:27_enchants \
  4 U U OK 1 W2 R R R D OK W2 SHOT:28_golden_machine
# A huge from the first egg (--huge)
SCREENS_HUGE = OK 3 OK W20 SHOT:30_huge_shake W60 SHOT:31_huge_coming W60 SHOT:32_huge_reveal

.PHONY: screens
screens: $(BUILD_DIR)/petsim-host
	@mkdir -p $(BUILD_DIR)/screens
	$(Q) $(BUILD_DIR)/petsim-host --out $(BUILD_DIR)/screens "$(SCREENS_SCRIPT)"
	$(Q) $(BUILD_DIR)/petsim-host --out $(BUILD_DIR)/screens --rich "$(SCREENS_RICH)"
	$(Q) $(BUILD_DIR)/petsim-host --out $(BUILD_DIR)/screens --huge "$(SCREENS_HUGE)"
	$(Q) python3 tools/ppm_to_png.py $(BUILD_DIR)/screens

# --- Web build (WebAssembly, the page is web/index.html) ---

WEB_CC ?= clang
WEB_SRC = $(COMMON_SRC) src/platform/platform_web.c
WEB_CFLAGS = --target=wasm32-wasi -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter
WEB_LDFLAGS = -nostartfiles -Wl,--no-entry -Wl,-z,stack-size=262144 -Wl,--initial-memory=8388608

.PHONY: web
web: $(BUILD_DIR)/web/petsim.html

$(BUILD_DIR)/web/petsim.wasm: $(WEB_SRC) $(wildcard src/*.h src/*/*.h)
	@mkdir -p $(dir $@)
	@echo "WASM    $@"
	$(Q) $(WEB_CC) $(WEB_CFLAGS) $(WEB_LDFLAGS) $(WEB_SRC) -lm -o $@

$(BUILD_DIR)/web/petsim.html: $(BUILD_DIR)/web/petsim.wasm web/index.html tools/build_web.py
	$(Q) python3 tools/build_web.py $< web/index.html $@

# --- Tests ---

GAME_SRC = $(wildcard src/game/*.c) src/generated/content.c

.PHONY: test
test:
	@mkdir -p $(BUILD_DIR)/tests
	$(Q) $(HOST_CC) $(HOST_CFLAGS) tests/test_numfmt.c src/game/numfmt.c -o $(BUILD_DIR)/tests/test_numfmt
	$(Q) $(BUILD_DIR)/tests/test_numfmt
	$(Q) $(HOST_CC) $(HOST_CFLAGS) tests/test_eggs.c $(GAME_SRC) -lm -o $(BUILD_DIR)/tests/test_eggs
	$(Q) $(BUILD_DIR)/tests/test_eggs
	$(Q) $(HOST_CC) $(HOST_CFLAGS) tests/test_save.c src/save/save.c src/save/storage.c $(GAME_SRC) -lm -o $(BUILD_DIR)/tests/test_save
	$(Q) $(BUILD_DIR)/tests/test_save
	$(Q) $(HOST_CC) $(HOST_CFLAGS) tests/test_sprites.c src/gfx/inflate.c src/generated/content.c -o $(BUILD_DIR)/tests/test_sprites
	$(Q) $(BUILD_DIR)/tests/test_sprites

# --- Balance simulator (a bot plays the whole game, see tests/balance_sim.c) ---

.PHONY: balance calibrate
balance: $(BUILD_DIR)/balance_sim
	$(Q) $(BUILD_DIR)/balance_sim | tee $(BUILD_DIR)/balance.txt

$(BUILD_DIR)/balance_sim: tests/balance_sim.c $(GAME_SRC) $(wildcard src/game/*.h src/generated/*.h)
	@mkdir -p $(BUILD_DIR)
	$(Q) $(HOST_CC) -std=c11 -O2 -Wall tests/balance_sim.c $(GAME_SRC) -lm -o $@

# tunes the coins of each zone for a ~6 h game (data/balance.csv), then make balance
calibrate:
	python3 tools/calibrate_balance.py

# --- Generated sources (committed, so building does not need Python) ---

.PHONY: assets
assets:
	python3 tools/gen_font.py src/generated/fonts.c
	python3 tools/gen_placeholder_art.py
	python3 tools/convert_images.py assets/sprites.txt src/generated/sprites
	python3 tools/gen_data.py
	python3 tools/gen_icon.py assets/icon.png

.PHONY: wiki
wiki:
	python3 tools/fetch_wiki.py --eggs 50

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
