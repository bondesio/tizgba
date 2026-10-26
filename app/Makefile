# TizGBA native ARM application and static mGBA core.
CC := $(HOME)/tizen-studio/tools/arm-linux-gnueabi-gcc-9.2/bin/arm-tizen-linux-gnueabi-gcc-9.2.0
AR := $(HOME)/tizen-studio/tools/arm-linux-gnueabi-gcc-9.2/bin/arm-linux-gnueabi-ar
TIZEN := $(HOME)/tizen-studio/tools/ide/bin/tizen
LIBGCC := $(shell $(CC) -print-libgcc-file-name)

VERSION := 0.2.0
PACKAGE := org.pablo.tizgba-$(VERSION)-arm.tpk
EXE := tizgba

MGBAROOT := ../vendor/mGBA
TV_LIB_DIR ?= build/tv-libs
TV_LIBS := libcapi-appfw-application.so.0 libelementary.so.1 libevas.so.1 \
           libecore.so.1 libecore_input.so.1 libeina.so.1 \
           libcapi-media-audio-io.so.0 libstorage.so.0.1 libm.so.6 libc.so.6

INCLUDES := -I include -I $(MGBAROOT)/include -I $(MGBAROOT)/src
HEADERS := $(shell find include $(MGBAROOT)/include -type f -name '*.h' -print) perf_overlay.h
BASE_CFLAGS := -nostdinc $(INCLUDES) -std=gnu11 -O2 -fPIE \
               -fno-strict-aliasing -ffunction-sections -fdata-sections \
               -Wall -Wno-unused-function -Wno-unused-variable \
               -Wno-unused-but-set-variable
CORE_DEFINES := -DM_CORE_GBA -DMINIMAL_CORE=2 -DDISABLE_THREADING \
                -DHAVE_LOCALTIME_R -DHAVE_STRDUP
CORE_CFLAGS := $(BASE_CFLAGS) $(CORE_DEFINES)
APP_CFLAGS := $(BASE_CFLAGS)

# Keep this list in sync with vendor/mGBA/src/{arm,core,gba,util}/CMakeLists.txt.
# It omits other platforms, GB emulation, debug frontends, scripting, GL, PNG,
# zlib, video logging and threading. `--gc-sections` at final link removes
# unused function sections while preserving the GBA core's function table.
CORE_SOURCES := \
  arm/arm.c arm/decoder-arm.c arm/decoder.c arm/decoder-thumb.c arm/isa-arm.c arm/isa-thumb.c \
  gb/audio.c \
  gba/audio.c gba/bios.c gba/cart/ereader.c gba/cart/gpio.c gba/cart/matrix.c gba/cart/vfame.c \
  gba/cheats.c gba/cheats/codebreaker.c gba/cheats/gameshark.c gba/cheats/parv3.c \
  gba/core.c gba/dma.c gba/gba.c gba/hle-bios.c gba/input.c gba/io.c gba/memory.c gba/overrides.c \
  gba/renderers/cache-set.c gba/renderers/common.c gba/renderers/software-bg.c \
  gba/renderers/software-mode0.c gba/renderers/software-obj.c gba/renderers/video-software.c \
  gba/savedata.c gba/serialize.c gba/sio.c gba/sio/gbp.c gba/sio/joybus.c gba/timer.c gba/video.c \
  core/bitmap-cache.c core/cache-set.c core/cheats.c core/config.c core/core.c \
  core/interface.c core/log.c core/map-cache.c core/serialize.c core/sync.c \
  core/tile-cache.c core/timing.c core/version.c \
  util/circle-buffer.c util/configuration.c util/crc32.c util/formatting.c \
  util/gbk-table.c util/hash.c util/memory.c util/patch.c util/patch-ips.c \
  util/patch-ups.c util/string.c util/table.c util/vfs.c \
  util/vfs/vfs-mem.c \
  third-party/blip_buf/blip_buf.c third-party/inih/ini.c
CORE_OBJECTS := $(addprefix build/mgba/,$(CORE_SOURCES:.c=.o))

.PHONY: all core package refresh-tv-libs clean
all: package
core: build/libmgba-tizgba.a

build/tv-libs/.ready: Makefile
	mkdir -p $(TV_LIB_DIR)
	ssh q60a-tv-root 'tar -chf - $(addprefix /usr/lib/,$(TV_LIBS)) 2>/dev/null' | tar -xf - -C $(TV_LIB_DIR) --strip-components=2
	touch $@

build/mgba/%.o: $(MGBAROOT)/src/%.c $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) -c $< -o $@

build/libmgba-tizgba.a: $(CORE_OBJECTS)
	mkdir -p build
	$(AR) rcs $@ $^

APP_OBJECTS := build/app/start.o build/app/port/startup.o build/app/gba_main.o \
              build/app/library.o build/app/filters.o build/app/platform.o \
              build/app/port/tiz_port.o

build/app/%.o: %.c $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(APP_CFLAGS) -c $< -o $@

build/app/platform.o: platform.c platform.h $(HEADERS)
	mkdir -p $(dir $@)
	$(CC) $(CORE_CFLAGS) -c $< -o $@

build/app/start.o: start.S
	mkdir -p $(dir $@)
	$(CC) -nostdinc -c $< -o $@

bin/$(EXE): $(APP_OBJECTS) build/libmgba-tizgba.a build/tv-libs/.ready
	mkdir -p bin
	$(CC) -nostdlib -pie -Wl,-e,_start -Wl,--dynamic-linker=/lib/ld-linux.so.3 \
	  -Wl,--allow-shlib-undefined -Wl,--gc-sections -o $@ $(APP_OBJECTS) \
	  build/libmgba-tizgba.a $(addprefix $(TV_LIB_DIR)/,$(TV_LIBS)) $(LIBGCC)

out/$(PACKAGE): bin/$(EXE) tizen-manifest.xml shared/res/icon.png
	rm -rf stage
	mkdir -p stage/bin stage/shared/res out
	cp bin/$(EXE) stage/bin/$(EXE)
	cp tizen-manifest.xml stage/tizen-manifest.xml
	cp shared/res/icon.png stage/shared/res/icon.png
	rm -f $@
	cd stage && zip -q -r ../$@ bin shared tizen-manifest.xml
	$(TIZEN) package -t tpk -s tvdev -- $@

package: out/$(PACKAGE)

refresh-tv-libs:
	python3 -c 'from pathlib import Path; import shutil; p=Path("build/tv-libs"); shutil.rmtree(p, ignore_errors=True)'
	$(MAKE) build/tv-libs/.ready

clean:
	python3 -c 'from pathlib import Path; import shutil; [shutil.rmtree(Path(p), ignore_errors=True) for p in ("build", "bin", "stage", "out")]'
