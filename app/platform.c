/* TizGBA runtime adapter. Filesystem I/O is confined to checked load and
 * explicit save flush; mGBA sees only memory-backed VFiles. No TV calls.
 */
#include "platform.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/core.h>
#include <mgba/gba/core.h>
#include <mgba/gba/interface.h>
#include <mgba/internal/gba/gba.h>
#include <mgba/internal/gba/savedata.h>
#include <mgba-util/vfs.h>

#ifdef COLOR_16_BIT
#error "TizGBA requires mGBA's default 32-bit XBGR color_t"
#endif

/* The target uses hand-written libc headers. Keep declarations here until
 * the build owner's symbol/ABI audit promotes them to the shared shims. */
extern int open(const char *, int, ...);
extern ssize_t read(int, void *, size_t);
extern ssize_t write(int, const void *, size_t);
extern off_t lseek(int, off_t, int);
extern int close(int);
extern int fsync(int);
extern int mkstemp(char *);
extern int rename(const char *, const char *);
extern int unlink(const char *);

#define GBAP_PATH_MAX 1024
#define GBAP_ROM_MAX (32u * 1024u * 1024u)
#define GBAP_BIOS_SIZE 16384u
#define GBAP_SAVE_MAX (131072u + 16u)

static uint32_t rotr32(uint32_t v, unsigned n) { return (v >> n) | (v << (32u - n)); }

static void sha256_block(uint32_t h[8], const unsigned char block[64])
{
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[i*4] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | block[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t x = w[i-15], y = w[i-2];
        w[i] = w[i-16] + (rotr32(x,7)^rotr32(x,18)^(x>>3)) + w[i-7] +
               (rotr32(y,17)^rotr32(y,19)^(y>>10));
    }
    uint32_t a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t t1=z+(rotr32(e,6)^rotr32(e,11)^rotr32(e,25))+((e&f)^((~e)&g))+k[i]+w[i];
        uint32_t t2=(rotr32(a,2)^rotr32(a,13)^rotr32(a,22))+((a&b)^(a&c)^(b&c));
        z=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=z;
}

static bool rom_save_path(const unsigned char *rom, size_t size, const char *directory,
                          char out[GBAP_PATH_MAX])
{
    uint32_t h[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                   0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    unsigned char block[64];
    size_t used = 0;
    for (size_t i = 0; i < size; ++i) {
        block[used++] = rom[i];
        if (used == sizeof block) { sha256_block(h, block); used = 0; }
    }
    block[used++] = 0x80;
    if (used > 56) { memset(block + used, 0, sizeof block - used); sha256_block(h, block); used = 0; }
    memset(block + used, 0, 56 - used);
    uint64_t bits = (uint64_t)size * 8u;
    for (int i = 0; i < 8; ++i) block[63-i] = (unsigned char)(bits >> (i*8));
    sha256_block(h, block);
    char hex[65];
    for (int i = 0; i < 8; ++i) snprintf(hex + i*8, 9, "%08x", h[i]);
    size_t n = strlen(directory);
    int written = snprintf(out, GBAP_PATH_MAX, "%s%s%s.sav", directory,
                           n && directory[n-1] == '/' ? "" : "/", hex);
    return written > 0 && written < GBAP_PATH_MAX;
}

/* Unlike a growable VFileMemChunk, this save VFile never reallocates while
 * SRAM/Flash/EEPROM is mapped. Size remains logical, so mGBA can detect and
 * grow the cartridge's save type without a fake 128 KiB input file. */
struct gbap_save {
    struct VFile vf;
    size_t size, offset;
    bool failed;
    _Alignas(8) unsigned char data[GBAP_SAVE_MAX];
};

struct gbap_runtime {
    struct mCore *core;
    struct gbap_save *save;       /* owned by core after loadSave succeeds */
    unsigned char *rom, *bios;    /* VFileFromMemory does not free backing RAM */
    color_t video[GBAP_WIDTH * GBAP_HEIGHT];
    uint32_t argb[GBAP_WIDTH * GBAP_HEIGHT];
    char save_path[GBAP_PATH_MAX];
    char error[128], warning[128];
    bool loaded, has_bios;
};

static bool fail(gbap_runtime *p, const char *message)
{
    if (p) {
        size_t n = strlen(message);
        if (n >= sizeof(p->error)) n = sizeof(p->error) - 1;
        memcpy(p->error, message, n);
        p->error[n] = 0;
    }
    return false;
}

static bool save_close(struct VFile *vf)
{
    free(vf);
    return true;
}

static off_t save_seek(struct VFile *vf, off_t offset, int whence)
{
    struct gbap_save *s = (struct gbap_save *)vf;
    off_t base;
    if (whence == SEEK_SET) base = 0;
    else if (whence == SEEK_CUR) base = (off_t)s->offset;
    else if (whence == SEEK_END) base = (off_t)s->size;
    else return -1;
    /* Compare before addition, including when offset is LONG_MIN. */
    if (offset < -base || offset > (off_t)GBAP_SAVE_MAX - base) return -1;
    s->offset = (size_t)(base + offset);
    return (off_t)s->offset;
}

static ssize_t save_read(struct VFile *vf, void *data, size_t size)
{
    struct gbap_save *s = (struct gbap_save *)vf;
    if (s->offset >= s->size) return 0;
    if (size > s->size - s->offset) size = s->size - s->offset;
    memcpy(data, s->data + s->offset, size);
    s->offset += size;
    return (ssize_t)size;
}

static ssize_t save_write(struct VFile *vf, const void *data, size_t size)
{
    struct gbap_save *s = (struct gbap_save *)vf;
    if (size > GBAP_SAVE_MAX - s->offset) {
        s->failed = true;
        return -1;
    }
    if (s->offset > s->size) memset(s->data + s->size, 0xff, s->offset - s->size);
    memcpy(s->data + s->offset, data, size);
    s->offset += size;
    if (s->offset > s->size) s->size = s->offset;
    return (ssize_t)size;
}

static void *save_map(struct VFile *vf, size_t size, int flags)
{
    struct gbap_save *s = (struct gbap_save *)vf;
    (void)flags;
    if (size > s->size) {
        s->failed = true;
        return NULL;
    }
    return s->data;
}

static void save_unmap(struct VFile *vf, void *data, size_t size)
{
    (void)vf; (void)data; (void)size;
}

static void save_truncate(struct VFile *vf, size_t size)
{
    struct gbap_save *s = (struct gbap_save *)vf;
    if (size > GBAP_SAVE_MAX) {
        s->failed = true;
        return;
    }
    if (size > s->size) memset(s->data + s->size, 0xff, size - s->size);
    s->size = size;
}

static ssize_t save_size(struct VFile *vf)
{
    return (ssize_t)((struct gbap_save *)vf)->size;
}

static bool save_sync(struct VFile *vf, void *data, size_t size)
{
    (void)data; (void)size;
    /* Core housekeeping only commits to RAM; the frontend chooses when to
     * call gbap_flush_save(), whose disk failures are visible to it. */
    return !((struct gbap_save *)vf)->failed;
}

static struct gbap_save *save_create(void)
{
    struct gbap_save *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    memset(s->data, 0xff, sizeof(s->data));
    s->vf.close = save_close;
    s->vf.seek = save_seek;
    s->vf.read = save_read;
    s->vf.readline = VFileReadline;
    s->vf.write = save_write;
    s->vf.map = save_map;
    s->vf.unmap = save_unmap;
    s->vf.truncate = save_truncate;
    s->vf.size = save_size;
    s->vf.sync = save_sync;
    return s;
}

enum file_result { FILE_ERROR = -1, FILE_MISSING = 0, FILE_OK = 1 };

/* Complete reads plus an EOF/size recheck reject removal, truncation and
 * growth during load. No stat struct ABI or filesystem VFS backend needed. */
static enum file_result read_file(const char *path, size_t maximum,
                                 unsigned char **data, size_t *size)
{
    int fd;
    off_t length;
    size_t done = 0;
    unsigned char *buffer, extra;
    ssize_t n;
    *data = NULL;
    *size = 0;
    do { fd = open(path, O_RDONLY); } while (fd < 0 && errno == EINTR);
    if (fd < 0) return errno == ENOENT ? FILE_MISSING : FILE_ERROR;
    length = lseek(fd, 0, SEEK_END);
    if (length < 0 || (size_t)length > maximum || lseek(fd, 0, SEEK_SET) != 0) {
        close(fd);
        return FILE_ERROR;
    }
    buffer = malloc(length ? (size_t)length : 1u);
    if (!buffer) { close(fd); return FILE_ERROR; }
    while (done < (size_t)length) {
        n = read(fd, buffer + done, (size_t)length - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) goto error;
        done += (size_t)n;
    }
    do { n = read(fd, &extra, 1); } while (n < 0 && errno == EINTR);
    if (n != 0 || lseek(fd, 0, SEEK_END) != length) goto error;
    if (close(fd) != 0) { free(buffer); return FILE_ERROR; }
    *data = buffer;
    *size = done;
    return FILE_OK;
error:
    close(fd);
    free(buffer);
    return FILE_ERROR;
}

static bool valid_save_size(size_t size)
{
    if (!size) return true;
    if ((size & 0xffu) == 16u) size -= 16u;
    return size == 512u || size == 8192u || size == 32768u ||
           size == 65536u || size == 131072u;
}

static void release_core(gbap_runtime *p)
{
    if (p->core) {
        /* deinit frees the enclosing core, so config must go first. */
        mCoreConfigDeinit(&p->core->config);
        p->core->deinit(p->core);
        p->core = NULL;
    }
    p->save = NULL;
    free(p->rom); p->rom = NULL;
    free(p->bios); p->bios = NULL;
    p->loaded = false;
    p->has_bios = false;
}

gbap_runtime *gbap_create(void)
{
    return calloc(1, sizeof(gbap_runtime));
}

void gbap_destroy(gbap_runtime *p)
{
    if (!p) return;
    release_core(p);
    free(p);
}

bool gbap_load(gbap_runtime *p, const char *rom_path, const char *save_directory,
               const char *import_path, const char *bios_path)
{
    struct mCore *core;
    struct VFile *vf = NULL;
    unsigned char *input = NULL;
    size_t size;
    enum file_result result;
    if (!p) return false;
    p->error[0] = 0;
    if (p->loaded) return fail(p, "cartridge already loaded");
    p->warning[0] = 0;
    if (!rom_path || !*rom_path || !save_directory || save_directory[0] != '/' ||
        !save_directory[1] || strlen(save_directory) >= sizeof(p->save_path) ||
        save_directory[strlen(save_directory) - 1] == '/')
        return fail(p, "invalid ROM/save path");

    result = read_file(rom_path, GBAP_ROM_MAX, &p->rom, &size);
    /* mGBA override detection reads through 0x11F even for a nominally
     * readable 192-byte cartridge header. Reject smaller data before mapping. */
    if (result != FILE_OK || size < 0x120u) {
        fail(p, "ROM missing, unreadable or invalid size");
        goto error;
    }
    if (!rom_save_path(p->rom, size, save_directory, p->save_path) ||
        !strcmp(rom_path, p->save_path) || (bios_path && !strcmp(bios_path, p->save_path))) {
        fail(p, "could not create content-keyed save path");
        goto error;
    }
    vf = VFileFromMemory(p->rom, size);
    if (!vf) { fail(p, "ROM VFile allocation failed"); goto error; }
    if (!GBAIsROM(vf)) { fail(p, "invalid GBA cartridge"); goto error; }
    if (GBAIsMB(vf)) { fail(p, "multiboot ROM is not a GBA cartridge"); goto error; }

    /* Pinned GBACoreCreate itself assumes its malloc succeeds; callers
     * cannot recover from that upstream OOM bug without a vendor fix. */
    core = GBACoreCreate();
    if (!core) { fail(p, "core allocation failed"); goto error; }
    if (!core->init(core)) {
        /* A failure before board setup leaves these null; later failures
         * (including audio-buffer OOM) can be safely deinitialized. */
        if (core->cpu && core->board) core->deinit(core);
        else free(core);
        fail(p, "core initialization failed");
        goto error;
    }
    p->core = core;
    mCoreInitConfig(core, "tizgba");
    if (!core->config.port) { fail(p, "core config allocation failed"); goto error; }
    core->opts.useBios = false;  /* no config-directory/filesystem probing */
    core->opts.skipBios = true;
    core->opts.volume = 0x100;
    core->opts.frameskip = 0;
    /* Direct platform callback only: mCoreLoadConfig would map/load foreign
     * settings. This callback preserves opts.useBios/skipBios as set above. */
    core->loadConfig(core, &core->config);
    core->setVideoBuffer(core, p->video, GBAP_WIDTH);
    if (!core->getAudioChannel(core, 0) || !core->getAudioChannel(core, 1)) {
        fail(p, "core audio initialization failed"); goto error;
    }
    core->setAudioBufferSize(core, 2048);
    if (!core->loadROM(core, vf)) { fail(p, "core rejected ROM"); goto error; }
    vf = NULL;  /* core now owns the VFile, adapter still owns its bytes */

    p->save = save_create();
    if (!p->save) { fail(p, "save allocation failed"); goto error; }
    result = read_file(p->save_path, GBAP_SAVE_MAX, &input, &size);
    if (result == FILE_MISSING && import_path && *import_path)
        result = read_file(import_path, GBAP_SAVE_MAX, &input, &size);
    if (result == FILE_ERROR || (result == FILE_OK && !valid_save_size(size))) {
        fail(p, "save unreadable or invalid size"); goto error_unowned_save;
    }
    if (result == FILE_OK) {
        memcpy(p->save->data, input, size);
        p->save->size = size;
    }
    free(input); input = NULL;
    if (!core->loadSave(core, &p->save->vf)) {
        fail(p, "core rejected save"); goto error_unowned_save;
    }

    if (bios_path && *bios_path) {
        result = read_file(bios_path, GBAP_BIOS_SIZE, &p->bios, &size);
        if (result == FILE_OK && size == GBAP_BIOS_SIZE)
            vf = VFileFromMemory(p->bios, size);
        if (vf && core->loadBIOS(core, vf, 0)) {
            /* loadBIOS's bool does not report GBALoadBIOS map failure. */
            if (((struct GBA *)core->board)->biosVf == vf) {
                vf = NULL;
                p->has_bios = true;
                core->opts.skipBios = false;
            }
        }
        if (!p->has_bios) {
            if (vf) { vf->close(vf); vf = NULL; }
            free(p->bios); p->bios = NULL;
            strcpy(p->warning, "external BIOS unavailable or invalid; using HLE");
        }
    }
    /* Default RTC source is mGBA's wall clock. Null rotation, solar and
     * rumble sources are explicitly supported by its cartridge GPIO code. */
    core->setPeripheral(core, mPERIPH_ROTATION, NULL);
    core->setPeripheral(core, mPERIPH_RUMBLE, NULL);
    core->setPeripheral(core, mPERIPH_GBA_LUMINANCE, NULL);
    blip_set_rates(core->getAudioChannel(core, 0), core->frequency(core), GBAP_AUDIO_RATE);
    blip_set_rates(core->getAudioChannel(core, 1), core->frequency(core), GBAP_AUDIO_RATE);
    core->reset(core);
    if (p->save->failed) { fail(p, "save initialization failed"); goto error; }
    core->setKeys(core, 0);
    for (size_t i = 0; i < GBAP_WIDTH * GBAP_HEIGHT; ++i) p->argb[i] = 0xff000000u;
    p->loaded = true;
    return true;

error_unowned_save:
    free(p->save);
    p->save = NULL;
error:
    free(input);
    if (vf) vf->close(vf);
    release_core(p);
    return false;
}

static int sync_file(int fd)
{
    int result;
    do { result = fsync(fd); } while (result < 0 && errno == EINTR);
    return result;
}

bool gbap_flush_save(gbap_runtime *p)
{
    char temporary[GBAP_PATH_MAX + 16], directory[GBAP_PATH_MAX];
    char *slash;
    int fd, directory_fd;
    size_t done = 0;
    struct GBASavedata *savedata;
    if (!p || !p->loaded) return fail(p, "no cartridge loaded");
    p->error[0] = 0;
    if (p->save->failed) return fail(p, "save memory VFile failed");
    savedata = &((struct GBA *)p->core->board)->memory.savedata;
    if (!GBASavedataSize(savedata)) return true;
    GBASavedataRTCWrite(savedata);
    if (p->save->failed) return fail(p, "save RTC update failed");
    if (!p->save->size) return true;
    if (!valid_save_size(p->save->size)) return fail(p, "core produced invalid save size");
    strcpy(directory, p->save_path);
    slash = strrchr(directory, '/');
    if (slash == directory) slash[1] = 0;
    else *slash = 0;
    do { directory_fd = open(directory, O_RDONLY); }
    while (directory_fd < 0 && errno == EINTR);
    if (directory_fd < 0) return fail(p, "cannot open save directory");
    strcpy(temporary, p->save_path);
    memcpy(temporary + strlen(temporary), ".tmp.XXXXXX", sizeof(".tmp.XXXXXX"));
    fd = mkstemp(temporary);
    if (fd < 0) {
        close(directory_fd);
        return fail(p, "cannot create save temporary file");
    }
    while (done < p->save->size) {
        ssize_t n = write(fd, p->save->data + done, p->save->size - done);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { fail(p, "save write failed"); goto error; }
        done += (size_t)n;
    }
    if (sync_file(fd) != 0) { fail(p, "save file sync failed"); goto error; }
    if (close(fd) != 0) {
        /* close is not retried: Linux may already have released the fd. */
        fd = -1;
        fail(p, "save file close failed"); goto error;
    }
    fd = -1;
    if (rename(temporary, p->save_path) != 0) { fail(p, "save rename failed"); goto error; }
    if (sync_file(directory_fd) != 0) {
        close(directory_fd);
        return fail(p, "save replaced; directory sync failed");
    }
    if (close(directory_fd) != 0) return fail(p, "save replaced; directory close failed");
    return true;
error:
    if (fd >= 0) close(fd);
    unlink(temporary);
    close(directory_fd);
    return false;
}

void gbap_set_keys(gbap_runtime *p, uint32_t keys)
{
    if (!p || !p->loaded) return;
    p->core->setKeys(p->core, keys & GBAP_KEY_MASK);
    p->error[0] = 0;
}

bool gbap_run_frame(gbap_runtime *p)
{
    uint32_t before;
    if (!p || !p->loaded) return fail(p, "no cartridge loaded");
    p->error[0] = 0;
    if (blip_samples_avail(p->core->getAudioChannel(p->core, 0)) ||
        blip_samples_avail(p->core->getAudioChannel(p->core, 1)))
        return fail(p, "drain audio before next frame");
    before = p->core->frameCounter(p->core);
    p->core->runFrame(p->core);
    if (before == p->core->frameCounter(p->core)) return fail(p, "core frame did not advance");
    for (size_t i = 0; i < GBAP_WIDTH * GBAP_HEIGHT; ++i) {
        uint32_t c = p->video[i]; /* mGBA: 0xXXBBGGRR; Evas: 0xFFRRGGBB */
        p->argb[i] = 0xff000000u | ((c & 0xffu) << 16) |
                     (c & 0xff00u) | ((c >> 16) & 0xffu);
    }
    return true;
}

const uint32_t *gbap_framebuffer(const gbap_runtime *p)
{
    return p && p->loaded ? p->argb : NULL;
}

size_t gbap_drain_audio(gbap_runtime *p, int16_t *output, size_t capacity)
{
    blip_t *left, *right;
    int available, other, n, read_left, read_right;
    if (!p || !p->loaded) { fail(p, "no cartridge loaded"); return 0; }
    p->error[0] = 0;
    if (!capacity) return 0;
    if (!output) { fail(p, "missing audio output buffer"); return 0; }
    left = p->core->getAudioChannel(p->core, 0);
    right = p->core->getAudioChannel(p->core, 1);
    available = blip_samples_avail(left);
    other = blip_samples_avail(right);
    if (available != other) { fail(p, "audio channels out of sync"); return 0; }
    if (available <= 0) return 0;
    n = available;
    if ((size_t)n > capacity) n = (int)capacity; /* capacity < n <= INT_MAX */
    /* blip's last argument is a boolean: stereo=1 selects a TWO-short
     * stride (blip_buf.c: `step = stereo ? 2 : 1`), not a stride of one. */
    read_left = blip_read_samples(left, output, n, 1);
    read_right = blip_read_samples(right, output + 1, n, 1);
    if (read_left != n || read_right != n) {
        fail(p, "audio channel short read"); return 0;
    }
    return (size_t)n;
}

double gbap_frame_period(const gbap_runtime *p)
{
    int32_t frequency;
    if (!p || !p->loaded) return 0;
    frequency = p->core->frequency(p->core);
    return frequency > 0 ? (double)p->core->frameCycles(p->core) / frequency : 0;
}

bool gbap_has_bios(const gbap_runtime *p)
{
    return p && p->loaded && p->has_bios;
}

const char *gbap_error(const gbap_runtime *p)
{
    return p ? p->error : "runtime allocation failed";
}

const char *gbap_warning(const gbap_runtime *p)
{
    return p ? p->warning : "";
}
