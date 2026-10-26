/* Host harness for app/gba_main.c: the real frontend and the real filter
 * module compiled unchanged (-Dmain=tizgba_app_main) against
 * harness/fake_efl.h, a fake library and a fake mGBA bridge.
 * First execution of the TizGBA UI outside the TV: library layout with the
 * Exit row, the Performance overlay, and persistence in tizgba-settings.conf. */
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../app/platform.h"
#include "../app/library.h"
#include "fake_efl.h"

#define DATA "out/data"

/* ---- fake library ---- */
static lib_rom g_roms[3];
static const char *g_names[3] = { "Advance Wars.gba", "Metroid Fusion.gba", "Sonic Advance.gba" };
static lib_stats g_stats;
void lib_scan(void)
{
    for (int i = 0; i < 3; i++) { snprintf(g_roms[i].name, sizeof g_roms[i].name, "%s", g_names[i]); snprintf(g_roms[i].path, sizeof g_roms[i].path, "/games/%d.gba", i); snprintf(g_roms[i].where, sizeof g_roms[i].where, "TizGBA data"); g_roms[i].root = 0; }
}
int lib_count(void) { return 3; }
const lib_rom *lib_get(int i) { return i >= 0 && i < 3 ? &g_roms[i] : 0; }
int lib_find_path(const char *p) { for (int i = 0; i < 3; i++) if (!strcmp(p, g_roms[i].path)) return i; return -1; }
const lib_stats *lib_last_stats(void) { return &g_stats; }
int lib_mounts_changed(void) { return 0; }
int lib_roots_changed(void) { return 0; }
int lib_root_is_external(int r) { (void)r; return 0; }

/* ---- fake mGBA bridge ---- */
struct gbap_runtime { uint32_t fb[GBAP_WIDTH * GBAP_HEIGHT]; unsigned frames; };
static struct gbap_runtime g_rt;
gbap_runtime *gbap_create(void) { memset(&g_rt, 0, sizeof g_rt); return &g_rt; }
void gbap_destroy(gbap_runtime *r) { (void)r; }
bool gbap_load(gbap_runtime *r, const char *rom, const char *dir, const char *imp, const char *bios) { (void)r; (void)rom; (void)dir; (void)imp; (void)bios; return true; }
bool gbap_flush_save(gbap_runtime *r) { (void)r; return true; }
void gbap_set_keys(gbap_runtime *r, uint32_t k) { (void)r; (void)k; }
bool gbap_run_frame(gbap_runtime *r)
{
    for (unsigned i = 0; i < GBAP_WIDTH * GBAP_HEIGHT; i++) r->fb[i] = 0xff000000u | (i * 2654435761u >> 8) | (r->frames & 0xff);
    r->frames++;
    return true;
}
const uint32_t *gbap_framebuffer(const gbap_runtime *r) { return r->fb; }
size_t gbap_drain_audio(gbap_runtime *r, int16_t *out, size_t cap) { (void)r; (void)out; (void)cap; return 0; }
double gbap_frame_period(const gbap_runtime *r) { (void)r; return 1.0 / 59.7275; }
bool gbap_has_bios(const gbap_runtime *r) { (void)r; return false; }
const char *gbap_error(const gbap_runtime *r) { (void)r; return ""; }
const char *gbap_warning(const gbap_runtime *r) { (void)r; return ""; }

extern int tizgba_app_main(int, char **);
static int file_has(const char *path, const char *needle)
{
    char buf[2048] = ""; FILE *f = fopen(path, "r"); size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0; if (f) fclose(f); buf[n] = 0;
    return strstr(buf, needle) != 0;
}

static void fk_run_scenarios(void)
{
    ((int (*)(void *))fk_cb_set.create)(0);
    fk_pump(0.3);                                          /* "Scanning..." then the scan */
    printf("-- library\n");
    fobj_t *exit_row = fk_find_shown("Exit TizGBA");
    FK_CHECK(exit_row && exit_row->y == 230, "Exit TizGBA is the first library row (y=%d)", exit_row ? exit_row->y : -1);
    FK_CHECK(fk_find_shown("Advance Wars") && fk_find_shown("Advance Wars")->y == 230 + 52 &&
             fk_find_shown("Settings and controller setup")->y == 230 + 4 * 52, "Exit, 3 games, Settings in order");
    FK_CHECK(fk_cursor() && fk_cursor()->y == 236 + 52, "startup selection is the first game, not Exit (cursor y=%d)", fk_cursor() ? fk_cursor()->y : -1);
    fk_key("Up");
    FK_CHECK(fk_cursor()->y == 236, "Up reaches Exit");
    for (int i = 0; i < 6; i++) fk_key("Down");
    FK_CHECK(fk_cursor()->y == 236 + 4 * 52, "Down stops on the Settings row (last)");

    printf("-- settings\n");
    fk_key("Return");
    FK_CHECK(fk_find_shown("TizGBA settings") != 0, "Settings opens from the last row");
    for (int i = 0; i < 17; i++) fk_key("Down");
    FK_CHECK(fk_find_shown("Performance overlay: Off") && fk_find_shown("Off, FPS, or Detailed") && fk_find_shown("SYSTEM    (4 / 4)"),
             "overlay row first in the SYSTEM page, default Off");
    fk_key("Right");
    FK_CHECK(fk_find_shown("Performance overlay: FPS") && file_has(DATA "/tizgba-settings.conf", "perf_overlay=1"), "overlay -> FPS, persisted");
    fk_key("Right"); fk_key("Right");
    FK_CHECK(fk_find_shown("Performance overlay: Off"), "overlay cycles FPS > Detailed > Off");
    fk_key("Left");
    FK_CHECK(fk_find_shown("Performance overlay: Detailed") && file_has(DATA "/tizgba-settings.conf", "perf_overlay=2"), "overlay Left from Off -> Detailed, persisted");
    FK_CHECK(file_has(DATA "/tizgba-settings.conf", "version=2\nvolume=100\nmute=0\nscale=0\nperf_overlay=2\n"), "settings file keeps its version 2 schema with perf_overlay beside the existing keys");
    fk_key("Escape");

    printf("-- game with Detailed overlay\n");
    FK_CHECK(fk_cursor()->y == 236 + 4 * 52, "back in the library on the Settings row");
    for (int i = 0; i < 3; i++) fk_key("Up");
    fk_key("Return");
    FK_CHECK(!fk_find_shown(" late "), "overlay hidden until the first 1 s window closes");
    fk_pump(2.6);
    fobj_t *perf = fk_find_shown(" late ");
    FK_CHECK(perf && strstr(perf->text, " / 59.73 FPS"), "Detailed overlay while playing: %s", perf ? perf->text : "(none)");
    FK_CHECK(perf && perf->y == 44 && perf->x + (int)strlen(perf->text) * 14 == 1920 - 48, "right-aligned 48 px from the edge, y=44");
    unsigned fps_x100 = 0; if (perf) { unsigned a = 0, b = 0; if (sscanf(perf->text, "%u.%u", &a, &b) == 2) fps_x100 = a * 100 + b; }
    FK_CHECK(fps_x100 >= 5700 && fps_x100 <= 6100, "emulated rate near 59.73 (%u.%02u)", fps_x100 / 100, fps_x100 % 100);
    fk_key("Escape");
    fk_pump(0.1);
    FK_CHECK(!fk_find_shown(" late ") && fk_find_shown("Exit TizGBA"), "Back leaves the game: overlay hidden, library shown");

    printf("-- FPS mode and Off\n");
    FK_CHECK(fk_cursor()->y == 236 + 52, "cursor on the game that was played (y=%d)", fk_cursor() ? fk_cursor()->y : -1);
    fk_key("Down"); fk_key("Down"); fk_key("Down"); fk_key("Return");
    for (int i = 0; i < 17; i++) fk_key("Down");
    fk_key("Left");                                         /* Detailed -> FPS */
    fk_key("Escape");
    for (int i = 0; i < 3; i++) fk_key("Up");
    fk_key("Return");
    fk_pump(2.2);
    perf = fk_find_shown(" FPS");
    FK_CHECK(perf && !strstr(perf->text, "late") && !strstr(perf->text, " / "), "FPS mode shows only the rate: %s", perf ? perf->text : "(none)");
    fk_key("Escape"); fk_pump(0.1);
    fk_key("Down"); fk_key("Down"); fk_key("Down"); fk_key("Return");
    for (int i = 0; i < 17; i++) fk_key("Down");
    fk_key("Left");                                         /* FPS -> Off */
    fk_key("Escape");
    for (int i = 0; i < 3; i++) fk_key("Up");
    fk_key("Return");
    fk_pump(2.2);
    FK_CHECK(!fk_find_shown(" FPS"), "overlay Off: nothing drawn in game");
    fk_key("Escape"); fk_pump(0.1);

    printf("-- Exit row\n");
    for (int i = 0; i < 6; i++) fk_key("Up");
    fk_ui_exit = 0;
    fk_key("Return");
    FK_CHECK(fk_ui_exit, "OK on the Exit row closes the app");
    printf("\ngba ui_test: %d passed, %d failed\n", fk_pass, fk_fail);
}

int main(void)
{
    system("rm -rf out && mkdir -p out/data");
    tizgba_app_main(0, 0);
    return fk_fail ? 1 : 0;
}
