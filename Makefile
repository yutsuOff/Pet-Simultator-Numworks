# Pet Simulator - NumWorks Edition
#
#   make            -> output/petsim.nwa  (calculator app, needs arm-none-eabi-gcc + Node.js)
#   make run        -> installs the app on a calculator plugged in USB
#   make host       -> output/petsim-host (PC build, for tests and screenshots)
#   make screens    -> PNG screenshots of every screen in output/screens/
#   make test       -> unit tests on PC
#   make assets     -> regenerates src/generated/ from tools/ and assets/ (needs Python 3 + Pillow)

Q ?= @
BUILD_DIR = output
NWLINK = npx --yes -- nwlink@1.0.0
CC = arm-none-eabi-gcc
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

$(BUILD_DIR)/device/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "CC      $<"
	$(Q) $(CC) $(CFLAGS) -c $< -o $@

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

SCREENS_SCRIPT = SHOT:00_title OK W2 SHOT:01_farm OK OK OK OK W2 SHOT:02_farm_hit \
  R R D SHOT:03_farm_select D SHOT:04_tabs_focus R SHOT:05_pets R SHOT:06_eggs R SHOT:07_zones \
  R SHOT:08_upgrades R SHOT:09_index R SHOT:10_quests

.PHONY: screens
screens: $(BUILD_DIR)/petsim-host
	@mkdir -p $(BUILD_DIR)/screens
	$(Q) $(BUILD_DIR)/petsim-host --out $(BUILD_DIR)/screens "$(SCREENS_SCRIPT)"
	$(Q) python3 tools/ppm_to_png.py $(BUILD_DIR)/screens

# --- Tests ---

.PHONY: test
test:
	@mkdir -p $(BUILD_DIR)/tests
	$(Q) $(HOST_CC) $(HOST_CFLAGS) tests/test_numfmt.c src/game/numfmt.c -o $(BUILD_DIR)/tests/test_numfmt
	$(Q) $(BUILD_DIR)/tests/test_numfmt

# --- Generated sources (committed, so building does not need Python) ---

.PHONY: assets
assets:
	python3 tools/gen_font.py src/generated/fonts.c
	python3 tools/gen_placeholder_art.py
	python3 tools/convert_images.py assets/sprites.txt src/generated/sprites
	python3 tools/gen_icon.py assets/icon.png

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
