/* TizGBA video filter library (phase A2).
 *
 * Self-contained and freestanding: needs only <stdint.h>/<stddef.h> and the
 * compiler's memcpy/memset builtins. No malloc, no libm, no stdio. All state
 * lives in a caller-provided gba_filter_t (about 3.5 MB; keep it in BSS or
 * heap-allocate it once).
 *
 * Pipeline, fixed order:
 *     colour correction -> frame blend -> scaler -> (replicate + overlay)
 * See docs/FILTERS.md for the policy, the filter descriptions, the costs and
 * the test/mutation record.
 *
 * Pixel format: uint32_t, 0x??RRGGBB in, 0xFFRRGGBB out (the input high byte is
 * ignored everywhere). All pitches are in BYTES and must be multiples of 4.
 */
#ifndef TIZGBA_FILTERS_H
#define TIZGBA_FILTERS_H

#include <stdint.h>
#include <stddef.h>

#define GBA_SRC_W 240
#define GBA_SRC_H 160

/* The intermediate buffer never exceeds this unless the caller raises the
 * limit in gba_filter_init() (the context buffers cap it at 1920x1280). */
#define GBA_FILTER_MAX_W 1920
#define GBA_FILTER_MAX_H 1280

/* ---- filter ids (stable numbering, matches docs/FILTERS.md) ---- */
enum {
    GBA_F_NEAREST         = 0,   /* scaler: passthrough, Evas scales nearest   */
    GBA_F_SMOOTH          = 1,   /* scaler: passthrough, Evas scales smooth    */
    GBA_F_SHARP_BILINEAR  = 2,   /* scaler: integer prescale + Evas linear     */
    GBA_F_SCALE2X         = 3,   /* scaler: AdvMAME2x                          */
    GBA_F_SCALE3X         = 4,   /* scaler: AdvMAME3x                          */
    GBA_F_SCALE4X         = 5,   /* scaler: Scale2x applied twice              */
    GBA_F_EPX_RESERVED    = 6,   /* rejected (== Scale2x variant), never valid */
    GBA_F_EDGE            = 7,   /* scaler: original edge-directed 2x/3x       */
    GBA_F_LCD_GRID        = 8,   /* overlay                                    */
    GBA_F_SCANLINES       = 9,   /* overlay                                    */
    GBA_F_COLOUR          = 10,  /* independent pass                           */
    GBA_F_BLEND           = 11,  /* independent pass                           */
    GBA_F_COUNT           = 12
};

/* settings.overlay */
enum { GBA_OVL_OFF = 0, GBA_OVL_LCD_GRID = 1, GBA_OVL_SCAN_SUBTLE = 2,
       GBA_OVL_SCAN_STRONG = 3, GBA_OVL_COUNT = 4 };

/* settings.colour */
enum { GBA_COLOUR_VIVID = 0,      /* raw core colours                         */
       GBA_COLOUR_LCD = 1,        /* gamma 2.2 + channel crosstalk (neutral)  */
       GBA_COLOUR_LCD_DARK = 2,   /* same matrix, LCD gamma 4.0 (darker mids) */
       GBA_COLOUR_COUNT = 3 };

/* Evas scaling hint the frontend must apply to the final upscale. */
enum { GBA_HINT_NEAREST = 0, GBA_HINT_SMOOTH = 1 };

/* Screen-size modes understood by gba_filter_fit_target(). */
enum { GBA_SCREEN_INTEGER = 0,  /* largest integer N with 240N x 160N inside   */
       GBA_SCREEN_FIT = 1,      /* largest 3:2 box inside the canvas           */
       GBA_SCREEN_FILL = 2,     /* whole canvas (non-square pixels)            */
       GBA_SCREEN_COUNT = 3 };

/* Cost classes (conservative Cortex-A72 estimate, see FILTERS.md). */
enum { GBA_COST_NONE = 0,       /* identity, the frontend can skip apply()     */
       GBA_COST_CHEAP = 1,      /* <= 1.5 ms/frame                              */
       GBA_COST_MEDIUM = 2,     /* <= 4 ms/frame                                */
       GBA_COST_HEAVY = 3 };    /* > 4 ms/frame                                 */

/* effective.notes bits: why the effective settings differ from the request. */
#define GBA_NOTE_SHARP_IS_NEAREST  0x001u /* integer target: sharp bilinear == nearest */
#define GBA_NOTE_OVERLAY_RAISED    0x002u /* extra integer replicate added for the overlay */
#define GBA_NOTE_OVERLAY_DROPPED   0x004u /* overlay impossible at this size/limit      */
#define GBA_NOTE_SIZE_CLAMPED      0x008u /* factor reduced to respect the max size     */
#define GBA_NOTE_SCALER_DOWNGRADED 0x010u /* scaler replaced by nearest (max size)      */
#define GBA_NOTE_BAD_SETTING       0x020u /* an out-of-range setting was sanitized      */
#define GBA_NOTE_BAD_TARGET        0x040u /* target size <= 0, 1x assumed               */

/* Trivially serialisable: five ints, in this order. */
#define GBA_FILTER_SETTINGS_INTS 5
typedef struct {
    int scaler;        /* GBA_F_NEAREST .. GBA_F_EDGE (never GBA_F_EPX_RESERVED) */
    int overlay;       /* GBA_OVL_*                                              */
    int lcd_strength;  /* LCD grid strength 0..255 (ignored for other overlays)  */
    int colour;        /* GBA_COLOUR_*                                           */
    int blend;         /* 0 / 1                                                  */
} gba_filter_settings_t;

typedef struct {
    int out_w, out_h;      /* size of the buffer apply() writes                  */
    int hint;              /* GBA_HINT_* for the final Evas upscale              */
    int scaler;            /* effective scaler id                                */
    int base_factor;       /* b: factor of the scaler algorithm (1 for none)     */
    int post_factor;       /* r: integer replicate applied after the scaler      */
    int total_factor;      /* T = b*r: output pixels per source pixel (per axis) */
    int overlay;           /* effective GBA_OVL_*                                */
    int lcd_strength;
    int colour;
    int blend;
    int identity;          /* 1: RGB output equals source; caller may show the
                              240x160 buffer directly with Evas alpha disabled  */
    int cost_class;        /* GBA_COST_*                                         */
    unsigned cost_ns;      /* conservative Cortex-A72 ns/frame estimate          */
    unsigned notes;        /* GBA_NOTE_* bits                                    */
} gba_filter_effective_t;

typedef struct { int x, y, w, h, n; } gba_rect_t; /* n = integer scale, 0 if none */

/* The context. Fields are private; the layout is public only so the caller can
 * allocate it statically. Zero-initialise nothing: call gba_filter_init(). */
typedef struct gba_filter {
    int max_w, max_h;
    int configured;
    int lut_mode;                /* colour mode the LUT holds, -1 = none         */
    int prev_valid;
    int tw, th;                  /* last target                                  */
    gba_filter_settings_t cfg;
    gba_filter_effective_t eff;
    int ovl_T;                   /* overlay cell size, 0 = none                  */
    uint16_t ovl_f[8 * 8 * 3];   /* Q8 channel factors per cell position (r,g,b) */
    int8_t ovl_dup[8];           /* row k identical to row ovl_dup[k] of the cell*/
    uint8_t ovl_uni[8 * 8];      /* all three factors equal                      */
    uint32_t lut[32768];         /* 15-bit colour-correction LUT                 */
    uint32_t frame[GBA_SRC_W * GBA_SRC_H];  /* post-colour / post-blend source   */
    uint32_t prev[GBA_SRC_W * GBA_SRC_H];   /* previous (post-colour) frame      */
    uint32_t s1[480 * 320];      /* scratch: 2x stage                            */
    uint32_t s2[960 * 640];      /* scratch: 3x / 4x stage                       */
} gba_filter_t;

/* ---------------------------------------------------------------- settings */
void gba_filter_settings_default(gba_filter_settings_t *s); /* nearest, no extras */
/* Clamp every field into range; returns nonzero if anything was changed. */
int  gba_filter_settings_sanitize(gba_filter_settings_t *s);
/* int array <-> struct (for tizgba-settings.conf). Keys: gba_filter_setting_key(i). */
void gba_filter_settings_to_ints(const gba_filter_settings_t *s, int out[GBA_FILTER_SETTINGS_INTS]);
void gba_filter_settings_from_ints(gba_filter_settings_t *s, const int in[GBA_FILTER_SETTINGS_INTS]);
const char *gba_filter_setting_key(int i);   /* "filter_scaler", ... or NULL */

/* ---------------------------------------------------------------- UI lists */
const char *gba_filter_name(int id);         /* brief ids 0..11, NULL if out of range */
const char *gba_filter_short_name(int id);   /* config / log token                   */
int  gba_filter_scaler_count(void);          /* ordered Settings list                 */
int  gba_filter_scaler_at(int i);            /* i -> GBA_F_* scaler id                */
int  gba_filter_scaler_index(int id);        /* id -> list position or -1             */
const char *gba_filter_overlay_name(int overlay);
const char *gba_filter_colour_name(int colour);
const char *gba_filter_cost_name(int cost_class);
const char *gba_filter_screen_name(int mode);

/* ---------------------------------------------------------------- lifecycle */
/* max_w/max_h bound the intermediate buffer (0 -> 1920x1280; values above the
 * context's 1920x1280 are clamped). Positive limits below 240x160 are invalid.
 * Returns 0 on success. */
int  gba_filter_init(gba_filter_t *ctx, int max_w, int max_h);
/* Forget the previous frame (call on ROM load, reset, state load). */
void gba_filter_reset(gba_filter_t *ctx);

/* Destination rectangle for a canvas and a Screen-size mode (centred). */
void gba_filter_fit_target(int canvas_w, int canvas_h, int mode, gba_rect_t *out);

/* Apply a (sanitized copy of the) settings for a final on-screen rectangle of
 * target_w x target_h canvas pixels. Fills *eff with what will really happen.
 * Returns 0 on success, negative on bad arguments. Cheap; call on every
 * setting or canvas change, not per frame. */
int  gba_filter_configure(gba_filter_t *ctx, const gba_filter_settings_t *s,
                          int target_w, int target_h, gba_filter_effective_t *eff);

/* Run the pipeline for one frame. src is 240x160, dst receives eff.out_w x
 * eff.out_h; pitches are bytes. src and dst must not overlap. Writes exactly
 * out_w*4 bytes per row. Returns 0 on success, negative on error (not
 * configured, null pointers, bad pitch). */
int  gba_filter_apply(gba_filter_t *ctx, const void *src, size_t src_pitch,
                      void *dst, size_t dst_pitch);

/* The configured effective settings (valid after gba_filter_configure). */
const gba_filter_effective_t *gba_filter_effective(const gba_filter_t *ctx);

/* ---------------------------------------------------------------- fallback */
/* Estimate for any effective configuration (also stored in eff by configure). */
void gba_filter_estimate(gba_filter_effective_t *eff);
/* Next cheaper settings in a fixed ladder. If lowering the scaler could raise
 * the effective overlay replicate factor, that transition drops the overlay
 * too. Returns 1 if *s was made cheaper, 0 if it already is the cheapest.
 * The caller re-configures and logs. */
int  gba_filter_cheaper(gba_filter_settings_t *s);

/* Exposed so the harness can test the pixel helpers directly. */
uint32_t gba_filter_lut_lookup(const gba_filter_t *ctx, uint32_t xrgb);

#endif
