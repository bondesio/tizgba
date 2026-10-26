/* TizGBA native EFL frontend. Input/lifecycle lineage: TizGB and TizNES.
 * The mGBA core is owned exclusively by platform.c and the UI thread.
 * Physical slots are alternatives for one GBA keypad, never link multiplayer.
 * No BIOS or cartridge data is bundled. */
#include "platform.h"
#include "filters.h"

typedef unsigned char  Eina_Bool;
typedef unsigned int   u32;
typedef struct { void *create, *terminate, *pause, *resume, *app_control; } callbacks_t;
typedef void Evas_Object;
typedef void Evas;

/* ---- appfw / EFL (hand-declared) ---- */
extern int   ui_app_main(int, char **, const callbacks_t *, void *);
extern Evas_Object *elm_win_util_standard_add(const char *, const char *);
extern void  elm_win_fullscreen_set(Evas_Object *, int);
extern void  elm_win_resize_object_add(Evas_Object *, Evas_Object *);
extern void  elm_object_focus_set(Evas_Object *, Eina_Bool);
extern Evas *evas_object_evas_get(Evas_Object *);
extern Evas_Object *evas_object_rectangle_add(Evas *);
extern Evas_Object *evas_object_text_add(Evas *);
extern Evas_Object *evas_object_image_filled_add(Evas *);
extern Evas_Object *elm_label_add(Evas_Object *);
extern void  elm_object_part_text_set(Evas_Object *, const char *, const char *);
extern void  evas_object_image_size_set(Evas_Object *, int, int);
extern void  evas_object_image_data_set(Evas_Object *, void *);
extern void  evas_object_image_data_update_add(Evas_Object *, int, int, int, int);
extern void  evas_object_image_smooth_scale_set(Evas_Object *, Eina_Bool);
extern void  evas_object_image_alpha_set(Evas_Object *, Eina_Bool);
extern void  evas_object_text_font_set(Evas_Object *, const char *, int);
extern void  evas_object_text_text_set(Evas_Object *, const char *);
extern void  evas_object_color_set(Evas_Object *, int, int, int, int);
extern void  evas_object_move(Evas_Object *, int, int);
extern void  evas_object_resize(Evas_Object *, int, int);
extern void  evas_object_show(Evas_Object *);
extern void  evas_object_hide(Evas_Object *);

extern int   ECORE_EVENT_KEY_DOWN;
extern int   ECORE_EVENT_KEY_UP;
extern void *ecore_event_handler_add(int, Eina_Bool (*)(void *, int, void *), const void *);
extern void *ecore_animator_add(Eina_Bool (*)(void *), const void *);
extern void *ecore_timer_add(double, Eina_Bool (*)(void *), const void *);
extern void  ui_app_exit(void);
extern void  _exit(int);

typedef struct { const char *keyname; const char *key; const char *string; } Ecore_Event_Key;

extern void evas_output_size_get(const Evas *, int *, int *);
extern void evas_object_image_data_copy_set(Evas_Object *, void *);
extern void *evas_object_image_data_get(const Evas_Object *, Eina_Bool);
extern int evas_object_image_stride_get(const Evas_Object *);
extern void *ecore_animator_del(void *);
extern void *ecore_event_handler_del(void *);
extern int fsync(int);
extern int rename(const char *, const char *);
extern int unlink(const char *);
extern int mkdir(const char *, unsigned int);
extern int getpid(void);

/* ---- libc / syscalls (hand-declared; libc.so.6 linked) ---- */
extern int   open(const char *, int, ...);
extern int   access(const char *, int);
#define F_OK 0
#define R_OK 4
#define W_OK 2
extern long  read(int, void *, unsigned long);
extern long  readlink(const char *, char *, unsigned long);
extern char *realpath(const char *, char *);
extern long  write(int, const void *, unsigned long);
extern int   close(int);
extern long  syscall(long, ...);
extern int   ioctl(int, unsigned long, ...);
extern int  *__errno_location(void);
extern int   snprintf(char *, unsigned long, const char *, ...);
extern unsigned long strlen(const char *);
extern int   strcasecmp(const char *, const char *);
extern int   strncmp(const char *, const char *, unsigned long);
extern void *memcpy(void *, const void *, unsigned long);
extern void *memset(void *, int, unsigned long);
struct gb_timespec { long tv_sec; long tv_nsec; };   /* glibc layout (long, long) */
extern int   clock_gettime(int, struct gb_timespec *);
#define CLOCK_MONOTONIC 1

#include "library.h"
#include "perf_overlay.h"
extern int evas_object_text_horiz_advance_get(const Evas_Object *);

#ifndef TIZGBA_DATA_DIR
#define TIZGBA_DATA_DIR "/opt/usr/home/owner/apps_rw/org.pablo.tizgba/data"
#endif
#ifndef TIZGBA_DEV_INPUT
#define TIZGBA_DEV_INPUT "/dev/input"
#endif
#ifndef TIZGBA_SYS_INPUT
#define TIZGBA_SYS_INPUT "/sys/class/input"
#endif

#define O_RDONLY     0
#define O_WRONLY     1
#define O_NONBLOCK   0x800
#define O_DIRECTORY  0x4000
#define O_CREAT      0x40
#define O_TRUNC      0x200
#define O_APPEND     0x400
#define SYS_getdents64 217        /* ARM EABI */
#define JSIOCGAXES     0x80016a11UL
#define JSIOCGBUTTONS  0x80016a12UL
#define JSIOCGNAME(len) (0x80006a13UL | ((unsigned long)(len) << 16))
#define JSIOCGAXMAP    0x80406a32UL
#define JSIOCGBTNMAP   0x84006a34UL
#define ABS_X          0x00
#define ABS_Y          0x01
#define ABS_RX         0x03
#define ABS_RY         0x04
#define ABS_HAT0X      0x10
#define ABS_HAT0Y      0x11
#define BTN_A          0x130
#define BTN_B          0x131
#define BTN_SELECT     0x13a
#define BTN_START      0x13b
#define BTN_MODE       0x13c
#define EAGAIN         11
#define EINTR          4
#define EVIOCGRAB      0x40044590UL
#define EVIOCGABS(axis) (0x80184540UL | (unsigned long)(axis))
#define EVIOCGKEY(len) (0x80004518UL | ((unsigned long)(len) << 16))
#define EV_SYN         0x00
#define SYN_REPORT     0
#define SYN_DROPPED    3
#define EV_KEY         0x01
#define EV_ABS         0x03
#define BTN_DPAD_UP    0x220
#define BTN_DPAD_DOWN  0x221
#define BTN_DPAD_LEFT  0x222
#define BTN_DPAD_RIGHT 0x223
#define EV_KEY_CODE_COUNT 768
#define ABS_CODE_COUNT 64

typedef struct { int value, minimum, maximum, fuzz, flat, resolution; } input_absinfo;

#define PAD_A GBAP_KEY_A
#define PAD_B GBAP_KEY_B
#define PAD_SELECT GBAP_KEY_SELECT
#define PAD_START GBAP_KEY_START
#define PAD_UP GBAP_KEY_UP
#define PAD_DOWN GBAP_KEY_DOWN
#define PAD_LEFT GBAP_KEY_LEFT
#define PAD_RIGHT GBAP_KEY_RIGHT
#define PAD_R GBAP_KEY_R
#define PAD_L GBAP_KEY_L

/* Opposing directions from several sources in one frame: neutral, not the
   last one wins (TizNES 1.0.21; games would otherwise see Up+Down). */
static unsigned int resolve_opposing_directions(unsigned int bits)
{
    if ((bits & (PAD_UP | PAD_DOWN)) == (PAD_UP | PAD_DOWN)) bits &= ~(PAD_UP | PAD_DOWN);
    if ((bits & (PAD_LEFT | PAD_RIGHT)) == (PAD_LEFT | PAD_RIGHT)) bits &= ~(PAD_LEFT | PAD_RIGHT);
    return bits;
}

#define TV_W 1920
#define TV_H 1080
#define PATH_MAX 512
#define VIS_ROWS 14

/* Settings rows in on-screen (and D-pad) order; section headers are not
   rows and can never be selected. */
enum {
    S_DEVICE, S_DEVICE2, S_EXCLUSIVE, S_MAP, S_MAP2, S_ANALOG, S_ANALOG2, S_INPUT_TEST,
    S_VOLUME, S_MUTE, S_AUDIO_TEST,
    S_SCALE, S_FILTER, S_OVERLAY, S_LCD_STRENGTH, S_COLOUR, S_BLEND,
    S_PERF_OVERLAY, S_RESCAN, S_BACK, S_EXIT, SETTINGS_ROWS
};
typedef struct { int first_row; const char *title; } settings_section;
static const settings_section g_settings_sections[] = {
    { S_DEVICE,  "CONTROLLERS" },
    { S_VOLUME,  "AUDIO" },
    { S_SCALE,   "VIDEO" },
    { S_PERF_OVERLAY, "SYSTEM" },
};
#define SETTINGS_SECTIONS ((int)(sizeof g_settings_sections / sizeof g_settings_sections[0]))

enum { ST_LIBRARY, ST_GAME, ST_HELP, ST_ERROR, ST_SETTINGS, ST_MAPPING, ST_DEVICES, ST_INPUT_TEST };

static Evas_Object *g_win, *g_bg, *g_img;
static Evas        *g_canvas;
static Evas_Object *g_title, *g_footer, *g_err;
static Evas_Object *g_row[VIS_ROWS], *g_cursor;
static Evas_Object *g_set_row[SETTINGS_ROWS];
static Evas_Object *g_set_head[SETTINGS_SECTIONS], *g_set_rule[SETTINGS_SECTIONS];
static Evas_Object *g_help;
static void        *g_anim;

static int   g_state = ST_LIBRARY;
static int   g_help_return_state = ST_GAME;
#define g_romcount lib_count()   /* library.c owns the list */
static int   g_sel, g_top;
/* Library scans run from the animator so the screen can say "Scanning"
   first: phase 1 = show the message, 2 = scan on the next tick. */
static int   g_scan_phase;
static int   g_rescan_pending;           /* drives changed while not in the library */
static char  g_library_message[160];     /* footer note until the next key */
static unsigned long g_mount_poll_frame;
#define MOUNT_POLL_FRAMES 120            /* ~2 s of animator ticks */

static unsigned int g_keybits;   /* pad bits from TV-remote/keyboard keys */

/* Two physical slots, each with independent capture and held-input state. */
typedef struct {
    int slot;
    int js_fd, ev_fd;
    unsigned int jsbits;
    unsigned int button_bits, stick_bits, hat_bits, ev_button_bits, mapped_axis_bits;
    unsigned char axis_count, button_count;
    unsigned char axis_map[64];
    unsigned short button_map[512];
    int axis_map_ok, button_map_ok;
    uint16_t button_pad[64];
    unsigned char custom_mapped[64];
    uint16_t ev_key_pad[EV_KEY_CODE_COUNT];
    uint16_t axis_neg_pad[64], axis_pos_pad[64];
    unsigned char key_down[EV_KEY_CODE_COUNT], button_down[64];
    int dropped;
    signed char axis_state[64];
    short axis_raw[64];
    unsigned char axis_raw_valid[64];
    int ev_abs_min[ABS_CODE_COUNT], ev_abs_max[ABS_CODE_COUNT];
    unsigned char ev_abs_range_ok[ABS_CODE_COUNT];
    int x_axis, y_axis, hat_x_axis, hat_y_axis;
    unsigned char axis_logged[64], button_logged[64];
    int device_index, ev_device_index, selected_js_index;
    char device_name[128];
    int capture_exclusive;
    char selected_usb_id[160];
    char map_model[16];
    int map_loaded;
    unsigned long capture_retry_frame, open_retry_frame;
    char capture_status[128];
} pad_t;
static pad_t g_pads[2];
#define PAD1 (&g_pads[0])
#define PAD2 (&g_pads[1])
static const char *pad_tag(const pad_t *p) { return p->slot ? "slot2 " : "slot1 "; }
static unsigned int g_js_prevbits;
static int g_exclusive_preferred = 1;
static int g_device_indices[16], g_device_count, g_device_sel;
static char g_device_names[16][128];
static char g_device_usb_ids[16][160];
static unsigned long g_input_frame, g_js_activity_frame;
static int g_game_input_log_bits = -1;
static int g_js_activity_valid;
static int g_settings_sel, g_map_step = -1, g_map_armed, g_map_slot, g_device_slot;
/* Bank 0: digital controls; bank 1: analog directions. */
static int g_map_bank, g_map_timed_out;
static unsigned long long g_map_deadline;
static char g_map_detected[96];
static int g_input_test_step = -1, g_input_test_wait_release;
static unsigned short g_input_test_wait_code, g_input_test_wait_type;
static int g_input_test_axis_rest[ABS_CODE_COUNT];
static unsigned char g_input_test_axis_rest_valid[ABS_CODE_COUNT];
static char g_input_test_last[160];
static char g_settings_message[160];

/* Version 2 settings and little-endian 16-bit action mapping schema. */
static int g_volume = 100, g_muted, g_audio_test_filter;
static int g_scale_mode, g_settings_return_state = ST_LIBRARY;
static gba_filter_t g_filter;
static gba_filter_settings_t g_filter_requested, g_filter_live;
static gba_filter_effective_t g_effective;
static gbap_runtime *g_core;
static int g_canvas_w, g_canvas_h, g_paused, g_filter_late;
static int g_perf_mode;                  /* perf_overlay_mode: Off / FPS / Detailed */
static perf_stats g_perf;
static Evas_Object *g_perf_bg, *g_perf_txt;
static int g_video_w, g_video_h;
static void *g_key_down_handler, *g_key_up_handler;
static unsigned long long g_last_save;
static const char *g_audio_test_names[3] = { "Stereo", "Left only", "Right only" };

/* Pacing: one GBA frame is 280896 cycles of 16777216 Hz = 16.742706 ms
   (59.7275 Hz). Paced by the monotonic clock, independent of the animator
   tick (compositor) rate. */
#define PACE_GB_NS 16742706u
#define PACE_MAX_PER_TICK 3      /* catch-up frames per animator tick */
#define PACE_MAX_LAG 6           /* beyond this many late frames, drop the backlog */
static unsigned int g_pace_period_ns = PACE_GB_NS;
static unsigned long long g_pace_t0, g_pace_done;
static unsigned long long g_pace_win_t0, g_pace_win_frames;
static unsigned long g_pace_ticks, g_pace_idle, g_pace_multi, g_pace_stalls;

static const char *g_settings_path     = TIZGBA_DATA_DIR "/tizgba-settings.conf";
static const char *g_input_config_path = TIZGBA_DATA_DIR "/tizgba-input.conf";
static const char *g_input_test_log_path = TIZGBA_DATA_DIR "/tizgba-gamepad-test.log";
static const char *g_input_active_path = TIZGBA_DATA_DIR "/tizgba-active";
static const char *g_map_dir = TIZGBA_DATA_DIR;

static const char *g_input_test_labels[] = {
    "A", "B", "X", "Y", "LB", "RB", "LT / ZL", "RT / ZR",
    "Select", "Start", "Left stick click", "Right stick click", "Home / Guide",
    "D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left"
};
#define INPUT_TEST_COUNT ((int)(sizeof(g_input_test_labels)/sizeof(g_input_test_labels[0])))

/* Per-model map: TGA2 header, version 2, ten GBA actions, 64 flags,
 * then four arrays of little-endian uint16_t masks (buttons/negative axes/
 * positive axes/raw evdev keys). No byte-sized NES/GB action IDs are reused. */
#define MAP_FILE_SIZE (8 + 64 + 2 * (64 + 64 + 64 + EV_KEY_CODE_COUNT))

static const uint16_t g_map_targets[10]={PAD_UP,PAD_DOWN,PAD_LEFT,PAD_RIGHT,PAD_A,PAD_B,PAD_SELECT,PAD_START,PAD_L,PAD_R};
static unsigned char g_map_backup[MAP_FILE_SIZE];
static int g_map_active;

static void settings_open(void);
static void settings_close(void);
static void settings_activate(void);
static void settings_adjust(int direction);
static void app_exit(void);
static void device_select_open(int slot);
static void device_select_activate(void);
static void help_open(int return_state);
static void help_close(void);
static void js_close_device(pad_t *p);
static int  identity_matches(const char *saved, const char *current);
static void apply_video_settings(void);

static unsigned long long mono_ns(void);
static void diag(const char *line);
static int atomic_write(const char *path, const void *data, unsigned long n);
static void audio_open(void);
static void audio_close(void);
static void audio_frame_done(void);
static void present_frame(void);
static int close_game(void);
static void mapping_cancel(void);
static void mapping_retry(void);
static void mapping_tick(void);
static void clear_pad_state(pad_t *p);
static void ev_resync(pad_t *p);
static int pad_is_mapping(const pad_t *p);

/* ---------- small string helpers ---------- */
static void str_copy(char *d, const char *s, int max)
{
    int i = 0; while (s[i] && i < max-1) { d[i]=s[i]; i++; } d[i]=0;
}
/* js_open_device retries every ~0.5 s while the controller is unavailable;
   log once on any change, then a heartbeat with a repeat count. */
#define DIAG_RL_SLOTS 6   /* 0 evdev line, 1 js line, 2 open-failed */
static char g_diag_last[DIAG_RL_SLOTS][192];
static unsigned int g_diag_repeat[DIAG_RL_SLOTS];
static void diag_rl(int slot, const char *line)
{
    if (slot<0 || slot>=DIAG_RL_SLOTS) { diag(line); return; }
    if (strncmp(line, g_diag_last[slot], sizeof g_diag_last[slot]) == 0) {
        g_diag_repeat[slot]++;
        if (g_diag_repeat[slot] % 20) return;
        char line2[224];
        snprintf(line2, sizeof line2, "%s (repeated x%u)", line, g_diag_repeat[slot]);
        diag(line2);
        return;
    }
    str_copy(g_diag_last[slot], line, sizeof g_diag_last[slot]);
    g_diag_repeat[slot] = 1;
    diag(line);
}


/* Only owned data files are written. A failed write retains the previous file. */
static int atomic_write(const char *path, const void *data, unsigned long n)
{
    char tmp[PATH_MAX];
    int len=snprintf(tmp,sizeof tmp,"%s.tmp",path);
    if (len<=0 || len>=(int)sizeof tmp) return 0;
    int fd=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,0600);
    if(fd<0) return 0;
    const unsigned char *bytes=data; unsigned long done=0;
    while(done<n) {
        long w=write(fd,bytes+done,n-done);
        if(w<0 && *__errno_location()==EINTR) continue;
        if(w<=0) break;
        done+=(unsigned long)w;
    }
    int ok=done==n && fsync(fd)==0;
    if(close(fd)!=0) ok=0;
    if(ok && rename(tmp,path)==0) return 1;
    unlink(tmp); return 0;
}
static void diag(const char *line)
{
    int fd=open(TIZGBA_DATA_DIR "/tizgba-runtime.log",O_WRONLY|O_CREAT|O_APPEND,0600);
    if(fd>=0) { write(fd,line,strlen(line)); write(fd,"\n",1); close(fd); }
}

/* The watcher grants selected event nodes only while this app process is
 * active. Its /proc check also clears the grant after an unexpected crash. */
static void input_active_set(int active)
{
    if (!active) {
        unlink(g_input_active_path);
        return;
    }
    char pid[24];
    int n = snprintf(pid, sizeof pid, "%d\n", getpid());
    if (n <= 0 || n >= (int)sizeof pid ||
        !atomic_write(g_input_active_path, pid, (unsigned long)n))
        diag("input-active-marker=write-failed");
}

/* library.c has no dependency on the EFL frontend; this narrow callback
 * keeps its bounded scanner diagnostics in the app-owned log. */
void gbap_diag(const char *line) { diag(line); }

/* ---------- ROM library (scanning lives in library.c) ---------- */
static void render_library(void);

/* Full rescan, keeping the cursor on the same game when it is still there. */
static void library_rescan(const char *reason)
{
    char keep[LIB_PATH_MAX]; keep[0] = 0;
    const lib_rom *cur = lib_get(g_sel-1);
    if (cur) str_copy(keep, cur->path, sizeof keep);
    int before = lib_count();
    int on_settings = before > 0 && g_sel > before;
    int first_scan = strncmp(reason, "startup", 8) == 0;
    char line[96]; snprintf(line, sizeof line, "library-rescan reason=%s", reason);
    diag(line);
    lib_scan();
    g_rescan_pending = 0;
    int at = lib_find_path(keep);
    if (at >= 0) g_sel = at+1;
    else if (on_settings || g_sel > lib_count()+1) g_sel = lib_count()+1;
    else if (first_scan) g_sel = lib_count() > 0 ? 1 : lib_count()+1;   /* first game, or Settings when empty; never Exit */
    const lib_stats *st = lib_last_stats();
    g_library_message[0] = 0;
    if (st->limits & (LIB_LIMIT_ROMS | LIB_LIMIT_DIRS | LIB_LIMIT_ENTRIES | LIB_LIMIT_TIME))
        snprintf(g_library_message, sizeof g_library_message,
                 "Library incomplete: scan stopped at the %s limit (%d games listed)",
                 (st->limits & LIB_LIMIT_ROMS) ? "2048-game" : (st->limits & LIB_LIMIT_TIME) ? "10-second" :
                 (st->limits & LIB_LIMIT_DIRS) ? "4096-folder" : "200000-file", lib_count());
    else if (strncmp(reason, "startup", 8) != 0 && strncmp(reason, "settings", 9) != 0)
        snprintf(g_library_message, sizeof g_library_message,
                 "Library updated: %d game(s) (%+d), %d on %d USB drive(s)",
                 lib_count(), lib_count() - before, st->external_roms, st->external_roots);
}

/* Ask for a scan; it runs from the library screen (deferred elsewhere). */
static char g_scan_reason[24];
static void library_request_scan(const char *reason)
{
    g_rescan_pending = 1;
    str_copy(g_scan_reason, reason, sizeof g_scan_reason);
}

/* Animator hook, every tick: poll the mount table (a change there only
   triggers a scan if the Storage API's set of mounted drives changed too),
   then run a pending scan once the library is on screen. */
static void library_tick(unsigned long frame)
{
    if (frame - g_mount_poll_frame >= MOUNT_POLL_FRAMES) {
        g_mount_poll_frame = frame;
        if (lib_mounts_changed() && lib_roots_changed()) {
            diag("library-drives-changed");
            library_request_scan("drives-changed");
        }
    }
    if (g_state != ST_LIBRARY) return;
    if (g_scan_phase == 2) {
        g_scan_phase = 0;
        library_rescan(g_scan_reason);
        render_library();
    } else if (g_scan_phase == 1) {
        g_scan_phase = 2;
        render_library();
    } else if (g_rescan_pending) {
        g_scan_phase = 1;
        render_library();
    }
}

/* ---------- settings persistence ---------- */
static int parse_int(const char *s)
{
    int v=0, neg=0; if(*s=='-') { neg=1; s++; }
    while(*s>='0' && *s<='9') { if(v>1000000) return neg?-1000000:1000000; v=v*10+(*s++-'0'); }
    return neg?-v:v;
}


static void load_settings(void)
{
    gba_filter_settings_default(&g_filter_requested);
    char data[1024]; int fd=open(g_settings_path,O_RDONLY);
    if(fd<0) return;
    long n=read(fd,data,sizeof data-1);close(fd);
    if(n<=0 || n==(long)sizeof data-1) return;
    data[n]=0;
    /* Do not misinterpret old or future layouts as the current schema. */
    if(strncmp(data,"version=2\n",10)) { diag("settings-version=unsupported defaults-used"); return; }
    int values[GBA_FILTER_SETTINGS_INTS]; gba_filter_settings_to_ints(&g_filter_requested,values);
    for(char *p=data;*p;) {
        char *end=p;while(*end && *end!='\n') end++;if(*end) *end++=0;
        if(!strncmp(p,"volume=",7)) g_volume=parse_int(p+7);
        else if(!strncmp(p,"mute=",5)) g_muted=!!parse_int(p+5);
        else if(!strncmp(p,"scale=",6)) g_scale_mode=parse_int(p+6);
        else if(!strncmp(p,"perf_overlay=",13)) g_perf_mode=parse_int(p+13);
        for(int i=0;i<GBA_FILTER_SETTINGS_INTS;i++) {
            const char *key=gba_filter_setting_key(i);unsigned long len=strlen(key);
            if(!strncmp(p,key,len) && p[len]=='=') values[i]=parse_int(p+len+1);
        }
        p=end;
    }
    if(g_volume<0) g_volume=0;
    if(g_volume>100) g_volume=100;
    if(g_scale_mode<0 || g_scale_mode>=GBA_SCREEN_COUNT) g_scale_mode=0;
    g_perf_mode=perf_mode_sanitize(g_perf_mode);
    gba_filter_settings_from_ints(&g_filter_requested,values);
    gba_filter_settings_sanitize(&g_filter_requested);
}


static void save_settings(void)
{
    char data[1024]; int values[GBA_FILTER_SETTINGS_INTS];
    gba_filter_settings_to_ints(&g_filter_requested,values);
    int n=snprintf(data,sizeof data,"version=2\nvolume=%d\nmute=%d\nscale=%d\nperf_overlay=%d\n",g_volume,g_muted,g_scale_mode,g_perf_mode);
    for(int i=0;i<GBA_FILTER_SETTINGS_INTS;i++) n+=snprintf(data+n,sizeof data-(unsigned long)n,"%s=%d\n",gba_filter_setting_key(i),values[i]);
    if(!atomic_write(g_settings_path,data,(unsigned long)n)) {
        diag("settings-save=failed"); str_copy(g_settings_message,"Could not save settings",sizeof g_settings_message);
    }
}


/* ---------- per-model mapping ---------- */
static void mapping_unpack(pad_t *p, const unsigned char *data, long n)
{
    memset(p->button_pad,0,sizeof p->button_pad);memset(p->custom_mapped,0,sizeof p->custom_mapped);
    memset(p->axis_neg_pad,0,sizeof p->axis_neg_pad);memset(p->axis_pos_pad,0,sizeof p->axis_pos_pad);
    memset(p->ev_key_pad,0,sizeof p->ev_key_pad);
    if(n!=MAP_FILE_SIZE || data[0]!='T'||data[1]!='G'||data[2]!='A'||data[3]!='2'||data[4]!=2||data[5]!=10) return;
    memcpy(p->custom_mapped,data+8,64);int at=72;
    for(int bank=0;bank<4;bank++) {
        uint16_t *v=bank==0?p->button_pad:bank==1?p->axis_neg_pad:bank==2?p->axis_pos_pad:p->ev_key_pad;
        int count=bank==3?EV_KEY_CODE_COUNT:64;
        for(int i=0;i<count;i++) { v[i]=(uint16_t)((data[at]|((unsigned)data[at+1]<<8))&GBAP_KEY_MASK);at+=2; }
    }
}


static void mapping_pack(const pad_t *p, unsigned char *data)
{
    memset(data,0,MAP_FILE_SIZE);
    data[0]='T';data[1]='G';data[2]='A';data[3]='2';data[4]=2;data[5]=10;
    memcpy(data+8,p->custom_mapped,64);
    int at=72;
    for(int bank=0;bank<4;bank++) {
        const uint16_t *v=bank==0?p->button_pad:bank==1?p->axis_neg_pad:bank==2?p->axis_pos_pad:p->ev_key_pad;
        int count=bank==3?EV_KEY_CODE_COUNT:64;
        for(int i=0;i<count;i++) { data[at++]=(unsigned char)v[i];data[at++]=(unsigned char)(v[i]>>8); }
    }
}


static int is_hex4(const char *s)
{
    for (int i = 0; i < 4; i++) {
        char c = s[i];
        if (!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F'))) return 0;
    }
    return 1;
}

/* "vvvv:pppp:serial:port" -> "vvvv-pppp"; 0 when there is no USB identity. */
static int identity_model(const char *identity, char *model, int cap)
{
    model[0] = 0;
    if (cap < 10 || strlen(identity) < 9 || identity[4] != ':' || !is_hex4(identity) || !is_hex4(identity+5)) return 0;
    for (int i = 0; i < 4; i++) { model[i] = identity[i]; model[5+i] = identity[5+i]; }
    model[4] = '-'; model[9] = 0;
    return 1;
}

static void model_map_path(const char *model, char *path, int cap)
{
    snprintf(path, (unsigned long)cap, "%s/tizgba-map-%s.bin", g_map_dir, model);
}

static void pad_save_model_mapping(pad_t *p)
{
    char path[PATH_MAX]; unsigned char data[MAP_FILE_SIZE];
    if (!p->map_model[0]) { diag("mapping-save=no-model-identity"); return; }
    mapping_pack(p, data); model_map_path(p->map_model, path, sizeof path);
    if (!atomic_write(path, data, sizeof data))
        str_copy(g_settings_message, "Could not save controller mapping", sizeof g_settings_message);
    pad_t *other = &g_pads[1-p->slot];
    if (!strncmp(other->map_model, p->map_model, sizeof p->map_model)) other->map_loaded = 0;
}


/* No file for the model: the standard defaults (BTN_A/B/SELECT/START, left
   stick, D-pad hat) apply. */
static void pad_apply_mapping(pad_t *p)
{
    char model[16], path[PATH_MAX], line[PATH_MAX + 64];
    unsigned char data[MAP_FILE_SIZE];
    if (!identity_model(p->selected_usb_id, model, sizeof model)) {
        if (p->map_loaded && !p->map_model[0]) return;
        mapping_unpack(p, data, 0);
        p->map_model[0] = 0; p->map_loaded = 1;
        return;
    }
    if (p->map_loaded && strncmp(model, p->map_model, sizeof model) == 0) return;
    str_copy(p->map_model, model, sizeof p->map_model);
    p->map_loaded = 1;
    model_map_path(model, path, sizeof path);
    int fd = open(path, O_RDONLY);
    if (fd >= 0) {
        long n = read(fd, data, sizeof data); close(fd);
        mapping_unpack(p, data, n);
        snprintf(line, sizeof line, "mapping model=%s source=model-file", model);
        diag(line);
        return;
    }
    mapping_unpack(p, data, 0);
    snprintf(line, sizeof line, "mapping model=%s source=defaults", model);
    diag(line);
}


/* Reference layout uses 1920x1080 logical coordinates; game video uses real
 * canvas pixels. Keep menus readable when the compositor chooses another size. */
static void ui_move(Evas_Object *o,int x,int y)
{
    if(o!=g_img && o!=g_win && g_canvas_w>0 && g_canvas_h>0){x=x*g_canvas_w/TV_W;y=y*g_canvas_h/TV_H;}
    evas_object_move(o,x,y);
}
static void ui_resize(Evas_Object *o,int w,int h)
{
    if(o!=g_img && o!=g_win && g_canvas_w>0 && g_canvas_h>0){w=w*g_canvas_w/TV_W;h=h*g_canvas_h/TV_H;}
    evas_object_resize(o,w,h);
}
static int ui_font_size(int size)
{
    if(g_canvas_h>0)size=size*g_canvas_h/TV_H;
    return size<12?12:size;
}
#define evas_object_move ui_move
#define evas_object_resize ui_resize
static void ui_layout(void)
{
    if(!g_title)return;
    evas_object_text_font_set(g_title,"Sans",ui_font_size(56));evas_object_move(g_title,360,120);
    evas_object_text_font_set(g_footer,"Sans",ui_font_size(30));evas_object_move(g_footer,360,1000);
    for(int i=0;i<VIS_ROWS;i++)evas_object_text_font_set(g_row[i],"Sans",ui_font_size(34));
    for(int i=0;i<SETTINGS_ROWS;i++)evas_object_text_font_set(g_set_row[i],"Sans",ui_font_size(30));
    for(int i=0;i<SETTINGS_SECTIONS;i++)evas_object_text_font_set(g_set_head[i],"Sans",ui_font_size(26));
    evas_object_text_font_set(g_perf_txt,"Sans",ui_font_size(28));
    evas_object_move(g_err,360,420);evas_object_resize(g_err,1200,300);
    evas_object_move(g_help,300,220);evas_object_resize(g_help,1320,740);
}

/* ---------------------------- UI helpers --------------------------------- */
static Evas_Object *mk_text(int size, int r, int g, int b)
{
    Evas_Object *t = evas_object_text_add(g_canvas);
    evas_object_text_font_set(t, "Sans", ui_font_size(size));
    evas_object_color_set(t, r, g, b, 255);
    return t;
}

static const char *rom_tag(int idx)
{
    const lib_rom *r=lib_get(idx);
    return r && lib_root_is_external(r->root)?"[USB]":"[GBA]";
}


/* Location label for a row: the most specific end of "where". */
static const char *where_tail(const char *where, int max)
{
    int n = (int)strlen(where);
    if (n <= max) return where;
    const char *p = where + n - (max - 2);
    while (*p && *p != '/') p++;               /* restart at a folder boundary */
    return *p ? p + 1 : where + n - (max - 2);
}

static void render_library(void)
{
    char line[256];int total=g_romcount+2;
    if(g_sel<0) g_sel=0;
    if(g_sel>=total) g_sel=total-1;
    if(g_sel<g_top) g_top=g_sel;
    if(g_sel>=g_top+VIS_ROWS) g_top=g_sel-VIS_ROWS+1;
    if(g_top>total-VIS_ROWS) g_top=total>VIS_ROWS?total-VIS_ROWS:0;
    evas_object_text_text_set(g_title,"TizGBA - Game Boy Advance");
    for(int i=0;i<VIS_ROWS;i++) {
        int row=g_top+i;
        if(row>=total) { evas_object_hide(g_row[i]);continue; }
        if(row==0) str_copy(line,"Exit TizGBA",sizeof line);
        else if(row==g_romcount+1) str_copy(line,"Settings and controller setup",sizeof line);
        else { const lib_rom *r=lib_get(row-1);snprintf(line,sizeof line,"%s  %.90s",rom_tag(row-1),r->name); }
        evas_object_text_text_set(g_row[i],line);evas_object_move(g_row[i],360,230+i*52);
        int c=row==g_sel?255:180;evas_object_color_set(g_row[i],c,c,c,255);evas_object_show(g_row[i]);
    }
    evas_object_move(g_cursor,330,236+(g_sel-g_top)*52);evas_object_resize(g_cursor,8,40);evas_object_show(g_cursor);
    const lib_rom *r=lib_get(g_sel-1);const lib_stats *st=lib_last_stats();
    if(g_scan_phase) str_copy(line,"Scanning app storage and USB drives...",sizeof line);
    else if(g_library_message[0]) str_copy(line,g_library_message,sizeof line);
    else if(!g_romcount) str_copy(line,"No games found. Copy .gba/.agb ROMs to app storage or a USB drive.",sizeof line);
    else if(r) snprintf(line,sizeof line,"In %s   OK play   Left/Right page   Start: Settings",where_tail(r->where,34));
    else snprintf(line,sizeof line,"%d game(s), %d on USB   OK select   Start: Settings   Select: Help",g_romcount,st->external_roms);
    evas_object_text_text_set(g_footer,line);
}


/* Library cursor movement; Left/Right (and Channel Up/Down) page. */
static void library_move(int delta)
{
    if (g_scan_phase) return;
    g_library_message[0] = 0;
    g_sel += delta;
    if (g_sel < 0) g_sel = 0;
    if (g_sel > g_romcount+1) g_sel = g_romcount+1;
    render_library();
}

static int pad_present(const pad_t *p) { return p->js_fd >= 0; }

static void pad_status_text(const pad_t *p, char *out, int cap)
{
    if (!pad_present(p)) {
        snprintf(out, (unsigned long)cap, "%s", p->selected_usb_id[0] ? "selected controller not connected" : "none detected");
        return;
    }
    snprintf(out, (unsigned long)cap, "%.70s [%s]", p->device_name[0] ? p->device_name : "USB gamepad",
             p->capture_exclusive ? "exclusive" : "shared");
}

/* Layout: title at y=120, footer at y=1000; 4 headers and 14 rows fit
   between them (last row ends near y=965), as in TizNES. */
#define SET_TOP        194
#define SET_HEAD_H      44
#define SET_SECTION_GAP 14
#define SET_ROW_H       40

static void settings_text(int id, char *out, int cap)
{
    char status[160];gba_rect_t rect;gba_filter_fit_target(g_canvas_w,g_canvas_h,g_scale_mode,&rect);
    switch(id) {
    case S_DEVICE:case S_DEVICE2:
        pad_status_text(id==S_DEVICE?PAD1:PAD2,status,sizeof status);
        snprintf(out,cap,"Controller %d: %s",id==S_DEVICE?1:2,status);break;
    case S_EXCLUSIVE:snprintf(out,cap,"Exclusive capture: %s",g_exclusive_preferred?"On":"Off");break;
    case S_MAP:case S_MAP2:snprintf(out,cap,"Map controller %d: D-pad, A, B, L, R, Select, Start",id==S_MAP?1:2);break;
    case S_ANALOG:case S_ANALOG2:snprintf(out,cap,"Map controller %d: analog directions (independent)",id==S_ANALOG?1:2);break;
    case S_INPUT_TEST:snprintf(out,cap,"Guided controller 1 input diagnostic");break;
    case S_VOLUME:snprintf(out,cap,"Volume: %d%%",g_volume);break;
    case S_MUTE:snprintf(out,cap,"Mute: %s",g_muted?"On":"Off");break;
    case S_AUDIO_TEST:snprintf(out,cap,"Audio output check: %s",g_audio_test_names[g_audio_test_filter]);break;
    case S_SCALE:
        if(g_scale_mode==GBA_SCREEN_INTEGER) snprintf(out,cap,"Screen size: Pixel-perfect %dx (%d x %d)",rect.n,rect.w,rect.h);
        else snprintf(out,cap,"Screen size: %s (%d x %d)",gba_filter_screen_name(g_scale_mode),rect.w,rect.h);
        break;
    case S_FILTER:snprintf(out,cap,"Scaling filter: %s",gba_filter_name(g_filter_requested.scaler));break;
    case S_OVERLAY:snprintf(out,cap,"Pixel overlay: %s",gba_filter_overlay_name(g_filter_requested.overlay));break;
    case S_LCD_STRENGTH:snprintf(out,cap,"LCD grid strength: %d / 255",g_filter_requested.lcd_strength);break;
    case S_COLOUR:snprintf(out,cap,"Colour: %s",gba_filter_colour_name(g_filter_requested.colour));break;
    case S_BLEND:snprintf(out,cap,"Frame blending: %s",g_filter_requested.blend?"On":"Off");break;
    case S_PERF_OVERLAY:snprintf(out,cap,"Performance overlay: %s",perf_mode_label((perf_overlay_mode)g_perf_mode));break;
    case S_RESCAN:snprintf(out,cap,"Rescan ROMs (app storage and all USB drives)");break;
    case S_BACK:snprintf(out,cap,"Back to library");break;
    case S_EXIT:snprintf(out,cap,"Exit TizGBA");break;
    default:out[0]=0;
    }
}


static void render_settings(void)
{
    char text[240];evas_object_text_text_set(g_title,"TizGBA settings");
    /* Page whole sections; headings are never selectable. */
    int section=0;while(section+1<SETTINGS_SECTIONS && g_settings_sel>=g_settings_sections[section+1].first_row) section++;
    int first=g_settings_sections[section].first_row;
    int last=section+1<SETTINGS_SECTIONS?g_settings_sections[section+1].first_row:SETTINGS_ROWS;
    for(int i=0;i<SETTINGS_ROWS;i++) evas_object_hide(g_set_row[i]);
    for(int i=0;i<SETTINGS_SECTIONS;i++) { evas_object_hide(g_set_head[i]);evas_object_hide(g_set_rule[i]); }
    snprintf(text,sizeof text,"%s    (%d / %d)",g_settings_sections[section].title,section+1,SETTINGS_SECTIONS);
    evas_object_text_text_set(g_set_head[section],text);evas_object_move(g_set_head[section],360,210);evas_object_show(g_set_head[section]);
    evas_object_move(g_set_rule[section],360,250);evas_object_resize(g_set_rule[section],1200,2);evas_object_show(g_set_rule[section]);
    for(int id=first;id<last;id++) {
        settings_text(id,text,sizeof text);evas_object_text_text_set(g_set_row[id],text);
        evas_object_move(g_set_row[id],390,280+(id-first)*72);
        int c=id==g_settings_sel?255:180;evas_object_color_set(g_set_row[id],c,c,c,255);evas_object_show(g_set_row[id]);
    }
    evas_object_move(g_cursor,360,284+(g_settings_sel-first)*72);evas_object_resize(g_cursor,8,36);evas_object_show(g_cursor);
    const char *hint="Up/Down moves through Controllers / Audio / Video / System; Left/Right changes";
    if(g_settings_sel<=S_INPUT_TEST) hint="Both controllers operate one GBA keypad. OK selects; Back returns.";
    if(g_settings_sel==S_PERF_OVERLAY) hint="Shows emulation speed over the game: Off, FPS, or Detailed. Left/Right changes; applies immediately";
    if(g_settings_sel==S_EXCLUSIVE) {
        snprintf(text,sizeof text,"Capture requested: %s. Controller 1: %s; controller 2: %s",g_exclusive_preferred?"On":"Off",PAD1->capture_exclusive?"exclusive":"shared / disconnected",PAD2->capture_exclusive?"exclusive":"shared / disconnected");hint=text;
    }
    evas_object_text_text_set(g_footer,g_settings_message[0]?g_settings_message:hint);
}


static void render_devices(void)
{
    char line[192];
    pad_t *p = &g_pads[g_device_slot];
    evas_object_text_text_set(g_title, g_device_slot?"Select controller 2":"Select controller 1");
    if (!g_device_count) {
        evas_object_text_text_set(g_row[0], "No USB gamepads found");
        evas_object_move(g_row[0], 360, 300); evas_object_show(g_row[0]);
        evas_object_text_text_set(g_footer, "Connect a controller, then Back and reopen this screen");
        return;
    }
    for (int i = 0; i < g_device_count && i < VIS_ROWS; i++) {
        int mine = identity_matches(p->selected_usb_id, g_device_usb_ids[i]);
        snprintf(line, sizeof line, "%.80s  %.48s%s", g_device_names[i], g_device_usb_ids[i], mine ? "  [selected]" : "");
        evas_object_text_text_set(g_row[i], line);
        evas_object_move(g_row[i], 360, 220 + i*52);
        evas_object_color_set(g_row[i], i==g_device_sel?255:180, i==g_device_sel?255:180, i==g_device_sel?255:180, 255);
        evas_object_show(g_row[i]);
    }
    evas_object_move(g_cursor, 330, 226 + g_device_sel*52); evas_object_resize(g_cursor, 8, 40); evas_object_show(g_cursor);
    snprintf(line, sizeof line, "OK selects   Back returns   Capture: %s",
        p->capture_exclusive ? "exclusive" : (pad_present(p) ? p->capture_status : "not connected"));
    evas_object_text_text_set(g_footer, line);
}

static void render_mapping(void)
{
    static const char *labels[10]={"Up","Down","Left","Right","A","B","Select","Start","L","R"};
    char line[256];pad_t *p=&g_pads[g_map_slot];
    snprintf(line,sizeof line,"Controller %d - %s mapping",g_map_slot+1,g_map_bank?"analog":"digital");evas_object_text_text_set(g_title,line);
    int count=g_map_bank?4:10;
    for(int i=0;i<VIS_ROWS;i++)evas_object_hide(g_row[i]);
    for(int i=0;i<count;i++) {
        const char *status=i<g_map_step?"accepted":i>g_map_step?"waiting":!pad_present(p)?"controller disconnected":g_map_timed_out?"timed out - OK retries this action":!g_map_armed?"release all controls":g_map_bank?"move analog stick":i<4?"press D-pad direction or button":"press button";
        snprintf(line,sizeof line,"%s%s: %s",i==g_map_step?"> ":"",labels[i],status);
        evas_object_text_text_set(g_row[i],line);evas_object_move(g_row[i],360,240+i*55);evas_object_show(g_row[i]);
    }
    snprintf(line,sizeof line,"Detected: %.100s",g_map_detected[0]?g_map_detected:"none");
    evas_object_text_text_set(g_row[count],line);evas_object_move(g_row[count],360,825);evas_object_show(g_row[count]);
    snprintf(line,sizeof line,"%s   OK retries   Right skips action   Back cancels",p->capture_exclusive?"Exclusive capture active":pad_present(p)?"Shared input (TV shortcuts possible)":"No controller connected");
    evas_object_text_text_set(g_footer,line);
}


static void render_input_test(void)
{
    char line[192];
    evas_object_text_text_set(g_title, "Gamepad input diagnostics");
    if (g_input_test_step < INPUT_TEST_COUNT) {
        snprintf(line, sizeof line, "Button %d of %d: press %s", g_input_test_step+1, INPUT_TEST_COUNT,
                 g_input_test_labels[g_input_test_step]);
        evas_object_text_text_set(g_row[0], line);
        evas_object_move(g_row[0], 360, 275); evas_object_color_set(g_row[0], 255, 255, 255, 255); evas_object_show(g_row[0]);
        snprintf(line, sizeof line, "%s", g_input_test_wait_release ? "Detected; release that control to continue" :
                 "Press only this control once; OK marks it not detected");
        evas_object_text_text_set(g_row[1], line); evas_object_move(g_row[1], 360, 350); evas_object_show(g_row[1]);
    } else {
        evas_object_text_text_set(g_row[0], "Diagnostic complete");
        evas_object_move(g_row[0], 360, 275); evas_object_color_set(g_row[0], 255, 255, 255, 255); evas_object_show(g_row[0]);
        evas_object_text_text_set(g_row[1], "Log saved in TizGBA app data");
        evas_object_move(g_row[1], 360, 350); evas_object_show(g_row[1]);
    }
    snprintf(line, sizeof line, "Last event: %.150s", g_input_test_last[0] ? g_input_test_last : "none");
    evas_object_text_text_set(g_row[2], line); evas_object_move(g_row[2], 360, 455); evas_object_show(g_row[2]);
    evas_object_text_text_set(g_row[3], "For D-pad, press each direction once. For sticks, click L3/R3.");
    evas_object_move(g_row[3], 360, 535); evas_object_show(g_row[3]);
    evas_object_text_text_set(g_row[4], "OK skips an unreported control; Back cancels and saves the log.");
    evas_object_move(g_row[4], 360, 600); evas_object_show(g_row[4]);
    evas_object_text_text_set(g_footer, g_input_test_step >= INPUT_TEST_COUNT ? "OK or Back returns to Settings" :
        (g_input_test_wait_release ? "Release control to continue   Back cancels" : "Press the named gamepad control   OK = not detected   Back = cancel"));
}

/* Performance overlay (emulator-framework/docs/overlay.md): top-right, drawn
   only while a game is on screen, text rebuilt once per published window.
   Positions are design units; the width is real pixels, so rescale it. */
static unsigned int g_audio_under;   /* defined with the other audio counters below */
static void perf_overlay_apply(void)
{
    char text[200];
    if(g_perf_mode==PERF_OVERLAY_OFF || g_state!=ST_GAME ||
       perf_format(&g_perf,(perf_overlay_mode)g_perf_mode,5973,__atomic_load_n(&g_audio_under,__ATOMIC_RELAXED),text,sizeof text)<=0) {
        evas_object_hide(g_perf_bg);evas_object_hide(g_perf_txt);return;
    }
    evas_object_text_text_set(g_perf_txt,text);
    int w=evas_object_text_horiz_advance_get(g_perf_txt);
    if(g_canvas_w>0) w=w*TV_W/g_canvas_w;
    int x=TV_W-48-w;
    evas_object_move(g_perf_bg,x-10,38);evas_object_resize(g_perf_bg,w+20,48);
    evas_object_move(g_perf_txt,x,44);
    evas_object_show(g_perf_bg);evas_object_show(g_perf_txt);
}

static void show_group(int state)
{
    ui_layout();
    evas_object_hide(g_perf_bg); evas_object_hide(g_perf_txt);
    evas_object_hide(g_img); evas_object_hide(g_title); evas_object_hide(g_footer);
    evas_object_hide(g_err); evas_object_hide(g_help); evas_object_hide(g_cursor);
    for (int i = 0; i < VIS_ROWS; i++) evas_object_hide(g_row[i]);
    for (int i = 0; i < SETTINGS_ROWS; i++) evas_object_hide(g_set_row[i]);
    for (int i = 0; i < SETTINGS_SECTIONS; i++) { evas_object_hide(g_set_head[i]); evas_object_hide(g_set_rule[i]); }

    if (state == ST_LIBRARY) {
        evas_object_show(g_title); evas_object_show(g_footer);
        render_library();
    } else if (state == ST_GAME) {
        evas_object_show(g_img);
        perf_reset(&g_perf, mono_ns());
        perf_overlay_apply();
    } else if (state == ST_ERROR) {
        evas_object_show(g_err);
    } else if (state == ST_HELP) {
        if (g_help_return_state == ST_GAME) evas_object_show(g_img);
        else {
            evas_object_show(g_title); evas_object_show(g_footer);
            evas_object_text_text_set(g_title, "TizGBA Help");
            evas_object_text_text_set(g_footer, "A, B, Start, Select, or Back returns");
        }
        evas_object_show(g_help);
    } else if (state == ST_SETTINGS) {
        evas_object_show(g_title); evas_object_show(g_footer); render_settings();
    } else if (state == ST_MAPPING) {
        evas_object_show(g_title); evas_object_show(g_footer); render_mapping();
    } else if (state == ST_DEVICES) {
        evas_object_show(g_title); evas_object_show(g_footer); render_devices();
    } else if (state == ST_INPUT_TEST) {
        evas_object_show(g_title); evas_object_show(g_footer); render_input_test();
    }
}

/* Video geometry is derived from the current Evas canvas, not a TV model. */
static void apply_video_settings(void)
{
    if(!g_canvas || !g_img)return;
    int w=0,h=0;evas_output_size_get(g_canvas,&w,&h);
    if(w<=0 || h<=0)return;
    g_canvas_w=w;g_canvas_h=h;
    gba_rect_t rect;gba_filter_fit_target(w,h,g_scale_mode,&rect);
    if(gba_filter_configure(&g_filter,&g_filter_live,rect.w,rect.h,&g_effective)!=0) { diag("filter-configure=failed");return; }
    /* Bound CPU destination allocation independently of untrusted canvas size. */
    if(g_effective.out_w<GBAP_WIDTH || g_effective.out_h<GBAP_HEIGHT || g_effective.out_w>GBA_FILTER_MAX_W || g_effective.out_h>GBA_FILTER_MAX_H) { diag("filter-dimensions=invalid");return; }
    if(g_video_w!=g_effective.out_w || g_video_h!=g_effective.out_h) {
        g_video_w=g_effective.out_w;g_video_h=g_effective.out_h;
        evas_object_image_size_set(g_img,g_video_w,g_video_h);
    }
    evas_object_move(g_img,rect.x,rect.y);evas_object_resize(g_img,rect.w,rect.h);
    evas_object_image_smooth_scale_set(g_img,g_effective.hint==GBA_HINT_SMOOTH);
    if(g_effective.notes) { char log[128];snprintf(log,sizeof log,"filter-effective=%s dimensions=%dx%d notes=0x%x",gba_filter_short_name(g_effective.scaler),g_video_w,g_video_h,g_effective.notes);diag(log); }
}



/* Tizen pull audio: one UI producer and one audio callback consumer. All
 * shared counters/settings use acquire/release atomics, not volatile races. */
typedef void *audio_out_h;
extern int audio_out_create_new(int,int,int,audio_out_h *);
extern int audio_out_set_stream_cb(audio_out_h,void (*)(audio_out_h,unsigned int,void *),void *);
extern int audio_out_prepare(audio_out_h);
extern int audio_out_unprepare(audio_out_h);
extern int audio_out_destroy(audio_out_h);
extern int audio_out_write(audio_out_h,void *,unsigned int);
#define AUDIO_RATE 44100u
#define AUDIO_RING 16384u
#define AUDIO_TARGET 2048u
static audio_out_h g_audio;
static int16_t g_ring[AUDIO_RING*2];
static unsigned int g_rd,g_wr,g_audio_under,g_audio_over,g_audio_phase;
static int g_audio_mute,g_audio_volume=100,g_audio_channels;
static int g_dc_x[2],g_dc_y[2],g_pull_last[2];
static int g_audio_stereo=1;
static int clamp16(int v) { return v>32767?32767:v< -32768?-32768:v; }
static void audio_controls(void)
{
    __atomic_store_n(&g_audio_volume,g_volume,__ATOMIC_RELEASE);
    __atomic_store_n(&g_audio_mute,g_muted,__ATOMIC_RELEASE);
    __atomic_store_n(&g_audio_channels,g_audio_test_filter,__ATOMIC_RELEASE);
}
static void audio_pull(audio_out_h handle,unsigned int bytes,void *unused)
{
    (void)unused;
    unsigned char tmp[4096];unsigned int frame_bytes=g_audio_stereo?4:2;
    while(bytes) {
        unsigned int chunk=bytes>sizeof tmp?sizeof tmp:bytes;
        unsigned int rd=__atomic_load_n(&g_rd,__ATOMIC_RELAXED);
        unsigned int wr=__atomic_load_n(&g_wr,__ATOMIC_ACQUIRE);
        int mute=__atomic_load_n(&g_audio_mute,__ATOMIC_ACQUIRE);
        int volume=__atomic_load_n(&g_audio_volume,__ATOMIC_ACQUIRE);
        int channels=__atomic_load_n(&g_audio_channels,__ATOMIC_ACQUIRE);
        unsigned int count=chunk/frame_bytes;
        for(unsigned int i=0;i<count;i++) {
            int v[2];
            if(rd!=wr) { v[0]=g_ring[(rd&(AUDIO_RING-1))*2];v[1]=g_ring[(rd&(AUDIO_RING-1))*2+1];rd++;g_pull_last[0]=v[0];g_pull_last[1]=v[1]; }
            else { __atomic_fetch_add(&g_audio_under,1,__ATOMIC_RELAXED);for(int c=0;c<2;c++) { g_pull_last[c]=g_pull_last[c]*15/16;v[c]=g_pull_last[c]; } }
            for(int c=0;c<2;c++) v[c]=mute?0:v[c]*volume/100;
            if(channels==1)v[1]=0;
            if(channels==2)v[0]=0;
            if(g_audio_stereo) for(int c=0;c<2;c++) { unsigned int u=(unsigned int)v[c];tmp[i*4+c*2]=(unsigned char)u;tmp[i*4+c*2+1]=(unsigned char)(u>>8); }
            else { unsigned int u=(unsigned int)((v[0]+v[1])/2);tmp[i*2]=(unsigned char)u;tmp[i*2+1]=(unsigned char)(u>>8); }
        }
        __atomic_store_n(&g_rd,rd,__ATOMIC_RELEASE);
        memset(tmp+count*frame_bytes,0,chunk-count*frame_bytes);
        audio_out_write(handle,tmp,chunk);bytes-=chunk;
    }
}
static void audio_close(void)
{
    /* unprepare/destroy quiesce the callback before any ring reset. */
    if(g_audio) { audio_out_unprepare(g_audio);audio_out_destroy(g_audio);g_audio=0; }
}
static void audio_open(void)
{
    audio_close();audio_controls();g_audio_stereo=1;
    memset(g_ring,0,sizeof g_ring);g_rd=0;g_wr=AUDIO_TARGET;g_audio_phase=0;
    memset(g_dc_x,0,sizeof g_dc_x);memset(g_dc_y,0,sizeof g_dc_y);memset(g_pull_last,0,sizeof g_pull_last);
    __atomic_store_n(&g_audio_under,0,__ATOMIC_RELAXED);g_audio_over=0;
    int rc=audio_out_create_new(AUDIO_RATE,0x81,0x71,&g_audio);
    if(rc || !g_audio) { if(g_audio)audio_out_destroy(g_audio);g_audio=0;g_audio_stereo=0;rc=audio_out_create_new(AUDIO_RATE,0x80,0x71,&g_audio); }
    if(rc || !g_audio) { if(g_audio)audio_out_destroy(g_audio);g_audio=0;diag("audio-open=failed; core still drains audio");return; }
    rc=audio_out_set_stream_cb(g_audio,audio_pull,0);
    if(!rc) rc=audio_out_prepare(g_audio);
    if(rc) { audio_close();diag("audio-prepare=failed"); }
    else diag(g_audio_stereo?"audio-open=44100 stereo s16":"audio-open=44100 mono s16");
}
static void audio_frame_done(void)
{
    /* Drain even when muted or the device failed: pending samples otherwise
     * make the platform reject the next frame. Current GBA frames are ~549
     * samples; retaining the full 2048-frame buffer bounds exceptional input. */
    int16_t samples[4096];size_t in=0,n;
    while(in<2048 && (n=gbap_drain_audio(g_core,samples+in*2,2048-in))!=0) in+=n;
    int16_t discard[512];int extra=0;
    while((n=gbap_drain_audio(g_core,discard,256))!=0) extra=1;
    if(extra) diag("audio-core-frame exceeded 2048 frames; excess drained");
    if(!g_audio || !in) return;
    unsigned int wr=__atomic_load_n(&g_wr,__ATOMIC_RELAXED),rd=__atomic_load_n(&g_rd,__ATOMIC_ACQUIRE);
    unsigned long long scaled=(unsigned long long)g_pace_period_ns*AUDIO_RATE+g_audio_phase;
    int out=(int)(scaled/1000000000ULL);g_audio_phase=(unsigned int)(scaled%1000000000ULL);
    int fill=(int)(wr-rd),adjust=((int)AUDIO_TARGET-fill)/512;
    if(adjust>2)adjust=2;
    if(adjust< -2)adjust=-2;
    if(fill<512)adjust=out/16;else if(fill>3*(int)AUDIO_TARGET)adjust=-out/16;
    out+=adjust;if(out<1)out=1;
    for(int i=0;i<out;i++) {
        unsigned int position=(unsigned int)i*(unsigned int)in,index=position/(unsigned int)out,frac=position%(unsigned int)out;
        unsigned int next=index+1<in?index+1:index;
        if(wr-rd>=AUDIO_RING-1) { g_audio_over++;break; }
        for(int c=0;c<2;c++) {
            int a=samples[index*2+c],b=samples[next*2+c];int v=a+(b-a)*(int)frac/out;
            /* Multiplication avoids C's undefined left-shift of negatives. */
            g_dc_y[c]=(v-g_dc_x[c])*256+g_dc_y[c]-g_dc_y[c]/256;g_dc_x[c]=v;
            g_ring[(wr&(AUDIO_RING-1))*2+c]=(int16_t)clamp16(g_dc_y[c]/256);
        }
        wr++;
    }
    __atomic_store_n(&g_wr,wr,__ATOMIC_RELEASE);
}

/* ---------------------------- game control -------------------------------- */
static Eina_Bool animate(void *data);

static unsigned long long mono_ns(void)
{
    struct gb_timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (unsigned long long)ts.tv_sec * 1000000000ULL + (unsigned long long)ts.tv_nsec;
}

static void show_error(const char *headline, const char *detail)
{
    char text[640];
    snprintf(text, sizeof text,
        "<align=center><font_size=40><color=#ffb478>%s<br/>%.200s<br/><br/>"
        "Press any key to return to the library.</color></font_size></align>", headline, detail);
    elm_object_part_text_set(g_err, 0, text);
    g_state = ST_ERROR; show_group(g_state);
}


static void present_frame(void)
{
    const uint32_t *src=gbap_framebuffer(g_core);if(!src || !g_video_w || !g_video_h)return;
    unsigned long long begin=mono_ns();
    /* Evas owns its storage: never pass core/filter memory to data_set(). */
    void *dst=evas_object_image_data_get(g_img,1);if(!dst) { diag("video-buffer=unavailable");return; }
    int stride=evas_object_image_stride_get(g_img);
    int ok=stride>=g_video_w*4 && gba_filter_apply(&g_filter,src,GBAP_WIDTH*4,dst,(size_t)stride)==0;
    evas_object_image_data_set(g_img,dst);
    if(ok)evas_object_image_data_update_add(g_img,0,0,g_video_w,g_video_h);
    else { diag("filter-apply=failed; falling back");gba_filter_settings_default(&g_filter_live);apply_video_settings();return; }
    unsigned long long elapsed=mono_ns()-begin;
    if(elapsed>g_pace_period_ns/3)g_filter_late++;else if(g_filter_late)g_filter_late--;
    if(g_filter_late>=8 && gba_filter_cheaper(&g_filter_live)) {
        diag("filter-budget=exceeded; downgraded live filter (saved preference retained)");g_filter_late=0;apply_video_settings();
    }
}
static int close_game(void)
{
    audio_close();
    if(!g_core)return 1;
    if(!gbap_flush_save(g_core)) { diag(gbap_error(g_core));show_error("Battery save failed; game retained for retry.",gbap_error(g_core));return 0; }
    gbap_destroy(g_core);g_core=0;return 1;
}

/* Back to the library from a game, an error or Settings; a scan that was
   requested meanwhile (drive inserted/removed, ROM missing) starts now. */
static void return_to_library(void)
{
    g_keybits = 0;
    g_state = ST_LIBRARY;
    if (g_rescan_pending && !g_scan_phase) g_scan_phase = 1;
    show_group(g_state);
}

static void start_game(int idx)
{
    const lib_rom *rom=lib_get(idx);if(!rom || g_scan_phase)return;
    if(!close_game())return;
    char import[PATH_MAX];
    mkdir(TIZGBA_DATA_DIR "/saves",0700);
    str_copy(import,rom->path,sizeof import);int len=(int)strlen(import),dot=len;
    while(dot>0 && import[dot-1]!='.' && import[dot-1]!='/')dot--;
    if(dot>0 && import[dot-1]=='.') { import[dot]=0;str_copy(import+dot,"sav",(int)sizeof import-dot); }else import[0]=0;
    g_core=gbap_create();
    if(!g_core || !gbap_load(g_core,rom->path,TIZGBA_DATA_DIR "/saves",import[0]?import:0,TIZGBA_DATA_DIR "/gba_bios.bin")) {
        char error[256];str_copy(error,gbap_error(g_core),sizeof error);gbap_destroy(g_core);g_core=0;
        show_error("Could not load this ROM.",error);library_request_scan("load-error");return;
    }
    if(gbap_warning(g_core)[0])diag(gbap_warning(g_core));
    double period=gbap_frame_period(g_core);g_pace_period_ns=period>0?(unsigned int)(period*1000000000.0+0.5):PACE_GB_NS;
    g_pace_t0=g_pace_done=0;g_pace_ticks=g_pace_idle=g_pace_multi=g_pace_stalls=0;
    g_last_save=mono_ns();g_keybits=g_js_prevbits=0;clear_pad_state(PAD1);clear_pad_state(PAD2);
    gba_filter_reset(&g_filter);g_filter_live=g_filter_requested;g_filter_late=0;apply_video_settings();
    audio_open();g_state=ST_GAME;show_group(g_state);
    diag("rom-start=loaded; battery save keyed by SHA-256");
}


static void stop_game(void)
{
    if(!close_game())return;
    g_keybits=0;clear_pad_state(PAD1);clear_pad_state(PAD2);return_to_library();
}


/* --------------------------- raw gamepad backend -------------------------- */
typedef struct { u32 time; short value; unsigned char type; unsigned char number; } js_event;
/* struct input_event: struct timeval (two longs) + type, code, value. */
typedef struct { long sec, usec; unsigned short type, code; int value; } input_event_t;
#define JS_DEAD 8000
#define PAD_RETRY_FRAMES 30   /* ~0.5 s between reopen attempts */

static void input_test_log(const char *line, int truncate_file)
{
    int flags = O_WRONLY|O_CREAT|(truncate_file ? O_TRUNC : O_APPEND);
    int fd = open(g_input_test_log_path, flags, 0644);
    if (fd < 0) { diag("input-test log=open-failed"); return; }
    unsigned long n = strlen(line);
    if (write(fd, line, n) != (long)n || write(fd, "\n", 1) != 1) diag("input-test log=short-write");
    close(fd);
}

static unsigned int input_test_aliases(unsigned short code, char *indices, int cap)
{
    pad_t *p = PAD1;
    int used = 0, first = 1;
    unsigned int bits = code < EV_KEY_CODE_COUNT ? p->ev_key_pad[code] : 0;
    if (cap > 0) indices[0] = 0;
    for (unsigned int i = 0; i < p->button_count && i < 64; i++) if (p->button_map[i] == code) {
        int n = snprintf(indices+used, (unsigned long)(cap-used), "%s%u", first ? "" : ",", i);
        if (n > 0 && n < cap-used) used += n;
        first = 0;
        bits |= p->button_pad[i];
    }
    if (first) snprintf(indices, (unsigned long)cap, "none");
    return bits;
}

static const char *pad_bit_names(unsigned int bits, char *out, int cap)
{
    int n = 0;
    out[0] = 0;
#define ADD_BIT_NAME(bit,name) if (bits&(bit)) { n += snprintf(out+n, (unsigned long)(cap-n), "%s%s", n ? "|" : "", name); }
    ADD_BIT_NAME(PAD_A,"A"); ADD_BIT_NAME(PAD_B,"B");
    ADD_BIT_NAME(PAD_L,"L"); ADD_BIT_NAME(PAD_R,"R");
    ADD_BIT_NAME(PAD_SELECT,"Select"); ADD_BIT_NAME(PAD_START,"Start");
    ADD_BIT_NAME(PAD_UP,"Up"); ADD_BIT_NAME(PAD_DOWN,"Down");
    ADD_BIT_NAME(PAD_LEFT,"Left"); ADD_BIT_NAME(PAD_RIGHT,"Right");
#undef ADD_BIT_NAME
    if (!n) snprintf(out, (unsigned long)cap, "unmapped");
    return out;
}

static int input_test_expected_key(unsigned short code)
{
    if (g_input_test_step >= 0 && g_input_test_step < 13) return 1;
    if (g_input_test_step == 13) return code == BTN_DPAD_UP;
    if (g_input_test_step == 14) return code == BTN_DPAD_RIGHT;
    if (g_input_test_step == 15) return code == BTN_DPAD_DOWN;
    if (g_input_test_step == 16) return code == BTN_DPAD_LEFT;
    return 0;
}

static int input_test_trigger_code(unsigned short code)
{
    return code != ABS_X && code != ABS_Y && code != ABS_RX && code != ABS_RY &&
           code != ABS_HAT0X && code != ABS_HAT0Y;
}

static int input_test_axis_delta(unsigned short code, int value)
{
    if (code >= ABS_CODE_COUNT || !g_input_test_axis_rest_valid[code]) return -1;
    int rest = g_input_test_axis_rest[code];
    return value >= rest ? value - rest : rest - value;
}

static int input_test_trigger_threshold(unsigned short code)
{
    int threshold = JS_DEAD / 2;
    if (PAD1->ev_fd >= 0 && code < ABS_CODE_COUNT && PAD1->ev_abs_range_ok[code]) {
        int range = PAD1->ev_abs_max[code] - PAD1->ev_abs_min[code];
        if (range > 0 && range / 8 > threshold) threshold = range / 8;
    }
    return threshold;
}

static int input_test_axis_released(unsigned short code, int value)
{
    if ((g_input_test_step == 6 || g_input_test_step == 7) && input_test_trigger_code(code)) {
        int delta = input_test_axis_delta(code, value);
        return delta >= 0 && delta <= input_test_trigger_threshold(code) / 2;
    }
    return value == 0;
}

static int input_test_axis_matches(unsigned short code, int value)
{
    if (g_input_test_step == 6 || g_input_test_step == 7) {
        if (!input_test_trigger_code(code)) return 0;
        int delta = input_test_axis_delta(code, value);
        return delta >= input_test_trigger_threshold(code);
    }
    if (!value) return 0;
    if (g_input_test_step == 13) return code == ABS_HAT0Y && value < 0;
    if (g_input_test_step == 14) return code == ABS_HAT0X && value > 0;
    if (g_input_test_step == 15) return code == ABS_HAT0Y && value > 0;
    if (g_input_test_step == 16) return code == ABS_HAT0X && value < 0;
    return 0;
}

static void input_test_finish(int cancelled)
{
    char line[96];
    snprintf(line, sizeof line, "session_end status=%s", cancelled ? "cancelled" : "complete");
    input_test_log(line, 0);
    g_input_test_wait_release = 0;
    g_input_test_step = -1;
    clear_pad_state(PAD1); clear_pad_state(PAD2);
    g_keybits = g_js_prevbits = 0;
    g_state = ST_SETTINGS;
    str_copy(g_settings_message, cancelled ? "Input test cancelled; log saved" : "Input test complete; log saved", sizeof g_settings_message);
    show_group(g_state);
}

static void input_test_advance(void)
{
    g_input_test_wait_release = 0;
    g_input_test_step++;
    if (g_input_test_step >= INPUT_TEST_COUNT) { input_test_finish(0); return; }
    show_group(ST_INPUT_TEST);
}

static void input_test_skip(void)
{
    if (g_input_test_step < 0 || g_input_test_step >= INPUT_TEST_COUNT) {
        g_state = ST_SETTINGS; show_group(g_state);
        return;
    }
    char line[192];
    snprintf(line, sizeof line, "prompt=%s result=not_detected source=remote_skip", g_input_test_labels[g_input_test_step]);
    input_test_log(line, 0);
    snprintf(g_input_test_last, sizeof g_input_test_last, "%s: not detected", g_input_test_labels[g_input_test_step]);
    input_test_advance();
}

static void input_test_event(unsigned short type, unsigned short code, int value, int index, const char *source)
{
    if (g_state != ST_INPUT_TEST || g_input_test_step < 0 || g_input_test_step >= INPUT_TEST_COUNT) return;
    if (type == EV_ABS && code < ABS_CODE_COUNT && !g_input_test_axis_rest_valid[code]) {
        /* If a driver supplied no initial axis sample, use its first value as
         * rest. The next movement can then be distinguished from a trigger
         * resting at -32767 or another nonzero endpoint. */
        g_input_test_axis_rest[code] = value;
        g_input_test_axis_rest_valid[code] = 1;
    }
    if (g_input_test_wait_release) {
        if (type != g_input_test_wait_type || code != g_input_test_wait_code) return;
        if ((type == EV_KEY && value == 0) ||
            (type == EV_ABS && input_test_axis_released(code, value))) {
            char indices[96], actions[96], line[320];
            unsigned int bits = type == EV_KEY ? input_test_aliases(code, indices, sizeof indices) : 0;
            if (type == EV_ABS) { snprintf(indices, sizeof indices, "axis"); snprintf(actions, sizeof actions, "axis"); }
            else pad_bit_names(bits, actions, sizeof actions);
            snprintf(line, sizeof line, "prompt=%s phase=release source=%s type=%u code=0x%x value=%d index=%d js_indices=%s mapped=%s",
                     g_input_test_labels[g_input_test_step], source, type, code, value, index, indices, actions);
            input_test_log(line, 0);
            input_test_advance();
        }
        return;
    }
    if (type == EV_KEY && value == 1 && input_test_expected_key(code)) {
        char indices[96], actions[96], line[320];
        unsigned int bits = input_test_aliases(code, indices, sizeof indices);
        pad_bit_names(bits, actions, sizeof actions);
        snprintf(line, sizeof line, "prompt=%s phase=press source=%s type=%u code=0x%x value=%d index=%d js_indices=%s mapped=%s",
                 g_input_test_labels[g_input_test_step], source, type, code, value, index, indices, actions);
        input_test_log(line, 0);
        snprintf(g_input_test_last, sizeof g_input_test_last, "%.24s code=0x%x indices=%.48s map=%.56s",
                 g_input_test_labels[g_input_test_step], code, indices, actions);
        g_input_test_wait_type = type; g_input_test_wait_code = code; g_input_test_wait_release = 1;
        show_group(ST_INPUT_TEST);
    } else if (type == EV_ABS && input_test_axis_matches(code, value)) {
        char line[256];
        snprintf(line, sizeof line, "prompt=%s phase=press source=%s type=%u code=0x%x value=%d index=%d js_indices=axis mapped=axis",
                 g_input_test_labels[g_input_test_step], source, type, code, value, index);
        input_test_log(line, 0);
        snprintf(g_input_test_last, sizeof g_input_test_last, "%s axis code=0x%x value=%d", g_input_test_labels[g_input_test_step], code, value);
        g_input_test_wait_type = type; g_input_test_wait_code = code; g_input_test_wait_release = 1;
        show_group(ST_INPUT_TEST);
    }
}

static void input_test_start(void)
{
    char line[320];
    pad_t *p = PAD1;
    g_input_test_step = 0; g_input_test_wait_release = 0; g_input_test_last[0] = 0;
    snprintf(line, sizeof line, "session_start identity=%s js=%d event=%d buttons=%u axes=%u",
             p->selected_usb_id[0] ? p->selected_usb_id : "unknown", p->device_index, p->ev_device_index,
             p->button_count, p->axis_count);
    input_test_log(line, 1);
    memset(g_input_test_axis_rest_valid, 0, sizeof g_input_test_axis_rest_valid);
    if (p->ev_fd < 0) for (unsigned int i = 0; i < p->axis_count && i < 64; ++i) {
        unsigned int code = p->axis_map_ok ? p->axis_map[i] : i;
        if (code < ABS_CODE_COUNT && p->axis_raw_valid[i]) {
            g_input_test_axis_rest[code] = p->axis_raw[i];
            g_input_test_axis_rest_valid[code] = 1;
        }
    }
    if (p->ev_fd >= 0) for (unsigned int code = 0; code < ABS_CODE_COUNT; ++code) {
        input_absinfo value;
        if (ioctl(p->ev_fd, EVIOCGABS(code), &value) == 0) {
            g_input_test_axis_rest[code] = value.value;
            g_input_test_axis_rest_valid[code] = 1;
        }
    }
    clear_pad_state(PAD1); clear_pad_state(PAD2);
    g_keybits = g_js_prevbits = 0;
    g_state = ST_INPUT_TEST; show_group(g_state);
}

static unsigned int js_button_to_pad(unsigned short code)
{
    if (code == BTN_A) return PAD_A;
    if (code == BTN_B) return PAD_B;
    if (code == BTN_SELECT) return PAD_SELECT;
    if (code == BTN_START) return PAD_START;
    if (code == 0x136) return PAD_L;
    if (code == 0x137) return PAD_R;
    if (code == BTN_DPAD_UP) return PAD_UP;
    if (code == BTN_DPAD_DOWN) return PAD_DOWN;
    if (code == BTN_DPAD_LEFT) return PAD_LEFT;
    if (code == BTN_DPAD_RIGHT) return PAD_RIGHT;
    return 0;
}

static void js_close_device(pad_t *p)
{
    clear_pad_state(p);
    if (p->ev_fd >= 0) {
        ioctl(p->ev_fd, EVIOCGRAB, 0);
        close(p->ev_fd); p->ev_fd = -1;
    }
    if (p->js_fd >= 0) { close(p->js_fd); p->js_fd = -1; }
    p->capture_exclusive = 0;
    p->ev_device_index=-1;
    p->ev_button_bits=0;
    p->mapped_axis_bits=0;
    for(int i=0;i<64;i++) p->axis_state[i]=0;
    for(int i=0;i<ABS_CODE_COUNT;i++) p->ev_abs_range_ok[i]=0;
}

static void pad_forget_device(pad_t *p)
{
    js_close_device(p);
    p->device_index=-1; p->device_name[0]=0; p->selected_js_index=-1;
    p->jsbits=p->button_bits=p->stick_bits=p->hat_bits=0;
    p->open_retry_frame=0; p->capture_retry_frame=0;
    str_copy(p->capture_status,"not connected",sizeof p->capture_status);
}

static int js_read_name(int index, char *name, int cap)
{
    char path[32];
    snprintf(path,sizeof path,TIZGBA_DEV_INPUT "/js%d",index);
    int fd=open(path,O_RDONLY|O_NONBLOCK);
    if(fd<0) return 0;
    name[0]=0;
    int ok=ioctl(fd,JSIOCGNAME(cap),name)==0;
    name[cap-1]=0;
    close(fd);
    return ok && name[0];
}

/* jsN and its matching eventN point to the same input-device parent in
   sysfs; compare those links so capture targets exactly the selected pad. */
static int sysfs_input_parent(const char *handler, int index, char *target, int cap)
{
    char path[96];
    snprintf(path,sizeof path,TIZGBA_SYS_INPUT "/%s%d/device",handler,index);
    long n=readlink(path,target,(unsigned long)(cap-1));
    if(n<=0 || n>=cap) return 0;
    target[n]=0;
    return 1;
}

static int read_small_file(const char *path, char *out, int cap)
{
    int fd=open(path,O_RDONLY|O_NONBLOCK); if(fd<0) return 0;
    long n=read(fd,out,(unsigned long)(cap-1)); close(fd);
    if(n<=0 || n>=cap) return 0;
    while(n>0 && (out[n-1]=='\n' || out[n-1]=='\r' || out[n-1]==' ')) n--;
    out[n]=0; return n>0;
}

/* One pad can expose several joystick nodes under one USB identity: the
   DualShock 4 adds a button-less "Motion Sensors" joystick and has no USB
   serial. Only a node whose input device reports key capabilities is a
   controller. The root watcher applies the same rule (TizNES 1.0.26). */
static int js_has_buttons(int index)
{
    char path[80], caps[512];
    snprintf(path,sizeof path,TIZGBA_SYS_INPUT "/js%d/device/capabilities/key",index);
    if(!read_small_file(path,caps,sizeof caps)) return 0;
    for(const char *c=caps;*c;c++)
        if((*c>='1'&&*c<='9')||(*c>='a'&&*c<='f')||(*c>='A'&&*c<='F')) return 1;
    return 0;
}

static int usb_identity_for_js(int index, char *id, int cap)
{
    char link[96],resolved[512],work[512],vendor[32],product[32],serial[96];
    snprintf(link,sizeof link,TIZGBA_SYS_INPUT "/js%d/device",index);
    if(!realpath(link,resolved)) return 0;
    str_copy(work,resolved,sizeof work);
    for(int depth=0;depth<12;depth++) {
        char path[600];
        snprintf(path,sizeof path,"%s/idVendor",work);
        if(read_small_file(path,vendor,sizeof vendor)) {
            snprintf(path,sizeof path,"%s/idProduct",work);
            if(!read_small_file(path,product,sizeof product)) return 0;
            snprintf(path,sizeof path,"%s/serial",work);
            if(!read_small_file(path,serial,sizeof serial)) serial[0]=0;
            char port[64]; char *slash=work+strlen(work);
            while(slash>work && slash[-1]!='/') slash--;
            if(slash<=work) return 0;
            str_copy(port,slash,sizeof port);
            slash[-1]=0;
            int n=snprintf(id,(unsigned long)cap,"%s:%s:%s:%s",vendor,product,serial,port);
            /* A partial identity cannot safely identify a physical controller. */
            if(n<0 || n>=cap) { id[0]=0;return 0; }
            return 1;
        }
        char *slash=work+strlen(work);
        while(slash>work && slash[-1]!='/') slash--;
        if(slash<=work) break;
        slash[-1]=0;
    }
    return 0;
}

/* "vid:pid:serial:port": exact equality preferred; a pad with a USB serial
   is also recognized on another port (same rule as the root watcher). */
static int identity_split(const char *id, char f[4][96])
{
    int k=0,j=0;
    for(int i=0;i<4;i++) f[i][0]=0;
    for(const char *c=id;*c;c++) {
        if(*c==':') { f[k][j]=0; if(++k>3) return 0; j=0; continue; }
        if(j<95) f[k][j++]=*c;
    }
    f[k][j]=0;
    return k==3;
}
static int identity_loose_match(const char *a, const char *b)
{
    char fa[4][96], fb[4][96];
    if(!identity_split(a,fa) || !identity_split(b,fb) || !fa[2][0]) return 0;
    return strncmp(fa[0],fb[0],96)==0 && strncmp(fa[1],fb[1],96)==0 && strncmp(fa[2],fb[2],96)==0;
}
static int identity_matches(const char *saved, const char *current)
{
    if(!saved[0] || !current[0]) return 0;
    return strncmp(saved,current,160)==0 || identity_loose_match(saved,current);
}

/* jsN for the saved identity, or -1. Re-resolved on every retry, so a pad
   plugged in after launch or re-enumerated after a replug is found. */
static int pad_resolve_index(const pad_t *p)
{
    if(!p->selected_usb_id[0]) return p->selected_js_index;
    int loose=-1, loose_count=0;
    for(int i=0;i<16;i++) {
        char identity[160];
        if(!usb_identity_for_js(i,identity,sizeof identity) || !js_has_buttons(i)) continue;
        if(strncmp(identity,p->selected_usb_id,sizeof identity)==0) return i;
        if(identity_loose_match(p->selected_usb_id,identity)) { loose=i; loose_count++; }
    }
    return loose_count==1 ? loose : -1;
}

/* tizgba-input.conf is read by the root watcher: exclusive=, identity=. */
static void save_input_config(void)
{
    char data[480];
    int n=snprintf(data,sizeof data,"exclusive=%d\nidentity=%s\n",
        g_exclusive_preferred?1:0,PAD1->selected_usb_id);
    if(n<=0 || n>=(int)sizeof data) return;
    if(PAD2->selected_usb_id[0]) {
        int m=snprintf(data+n,sizeof data-(unsigned long)n,"identity2=%s\n",PAD2->selected_usb_id);
        if(m<=0 || n+m>=(int)sizeof data) return;
        n+=m;
    }
    if(!atomic_write(g_input_config_path,data,(unsigned long)n))diag("input-config-save=failed");
}

static void load_input_config(void)
{
    char data[480]; int fd=open(g_input_config_path,O_RDONLY);
    if(fd<0) return;
    long n=read(fd,data,sizeof data-1); close(fd); if(n<=0) return; data[n]=0;
    char *p=data;
    while(*p) {
        char *end=p; while(*end && *end!='\n') end++;
        if(*end) *end++=0;
        if(strncmp(p,"exclusive=0",11)==0) g_exclusive_preferred=0;
        else if(strncmp(p,"exclusive=1",11)==0) g_exclusive_preferred=1;
        else if(strncmp(p,"identity=",9)==0) str_copy(PAD1->selected_usb_id,p+9,sizeof PAD1->selected_usb_id);
        else if(strncmp(p,"identity2=",10)==0) str_copy(PAD2->selected_usb_id,p+10,sizeof PAD2->selected_usb_id);
        p=end;
    }
    if(PAD2->selected_usb_id[0] && identity_matches(PAD1->selected_usb_id,PAD2->selected_usb_id))
        PAD2->selected_usb_id[0]=0;   /* one physical controller cannot occupy both slots */

}

/* USB controllers that can be chosen; button-less joystick nodes are not
   listed. */
static void scan_js_devices(int slot)
{
    pad_t *p=&g_pads[slot];
    g_device_count=0;
    g_device_sel=0;
    if(slot==1) {
        g_device_indices[0]=-1;
        str_copy(g_device_names[0],"None (one GBA keypad)",sizeof g_device_names[0]);
        g_device_usb_ids[0][0]=0;
        g_device_count=1;
    }
    for(int i=0;i<16 && g_device_count<VIS_ROWS;i++) {
        char name[128],identity[160];
        if(usb_identity_for_js(i,identity,sizeof identity) && js_has_buttons(i)) {
            if(!js_read_name(i,name,sizeof name)) snprintf(name,sizeof name,"USB gamepad js%d",i);
            int row=g_device_count;
            g_device_indices[row]=i;
            str_copy(g_device_names[row],name,sizeof g_device_names[row]);
            str_copy(g_device_usb_ids[row],identity,sizeof g_device_usb_ids[row]);
            if(p->selected_usb_id[0] && identity_matches(p->selected_usb_id,identity)) g_device_sel=row;
            else if(!p->selected_usb_id[0] && slot==0 && i==p->selected_js_index) g_device_sel=row;
            g_device_count++;
        }
    }
    if(g_device_count && g_device_sel>=g_device_count) g_device_sel=0;
}

static int target_user_mapped(const pad_t *p,unsigned int bit)
{
    /* This predicate controls digital defaults only. Analog bindings cannot
     * disable the D-pad, and remapping the D-pad cannot disable the stick. */
    for(int c=0;c<EV_KEY_CODE_COUNT;c++)if(p->ev_key_pad[c]&bit)return 1;
    for(int i=0;i<64;i++)if(p->custom_mapped[i] && (p->button_pad[i]&bit))return 1;
    return ((p->axis_neg_pad[ABS_HAT0X]|p->axis_pos_pad[ABS_HAT0X]|p->axis_neg_pad[ABS_HAT0Y]|p->axis_pos_pad[ABS_HAT0Y])&bit)!=0;
}


static void js_open_device(pad_t *p, int requested_index)
{
    int wanted=requested_index;
    if(wanted<0) wanted=p->selected_js_index;
    js_close_device(p);
    p->device_index=-1; p->device_name[0]=0;
    if(wanted>=0 && wanted<16) {
        char path[32];
        snprintf(path,sizeof path,TIZGBA_DEV_INPUT "/js%d",wanted);
        p->js_fd=open(path,O_RDONLY|O_NONBLOCK);
        if(p->js_fd>=0) {
            p->device_index=wanted;
            ioctl(p->js_fd,JSIOCGNAME(sizeof p->device_name),p->device_name);
            p->device_name[sizeof p->device_name-1]=0;
        }
    }
    /* Player 1 only, and only when nothing was ever selected: take the first
       real controller (skipping button-less nodes and player 2's pad), else
       a Tizen "Virtual Gamepad". */
    if(p->js_fd<0 && p->slot==0 && requested_index<0 && p->selected_js_index<0 && !p->selected_usb_id[0]) {
        int fallback_fd=-1, fallback_index=-1;
        char fallback_name[128]; fallback_name[0]=0;
        for(int i=0;i<16;i++) {
            char path[32],name[128],identity[160];
            if(PAD2->selected_usb_id[0] && usb_identity_for_js(i,identity,sizeof identity) &&
               identity_matches(PAD2->selected_usb_id,identity)) continue;
            snprintf(path,sizeof path,TIZGBA_DEV_INPUT "/js%d",i);
            int fd=open(path,O_RDONLY|O_NONBLOCK); if(fd<0) continue;
            name[0]=0; ioctl(fd,JSIOCGNAME(sizeof name),name); name[sizeof name-1]=0;
            if(strncmp(name,"Virtual Gamepad",15)==0) {
                if(fallback_fd<0) { fallback_fd=fd; fallback_index=i; str_copy(fallback_name,name,sizeof fallback_name); }
                else close(fd);
            } else if(!js_has_buttons(i)) {
                close(fd);
            } else {
                p->js_fd=fd; p->device_index=i; str_copy(p->device_name,name,sizeof p->device_name); break;
            }
        }
        if(p->js_fd<0 && fallback_fd>=0) {
            p->js_fd=fallback_fd; p->device_index=fallback_index;
            str_copy(p->device_name,fallback_name,sizeof p->device_name);
        } else if(fallback_fd>=0) close(fallback_fd);
    }
    if(p->js_fd<0) {
        char fail[96]; int err=*__errno_location();
        p->capture_exclusive=0; str_copy(p->capture_status,"no selected device",sizeof p->capture_status);
        snprintf(fail,sizeof fail,"%sinput joystick=open-failed index=%d errno=%d",pad_tag(p),wanted,err);
        diag_rl(4+p->slot,fail); return;
    }
    p->selected_js_index=p->device_index;
    if(!p->selected_usb_id[0]) usb_identity_for_js(p->device_index,p->selected_usb_id,sizeof p->selected_usb_id);
    if(p->selected_usb_id[0]) save_input_config();
    pad_apply_mapping(p);
    p->axis_map_ok = ioctl(p->js_fd, JSIOCGAXMAP, p->axis_map) == 0;
    p->button_map_ok = ioctl(p->js_fd, JSIOCGBTNMAP, p->button_map) == 0;
    ioctl(p->js_fd, JSIOCGAXES, &p->axis_count);
    ioctl(p->js_fd, JSIOCGBUTTONS, &p->button_count);
    p->hat_x_axis = p->hat_y_axis = -1;
    if (p->axis_map_ok) {
        for (int i=0;i<p->axis_count && i<64;i++) {
            if(p->axis_map[i]==ABS_X) p->x_axis=i;
            else if(p->axis_map[i]==ABS_Y) p->y_axis=i;
            else if(p->axis_map[i]==ABS_HAT0X) p->hat_x_axis=i;
            else if(p->axis_map[i]==ABS_HAT0Y) p->hat_y_axis=i;
        }
    }
    if(p->hat_x_axis<0 && p->axis_count>=6) { p->hat_x_axis=4; p->hat_y_axis=5; }
    else if(p->hat_x_axis<0 && p->axis_count>=4) { p->hat_x_axis=2; p->hat_y_axis=3; }
    if(!p->button_map_ok) {
        p->button_map[0]=BTN_A; p->button_map[1]=BTN_B;
        p->button_map[4]=0x136; p->button_map[5]=0x137;
        p->button_map[8]=BTN_SELECT; p->button_map[9]=BTN_START;
    }
    /* Default button bindings apply only to GBA inputs the user has not
       mapped. Otherwise the stick clicks (also reported as Select/Start by the
       joystick map) would keep triggering those inputs after a full remap. */
    for(int i=0;i<64;i++) if(!p->custom_mapped[i]) {
        unsigned int pad=js_button_to_pad(p->button_map[i]);
        if(pad && target_user_mapped(p,pad)) pad=0;
        p->button_pad[i]=(uint16_t)pad;
    }

    /* Match through sysfs parent links, then open only the selected pad's
       event node. Never probe/grab unrelated event nodes such as the TV remote. */
    p->capture_exclusive=0;
    str_copy(p->capture_status,"shared; Tizen shortcuts may still fire",sizeof p->capture_status);
    char js_parent[256],event_parent[256],path[40],diag[160];
    int matching_event=-1;
    js_parent[0]=0;
    if(sysfs_input_parent("js",p->device_index,js_parent,sizeof js_parent)) {
        for(int e=0;e<32;e++) {
            if(sysfs_input_parent("event",e,event_parent,sizeof event_parent) &&
               strncmp(js_parent,event_parent,sizeof js_parent)==0) {
                matching_event=e; break;
            }
        }
    }
    if(matching_event>=0 && g_exclusive_preferred) {
        snprintf(path,sizeof path,TIZGBA_DEV_INPUT "/event%d",matching_event);
        /* access() reports the permission bits the root-side watcher is
           supposed to have granted, without needing a shell session on the TV
           to run `ls -l`: this is what actually distinguishes "watcher has not
           granted this node yet" from every other reason open() can fail. */
        int can_r=access(path,R_OK)==0, can_w=access(path,W_OK)==0;
        int fd=open(path,O_RDONLY|O_NONBLOCK);
        if(fd<0) {
            int err=*__errno_location();
            snprintf(diag,sizeof diag,"%sinput evdev=candidate=%d open_errno=%d access_r=%d access_w=%d",
                pad_tag(p),matching_event,err,can_r,can_w);
            snprintf(p->capture_status,sizeof p->capture_status,"shared; event%d access denied (%d)",matching_event,err);
            diag_rl(p->slot*2,diag);
        } else {
            if(ioctl(fd,EVIOCGRAB,1)==0) {
                p->ev_fd=fd; p->capture_exclusive=1;
                for(int axis=0;axis<ABS_CODE_COUNT;axis++) {
                    input_absinfo info;
                    if(ioctl(p->ev_fd,EVIOCGABS(axis),&info)==0 && info.maximum>info.minimum) {
                        p->ev_abs_min[axis]=info.minimum;
                        p->ev_abs_max[axis]=info.maximum;
                        p->ev_abs_range_ok[axis]=1;
                    }
                }
                str_copy(p->capture_status,"exclusive capture active",sizeof p->capture_status);
                snprintf(diag,sizeof diag,"%sinput evdev=exclusive event=%d",pad_tag(p),matching_event);
                diag_rl(p->slot*2,diag);
            } else {
                int err=*__errno_location();
                snprintf(diag,sizeof diag,"%sinput evdev=candidate=%d grab_errno=%d access_r=%d access_w=%d",
                    pad_tag(p),matching_event,err,can_r,can_w);
                snprintf(p->capture_status,sizeof p->capture_status,"shared; event%d grab denied (%d)",matching_event,err);
                diag_rl(p->slot*2,diag); close(fd);
            }
        }
    } else if(matching_event<0) {
        char nodiag[256];
        snprintf(nodiag,sizeof nodiag,"%sinput evdev=no-sysfs-match js_parent=%.200s",
            pad_tag(p),js_parent[0]?js_parent:"(unresolved)");
        diag_rl(p->slot*2,nodiag);
    } else {
        snprintf(diag,sizeof diag,"%sinput evdev=exclusive-capture-disabled-by-setting",pad_tag(p));
        diag_rl(p->slot*2,diag);
    }
    p->ev_device_index=matching_event;
    char line[224];
    snprintf(line,sizeof line,"%sinput js%d=open name=%s axes=%u buttons=%u exclusive=%d",
        pad_tag(p),p->device_index,p->device_name,p->axis_count,p->button_count,p->capture_exclusive);
    diag_rl(p->slot*2+1,line);
    if(p->ev_fd>=0)ev_resync(p);
}

static void js_log_first_event(pad_t *p, int is_button, unsigned int number,
                               unsigned short code, short value,
                               unsigned int pad_bit)
{
    unsigned char *seen = is_button ? p->button_logged : p->axis_logged;
    if (number >= 64 || seen[number]) return;
    seen[number] = 1;
    char line[128];
    snprintf(line, sizeof line,
        "%sinput js%d %s index=%u code=0x%x value=%d nes_bit=0x%x",
        pad_tag(p),p->device_index,is_button ? "button" : "axis", number, code, value, pad_bit);
    diag(line);
}

static void mapping_advance(pad_t *p)
{
    g_map_step++;g_map_armed=0;g_map_timed_out=0;g_map_deadline=mono_ns()+15000000000ULL;
    if(g_map_step>=(g_map_bank?4:10)) {
        g_map_active=0;g_state=ST_SETTINGS;
        str_copy(g_settings_message,"Controller model mapping saved",sizeof g_settings_message);pad_save_model_mapping(p);
        clear_pad_state(p);g_js_prevbits=0;show_group(g_state);
    }else render_mapping();
}


static void mapping_clear_target(pad_t *p, uint16_t target)
{
    /* Digital and analog banks deliberately never erase each other. */
    for(int i=0;i<64;i++) {
        int hat=i==ABS_HAT0X || i==ABS_HAT0Y;
        if((g_map_bank && !hat) || (!g_map_bank && hat)) { p->axis_neg_pad[i]&=(uint16_t)~target;p->axis_pos_pad[i]&=(uint16_t)~target; }
        if(!g_map_bank) p->button_pad[i]&=(uint16_t)~target;
    }
    if(!g_map_bank) for(int i=0;i<EV_KEY_CODE_COUNT;i++)p->ev_key_pad[i]&=(uint16_t)~target;
}



static int pad_is_mapping(const pad_t *p)
{
    return g_state==ST_MAPPING && g_map_active && p->slot==g_map_slot && g_map_step>=0 && g_map_step<(g_map_bank?4:10);
}



static void clear_pad_state(pad_t *p)
{
    p->jsbits=p->button_bits=p->stick_bits=p->hat_bits=p->ev_button_bits=p->mapped_axis_bits=0;
    memset(p->key_down,0,sizeof p->key_down);memset(p->button_down,0,sizeof p->button_down);memset(p->axis_state,0,sizeof p->axis_state);
    p->dropped=0;
}
static int pad_released(const pad_t *p)
{
    for(int i=0;i<EV_KEY_CODE_COUNT;i++)if(p->key_down[i])return 0;
    for(int i=0;i<64;i++)if(p->button_down[i])return 0;
    /* Only axes involved in the selected bank gate releases; noisy triggers
     * and motion sensors cannot keep the digital mapper permanently disarmed. */
    for(int i=0;i<64;i++) {
        int hat=i==ABS_HAT0X || i==ABS_HAT0Y;
        if(p->axis_state[i] && (g_map_bank?(i==ABS_X || i==ABS_Y || i==ABS_RX || i==ABS_RY || p->axis_neg_pad[i] || p->axis_pos_pad[i]):hat))return 0;
    }
    return 1;
}
static void mapping_retry(void)
{
    pad_t *p=&g_pads[g_map_slot];g_map_armed=0;g_map_timed_out=0;g_map_deadline=mono_ns()+15000000000ULL;
    /* Re-sample held evdev keys; a TV overlay may have hidden their release. */
    if(p->ev_fd>=0) {
        unsigned char keys[96];memset(keys,0,sizeof keys);
        if(ioctl(p->ev_fd,EVIOCGKEY(sizeof keys),keys)==0)
            for(int i=0;i<EV_KEY_CODE_COUNT;i++)p->key_down[i]=!!(keys[i>>3]&(1<<(i&7)));
        for(int axis=0;axis<ABS_CODE_COUNT;axis++) {
            input_absinfo v;
            if(p->ev_abs_range_ok[axis] && ioctl(p->ev_fd,EVIOCGABS(axis),&v)==0) {
                int mid=v.minimum+(v.maximum-v.minimum)/2,dead=(v.maximum-v.minimum)/4;
                p->axis_state[axis]=(signed char)(v.value<mid-dead?-1:v.value>mid+dead?1:0);
            }
        }
    }
    render_mapping();
}
static void mapping_tick(void)
{
    if(g_state!=ST_MAPPING || !g_map_active)return;
    pad_t *p=&g_pads[g_map_slot];unsigned long long now=mono_ns();
    if(!pad_present(p)) { if(g_map_armed){g_map_armed=0;render_mapping();}return; }
    if(!g_map_timed_out && now>=g_map_deadline) { g_map_timed_out=1;g_map_armed=0;render_mapping(); }
    if(!g_map_timed_out && !g_map_armed && pad_released(p)) { g_map_armed=1;render_mapping(); }
}
static void mapping_cancel(void)
{
    pad_t *p=&g_pads[g_map_slot];
    if(g_map_active)mapping_unpack(p,g_map_backup,sizeof g_map_backup);
    g_map_active=0;g_map_step=-1;clear_pad_state(p);g_js_prevbits=0;g_state=ST_SETTINGS;
    str_copy(g_settings_message,"Mapping cancelled; previous profile retained",sizeof g_settings_message);show_group(g_state);
}
static void rebuild_buttons(pad_t *p)
{
    p->button_bits=p->ev_button_bits=0;
    for(int i=0;i<64;i++)if(p->button_down[i])p->button_bits|=p->button_pad[i];
    for(int code=0;code<EV_KEY_CODE_COUNT;code++)if(p->key_down[code]) {
        unsigned int bit=p->ev_key_pad[code];
        if(!bit) {
            int mapped=0;for(int i=0;i<64;i++)if(p->button_map[i]==code){mapped=1;bit|=p->button_pad[i];}
            if(!mapped){bit=js_button_to_pad((unsigned short)code);if(target_user_mapped(p,bit))bit=0;}
        }
        p->ev_button_bits|=bit;
    }
}

static void js_button_event(pad_t *p,unsigned int index,unsigned short code,int value)
{
    if(index>=64)return;
    p->button_down[index]=value!=0;
    js_log_first_event(p,1,index,code,(short)value,p->button_pad[index]);
    if(pad_is_mapping(p)) {
        mapping_tick();
        if(!g_map_bank && value==1 && g_map_armed && !g_map_timed_out) {
            uint16_t target=g_map_targets[g_map_step];mapping_clear_target(p,target);
            p->button_pad[index]=target;p->custom_mapped[index]=1;
            if(code<EV_KEY_CODE_COUNT)p->ev_key_pad[code]=target;
            snprintf(g_map_detected,sizeof g_map_detected,"button %u code 0x%x -> 0x%x",index,code,target);mapping_advance(p);
        }
        return;
    }
    if(g_state==ST_MAPPING)return;
    if(code==BTN_MODE && !p->custom_mapped[index] && !p->ev_key_pad[code]) {
        if(value==1) { if(g_state==ST_GAME || g_state==ST_LIBRARY)settings_open();else if(g_state==ST_SETTINGS)settings_close(); }
        return;
    }
    rebuild_buttons(p);
}


/* The evdev key code is the device's stable identity; keep a direct map as
   well as the joydev-index map (some drivers emit codes with no BTNMAP). */
static void ev_key_event(pad_t *p,unsigned short code,int value)
{
    if(code>=EV_KEY_CODE_COUNT)return;
    p->key_down[code]=value!=0;
    for(unsigned i=0;i<64;i++)if(p->button_map[i]==code)p->button_down[i]=value!=0;
    if(pad_is_mapping(p)) {
        mapping_tick();
        if(!g_map_bank && value==1 && g_map_armed && !g_map_timed_out) {
            uint16_t target=g_map_targets[g_map_step];mapping_clear_target(p,target);
            for(unsigned i=0;i<64;i++)if(p->button_map[i]==code){p->button_pad[i]=target;p->custom_mapped[i]=1;}
            p->ev_key_pad[code]=target;
            snprintf(g_map_detected,sizeof g_map_detected,"event code 0x%x -> 0x%x",code,target);mapping_advance(p);
        }
        return;
    }
    if(g_state!=ST_MAPPING)rebuild_buttons(p);
}


/* Axes are identified by their Linux ABS code, never by a joydev index (the
   numberings differ; mixing them made ABS_RZ look like the D-pad hat in
   TizNES 1.0.21 and held Down). */
static void js_axis_event(pad_t *p,unsigned int number,unsigned short code,int value,int evdev)
{
    if(number>=64)return;
    p->axis_raw[number]=(short)value;p->axis_raw_valid[number]=1;
    int hat=code==ABS_HAT0X || code==ABS_HAT0Y,direction=0;
    if(hat)direction=value<0?-1:value>0?1:0;
    else if(evdev && code<ABS_CODE_COUNT && p->ev_abs_range_ok[code]) {
        int min=p->ev_abs_min[code],max=p->ev_abs_max[code],mid=min+(max-min)/2,dead=(max-min)/4;
        direction=value<mid-dead?-1:value>mid+dead?1:0;
    }else direction=value<=-JS_DEAD?-1:value>=JS_DEAD?1:0;
    p->axis_state[number]=(signed char)direction;
    if(pad_is_mapping(p)) {
        mapping_tick();
        if(g_map_step<4 && direction && g_map_armed && !g_map_timed_out && (g_map_bank?!hat:hat)) {
            uint16_t target=g_map_targets[g_map_step];mapping_clear_target(p,target);
            if(direction<0)p->axis_neg_pad[number]=target;else p->axis_pos_pad[number]=target;
            snprintf(g_map_detected,sizeof g_map_detected,"ABS 0x%x %s -> 0x%x",code,direction<0?"negative":"positive",target);mapping_advance(p);
        }
        return;
    }
    p->mapped_axis_bits=p->stick_bits=p->hat_bits=0;
    for(int i=0;i<64;i++) {
        if(!p->axis_state[i])continue;
        int is_hat=i==ABS_HAT0X || i==ABS_HAT0Y;
        uint16_t mapped=p->axis_state[i]<0?p->axis_neg_pad[i]:p->axis_pos_pad[i];
        if(mapped){p->mapped_axis_bits|=mapped;continue;}
        unsigned int bit=0;
        if(i==ABS_X || i==ABS_HAT0X)bit=p->axis_state[i]<0?PAD_LEFT:PAD_RIGHT;
        if(i==ABS_Y || i==ABS_HAT0Y)bit=p->axis_state[i]<0?PAD_UP:PAD_DOWN;
        if(is_hat) { if(!target_user_mapped(p,bit))p->hat_bits|=bit; }
        else {
            int custom=0;for(int a=0;a<64;a++)if(a!=ABS_HAT0X && a!=ABS_HAT0Y && ((p->axis_neg_pad[a]|p->axis_pos_pad[a])&bit))custom=1;
            if(!custom)p->stick_bits|=bit;
        }
    }
}


/* After SYN_DROPPED, rebuild held keys/axes from the device's state. */
static void ev_resync(pad_t *p)
{
    if(p->ev_fd<0)return;
    clear_pad_state(p);unsigned char keys[96];memset(keys,0,sizeof keys);
    if(ioctl(p->ev_fd,EVIOCGKEY(sizeof keys),keys)==0)
        for(int c=0;c<EV_KEY_CODE_COUNT;c++)p->key_down[c]=!!(keys[c>>3]&(1<<(c&7)));
    rebuild_buttons(p);
    if(pad_is_mapping(p)){g_map_armed=0;mapping_retry();}
    else if(g_state!=ST_INPUT_TEST)for(int axis=0;axis<ABS_CODE_COUNT;axis++){
        input_absinfo v;if(p->ev_abs_range_ok[axis] && ioctl(p->ev_fd,EVIOCGABS(axis),&v)==0)js_axis_event(p,axis,axis,v.value,1);
    }
    diag("input evdev=resynchronized");
}


static void pad_poll(pad_t *p)
{
    if(p->js_fd>=0 && !p->map_loaded) { js_open_device(p,p->device_index); }
    if(p->ev_fd<0 && g_exclusive_preferred && p->js_fd>=0 && g_input_frame>=p->capture_retry_frame) {
        int selected=p->device_index;
        p->capture_retry_frame=g_input_frame+PAD_RETRY_FRAMES;
        js_open_device(p,selected);
    }
    if(p->js_fd<0) {
        if(g_input_frame<p->open_retry_frame) return;
        p->open_retry_frame=g_input_frame+PAD_RETRY_FRAMES;
        if(p->selected_usb_id[0]) p->selected_js_index=pad_resolve_index(p);
        js_open_device(p,p->selected_js_index);
        if(p->js_fd<0) return;
    }
    long n=-1;
    /* Only a shared (joydev) pad stamps g_js_activity_frame: Tizen also turns
       its buttons into navigation keys, which on_key_down must then drop as
       duplicates. An exclusively grabbed pad (ev_fd) never reaches Tizen, so
       its activity, e.g. a DualShock 4's analog sticks jittering, must not
       swallow genuine TV-remote keys. */
    if(p->ev_fd>=0) {
        input_event_t e;

        while((n=read(p->ev_fd,&e,sizeof e))==(long)sizeof e) {
            if(e.type==EV_SYN && e.code==SYN_DROPPED) { p->dropped=1; continue; }
            if(p->dropped) {
                if(e.type==EV_SYN && e.code==SYN_REPORT) { p->dropped=0; ev_resync(p); }
                continue;
            }
            if(g_state==ST_INPUT_TEST && p->slot==0 && (e.type==EV_KEY || e.type==EV_ABS)) {
                int index=-1;
                if(e.type==EV_KEY) for(unsigned int i=0;i<p->button_count && i<64;i++)
                    if(p->button_map[i]==e.code) { index=(int)i; break; }
                input_test_event(e.type,e.code,e.value,index,"evdev");
            } else if(pad_is_mapping(p) && e.type==EV_KEY) {
                if(g_state==ST_MAPPING) ev_key_event(p,e.code,e.value);
            } else if(e.type==EV_KEY) {
                ev_key_event(p,e.code,e.value);
                for(unsigned int i=0;i<p->button_count && i<64;i++)
                    if(p->button_map[i]==e.code) { js_button_event(p,i,e.code,e.value); break; }
            } else if(e.type==EV_ABS) {
                js_axis_event(p,e.code,e.code,e.value,1);
            }
        }
    } else {
        js_event e;
        while((n=read(p->js_fd,&e,sizeof e))==(long)sizeof e) {
            g_js_activity_frame=g_input_frame; g_js_activity_valid=1;
            unsigned char t=e.type&0x7f;
            if((e.type&0x80) && pad_is_mapping(p))g_map_armed=0;
            if(t==0x01 && g_state==ST_INPUT_TEST && p->slot==0)
                input_test_event(EV_KEY,e.number<64?p->button_map[e.number]:0,e.value,e.number,"joydev");
            else if(t==0x02 && g_state==ST_INPUT_TEST && p->slot==0)
                input_test_event(EV_ABS,e.number<64?p->axis_map[e.number]:0xffff,e.value,e.number,"joydev");
            else if(t==0x01) js_button_event(p,e.number,e.number<64?p->button_map[e.number]:0,e.value);
            else if(t==0x02) {
                unsigned short axis_code=(e.number<64 && p->axis_map_ok)?p->axis_map[e.number]:e.number;
                js_axis_event(p,axis_code,axis_code,e.value,0);
            }
        }
    }
    if(n==0 || (n<0 && *__errno_location()!=EAGAIN && *__errno_location()!=EINTR)) {
        char line[64];
        snprintf(line,sizeof line,"%sinput device=disconnected",pad_tag(p));
        diag(line); js_close_device(p);
        p->button_bits=p->stick_bits=p->hat_bits=0;
    }
    p->jsbits = p->button_bits | p->stick_bits | p->hat_bits | p->mapped_axis_bits | p->ev_button_bits;
}

/* Set by app_exit: events and animator ticks that arrive before the main
   loop stops must not reopen (and re-grab) the controller. */
static int g_exiting;
static void js_poll(void)
{
    if(g_exiting || g_paused)return;
    pad_poll(PAD1);
    if(PAD2->selected_usb_id[0]) pad_poll(PAD2);
    mapping_tick();
}

static Eina_Bool animate(void *data)
{
    (void)data;
    if (g_exiting || g_paused) return 1;
    int cw=0,ch=0;evas_output_size_get(g_canvas,&cw,&ch);
    if(cw>0 && ch>0 && (cw!=g_canvas_w || ch!=g_canvas_h)) { apply_video_settings();show_group(g_state); }
    g_input_frame++;
    js_poll();
    library_tick(g_input_frame);
    unsigned int ctl = PAD1->jsbits | PAD2->jsbits;
    if (g_state == ST_GAME) {
        unsigned int pad = g_keybits | ctl;
        if ((int)pad != g_game_input_log_bits) {
            char line[128];
            snprintf(line, sizeof line, "game-input pad=0x%x keyboard=0x%x controller=0x%x", pad, g_keybits, ctl);
            diag(line);
            g_game_input_log_bits = (int)pad;
        }
    }
    unsigned int nav_bits = resolve_opposing_directions(ctl);
    unsigned int pressed = nav_bits & ~g_js_prevbits;
    if (g_state == ST_LIBRARY) {
        if (pressed & PAD_UP) library_move(-1);
        else if (pressed & PAD_DOWN) library_move(1);
        else if (pressed & PAD_LEFT) library_move(-VIS_ROWS);
        else if (pressed & PAD_RIGHT) library_move(VIS_ROWS);
        else if (pressed & PAD_A) { if (g_sel==0) app_exit(); else if (g_scan_phase) {} else if (g_sel==g_romcount+1) settings_open(); else start_game(g_sel-1); }
        else if (pressed & PAD_START) settings_open();
        else if (pressed & PAD_SELECT) help_open(ST_LIBRARY);
    } else if (g_state == ST_SETTINGS) {
        if (pressed & PAD_UP) { if (g_settings_sel > 0) g_settings_sel--; g_settings_message[0] = 0; render_settings(); }
        else if (pressed & PAD_DOWN) { if (g_settings_sel < SETTINGS_ROWS-1) g_settings_sel++; g_settings_message[0] = 0; render_settings(); }
        else if (pressed & PAD_LEFT) settings_adjust(-1);
        else if (pressed & PAD_RIGHT) settings_adjust(1);
        else if (pressed & PAD_A) settings_activate();
        else if (pressed & (PAD_B|PAD_START)) settings_close();
    } else if (g_state == ST_DEVICES) {
        if (pressed & PAD_UP) { if (g_device_sel > 0) g_device_sel--; render_devices(); }
        else if (pressed & PAD_DOWN) { if (g_device_sel < g_device_count-1) g_device_sel++; render_devices(); }
        else if (pressed & PAD_A) device_select_activate();
        else if (pressed & (PAD_B|PAD_START)) { g_state = ST_SETTINGS; show_group(g_state); }
    } else if (g_state == ST_HELP) {
        if (pressed & (PAD_A|PAD_B|PAD_START|PAD_SELECT)) help_close();
    } else if (g_state == ST_ERROR) {
        if (pressed & (PAD_A|PAD_B|PAD_START)) return_to_library();
    }
    g_js_prevbits = nav_bits;
    if (g_state != ST_GAME || !g_core) return 1;

    gbap_set_keys(g_core,resolve_opposing_directions(g_keybits|ctl));
    unsigned long long now=mono_ns(),period=g_pace_period_ns;
    if(!now)now=(g_pace_done+1)*period;
    if(!g_pace_t0) { g_pace_t0=now;g_pace_done=0;g_pace_win_t0=now;g_pace_win_frames=0; }
    unsigned long long due=(now-g_pace_t0+period/2)/period+1;
    if(due>g_pace_done+PACE_MAX_LAG) {
        /* Rebase wall-clock debt, never skip a core frame. Report every stall
         * and first remove video cost; every executed frame drains audio. */
        diag("pacing-stall: rebasing clock debt; no emulation frames skipped");
        if(gba_filter_cheaper(&g_filter_live)){diag("pacing-stall: filter downgraded");apply_video_settings();}
        g_pace_t0=now-g_pace_done*period;due=g_pace_done+1;g_pace_stalls++;
    }
    int ran=0;
    while(g_pace_done<due && ran<PACE_MAX_PER_TICK) {
        if(!gbap_run_frame(g_core)) {
            char reason[256];str_copy(reason,gbap_error(g_core),sizeof reason);audio_close();
            /* Retain the runtime for an explicit battery-save retry. */
            show_error("The game stopped with an emulation error.",reason);return 1;
        }
        audio_frame_done();present_frame();g_pace_done++;ran++;
    }
    if(g_perf_mode!=PERF_OVERLAY_OFF && ran) {
        unsigned long long end=mono_ns();
        unsigned long long work=end>now?(end-now)/(unsigned long long)ran:0;
        int published=0;
        for(int k=0;k<ran;k++) published|=perf_frame(&g_perf,end,work,k>0);
        if(published) perf_overlay_apply();
    }
    g_pace_ticks++;g_pace_win_frames+=(unsigned)ran;
    if(!ran)g_pace_idle++;else if(ran>1)g_pace_multi++;
    if(now-g_last_save>=10000000000ULL) {
        g_last_save=now;
        if(!gbap_flush_save(g_core))diag(gbap_error(g_core));
    }
    if(now-g_pace_win_t0>=60000000000ULL) {
        char log[256];unsigned long long span=now-g_pace_win_t0;
        unsigned fps=(unsigned)(g_pace_win_frames*100000000000ULL/span);
        snprintf(log,sizeof log,"pace fps=%u.%02u stalls=%lu audio_under=%u audio_over=%u filter=%s",fps/100,fps%100,g_pace_stalls,__atomic_load_n(&g_audio_under,__ATOMIC_RELAXED),g_audio_over,gba_filter_short_name(g_effective.scaler));
        diag(log);g_pace_win_t0=now;g_pace_win_frames=0;
    }
    return 1;
}

/* ------------------------------ input keys -------------------------------- */
static unsigned int key_to_pad(const char *kn)
{
    if (!kn) return 0;
    if (!strcasecmp(kn, "Up"))    return PAD_UP;
    if (!strcasecmp(kn, "Down"))  return PAD_DOWN;
    if (!strcasecmp(kn, "Left"))  return PAD_LEFT;
    if (!strcasecmp(kn, "Right")) return PAD_RIGHT;
    if (!strcasecmp(kn, "x"))     return PAD_A;
    if (!strcasecmp(kn, "z"))     return PAD_B;
    if (!strcasecmp(kn, "q"))     return PAD_L;
    if (!strcasecmp(kn, "w"))     return PAD_R;
    if (!strcasecmp(kn, "a"))     return PAD_SELECT;
    if (!strcasecmp(kn, "s"))     return PAD_START;
    if (!strcasecmp(kn, "Return") || !strcasecmp(kn, "KP_Enter")) return PAD_A;
    return 0;
}
static int is_back(const char *kn)
{
    return kn && (!strcasecmp(kn,"XF86Back") || !strcasecmp(kn,"BackSpace") ||
                  !strcasecmp(kn,"Escape") || !strcasecmp(kn,"XF86AudioStop"));
}
static int is_menu(const char *kn)
{
    return kn && (!strcasecmp(kn,"Menu") || !strcasecmp(kn,"XF86Menu") ||
                  !strcasecmp(kn,"XF86Tools") || !strcasecmp(kn,"F1"));
}
static int is_navigation_key(const char *kn)
{
    return kn && (!strcasecmp(kn,"Up") || !strcasecmp(kn,"Down") ||
                  !strcasecmp(kn,"Left") || !strcasecmp(kn,"Right") ||
                  !strcasecmp(kn,"Return") || !strcasecmp(kn,"KP_Enter") ||
                  is_back(kn) || is_menu(kn));
}
static int joystick_key_was_recent(void)
{
    return g_js_activity_valid && g_input_frame - g_js_activity_frame <= 3;
}

/* Quit the whole process: save, release the controller grab, ask the app
   framework to leave its main loop, and force the exit if it does not
   finish promptly (TizNES 1.0.22 Exit pattern). */
static Eina_Bool force_exit(void *data)
{
    (void)data;
    _exit(0);
    return 0;
}

static void app_exit(void)
{
    if(!close_game())return;
    diag("exit requested");g_exiting=1;
    input_active_set(0);
    js_close_device(PAD1);js_close_device(PAD2);g_keybits=0;
    ecore_timer_add(2.0,force_exit,0);ui_app_exit();
}


static void settings_open(void)
{
    if(g_state==ST_GAME) { g_settings_return_state=ST_GAME;audio_close(); }
    else if(g_state!=ST_SETTINGS) g_settings_return_state=ST_LIBRARY;
    g_keybits=0;g_settings_sel=0;g_settings_message[0]=0;g_state=ST_SETTINGS;show_group(g_state);
}


static void settings_close(void)
{
    if(g_settings_return_state==ST_GAME && g_core) { g_state=ST_GAME;g_pace_t0=0;audio_open();show_group(g_state); }
    else return_to_library();
}


static void help_open(int return_state)
{
    if(return_state==ST_GAME)audio_close();
    g_keybits=0;g_help_return_state=return_state;g_state=ST_HELP;show_group(g_state);
}


static void help_close(void)
{
    g_state=g_help_return_state;show_group(g_state);
    if(g_state==ST_GAME && g_core) { g_pace_t0=0;audio_open(); }
}


static void settings_adjust(int direction)
{
    g_settings_message[0]=0;
    if(g_settings_sel==S_VOLUME) { g_volume+=direction*10;if(g_volume<0)g_volume=0;if(g_volume>100)g_volume=100; }
    else if(g_settings_sel==S_MUTE) g_muted=!g_muted;
    else if(g_settings_sel==S_AUDIO_TEST) g_audio_test_filter=(g_audio_test_filter+direction+3)%3;
    else if(g_settings_sel==S_SCALE) g_scale_mode=(g_scale_mode+direction+GBA_SCREEN_COUNT)%GBA_SCREEN_COUNT;
    else if(g_settings_sel==S_FILTER) { int n=gba_filter_scaler_count();int i=gba_filter_scaler_index(g_filter_requested.scaler);g_filter_requested.scaler=gba_filter_scaler_at((i+direction+n)%n); }
    else if(g_settings_sel==S_OVERLAY) g_filter_requested.overlay=(g_filter_requested.overlay+direction+GBA_OVL_COUNT)%GBA_OVL_COUNT;
    else if(g_settings_sel==S_LCD_STRENGTH) { g_filter_requested.lcd_strength+=direction*16;if(g_filter_requested.lcd_strength<0)g_filter_requested.lcd_strength=0;if(g_filter_requested.lcd_strength>255)g_filter_requested.lcd_strength=255; }
    else if(g_settings_sel==S_COLOUR) g_filter_requested.colour=(g_filter_requested.colour+direction+GBA_COLOUR_COUNT)%GBA_COLOUR_COUNT;
    else if(g_settings_sel==S_BLEND) g_filter_requested.blend=!g_filter_requested.blend;
    else if(g_settings_sel==S_PERF_OVERLAY) g_perf_mode=(g_perf_mode+direction+PERF_OVERLAY_MODES)%PERF_OVERLAY_MODES;
    else return;
    audio_controls();g_filter_live=g_filter_requested;g_filter_late=0;apply_video_settings();save_settings();render_settings();
}


static void pad_reopen(pad_t *p)
{
    if(p->slot==1 && !p->selected_usb_id[0]) return;
    if(p->selected_usb_id[0]) p->selected_js_index=pad_resolve_index(p);
    js_open_device(p,p->selected_js_index);
}

static void mapping_start(int slot)
{
    pad_t *p=&g_pads[slot];if(!pad_present(p))pad_reopen(p);
    if(!pad_present(p) || !p->map_model[0]) { str_copy(g_settings_message,"Select and connect a physical USB controller first",sizeof g_settings_message);render_settings();return; }
    mapping_pack(p,g_map_backup);g_map_active=1;g_map_slot=slot;g_map_step=0;g_map_detected[0]=0;
    g_state=ST_MAPPING;mapping_retry();show_group(g_state);
}


static void settings_activate(void)
{
    switch(g_settings_sel) {
    case S_DEVICE:device_select_open(0);break;
    case S_DEVICE2:device_select_open(1);break;
    case S_EXCLUSIVE:
        g_exclusive_preferred=!g_exclusive_preferred;save_input_config();
        pad_reopen(PAD1);pad_reopen(PAD2);render_settings();break;
    case S_MAP:case S_MAP2:case S_ANALOG:case S_ANALOG2:
        g_map_bank=g_settings_sel==S_ANALOG || g_settings_sel==S_ANALOG2;
        mapping_start(g_settings_sel==S_MAP2 || g_settings_sel==S_ANALOG2);break;
    case S_INPUT_TEST:
        if(!pad_present(PAD1)) pad_reopen(PAD1);
        if(pad_present(PAD1)) input_test_start();
        else { str_copy(g_settings_message,"Connect controller 1 first",sizeof g_settings_message);render_settings(); }break;
    case S_RESCAN:
        library_request_scan("settings");if(close_game()) return_to_library();break;
    case S_BACK:if(close_game()) return_to_library();break;
    case S_EXIT:app_exit();break;
    default:settings_adjust(1);
    }
}


static void device_select_open(int slot)
{
    g_device_slot=slot;
    scan_js_devices(slot);
    g_state=ST_DEVICES; show_group(g_state);
}

static void device_select_activate(void)
{
    if(g_device_sel<0 || g_device_sel>=g_device_count) return;
    pad_t *p=&g_pads[g_device_slot], *o=&g_pads[1-g_device_slot];
    int index=g_device_indices[g_device_sel];
    char line[224];
    g_state=ST_SETTINGS;
    if(index<0) {
        pad_forget_device(p);
        p->selected_usb_id[0]=0; p->map_model[0]=0; p->map_loaded=0;
        save_input_config();
        diag("p2 input selection=none");
        str_copy(g_settings_message,"Controller 2: None (one GBA keypad)",sizeof g_settings_message);
        show_group(g_state);
        return;
    }
    char chosen[160], previous[160];
    str_copy(chosen,g_device_usb_ids[g_device_sel],sizeof chosen);
    str_copy(previous,p->selected_usb_id,sizeof previous);
    int swapped=0;
    if(identity_matches(o->selected_usb_id,chosen)) {
        pad_forget_device(o);
        str_copy(o->selected_usb_id,previous,sizeof o->selected_usb_id);
        o->map_model[0]=0; o->map_loaded=0;
        swapped=1;
    }
    if(!identity_matches(previous,chosen)) { p->map_model[0]=0; p->map_loaded=0; }
    p->selected_js_index=index;
    str_copy(p->selected_usb_id,chosen,sizeof p->selected_usb_id);
    js_open_device(p,index);
    if(swapped && o->selected_usb_id[0]) pad_reopen(o);
    save_settings();
    save_input_config();
    snprintf(line,sizeof line,"%sinput selection identity=%s js=%d swapped=%d",pad_tag(p),chosen,index,swapped);
    diag(line);
    if(p->js_fd<0) str_copy(g_settings_message,"Could not open selected device",sizeof g_settings_message);
    else if(swapped) str_copy(g_settings_message,"Controllers swapped between slots",sizeof g_settings_message);
    else if(!p->capture_exclusive) {
        str_copy(g_settings_message,g_exclusive_preferred?"Selected; waiting for exclusive access":"Shared input: Tizen shortcuts may still fire",sizeof g_settings_message);
    } else str_copy(g_settings_message,"Exclusive capture active",sizeof g_settings_message);
    show_group(g_state);
}

static Eina_Bool on_key_down(void *d, int t, void *ev)
{
    Ecore_Event_Key *e = (Ecore_Event_Key *)ev; (void)d; (void)t;
    const char *kn = e ? e->keyname : 0;
    js_poll();
    if(g_exiting || g_paused)return 1;
    /* Raw capture consumes physical-pad events; real remote keys remain usable
     * during mapping, diagnostics, and exclusive capture. */
    if(g_state!=ST_INPUT_TEST && g_state!=ST_MAPPING && is_navigation_key(kn) && joystick_key_was_recent())return 1;
    if (g_state == ST_LIBRARY) {
        if (is_menu(kn) || (kn && (!strcasecmp(kn,"o") || !strcasecmp(kn,"O")))) settings_open();
        else if (kn && !strcasecmp(kn,"h")) help_open(ST_LIBRARY);
        else if (kn && !strcasecmp(kn,"Up"))   library_move(-1);
        else if (kn && !strcasecmp(kn,"Down")) library_move(1);
        else if (kn && (!strcasecmp(kn,"Left") || !strcasecmp(kn,"XF86RaiseChannel"))) library_move(-VIS_ROWS);
        else if (kn && (!strcasecmp(kn,"Right") || !strcasecmp(kn,"XF86LowerChannel"))) library_move(VIS_ROWS);
        else if (kn && (!strcasecmp(kn,"Return") || !strcasecmp(kn,"KP_Enter"))) { if (g_sel==0) app_exit(); else if (g_scan_phase) {} else if (g_sel==g_romcount+1) settings_open(); else start_game(g_sel-1); }
        return 1;
    }
    if (g_state == ST_SETTINGS) {
        if (is_back(kn)) { settings_close(); return 1; }
        if (kn && !strcasecmp(kn,"Up")) { if (g_settings_sel > 0) g_settings_sel--; g_settings_message[0] = 0; render_settings(); }
        else if (kn && !strcasecmp(kn,"Down")) { if (g_settings_sel < SETTINGS_ROWS-1) g_settings_sel++; g_settings_message[0] = 0; render_settings(); }
        else if (kn && !strcasecmp(kn,"Left")) settings_adjust(-1);
        else if (kn && !strcasecmp(kn,"Right")) settings_adjust(1);
        else if (kn && (!strcasecmp(kn,"Return") || !strcasecmp(kn,"KP_Enter"))) settings_activate();
        return 1;
    }
    if (g_state == ST_DEVICES) {
        if (is_back(kn)) { g_state = ST_SETTINGS; show_group(g_state); return 1; }
        if (kn && !strcasecmp(kn,"Up")) { if (g_device_sel > 0) g_device_sel--; render_devices(); }
        else if (kn && !strcasecmp(kn,"Down")) { if (g_device_sel < g_device_count-1) g_device_sel++; render_devices(); }
        else if (kn && (!strcasecmp(kn,"Return") || !strcasecmp(kn,"KP_Enter"))) device_select_activate();
        return 1;
    }
    if (g_state == ST_MAPPING) {
        mapping_tick();
        if(is_back(kn))mapping_cancel();
        else if(kn && (!strcasecmp(kn,"Return") || !strcasecmp(kn,"KP_Enter")))mapping_retry();
        else if(kn && !strcasecmp(kn,"Right")) { str_copy(g_map_detected,"action skipped (binding retained)",sizeof g_map_detected);mapping_advance(&g_pads[g_map_slot]); }
        return 1;
    }
    if (g_state == ST_INPUT_TEST) {
        if (is_back(kn)) input_test_finish(1);
        else if (kn && (!strcasecmp(kn,"Return") || !strcasecmp(kn,"KP_Enter"))) input_test_skip();
        return 1;
    }
    if (g_state == ST_ERROR) { return_to_library(); return 1; }
    if (g_state == ST_HELP) {
        if (is_back(kn) || is_menu(kn) || (kn && !strcasecmp(kn,"h"))) help_close();
        return 1;
    }
    if (g_state == ST_GAME) {
        if (is_back(kn)) { stop_game(); return 1; }
        if (is_menu(kn) || (kn && !strcasecmp(kn,"h"))) { help_open(ST_GAME); return 1; }
        if (kn && !strcasecmp(kn,"o")) { settings_open();return 1; }
        if (kn && !strcasecmp(kn,"m")) { g_muted=!g_muted;audio_controls();save_settings();return 1; }
        if (kn && !strcasecmp(kn,"comma")) { g_volume-=10;if(g_volume<0)g_volume=0;audio_controls();save_settings();return 1; }
        if (kn && !strcasecmp(kn,"period")) { g_volume+=10;if(g_volume>100)g_volume=100;audio_controls();save_settings();return 1; }
        g_keybits |= key_to_pad(kn);
    }
    return 1;
}
static Eina_Bool on_key_up(void *d, int t, void *ev)
{
    Ecore_Event_Key *e = (Ecore_Event_Key *)ev; (void)d; (void)t;
    const char *kn = e ? e->keyname : 0;
    js_poll();
    g_keybits &= ~key_to_pad(kn);
    return 1;
}

/* --------------------------------- setup ---------------------------------- */
static void pads_init(void)
{
    for(int i=0;i<2;i++) {
        pad_t *p=&g_pads[i];memset(p,0,sizeof *p);p->slot=i;
        p->js_fd=p->ev_fd=-1;p->x_axis=0;p->y_axis=1;
        p->hat_x_axis=p->hat_y_axis=-1;p->device_index=p->ev_device_index=p->selected_js_index=-1;
    }
}


static int create(void *data)
{
    (void)data;
    pads_init();
    mkdir(TIZGBA_DATA_DIR,0700);
    if(gba_filter_init(&g_filter,GBA_FILTER_MAX_W,GBA_FILTER_MAX_H)!=0)return 0;
    load_settings();g_filter_live=g_filter_requested;
    save_settings();
    load_input_config();
    if (PAD1->selected_usb_id[0]) PAD1->selected_js_index = pad_resolve_index(PAD1);

    g_win = elm_win_util_standard_add("tizgba", "TizGBA");
    if (!g_win) return 0;
    elm_win_fullscreen_set(g_win, 1);
    evas_object_resize(g_win, TV_W, TV_H);
    g_canvas = evas_object_evas_get(g_win);

    g_bg = evas_object_rectangle_add(g_canvas);
    evas_object_color_set(g_bg, 12, 14, 20, 255);
    elm_win_resize_object_add(g_win, g_bg);
    evas_object_show(g_bg);

    g_img = evas_object_image_filled_add(g_canvas);
    evas_object_image_size_set(g_img, GBAP_WIDTH, GBAP_HEIGHT);
    evas_object_image_alpha_set(g_img, 0);
    /* Evas allocates/owns its buffer on the first rendered frame. */
    apply_video_settings();

    g_title  = mk_text(56, 255, 255, 255);  evas_object_move(g_title, 360, 120);
    g_footer = mk_text(30, 120, 200, 255);  evas_object_move(g_footer, 360, 1000);
    g_cursor = evas_object_rectangle_add(g_canvas);
    evas_object_color_set(g_cursor, 120, 200, 255, 255);
    for (int i = 0; i < VIS_ROWS; i++) g_row[i] = mk_text(34, 200, 200, 200);
    for (int i = 0; i < SETTINGS_ROWS; i++) g_set_row[i] = mk_text(30, 200, 200, 200);
    for (int i = 0; i < SETTINGS_SECTIONS; i++) {
        g_set_head[i] = mk_text(26, 120, 200, 255);
        g_set_rule[i] = evas_object_rectangle_add(g_canvas);
        evas_object_color_set(g_set_rule[i], 60, 90, 120, 255);
    }

    g_err = elm_label_add(g_win); evas_object_move(g_err, 360, 420);
    evas_object_resize(g_err, 1200, 300);

    g_help = elm_label_add(g_win); evas_object_move(g_help, 300, 260);
    evas_object_resize(g_help, 1320, 600);
    elm_object_part_text_set(g_help, 0,
        "<align=left><font_size=38><color=#ebebeb>"
        "Controls<br/><br/>"
        "D-pad: arrow keys, left stick, or controller D-pad<br/>"
        "A: X key / controller A    B: Z key / controller B<br/>"
        "Select: A key / controller Select    Start: S / Start button<br/>"
        "L / R: Q / W keys or controller shoulders<br/>"
        "Two controllers are alternatives for one GBA keypad; no link play.<br/><br/>"
        "Library: first row Exit TizGBA closes the app; Left/Right pages;<br/>Start opens Settings, Select opens Help<br/>"
        "USB drives are searched 8 folders deep; games play from the drive<br/>"
        "and saves stay on the TV. Plug a drive in or out: the list updates.<br/>"
        "Settings: D-pad moves, Left/Right changes, A selects, B/Start returns<br/>"
        "System > Performance overlay shows FPS (or details) over the game<br/>"
        "In game: Back returns to the library (the game's save is written)<br/>"
        "O: Settings (keyboard)    M: mute    , / .: volume    H: help"
        "</color></font_size></align>");

    /* Created last: stays above the game image. */
    g_perf_bg = evas_object_rectangle_add(g_canvas);
    evas_object_color_set(g_perf_bg, 0, 0, 0, 160);
    g_perf_txt = mk_text(28, 255, 255, 255);

    elm_object_focus_set(g_win, 1);
    g_key_down_handler=ecore_event_handler_add(ECORE_EVENT_KEY_DOWN, on_key_down, 0);
    g_key_up_handler=ecore_event_handler_add(ECORE_EVENT_KEY_UP, on_key_up, 0);

    input_active_set(1);
    js_open_device(PAD1, PAD1->selected_js_index);
    pad_reopen(PAD2);
    /* First scan from the animator, after "Scanning" is on screen. */
    g_sel = g_top = 0;
    library_request_scan("startup");
    g_scan_phase = 1;
    g_state = ST_LIBRARY;
    show_group(g_state);
    evas_object_show(g_win);
    g_anim = ecore_animator_add(animate, 0);
    return 1;
}

/* Leaving the foreground (Home, another app, TV standby): write the battery
   save now and release the controller so other apps can use it. */
static void on_app_pause(void *data)
{
    (void)data;input_active_set(0);g_paused=1;g_keybits=g_js_prevbits=0;audio_close();
    if(g_core) { gbap_set_keys(g_core,0);if(!gbap_flush_save(g_core))diag(gbap_error(g_core)); }
    js_close_device(PAD1);js_close_device(PAD2);
    if(g_state==ST_MAPPING){g_map_armed=0;g_map_timed_out=0;}
}


static void on_app_resume(void *data)
{
    (void)data;if(g_exiting)return;
    g_paused=0;input_active_set(1);pad_reopen(PAD1);pad_reopen(PAD2);g_pace_t0=0;
    if(g_state==ST_MAPPING)mapping_retry();
    if(g_state==ST_GAME && g_core)audio_open();
    apply_video_settings();
    if(lib_roots_changed())library_request_scan("resume");
}


static void on_app_terminate(void *data)
{
    (void)data;input_active_set(0);g_exiting=1;audio_close();
    if(g_core){if(!gbap_flush_save(g_core))diag(gbap_error(g_core));gbap_destroy(g_core);g_core=0;}
    js_close_device(PAD1);js_close_device(PAD2);
    if(g_anim){ecore_animator_del(g_anim);g_anim=0;}
    if(g_key_down_handler){ecore_event_handler_del(g_key_down_handler);g_key_down_handler=0;}
    if(g_key_up_handler){ecore_event_handler_del(g_key_up_handler);g_key_up_handler=0;}
}


int main(int argc, char **argv)
{
    callbacks_t cb = {(void *)create, (void *)on_app_terminate,
                      (void *)on_app_pause, (void *)on_app_resume, 0};
    return ui_app_main(argc, argv, &cb, 0);
}
