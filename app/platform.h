/* TizGBA's single-threaded mGBA 0.10.5 bridge. No EFL or audio-device API
 * leaks through this interface. Compile the adapter and core with identical
 * mGBA feature macros (MINIMAL_CORE=2, DISABLE_THREADING, 32-bit color_t).
 */
#ifndef TIZGBA_PLATFORM_H
#define TIZGBA_PLATFORM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBAP_WIDTH 240
#define GBAP_HEIGHT 160
#define GBAP_AUDIO_RATE 32768

/* Active-high GBA KEYINPUT bit positions, including R before L. */
enum gbap_key {
    GBAP_KEY_A      = 1u << 0,
    GBAP_KEY_B      = 1u << 1,
    GBAP_KEY_SELECT = 1u << 2,
    GBAP_KEY_START  = 1u << 3,
    GBAP_KEY_RIGHT  = 1u << 4,
    GBAP_KEY_LEFT   = 1u << 5,
    GBAP_KEY_UP     = 1u << 6,
    GBAP_KEY_DOWN   = 1u << 7,
    GBAP_KEY_R      = 1u << 8,
    GBAP_KEY_L      = 1u << 9,
    GBAP_KEY_MASK   = 0x3ffu
};

typedef struct gbap_runtime gbap_runtime;

/* Allocate an unloaded runtime; NULL means allocation failed. All functions
 * on a runtime must run on the same thread. No core starts until load(). */
gbap_runtime *gbap_create(void);

/* Destroy the core and its VFiles/buffers; NULL is harmless. Does NOT save:
 * call flush_save() and handle failure before destroying a loaded runtime. */
void gbap_destroy(gbap_runtime *runtime);

/* Load one cartridge into an unloaded runtime. The ROM (288 bytes..32 MiB)
 * is read completely, validated by mGBA and retained in RAM. save_directory
 * must be an absolute, existing internal-storage directory. The adapter hashes
 * the exact loaded ROM bytes with SHA-256 and stores <hash>.sav there. An
 * existing save always wins over import_path; the optional import_path is read
 * only if that content-keyed save is absent.
 * Supported raw save lengths are 512, 8192, 32768, 65536 and 131072 bytes,
 * optionally followed by mGBA's 16-byte RTC trailer; empty saves are allowed.
 * Missing/invalid optional bios_path falls back to the built-in HLE BIOS,
 * with a warning. External BIOS must be exactly 16384 bytes and pass mGBA's
 * vector check. ROMs, BIOS and imported saves are never written; only the generated save in
 * save_directory is writable.
 * On failure all partial core state is freed and load() may be retried.
 * On success a second load() is refused; flush/destroy/create to change ROM.
 */
bool gbap_load(gbap_runtime *runtime, const char *rom_path,
               const char *save_directory, const char *import_path,
               const char *bios_path);

/* Explicit battery/RTC persistence: write a unique temporary file beside
 * save_path, fsync it, close it, rename atomically, then fsync its directory.
 * No battery data yet is a successful no-op. Errors preserve the old file
 * until rename; a directory-sync error means replacement happened but its
 * power-loss durability is unknown. The live runtime remains retryable.
 * This is the only function that writes to the filesystem.
 */
bool gbap_flush_save(gbap_runtime *runtime);

/* Replace the active-high button mask; unsupported bits are removed. */
void gbap_set_keys(gbap_runtime *runtime, uint32_t keys);

/* Run one core frame and convert it to opaque 0xFFRRGGBB for Evas/filters.
 * Drain all audio after every successful frame, including when muted.
 * Unread audio refuses the next frame, avoiding a silent audio overflow.
 * Returns false if unloaded, audio is pending, or the core did not advance.
 */
bool gbap_run_frame(gbap_runtime *runtime);

/* Borrow the tightly packed 240x160 ARGB8888 image (stride 240 pixels).
 * Valid until destroy(), updated by run_frame(); NULL when unloaded.
 */
const uint32_t *gbap_framebuffer(const gbap_runtime *runtime);

/* Drain up to capacity stereo FRAMES to interleaved native-endian S16 L,R
 * (2*capacity int16_t elements). Read both blip channels; return frame count.
 * Call repeatedly until zero. The stream is fixed at 32768 Hz and needs the
 * frontend's resampler/ring buffer. NULL output is allowed only at capacity 0.
 */
size_t gbap_drain_audio(gbap_runtime *runtime, int16_t *output, size_t capacity);

/* Frame duration in seconds from core frameCycles()/frequency(), or 0 when
 * unloaded. Use this for pacing rather than rounded 60 Hz constants. */
double gbap_frame_period(const gbap_runtime *runtime);

/* True only when load() successfully attached a validated external BIOS. */
bool gbap_has_bios(const gbap_runtime *runtime);

/* Last failed operation's concise error, empty after a successful mutating
 * operation. Strings are borrowed until the next operation or destroy().
 * NULL runtime returns a fixed error. Reading pixels/timing does not clear it.
 */
const char *gbap_error(const gbap_runtime *runtime);

/* Nonfatal BIOS fallback reason from the last load, or the empty string. */
const char *gbap_warning(const gbap_runtime *runtime);

#endif
