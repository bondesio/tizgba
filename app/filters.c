/* TizGBA video filter library (phase A2). See filters.h and docs/FILTERS.md.
 *
 * Freestanding: only memcpy/memset-class builtins. Every algorithm here is
 * written from published descriptions (Scale2x/Scale3x rules as published at
 * scale2x.sourceforge.net, the gamma/crosstalk colour model as described for
 * byuu's higan GBA colour and Talarubi's shader) or is original (the edge
 * directed scaler, the LCD grid, the scanline weights). No GPL/LGPL source was
 * read or copied. See NOTICE notes in docs/FILTERS.md.
 */
#include "filters.h"

typedef uint32_t px;

#define ALPHA 0xFF000000u
#define RGBM  0x00FFFFFFu

#define MEMCPY(d, s, n) __builtin_memcpy((d), (s), (n))
#define MEMSET(d, v, n) __builtin_memset((d), (v), (n))

static inline int imin(int a, int b) { return a < b ? a : b; }
static inline int imax(int a, int b) { return a > b ? a : b; }
static inline int iclamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

/* exact colour equality ignoring the high byte */
static inline int eq(px a, px b) { return ((a ^ b) & RGBM) == 0; }

/* ------------------------------------------------------------------------ */
/* Names, lists, settings                                                    */
/* ------------------------------------------------------------------------ */

static const char *const k_names[GBA_F_COUNT] = {
    "Sharp pixels (nearest)", "Smooth (bilinear)", "Sharp bilinear",
    "Scale2x (AdvMAME2x)", "Scale3x (AdvMAME3x)", "Scale4x (Scale2x chain)",
    "EPX (not implemented)", "Edge-directed 2x/3x", "LCD grid", "Scanlines",
    "GBA colour correction", "Frame blending"
};
static const char *const k_short[GBA_F_COUNT] = {
    "nearest", "smooth", "sharp-bilinear", "scale2x", "scale3x", "scale4x",
    "epx", "edge", "lcd-grid", "scanlines", "colour", "blend"
};
static const int k_scaler_order[] = {
    GBA_F_NEAREST, GBA_F_SMOOTH, GBA_F_SHARP_BILINEAR, GBA_F_SCALE2X,
    GBA_F_SCALE3X, GBA_F_SCALE4X, GBA_F_EDGE
};
#define N_SCALERS ((int)(sizeof k_scaler_order / sizeof k_scaler_order[0]))

const char *gba_filter_name(int id) { return (id >= 0 && id < GBA_F_COUNT) ? k_names[id] : 0; }
const char *gba_filter_short_name(int id) { return (id >= 0 && id < GBA_F_COUNT) ? k_short[id] : 0; }
int gba_filter_scaler_count(void) { return N_SCALERS; }
int gba_filter_scaler_at(int i) { return (i >= 0 && i < N_SCALERS) ? k_scaler_order[i] : -1; }
int gba_filter_scaler_index(int id)
{
    for (int i = 0; i < N_SCALERS; ++i) if (k_scaler_order[i] == id) return i;
    return -1;
}
const char *gba_filter_overlay_name(int o)
{
    static const char *const n[GBA_OVL_COUNT] = { "Off", "LCD grid", "Scanlines (subtle)", "Scanlines (strong)" };
    return (o >= 0 && o < GBA_OVL_COUNT) ? n[o] : 0;
}
const char *gba_filter_colour_name(int c)
{
    static const char *const n[GBA_COLOUR_COUNT] = { "Vivid (raw)", "GBA LCD", "GBA LCD (dark)" };
    return (c >= 0 && c < GBA_COLOUR_COUNT) ? n[c] : 0;
}
const char *gba_filter_cost_name(int c)
{
    static const char *const n[4] = { "none", "cheap", "medium", "heavy" };
    return (c >= 0 && c < 4) ? n[c] : 0;
}
const char *gba_filter_screen_name(int m)
{
    static const char *const n[GBA_SCREEN_COUNT] = { "Pixel-perfect integer", "Fit height (3:2)", "Fill screen" };
    return (m >= 0 && m < GBA_SCREEN_COUNT) ? n[m] : 0;
}

void gba_filter_settings_default(gba_filter_settings_t *s)
{
    s->scaler = GBA_F_NEAREST;
    s->overlay = GBA_OVL_OFF;
    s->lcd_strength = 128;
    s->colour = GBA_COLOUR_VIVID;
    s->blend = 0;
}

int gba_filter_settings_sanitize(gba_filter_settings_t *s)
{
    int changed = 0;
    if (gba_filter_scaler_index(s->scaler) < 0) { s->scaler = GBA_F_NEAREST; changed = 1; }
    if (s->overlay < 0 || s->overlay >= GBA_OVL_COUNT) { s->overlay = GBA_OVL_OFF; changed = 1; }
    if (s->lcd_strength < 0 || s->lcd_strength > 255) { s->lcd_strength = iclamp(s->lcd_strength, 0, 255); changed = 1; }
    if (s->colour < 0 || s->colour >= GBA_COLOUR_COUNT) { s->colour = GBA_COLOUR_VIVID; changed = 1; }
    if (s->blend != 0 && s->blend != 1) { s->blend = s->blend ? 1 : 0; changed = 1; }
    return changed;
}

void gba_filter_settings_to_ints(const gba_filter_settings_t *s, int out[GBA_FILTER_SETTINGS_INTS])
{
    out[0] = s->scaler; out[1] = s->overlay; out[2] = s->lcd_strength;
    out[3] = s->colour; out[4] = s->blend;
}
void gba_filter_settings_from_ints(gba_filter_settings_t *s, const int in[GBA_FILTER_SETTINGS_INTS])
{
    s->scaler = in[0]; s->overlay = in[1]; s->lcd_strength = in[2];
    s->colour = in[3]; s->blend = in[4];
    gba_filter_settings_sanitize(s);
}
const char *gba_filter_setting_key(int i)
{
    static const char *const k[GBA_FILTER_SETTINGS_INTS] = {
        "filter_scaler", "filter_overlay", "filter_lcd_strength", "filter_colour", "filter_blend"
    };
    return (i >= 0 && i < GBA_FILTER_SETTINGS_INTS) ? k[i] : 0;
}

/* ------------------------------------------------------------------------ */
/* Small float maths for the one-time LUT build (no libm)                    */
/* ------------------------------------------------------------------------ */

#define LN2 0.69314718055994530942

static double fl_log(double x) /* x > 0 */
{
    uint64_t u;
    MEMCPY(&u, &x, 8);
    int e = (int)((u >> 52) & 0x7FF) - 1023;
    u = (u & 0x000FFFFFFFFFFFFFull) | 0x3FF0000000000000ull;
    double m;
    MEMCPY(&m, &u, 8);
    if (m > 1.41421356237309505) { m *= 0.5; ++e; }
    double s = (m - 1.0) / (m + 1.0), s2 = s * s;
    double sum = 1.0 / 23.0;
    for (int k = 21; k >= 1; k -= 2) sum = sum * s2 + 1.0 / (double)k;
    return 2.0 * s * sum + (double)e * LN2;
}

static double fl_exp(double z) /* z <= ~0, moderate */
{
    if (z < -700.0) return 0.0;
    int k = (int)(z / LN2 + (z < 0 ? -0.5 : 0.5));
    double r = z - (double)k * LN2, term = 1.0, sum = 1.0;
    for (int i = 1; i < 18; ++i) { term = term * r / (double)i; sum += term; }
    uint64_t u = (uint64_t)(k + 1023) << 52;
    double p;
    MEMCPY(&p, &u, 8);
    return sum * p;
}

static double fl_pow(double x, double y)
{
    if (x <= 0.0) return 0.0;
    return fl_exp(y * fl_log(x));
}

/* GBA LCD colour model (see docs/FILTERS.md "Colour correction"):
 *   lin_c = (c5/31)^lcd_gamma
 *   out_c = ( sum_k M[c][k] * lin_k )^(1/2.2)
 * M is the channel-crosstalk matrix with every row normalised to sum 1.0, so
 * greys stay grey, black -> black and white -> white. Rows are the commonly
 * circulated higan values (R: 255 50 0 / G: 10 230 30 / B: 50 10 220). */
static void lut_build(gba_filter_t *c, int mode)
{
    static const double M[3][3] = {
        { 255.0 / 305.0, 50.0 / 305.0, 0.0 },
        { 10.0 / 270.0, 230.0 / 270.0, 30.0 / 270.0 },
        { 50.0 / 280.0, 10.0 / 280.0, 220.0 / 280.0 }
    };
    const double lcd_gamma = (mode == GBA_COLOUR_LCD_DARK) ? 4.0 : 2.2;
    const double inv_out = 1.0 / 2.2;
    double dec[32];
    for (int i = 0; i < 32; ++i) dec[i] = fl_pow((double)i / 31.0, lcd_gamma);
    for (int r = 0; r < 32; ++r)
        for (int g = 0; g < 32; ++g)
            for (int b = 0; b < 32; ++b) {
                unsigned o[3];
                for (int ch = 0; ch < 3; ++ch) {
                    double v = M[ch][0] * dec[r] + M[ch][1] * dec[g] + M[ch][2] * dec[b];
                    if (v > 1.0) v = 1.0;
                    int q = (int)(255.0 * fl_pow(v, inv_out) + 0.5);
                    o[ch] = (unsigned)iclamp(q, 0, 255);
                }
                c->lut[(r << 10) | (g << 5) | b] = ALPHA | (o[0] << 16) | (o[1] << 8) | o[2];
            }
    c->lut_mode = mode;
}

static inline unsigned lut_index(px p)
{
    return ((p >> 9) & 0x7C00u) | ((p >> 6) & 0x03E0u) | ((p >> 3) & 0x001Fu);
}

uint32_t gba_filter_lut_lookup(const gba_filter_t *c, uint32_t xrgb)
{
    if (c->lut_mode < 0) return xrgb | ALPHA;
    return c->lut[lut_index(xrgb)];
}

/* ------------------------------------------------------------------------ */
/* Pixel helpers                                                             */
/* ------------------------------------------------------------------------ */

/* floor average per channel */
static inline px avg2(px a, px b) { return (((a ^ b) & 0xFEFEFEFEu) >> 1) + (a & b); }

/* (a*(8-w) + b*w) / 8 per channel, w in 0..8; result alpha = 0xFF */
static inline px mix8(px a, px b, unsigned w)
{
    unsigned rb = ((((a & 0x00FF00FFu) * (8u - w)) + ((b & 0x00FF00FFu) * w)) >> 3) & 0x00FF00FFu;
    unsigned g = (((((a >> 8) & 0xFFu) * (8u - w)) + (((b >> 8) & 0xFFu) * w)) >> 3) & 0xFFu;
    return ALPHA | rb | (g << 8);
}

/* L1 colour distance over R,G,B, 0..765 */
static inline int dist(px a, px b)
{
    int dr = (int)((a >> 16) & 255u) - (int)((b >> 16) & 255u);
    int dg = (int)((a >> 8) & 255u) - (int)((b >> 8) & 255u);
    int db = (int)(a & 255u) - (int)(b & 255u);
    return (dr < 0 ? -dr : dr) + (dg < 0 ? -dg : dg) + (db < 0 ? -db : db);
}

#define EDGE_TH 48 /* "near" = L1 distance <= 48 of 765 (about 2 5-bit levels per channel on average) */
static inline int nr(px a, px b) { return dist(a, b) <= EDGE_TH; }

/* ------------------------------------------------------------------------ */
/* Scale2x / Scale3x (AdvMAME rules)                                         */
/* ------------------------------------------------------------------------ */

static void scale2x(const px *s, size_t sp, int w, int h, px *d, size_t dp)
{
    for (int y = 0; y < h; ++y) {
        const px *r0 = s + (size_t)(y > 0 ? y - 1 : 0) * sp;
        const px *r1 = s + (size_t)y * sp;
        const px *r2 = s + (size_t)(y < h - 1 ? y + 1 : y) * sp;
        px *o0 = d + (size_t)(2 * y) * dp, *o1 = o0 + dp;
        for (int x = 0; x < w; ++x) {
            int xl = x > 0 ? x - 1 : 0, xr = x < w - 1 ? x + 1 : x;
            px B = r0[x], D = r1[xl], E = r1[x], F = r1[xr], H = r2[x];
            px e0 = E, e1 = E, e2 = E, e3 = E;
            if (!eq(B, H) && !eq(D, F)) {
                if (eq(D, B)) e0 = D;
                if (eq(B, F)) e1 = F;
                if (eq(D, H)) e2 = D;
                if (eq(H, F)) e3 = F;
            }
            o0[2 * x] = e0 | ALPHA; o0[2 * x + 1] = e1 | ALPHA;
            o1[2 * x] = e2 | ALPHA; o1[2 * x + 1] = e3 | ALPHA;
        }
    }
}

static void scale3x(const px *s, size_t sp, int w, int h, px *d, size_t dp)
{
    for (int y = 0; y < h; ++y) {
        const px *r0 = s + (size_t)(y > 0 ? y - 1 : 0) * sp;
        const px *r1 = s + (size_t)y * sp;
        const px *r2 = s + (size_t)(y < h - 1 ? y + 1 : y) * sp;
        px *o0 = d + (size_t)(3 * y) * dp, *o1 = o0 + dp, *o2 = o1 + dp;
        for (int x = 0; x < w; ++x) {
            int xl = x > 0 ? x - 1 : 0, xr = x < w - 1 ? x + 1 : x;
            px A = r0[xl], B = r0[x], C = r0[xr];
            px D = r1[xl], E = r1[x], F = r1[xr];
            px G = r2[xl], H = r2[x], I = r2[xr];
            px e[9];
            for (int k = 0; k < 9; ++k) e[k] = E;
            if (!eq(B, H) && !eq(D, F)) {
                if (eq(D, B)) e[0] = D;
                if ((eq(D, B) && !eq(E, C)) || (eq(B, F) && !eq(E, A))) e[1] = B;
                if (eq(B, F)) e[2] = F;
                if ((eq(D, B) && !eq(E, G)) || (eq(D, H) && !eq(E, A))) e[3] = D;
                if ((eq(B, F) && !eq(E, I)) || (eq(H, F) && !eq(E, C))) e[5] = F;
                if (eq(D, H)) e[6] = D;
                if ((eq(D, H) && !eq(E, I)) || (eq(H, F) && !eq(E, G))) e[7] = H;
                if (eq(H, F)) e[8] = F;
            }
            for (int k = 0; k < 3; ++k) {
                o0[3 * x + k] = e[k] | ALPHA;
                o1[3 * x + k] = e[3 + k] | ALPHA;
                o2[3 * x + k] = e[6 + k] | ALPHA;
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/* Original edge-directed scaler ("Edge2x/Edge3x"). Same neighbourhood and    */
/* guard structure as Scale2x/3x, but with a bounded colour distance instead  */
/* of exact equality and a 6/8 (corner) or 4/8 (edge) blend toward the        */
/* neighbouring region instead of a hard copy, so staircase diagonals are     */
/* softened by about half a level and near-identical dither is left alone.    */
/* ------------------------------------------------------------------------ */

/* Corner pixel of E between orthogonal neighbours P,Q (opposites P2,Q2). */
static inline px edge_corner(px E, px P, px Q)
{
    if (nr(P, Q)) {
        px R = avg2(P, Q);
        if (!nr(E, R)) return mix8(E, R, 6);
    }
    return E | ALPHA;
}

static inline px edge_side(px E, px X) { return nr(E, X) ? (E | ALPHA) : mix8(E, X, 4); }

static void edge2x(const px *s, size_t sp, int w, int h, px *d, size_t dp)
{
    for (int y = 0; y < h; ++y) {
        const px *r0 = s + (size_t)(y > 0 ? y - 1 : 0) * sp;
        const px *r1 = s + (size_t)y * sp;
        const px *r2 = s + (size_t)(y < h - 1 ? y + 1 : y) * sp;
        px *o0 = d + (size_t)(2 * y) * dp, *o1 = o0 + dp;
        for (int x = 0; x < w; ++x) {
            int xl = x > 0 ? x - 1 : 0, xr = x < w - 1 ? x + 1 : x;
            px B = r0[x], D = r1[xl], E = r1[x], F = r1[xr], H = r2[x];
            px e0 = E | ALPHA, e1 = e0, e2 = e0, e3 = e0;
            if (!nr(B, H) && !nr(D, F)) {
                e0 = edge_corner(E, B, D);
                e1 = edge_corner(E, B, F);
                e2 = edge_corner(E, D, H);
                e3 = edge_corner(E, H, F);
            }
            o0[2 * x] = e0; o0[2 * x + 1] = e1;
            o1[2 * x] = e2; o1[2 * x + 1] = e3;
        }
    }
}

static void edge3x(const px *s, size_t sp, int w, int h, px *d, size_t dp)
{
    for (int y = 0; y < h; ++y) {
        const px *r0 = s + (size_t)(y > 0 ? y - 1 : 0) * sp;
        const px *r1 = s + (size_t)y * sp;
        const px *r2 = s + (size_t)(y < h - 1 ? y + 1 : y) * sp;
        px *o0 = d + (size_t)(3 * y) * dp, *o1 = o0 + dp, *o2 = o1 + dp;
        for (int x = 0; x < w; ++x) {
            int xl = x > 0 ? x - 1 : 0, xr = x < w - 1 ? x + 1 : x;
            px A = r0[xl], B = r0[x], C = r0[xr];
            px D = r1[xl], E = r1[x], F = r1[xr];
            px G = r2[xl], H = r2[x], I = r2[xr];
            px e[9];
            for (int k = 0; k < 9; ++k) e[k] = E | ALPHA;
            if (!nr(B, H) && !nr(D, F)) {
                int db = nr(D, B), bf = nr(B, F), dh = nr(D, H), hf = nr(H, F);
                e[0] = edge_corner(E, B, D);
                e[2] = edge_corner(E, B, F);
                e[6] = edge_corner(E, D, H);
                e[8] = edge_corner(E, H, F);
                if ((db && !nr(E, C)) || (bf && !nr(E, A))) e[1] = edge_side(E, B);
                if ((db && !nr(E, G)) || (dh && !nr(E, A))) e[3] = edge_side(E, D);
                if ((bf && !nr(E, I)) || (hf && !nr(E, C))) e[5] = edge_side(E, F);
                if ((dh && !nr(E, I)) || (hf && !nr(E, G))) e[7] = edge_side(E, H);
            }
            for (int k = 0; k < 3; ++k) {
                o0[3 * x + k] = e[k];
                o1[3 * x + k] = e[3 + k];
                o2[3 * x + k] = e[6 + k];
            }
        }
    }
}

/* plain copy that normalises the alpha byte (identity pipeline) */
static void copy_frame(const px *s, size_t sp, int w, int h, px *d, size_t dp)
{
    for (int y = 0; y < h; ++y) {
        const px *a = s + (size_t)y * sp;
        px *o = d + (size_t)y * dp;
        for (int x = 0; x < w; ++x) o[x] = a[x] | ALPHA;
    }
}

/* ------------------------------------------------------------------------ */
/* Replicate + overlay stage                                                 */
/* ------------------------------------------------------------------------ */

static void hrep(px *o, const px *s, int w, int r)
{
    switch (r) {
    case 2: for (int x = 0; x < w; ++x) { px p = s[x] | ALPHA; o[2 * x] = p; o[2 * x + 1] = p; } break;
    case 3: for (int x = 0; x < w; ++x) { px p = s[x] | ALPHA; o[3 * x] = p; o[3 * x + 1] = p; o[3 * x + 2] = p; } break;
    case 4: for (int x = 0; x < w; ++x) { px p = s[x] | ALPHA; o[4 * x] = p; o[4 * x + 1] = p; o[4 * x + 2] = p; o[4 * x + 3] = p; } break;
    default:
        for (int x = 0; x < w; ++x) { px p = s[x] | ALPHA; for (int k = 0; k < r; ++k) o[x * r + k] = p; }
    }
}

/* Q8 multiply of all three channels by the same factor f (0..256). */
static inline px mul_uni(px p, unsigned f)
{
    unsigned rb = (((p & 0x00FF00FFu) * f) >> 8) & 0x00FF00FFu;
    unsigned g = (((p & 0x0000FF00u) * f) >> 8) & 0x0000FF00u;
    return ALPHA | rb | g;
}

static inline px mul_rgb(px p, unsigned fr, unsigned fg, unsigned fb)
{
    unsigned r = (((p >> 16) & 255u) * fr) >> 8;
    unsigned g = (((p >> 8) & 255u) * fg) >> 8;
    unsigned b = ((p & 255u) * fb) >> 8;
    return ALPHA | (r << 16) | (g << 8) | b;
}

/* One output row of an overlay: source row s (sw px), b = scaler factor,
 * r = replicate factor, cell row factors f (8 columns x 3), uni flags. */
static void row_overlay(px *o, const px *s, int sw, int b, int r, const uint16_t *f, const uint8_t *uni)
{
    int j0 = 0;
    for (int x = 0; x < sw; ) {
        for (int xi = 0; xi < b; ++xi, ++x) {
            px p = s[x];
            for (int jj = 0; jj < r; ++jj) {
                int j = xi * r + jj;
                o[x * r + jj] = uni[j] ? mul_uni(p, f[j * 3]) : mul_rgb(p, f[j * 3], f[j * 3 + 1], f[j * 3 + 2]);
            }
        }
        (void)j0;
    }
}

static void post_stage(const gba_filter_t *c, const px *s, size_t sp, int sw, int sh,
                       int b, int r, px *d, size_t dp)
{
    const size_t row_bytes = (size_t)sw * (size_t)r * sizeof(px);
    int g = 0;
    for (int ys = 0; ys < sh; ++ys) {
        const px *srow = s + (size_t)ys * sp;
        px *orow0 = d + (size_t)ys * (size_t)r * dp;
        for (int kk = 0; kk < r; ++kk) {
            px *o = orow0 + (size_t)kk * dp;
            if (!c->ovl_T) {
                if (kk == 0) hrep(o, srow, sw, r);
                else MEMCPY(o, orow0, row_bytes);
                continue;
            }
            int ky = g * r + kk, dup = c->ovl_dup[ky];
            if (dup >= 0) { MEMCPY(o, orow0 + (size_t)(dup - g * r) * dp, row_bytes); continue; }
            row_overlay(o, srow, sw, b, r, &c->ovl_f[ky * 8 * 3], &c->ovl_uni[ky * 8]);
        }
        if (++g == b) g = 0;
    }
}

/* Build the per-cell factor table for an overlay at cell size T (3..8 grid,
 * 2..8 scanlines) given replicate factor r (for the duplicate-row map). */
static void ovl_build(gba_filter_t *c, int overlay, int strength, int T, int r)
{
    uint16_t *f = c->ovl_f;
    for (int i = 0; i < 8 * 8 * 3; ++i) f[i] = 256;
    if (overlay == GBA_OVL_LCD_GRID) {
        int se = strength * imin(T, 6) / 6;               /* thinner cells look harsher: soften */
        int th = T >= 6 ? 2 : 1;                           /* line thickness */
        unsigned gl = 256u - (unsigned)(se * 217 / 255);   /* line brightness, >= 39 (15%) */
        unsigned ft = 256u - (unsigned)(se * 64 / 255);    /* subpixel dimming of the other two channels */
        for (int k = 0; k < T; ++k)
            for (int j = 0; j < T; ++j) {
                unsigned fr = 256, fg = 256, fb = 256;
                int z = j * 3 / T;                         /* 0 red, 1 green, 2 blue stripe */
                if (z != 0) fr = ft;
                if (z != 1) fg = ft;
                if (z != 2) fb = ft;
                if (k >= T - th) { fr = fr * gl >> 8; fg = fg * gl >> 8; fb = fb * gl >> 8; }
                if (j >= T - th) { fr = fr * gl >> 8; fg = fg * gl >> 8; fb = fb * gl >> 8; }
                uint16_t *e = &f[(k * 8 + j) * 3];
                e[0] = (uint16_t)fr; e[1] = (uint16_t)fg; e[2] = (uint16_t)fb;
            }
    } else { /* scanlines: the last (T+1)/3 rows of each source row are dimmed */
        int d = (T + 1) / 3;
        unsigned keep = (overlay == GBA_OVL_SCAN_STRONG) ? 148u : 200u; /* 58% / 78%, TizNeo's weights */
        for (int k = T - d; k < T; ++k)
            for (int j = 0; j < T; ++j) {
                uint16_t *e = &f[(k * 8 + j) * 3];
                e[0] = e[1] = e[2] = (uint16_t)keep;
            }
    }
    for (int k = 0; k < 8; ++k)
        for (int j = 0; j < 8; ++j) {
            const uint16_t *e = &f[(k * 8 + j) * 3];
            c->ovl_uni[k * 8 + j] = (uint8_t)(e[0] == e[1] && e[1] == e[2]);
        }
    for (int k = 0; k < 8; ++k) c->ovl_dup[k] = -1;
    for (int k = 0; k < T; ++k) {
        int lo = (k / r) * r;
        for (int k2 = lo; k2 < k; ++k2) {
            int same = 1;
            for (int i = 0; i < 8 * 3; ++i) if (f[k * 24 + i] != f[k2 * 24 + i]) { same = 0; break; }
            if (same) { c->ovl_dup[k] = (int8_t)k2; break; }
        }
    }
    c->ovl_T = T;
}

/* ------------------------------------------------------------------------ */
/* Provisional cost model in sixteenths of a nanosecond per pixel. Calibrate
 * against the host harness and Q60A before treating these as measured costs. */
/* ------------------------------------------------------------------------ */

#define C_COLOUR   (40)   /* per source pixel   */
#define C_BLEND    (28)   /* per source pixel   */
#define C_BOTH     (56)
#define C_SCALE2   (180)  /* per OUTPUT pixel   */
#define C_SCALE3   (150)
#define C_EDGE2    (420)
#define C_EDGE3    (340)
#define C_COPY     (14)
#define C_REP      (16)
#define C_OVL_UNI  (60)
#define C_OVL_RGB  (84)

void gba_filter_estimate(gba_filter_effective_t *e)
{
    unsigned long long u = 0; /* ns * 16 */
    const unsigned long long src = (unsigned long long)GBA_SRC_W * GBA_SRC_H;
    if (e->colour && e->blend) u += src * C_BOTH;
    else if (e->colour) u += src * C_COLOUR;
    else if (e->blend) u += src * C_BLEND;
    int b = e->base_factor;
    if (b > 1) {
        unsigned long long o = src * (unsigned long long)(b * b);
        int edge = (e->scaler == GBA_F_EDGE);
        if (b == 2) u += o * (edge ? C_EDGE2 : C_SCALE2);
        else if (b == 3) u += o * (edge ? C_EDGE3 : C_SCALE3);
        else if (b == 4) u += (src * 4) * C_SCALE2 + o * C_SCALE2;
    }
    unsigned long long outp = (unsigned long long)e->out_w * (unsigned long long)e->out_h;
    if (e->overlay) {
        int tint = (e->overlay == GBA_OVL_LCD_GRID && e->total_factor >= 3);
        u += outp * (tint ? C_OVL_RGB : C_OVL_UNI);
    } else if (e->post_factor > 1) u += outp * C_REP;
    else if (b == 1 && !e->identity) u += outp * C_COPY;
    e->cost_ns = (unsigned)(u / 16);
    if (e->identity && !e->colour && !e->blend) e->cost_class = GBA_COST_NONE;
    else if (e->cost_ns <= 1500000u) e->cost_class = GBA_COST_CHEAP;
    else if (e->cost_ns <= 4000000u) e->cost_class = GBA_COST_MEDIUM;
    else e->cost_class = GBA_COST_HEAVY;
}

int gba_filter_cheaper(gba_filter_settings_t *s)
{
    gba_filter_settings_sanitize(s);
    if (s->scaler == GBA_F_SCALE4X || s->scaler == GBA_F_EDGE) { s->scaler = GBA_F_SCALE2X; return 1; }
    if (s->scaler == GBA_F_SCALE2X || s->scaler == GBA_F_SCALE3X || s->scaler == GBA_F_SHARP_BILINEAR) {
        s->scaler = GBA_F_NEAREST;
        /* At some fractional canvases nearest+overlay raises the replicate
           factor above the scaler being removed. Drop the overlay with this
           step so the fallback cannot increase its own estimated work. */
        s->overlay = GBA_OVL_OFF;
        return 1;
    }
    if (s->scaler == GBA_F_SMOOTH && s->overlay == GBA_OVL_OFF && !s->blend && !s->colour) {
        s->scaler = GBA_F_NEAREST; return 1; /* both are free; keep the ladder strictly decreasing */
    }
    if (s->overlay != GBA_OVL_OFF) { s->overlay = GBA_OVL_OFF; return 1; }
    if (s->blend) { s->blend = 0; return 1; }
    if (s->colour) { s->colour = GBA_COLOUR_VIVID; return 1; }
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Lifecycle, planning                                                       */
/* ------------------------------------------------------------------------ */

int gba_filter_init(gba_filter_t *c, int max_w, int max_h)
{
    if (!c) return -1;
    if ((max_w > 0 && max_w < GBA_SRC_W) || (max_h > 0 && max_h < GBA_SRC_H)) return -1;
    MEMSET(c, 0, sizeof *c);
    c->max_w = (max_w <= 0) ? GBA_FILTER_MAX_W : imin(max_w, GBA_FILTER_MAX_W);
    c->max_h = (max_h <= 0) ? GBA_FILTER_MAX_H : imin(max_h, GBA_FILTER_MAX_H);
    c->lut_mode = -1;
    gba_filter_settings_default(&c->cfg);
    return 0;
}

void gba_filter_reset(gba_filter_t *c) { c->prev_valid = 0; }

const gba_filter_effective_t *gba_filter_effective(const gba_filter_t *c) { return c->configured ? &c->eff : 0; }

void gba_filter_fit_target(int cw, int ch, int mode, gba_rect_t *o)
{
    int w, h, n = 0;
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    int fit = imin(cw / GBA_SRC_W, ch / GBA_SRC_H);
    if (mode == GBA_SCREEN_INTEGER && fit >= 1) { n = fit; w = GBA_SRC_W * n; h = GBA_SRC_H * n; }
    else if (mode == GBA_SCREEN_FILL) { w = cw; h = ch; }
    else { /* fit-height (also the integer fallback on tiny canvases) */
        long long fit_w = (long long)ch * 3 / 2;
        h = ch;
        if (fit_w > cw) { w = cw; h = (int)((long long)cw * 2 / 3); }
        else w = (int)fit_w;
        if (w < 1) w = 1;
        if (h < 1) h = 1;
        if (w % GBA_SRC_W == 0 && h % GBA_SRC_H == 0 && w / GBA_SRC_W == h / GBA_SRC_H) n = w / GBA_SRC_W;
    }
    o->w = w; o->h = h; o->n = n;
    o->x = (cw - w) / 2; o->y = (ch - h) / 2;
}

static int pick_edge_factor(int n)
{
    if (n % 2 == 0) return 2;
    if (n % 3 == 0) return 3;
    return n >= 3 ? 3 : 2;
}

static int base_factor_of(int scaler, int nf)
{
    switch (scaler) {
    case GBA_F_SCALE2X: return 2;
    case GBA_F_SCALE3X: return 3;
    case GBA_F_SCALE4X: return 4;
    case GBA_F_EDGE: return pick_edge_factor(nf);
    default: return 1;
    }
}

int gba_filter_configure(gba_filter_t *c, const gba_filter_settings_t *s, int tw, int th,
                         gba_filter_effective_t *out)
{
    if (!c || !s || c->max_w <= 0) return -1;
    gba_filter_settings_t cs = *s;
    unsigned notes = 0;
    if (gba_filter_settings_sanitize(&cs)) notes |= GBA_NOTE_BAD_SETTING;
    if (tw <= 0 || th <= 0) { tw = GBA_SRC_W; th = GBA_SRC_H; notes |= GBA_NOTE_BAD_TARGET; }

    const int nf = imax(1, imin(tw / GBA_SRC_W, th / GBA_SRC_H));
    const int ncw = (int)(((long long)tw + GBA_SRC_W - 1) / GBA_SRC_W);
    const int nch = (int)(((long long)th + GBA_SRC_H - 1) / GBA_SRC_H);
    const int nc = imax(1, imax(ncw, nch));
    const int exact = (tw % GBA_SRC_W == 0 && th % GBA_SRC_H == 0);

    int scaler = cs.scaler, overlay = cs.overlay;
    if (overlay == GBA_OVL_LCD_GRID && cs.lcd_strength == 0) overlay = GBA_OVL_OFF;
    if (scaler == GBA_F_SHARP_BILINEAR && exact) { scaler = GBA_F_NEAREST; notes |= GBA_NOTE_SHARP_IS_NEAREST; }

    int b = 1, r = 1;
    for (int pass = 0; pass < 2; ++pass) {
        b = base_factor_of(scaler, nf);
        r = 1;
        const int minT = overlay == GBA_OVL_LCD_GRID ? 3 : (overlay != GBA_OVL_OFF ? 2 : 0);
        if (scaler == GBA_F_SHARP_BILINEAR) r = iclamp(nc, 2, 8);
        if (minT) {
            int goal = (scaler == GBA_F_SHARP_BILINEAR) ? nc : nf;
            goal = iclamp(goal, minT, 8);
            int r2 = imax(1, goal / b);
            while (b * r2 < minT) ++r2;
            if (scaler != GBA_F_SHARP_BILINEAR && r2 > 1) notes |= GBA_NOTE_OVERLAY_RAISED;
            r = r2;
        }
        while (r > 1 && (GBA_SRC_W * b * r > c->max_w || GBA_SRC_H * b * r > c->max_h || (overlay && b * r > 8))) {
            --r; notes |= GBA_NOTE_SIZE_CLAMPED;
        }
        if (GBA_SRC_W * b <= c->max_w && GBA_SRC_H * b <= c->max_h) break;
        scaler = GBA_F_NEAREST; notes |= GBA_NOTE_SCALER_DOWNGRADED; /* second pass plans a 1x scaler */
    }
    if (overlay && b * r < (overlay == GBA_OVL_LCD_GRID ? 3 : 2)) {
        overlay = GBA_OVL_OFF; notes |= GBA_NOTE_OVERLAY_DROPPED;
        if (scaler != GBA_F_SHARP_BILINEAR) r = 1;
    }

    gba_filter_effective_t e;
    MEMSET(&e, 0, sizeof e);
    e.scaler = scaler; e.base_factor = b; e.post_factor = r; e.total_factor = b * r;
    e.out_w = GBA_SRC_W * b * r; e.out_h = GBA_SRC_H * b * r;
    e.overlay = overlay; e.lcd_strength = cs.lcd_strength;
    e.colour = cs.colour; e.blend = cs.blend;
    e.notes = notes;
    e.identity = (e.total_factor == 1 && !e.colour && !e.blend && !e.overlay);
    if (scaler == GBA_F_SMOOTH) e.hint = GBA_HINT_SMOOTH;
    else if (tw >= e.out_w && th >= e.out_h && tw % e.out_w == 0 && th % e.out_h == 0) e.hint = GBA_HINT_NEAREST;
    else if (scaler == GBA_F_NEAREST && !overlay) e.hint = GBA_HINT_NEAREST;
    else e.hint = GBA_HINT_SMOOTH;
    gba_filter_estimate(&e);

    if (e.colour && c->lut_mode != e.colour) lut_build(c, e.colour);
    if (!e.blend || !c->configured || c->cfg.blend != e.blend || c->cfg.colour != cs.colour)
        c->prev_valid = 0;
    if (e.overlay) ovl_build(c, e.overlay, e.lcd_strength, e.total_factor, r);
    else c->ovl_T = 0;

    c->cfg = cs;
    c->eff = e;
    c->tw = tw; c->th = th;
    c->configured = 1;
    if (out) *out = e;
    return 0;
}

/* ------------------------------------------------------------------------ */
/* Per-frame                                                                 */
/* ------------------------------------------------------------------------ */

static void prep_frame(gba_filter_t *c, const px *src, size_t sp)
{
    const int cc = c->eff.colour != 0, bl = c->eff.blend;
    const int valid = c->prev_valid;
    for (int y = 0; y < GBA_SRC_H; ++y) {
        const px *s = src + (size_t)y * sp;
        px *f = c->frame + (size_t)y * GBA_SRC_W;
        px *pv = c->prev + (size_t)y * GBA_SRC_W;
        const px *lut = c->lut;
        if (cc && !bl) {
            for (int x = 0; x < GBA_SRC_W; ++x) f[x] = lut[lut_index(s[x])];
        } else if (!cc) {
            for (int x = 0; x < GBA_SRC_W; ++x) {
                px v = s[x] | ALPHA;
                f[x] = valid ? avg2(v, pv[x]) : v;
                pv[x] = v;
            }
        } else {
            for (int x = 0; x < GBA_SRC_W; ++x) {
                px v = lut[lut_index(s[x])];
                f[x] = valid ? avg2(v, pv[x]) : v;
                pv[x] = v;
            }
        }
    }
    if (bl) c->prev_valid = 1;
}

int gba_filter_apply(gba_filter_t *c, const void *src, size_t src_pitch, void *dst, size_t dst_pitch)
{
    if (!c || !c->configured || !src || !dst) return -1;
    const gba_filter_effective_t *e = &c->eff;
    if ((src_pitch & 3) || (dst_pitch & 3) || src_pitch < GBA_SRC_W * 4 || dst_pitch < (size_t)e->out_w * 4) return -2;
    if (((uintptr_t)src & 3) || ((uintptr_t)dst & 3)) return -2;
    const px *cur = (const px *)src;
    size_t cp = src_pitch / 4;
    px *out = (px *)dst;
    const size_t dp = dst_pitch / 4;

    if (e->colour || e->blend) { prep_frame(c, cur, cp); cur = c->frame; cp = GBA_SRC_W; }

    const int b = e->base_factor, r = e->post_factor;
    const int post = (r > 1 || e->overlay);
    const px *mid = cur;      /* scaler output */
    size_t mp = cp;
    int mw = GBA_SRC_W, mh = GBA_SRC_H;

    if (b == 1) {
        if (!post) { copy_frame(cur, cp, GBA_SRC_W, GBA_SRC_H, out, dp); return 0; }
    } else if (b == 2) {
        px *o = post ? c->s1 : out; size_t op = post ? 480 : dp;
        if (e->scaler == GBA_F_EDGE) edge2x(cur, cp, mw, mh, o, op); else scale2x(cur, cp, mw, mh, o, op);
        mid = o; mp = op; mw = 480; mh = 320;
    } else if (b == 3) {
        px *o = post ? c->s2 : out; size_t op = post ? 720 : dp;
        if (e->scaler == GBA_F_EDGE) edge3x(cur, cp, mw, mh, o, op); else scale3x(cur, cp, mw, mh, o, op);
        mid = o; mp = op; mw = 720; mh = 480;
    } else { /* 4: Scale2x twice */
        scale2x(cur, cp, mw, mh, c->s1, 480);
        px *o = post ? c->s2 : out; size_t op = post ? 960 : dp;
        scale2x(c->s1, 480, 480, 320, o, op);
        mid = o; mp = op; mw = 960; mh = 640;
    }
    if (post) post_stage(c, mid, mp, mw, mh, b, r, out, dp);
    return 0;
}
