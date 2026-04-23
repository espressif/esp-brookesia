/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Visual regression test: captures screenshots of all UI screens at different resolutions.
 * Screenshots are saved as PNG files for comparison.
 *
 * Usage:
 *   ./build/rainmaker_ui_visual_test [-w WIDTH] [-H HEIGHT] [-o OUTPUT_DIR] [--create-baseline]
 *
 * Examples:
 *   ./build/rainmaker_ui_visual_test -o out                  # Default: 6 resolutions → out/
 *   ./build/rainmaker_ui_visual_test -w 800 -H 480 -o out    # Single resolution
 *   RM_PC_LCD_HOR=320 RM_PC_LCD_VER=240 ./build/rainmaker_ui_visual_test -o out
 *
 * Create baseline (default: all resolutions used by run_visual_tests.sh; use -w/-H or
 * RM_PC_LCD_HOR / RM_PC_LCD_VER for a single resolution):
 *   ./build/rainmaker_ui_visual_test --create-baseline
 *
 * Compare with baseline:
 *   ./build/rainmaker_ui_visual_test --compare baseline_dir output_dir
 */

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "lvgl.h"
#if LVGL_VERSION_MAJOR < 9
#include "src/extra/themes/basic/lv_theme_basic.h"
#include "src/extra/others/snapshot/lv_snapshot.h"
#else
#include "src/others/snapshot/lv_snapshot.h"
#endif

#include "esp_err.h"

#include "rainmaker_app_backend_layout_test.h"
#include "rainmaker_device_group_manager.h"
#include "ui.h"
#include "ui_display.h"
#include "ui_i18n.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32) || defined(_WIN64)
#include <direct.h>
#else
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef RM_PC_DIM_MIN
#define RM_PC_DIM_MIN 240
#endif
#ifndef RM_PC_DIM_MAX
#define RM_PC_DIM_MAX 4096
#endif

#define BUF_LINES 40

/* Test modes */
typedef enum {
    TEST_MODE_CAPTURE,
    TEST_MODE_CREATE_BASELINE,
    TEST_MODE_COMPARE
} test_mode_t;

/* Screen definitions for testing */
typedef struct {
    const char *name;
    lv_obj_t *(*show_func)(void);
    bool (*reload_func)(void);
} screen_def_t;

static int s_hor = 1024;
static int s_ver = 600;
/** True if -w / -H were passed on the command line. */
static bool s_cli_w_set;
static bool s_cli_h_set;
/** True if RM_PC_LCD_HOR / RM_PC_LCD_VER were read successfully from the environment. */
static bool s_env_hor_used;
static bool s_env_ver_used;
#if LVGL_VERSION_MAJOR >= 9
static uint8_t *s_buf;
static lv_display_t *s_disp;
#else
static lv_color_t *s_buf;
static lv_disp_draw_buf_t s_draw_buf;
#endif
static char s_output_dir[512] = "visual_test_output";
static char s_baseline_dir[512] = "visual_test_baseline";
static test_mode_t s_mode = TEST_MODE_CAPTURE;
static bool s_capture_both_langs = true;

#if LVGL_VERSION_MAJOR >= 9
static void headless_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}
#else
static void headless_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)area;
    (void)color_p;
    lv_disp_flush_ready(disp_drv);
}
#endif

static int parse_dim_env(const char *name, int *out)
{
    const char *v = getenv(name);
    if (!v || !v[0]) {
        return -1;
    }
    char buf[16];
    size_t n = 0;
    while (v[n] && n + 1 < sizeof(buf)) {
        buf[n] = v[n];
        n++;
    }
    if (v[n] != '\0') {
        return -1;
    }
    buf[n] = '\0';
    char *end = NULL;
    long x = strtol(buf, &end, 10);
    if (end == buf || *end != '\0' || x < RM_PC_DIM_MIN || x > RM_PC_DIM_MAX) {
        return -1;
    }
    *out = (int)x;
    return 0;
}

static int parse_dim_arg(const char *s, const char *exe, const char *what, int *out)
{
    char *end = NULL;
    long v = strtol(s, &end, 10);
    if (end == s || *end != '\0' || v < RM_PC_DIM_MIN || v > RM_PC_DIM_MAX) {
        fprintf(stderr, "%s: invalid %s %s\n", exe, what, s);
        return 1;
    }
    *out = (int)v;
    return 0;
}

static int parse_args(int argc, char **argv)
{
    int width = 0;
    int height = 0;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf(
                "Visual Regression Test for RainMaker UI\n"
                "\n"
                "Usage: %s [options]\n"
                "\n"
                "Options:\n"
                "  -w, --width <px>       Horizontal resolution (default: 1024)\n"
                "  -H, --height <px>      Vertical resolution (default: 600)\n"
                "  -o, --output <dir>      Output directory (default: visual_test_output)\n"
                "  -b, --baseline <dir>    Baseline directory for comparison\n"
                "  --create-baseline        Create baseline screenshots (default: all standard\n"
                "                            resolutions; use -w/-H or RM_PC_LCD_* for one size)\n"
                "  --compare               Compare with baseline\n"
                "  --en-only              Capture English only\n"
                "  -h, --help             Show this help\n"
                "\n"
                "Environment:\n"
                "  RM_PC_LCD_HOR / RM_PC_LCD_VER  Set resolution\n"
                "\n"
                "Examples:\n"
                "  %s -o out                             # Capture at all standard resolutions\n"
                "  %s -w 800 -H 480                   # Capture at 800x480\n"
                "  %s --create-baseline                   # Create baseline (all resolutions)\n"
                "  %s --compare baseline output            # Compare screenshots\n",
                argv[0], argv[0], argv[0], argv[0], argv[0]);
            return 2;
        }
        if (strcmp(a, "-w") == 0 || strcmp(a, "--width") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: missing value for %s\n", argv[0], a);
                return 1;
            }
            if (parse_dim_arg(argv[++i], argv[0], "width", &width) != 0) {
                return 1;
            }
            s_cli_w_set = true;
            continue;
        }
        if (strcmp(a, "-H") == 0 || strcmp(a, "--height") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: missing value for %s\n", argv[0], a);
                return 1;
            }
            if (parse_dim_arg(argv[++i], argv[0], "height", &height) != 0) {
                return 1;
            }
            s_cli_h_set = true;
            continue;
        }
        if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: missing value for %s\n", argv[0], a);
                return 1;
            }
            snprintf(s_output_dir, sizeof(s_output_dir), "%s", argv[++i]);
            continue;
        }
        if (strcmp(a, "-b") == 0 || strcmp(a, "--baseline") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "%s: missing value for %s\n", argv[0], a);
                return 1;
            }
            snprintf(s_baseline_dir, sizeof(s_baseline_dir), "%s", argv[++i]);
            continue;
        }
        if (strcmp(a, "--create-baseline") == 0) {
            s_mode = TEST_MODE_CREATE_BASELINE;
            snprintf(s_output_dir, sizeof(s_output_dir), "%s", s_baseline_dir);
            continue;
        }
        if (strcmp(a, "--compare") == 0) {
            s_mode = TEST_MODE_COMPARE;
            continue;
        }
        if (strcmp(a, "--en-only") == 0) {
            s_capture_both_langs = false;
            continue;
        }
        fprintf(stderr, "%s: unknown argument %s\n", argv[0], a);
        return 1;
    }

    if (width > 0) {
        s_hor = width;
    } else if (parse_dim_env("RM_PC_LCD_HOR", &s_hor) == 0) {
        s_env_hor_used = true;
    } else {
        s_hor = 1024;
    }

    if (height > 0) {
        s_ver = height;
    } else if (parse_dim_env("RM_PC_LCD_VER", &s_ver) == 0) {
        s_env_ver_used = true;
    } else {
        s_ver = 600;
    }

    return 0;
}

/** Same resolution list as run_visual_tests.sh (RESOLUTIONS). */
static const struct {
    int w;
    int h;
} s_baseline_resolutions[] = {
    {320, 240},
    {480, 272},
    {800, 480},
    {1024, 600},
    {1280, 720},
    {1920, 1080},
};

#define BASELINE_RES_COUNT ((int)(sizeof(s_baseline_resolutions) / sizeof(s_baseline_resolutions[0])))

/**
 * When neither -w/-H nor RM_PC_LCD_* are set, run all standard resolutions (same as
 * run_visual_tests.sh) for both capture and --create-baseline.
 */
static bool visual_test_multi_res_default(void)
{
    if (s_mode != TEST_MODE_CAPTURE && s_mode != TEST_MODE_CREATE_BASELINE) {
        return false;
    }
    if (s_cli_w_set || s_cli_h_set) {
        return false;
    }
    if (s_env_hor_used || s_env_ver_used) {
        return false;
    }
    return true;
}

static bool headless_disp_init(void)
{
#if LVGL_VERSION_MAJOR >= 9
    size_t buf_bytes = (size_t)s_hor * (size_t)BUF_LINES * (size_t)lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
    s_buf = (uint8_t *)malloc(buf_bytes);
    if (!s_buf) {
        return false;
    }
    s_disp = lv_display_create(s_hor, s_ver);
    if (!s_disp) {
        free(s_buf);
        s_buf = NULL;
        return false;
    }
    lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(s_disp, s_buf, NULL, (uint32_t)buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_disp, headless_flush_cb);
    return true;
#else
    size_t pixels = (size_t)s_hor * (size_t)BUF_LINES;
    s_buf = (lv_color_t *)malloc(pixels * sizeof(lv_color_t));
    if (!s_buf) {
        return false;
    }

    lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    lv_disp_draw_buf_init(&s_draw_buf, s_buf, NULL, (uint32_t)(s_hor * BUF_LINES));
    drv.hor_res = s_hor;
    drv.ver_res = s_ver;
    drv.flush_cb = headless_flush_cb;
    drv.draw_buf = &s_draw_buf;
    lv_disp_t *d = lv_disp_drv_register(&drv);
    return d != NULL;
#endif
}

static void headless_disp_free(void)
{
#if LVGL_VERSION_MAJOR >= 9
    if (s_disp) {
        lv_display_delete(s_disp);
        s_disp = NULL;
    }
#else
    {
        lv_disp_t *d = lv_disp_get_default();
        if (d) {
            lv_disp_remove(d);
        }
    }
#endif
    if (s_buf) {
        free(s_buf);
        s_buf = NULL;
    }
}

/* Headless: avoid lv_timer_handler() loops — can fault on macOS/arm64 (see ui_pc_smoke_test.c). */
static void lv_timer_pump(unsigned rounds)
{
    for (unsigned i = 0; i < rounds; i++) {
        lv_tick_inc(5);
        lv_timer_handler();
    }
}

static void mkdir_p(const char *path)
{
#if defined(_WIN32) || defined(_WIN64)
    (void)_mkdir(path);
#else
    (void)mkdir(path, 0755);
#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
static bool output_dir_path_is_safe_for_remove(const char *path)
{
    if (!path || !path[0]) {
        return false;
    }
    if (strcmp(path, "/") == 0) {
        return false;
    }
    if (strcmp(path, ".") == 0 || strcmp(path, "..") == 0) {
        return false;
    }
    return true;
}

static int rm_rf_at(const char *path)
{
    struct stat st;

    if (stat(path, &st) != 0) {
        return (errno == ENOENT) ? 0 : -1;
    }
    if (S_ISDIR(st.st_mode)) {
        DIR *d = opendir(path);
        if (!d) {
            return -1;
        }
        struct dirent *de;
        char sub[PATH_MAX];
        while ((de = readdir(d)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0) {
                continue;
            }
            int n = snprintf(sub, sizeof(sub), "%s/%s", path, de->d_name);
            if (n < 0 || (size_t)n >= sizeof(sub)) {
                closedir(d);
                errno = ENAMETOOLONG;
                return -1;
            }
            if (rm_rf_at(sub) != 0) {
                closedir(d);
                return -1;
            }
        }
        closedir(d);
        return rmdir(path);
    }
    return unlink(path);
}

/**
 * Remove the output directory tree (if present) and recreate it empty so that
 * baseline / capture runs do not leave stale PNG/PPM from older UI versions.
 */
static void prepare_fresh_output_dir(void)
{
    if (!output_dir_path_is_safe_for_remove(s_output_dir)) {
        fprintf(stderr, "Refusing to remove unsafe output path: %s\n", s_output_dir);
        exit(1);
    }
    if (rm_rf_at(s_output_dir) != 0) {
        fprintf(stderr, "Failed to clear output directory: %s (%s)\n", s_output_dir, strerror(errno));
        exit(1);
    }
    mkdir_p(s_output_dir);
}
#else
static void prepare_fresh_output_dir(void)
{
    mkdir_p(s_output_dir);
}
#endif

static void save_screenshot(const char *screen_name, const char *lang_suffix)
{
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/%dx%d_%s_%s.png",
             s_output_dir, s_hor, s_ver, screen_name, lang_suffix);

    lv_obj_t *scr = lv_scr_act();
    if (!scr) {
        fprintf(stderr, "save_screenshot: no active screen\n");
        return;
    }

    lv_disp_t *d_snap = lv_disp_get_default();
    if (d_snap) {
        lv_coord_t sw = lv_disp_get_hor_res(d_snap);
        lv_coord_t sh = lv_disp_get_ver_res(d_snap);
        if (sw > 0 && sh > 0) {
            lv_obj_set_size(scr, sw, sh);
        }
    }
    lv_obj_update_layout(scr);

#if LVGL_VERSION_MAJOR >= 9
    {
        lv_draw_buf_t *db = lv_snapshot_take(scr, LV_COLOR_FORMAT_RGB888);
        if (!db || !db->data) {
            fprintf(stderr, "save_screenshot: snapshot failed for %s\n", screen_name);
            if (db) {
                lv_draw_buf_destroy(db);
            }
            return;
        }

        uint32_t w = db->header.w;
        uint32_t h = db->header.h;
        uint32_t stride = db->header.stride;
        if (w == 0 || h == 0 || stride == 0) {
            fprintf(stderr, "save_screenshot: bad snapshot dimensions for %s\n", screen_name);
            lv_draw_buf_destroy(db);
            return;
        }

        char ppm_filename[1024];
        snprintf(ppm_filename, sizeof(ppm_filename), "%s.ppm", filename);

        FILE *f = fopen(ppm_filename, "wb");
        if (!f) {
            fprintf(stderr, "Failed to save screenshot: %s\n", ppm_filename);
            lv_draw_buf_destroy(db);
            return;
        }

        fprintf(f, "P6\n%u %u\n255\n", w, h);
        const size_t row_bytes = (size_t)w * 3u;
        uint8_t *row = (uint8_t *)malloc(row_bytes);
        if (!row) {
            fprintf(stderr, "save_screenshot: row buffer alloc failed for %s\n", screen_name);
            fclose(f);
            lv_draw_buf_destroy(db);
            return;
        }
        const uint8_t *base = db->data;
        for (uint32_t y = 0; y < h; y++) {
            const uint8_t *src = base + (size_t)y * (size_t)stride;
            memcpy(row, src, row_bytes);
            (void)fwrite(row, 1, row_bytes, f);
        }
        free(row);
        fclose(f);
        lv_draw_buf_destroy(db);
        printf("Saved: %s (PPM; convert: convert %s %s)\n", ppm_filename, ppm_filename, filename);
    }
    return;
#else
    /* Guard: bogus object dimensions can make lv_snapshot_buf_size_needed enormous and stall lv_mem_alloc. */
    uint32_t need = lv_snapshot_buf_size_needed(scr, LV_IMG_CF_TRUE_COLOR);
    if (need == 0) {
        fprintf(stderr, "save_screenshot: buffer size 0 for %s\n", screen_name);
        return;
    }
    const uint32_t hard_max = 64u * 1024u * 1024u;
    if (need > hard_max) {
        fprintf(stderr,
                "save_screenshot: need %u bytes exceeds hard max for %s (skip snapshot)\n",
                (unsigned)need,
                screen_name);
        return;
    }

    /* Extra tail bytes: some draw paths (e.g. lv_color_fill) can slightly overshoot the nominal w*h*px. */
    const uint32_t tail_pad = 256u * 1024u;
    uint64_t alloc_u = (uint64_t)need + (uint64_t)tail_pad;
    if (alloc_u > (uint64_t)hard_max) {
        fprintf(stderr,
                "save_screenshot: alloc %llu exceeds hard max for %s (skip snapshot)\n",
                (unsigned long long)alloc_u,
                screen_name);
        return;
    }

    /* Use system malloc for snapshot buffer: lv_mem pool can OOM and LV_ASSERT_MALLOC spins forever. */
    void *buf = malloc((size_t)alloc_u);
    if (!buf) {
        fprintf(stderr, "save_screenshot: malloc(%llu) failed for %s\n", (unsigned long long)alloc_u, screen_name);
        return;
    }
    lv_img_dsc_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));
    if (lv_snapshot_take_to_buf(scr, LV_IMG_CF_TRUE_COLOR, &snapshot, buf, (uint32_t)alloc_u) != LV_RES_OK) {
        fprintf(stderr, "save_screenshot: take_to_buf failed for %s\n", screen_name);
        free(buf);
        return;
    }
    if (!snapshot.data) {
        fprintf(stderr, "save_screenshot: no data for %s\n", screen_name);
        free(buf);
        return;
    }

    const size_t px_bytes = sizeof(lv_color_t);
    /*
     * Pixel count must come from the same lv_snapshot_buf_size_needed() result as take_to_buf
     * uses internally (dsc->data_size should equal `need`, but reading data_size has been seen
     * inconsistent with header on some screens — trust `need`).
     */
    size_t npx = (size_t)need / px_bytes;
    if (npx == 0) {
        fprintf(stderr, "save_screenshot: zero pixels for %s\n", screen_name);
        free(buf);
        return;
    }
    if (snapshot.data_size != need) {
        fprintf(stderr,
                "save_screenshot: note data_size %u != need %u for %s (using need)\n",
                (unsigned)snapshot.data_size,
                (unsigned)need,
                screen_name);
    }

    lv_disp_t *dp = lv_disp_get_default();
    lv_coord_t dw = dp ? lv_disp_get_hor_res(dp) : (lv_coord_t)s_hor;
    lv_coord_t dh = dp ? lv_disp_get_ver_res(dp) : (lv_coord_t)s_ver;
    uint32_t full_need = 0;
    if (dw > 0 && dh > 0) {
        uint64_t fb = (uint64_t)(uint32_t)dw * (uint64_t)(uint32_t)dh * (uint64_t)px_bytes;
        if (fb <= 0xFFFFFFFFu) {
            full_need = (uint32_t)fb;
        }
    }
    lv_coord_t w = (lv_coord_t)snapshot.header.w;
    lv_coord_t h = (lv_coord_t)snapshot.header.h;
    if (full_need > 0 && need >= full_need && dw > 0 && dh > 0) {
        /* Full display buffer: use panel size (header can be wrong). */
        w = dw;
        h = dh;
    } else if (w > 0 && h > 0 && (size_t)w * (size_t)h == npx) {
        /* header matches need */
    } else if (dw > 0 && dh > 0 && npx == (size_t)dw * (size_t)dh) {
        w = dw;
        h = dh;
    } else if (s_hor > 0 && s_ver > 0 && npx == (size_t)s_hor * (size_t)s_ver) {
        w = (lv_coord_t)s_hor;
        h = (lv_coord_t)s_ver;
    } else if (dw > 0 && npx % (size_t)dw == 0) {
        w = dw;
        h = (lv_coord_t)(npx / (size_t)dw);
    } else if (s_hor > 0 && npx % (size_t)s_hor == 0) {
        w = (lv_coord_t)s_hor;
        h = (lv_coord_t)(npx / (size_t)s_hor);
    } else {
        /* Last resort: factor npx=w*h; prefer width near panel width, else best aspect vs header. */
        lv_coord_t hw = (lv_coord_t)snapshot.header.w;
        lv_coord_t hh = (lv_coord_t)snapshot.header.h;
        size_t best_w = 0, best_h = 0;
        if (dw > 0) {
            for (lv_coord_t guess = dw; guess >= 1; guess--) {
                if (npx % (size_t)guess != 0) {
                    continue;
                }
                size_t gh = npx / (size_t)guess;
                if (gh <= (size_t)LV_COORD_MAX && guess <= (lv_coord_t)LV_COORD_MAX) {
                    best_w = (size_t)guess;
                    best_h = gh;
                    break;
                }
            }
        }
        if (best_w == 0) {
            double targ = (hw > 0 && hh > 0) ? (double)hh / (double)hw : -1.0;
            double best_err = 1e18;
            size_t lim = npx;
            if (lim > 4096u) {
                lim = 4096u;
            }
            for (size_t cw = 1; cw <= lim && cw <= npx; cw++) {
                if (npx % cw != 0) {
                    continue;
                }
                size_t ch = npx / cw;
                if (ch > (size_t)LV_COORD_MAX || cw > (size_t)LV_COORD_MAX) {
                    continue;
                }
                double err;
                if (targ > 0) {
                    double ar = (double)ch / (double)cw;
                    err = ar > targ ? ar - targ : targ - ar;
                } else {
                    err = (double)(cw + ch);
                }
                if (err < best_err) {
                    best_err = err;
                    best_w = cw;
                    best_h = ch;
                }
            }
        }
        if (best_w > 0 && best_h > 0) {
            w = (lv_coord_t)best_w;
            h = (lv_coord_t)best_h;
        } else {
            fprintf(stderr,
                    "save_screenshot: cannot derive size (hdr %ux%u, need=%u, npx=%zu) for %s\n",
                    (unsigned)snapshot.header.w,
                    (unsigned)snapshot.header.h,
                    (unsigned)need,
                    npx,
                    screen_name);
            free(buf);
            return;
        }
    }

    /* PPM P6: RGB888 (convert from LVGL true color) */
    char ppm_filename[1024];
    snprintf(ppm_filename, sizeof(ppm_filename), "%s.ppm", filename);

    FILE *f = fopen(ppm_filename, "wb");
    if (f) {
        fprintf(f, "P6\n%d %d\n255\n", (int)w, (int)h);
        const lv_color_t *pixels = (const lv_color_t *)snapshot.data;
        const size_t row_bytes = (size_t)w * 3u;
        uint8_t *row = (uint8_t *)malloc(row_bytes);
        if (!row) {
            fprintf(stderr, "save_screenshot: row buffer alloc failed for %s\n", screen_name);
            fclose(f);
            free(buf);
            return;
        }
        for (lv_coord_t y = 0; y < h; y++) {
            uint8_t *rp = row;
            for (lv_coord_t x = 0; x < w; x++) {
                lv_color_t c = pixels[y * w + x];
                uint32_t c32 = lv_color_to32(c);
                *rp++ = (uint8_t)((c32 >> 16) & 0xFFu);
                *rp++ = (uint8_t)((c32 >> 8) & 0xFFu);
                *rp++ = (uint8_t)(c32 & 0xFFu);
            }
            (void)fwrite(row, 1, row_bytes, f);
        }
        free(row);
        fclose(f);
        printf("Saved: %s (PPM; convert: convert %s %s)\n", ppm_filename, ppm_filename, filename);
    } else {
        fprintf(stderr, "Failed to save screenshot: %s\n", ppm_filename);
    }

    free(buf);
#endif
}

static void run_visual_tests(void)
{
    printf("Visual regression test: %dx%d\n", s_hor, s_ver);
    printf("Output directory: %s\n", s_output_dir);

    mkdir_p(s_output_dir);

    /* Initialize UI system */
    lv_disp_t *disp = lv_disp_get_default();
    ui_display_init(disp);
    ui_i18n_init();
#if LVGL_VERSION_MAJOR >= 9
    (void)lv_theme_simple_init(disp);
#else
    (void)lv_theme_basic_init(disp);
#endif

    /* Initialize backend with test data */
    if (rm_device_group_manager_init() != ESP_OK) {
        fprintf(stderr, "rm_device_group_manager_init failed\n");
        headless_disp_free();
        exit(1);
    }
    rm_app_backend_layout_test_inject_minimal_home();

    ui_init_all_screens();
    lv_timer_pump(0);

    /* Test each screen in English */
    printf("\n--- Testing English UI ---\n");
    ui_i18n_set_lang(UI_LANG_EN);
    ui_refresh_all_texts();
    lv_timer_pump(0);

    /* Login screen */
    lv_disp_load_scr(ui_Screen_Login);
    lv_timer_pump(0);
    save_screenshot("Login", "en");

    /* Home screen */
    ui_Screen_Home_reload_device_cards();
    lv_disp_load_scr(ui_Screen_Home);
    lv_timer_pump(0);
    save_screenshot("Home", "en");

    /* Schedules screen */
    lv_disp_load_scr(ui_Screen_Schedules);
    lv_timer_pump(0);
    save_screenshot("Schedules", "en");

    /* Create Schedule screen */
    lv_disp_load_scr(ui_Screen_CreateSchedule);
    lv_timer_pump(0);
    save_screenshot("CreateSchedule", "en");

    /* Scenes screen */
    lv_disp_load_scr(ui_Screen_Scenes);
    lv_timer_pump(0);
    save_screenshot("Scenes", "en");

    /* Create Scene screen */
    lv_disp_load_scr(ui_Screen_CreateScene);
    lv_timer_pump(0);
    save_screenshot("CreateScene", "en");

    /* Select Devices screen */
    ui_Screen_SelectDevices_show(NULL);
    lv_timer_pump(0);
    save_screenshot("SelectDevices", "en");

    /* Select Actions screen */
    lv_disp_load_scr(ui_Screen_SelectActions);
    lv_timer_pump(0);
    save_screenshot("SelectActions", "en");

    /* User screen */
    lv_disp_load_scr(ui_Screen_User);
    lv_timer_pump(0);
    save_screenshot("User", "en");

    /* Light Detail screen */
    lv_disp_load_scr(ui_Screen_LightDetail);
    lv_timer_pump(0);
    save_screenshot("LightDetail", "en");

    /* Switch Detail screen */
    lv_disp_load_scr(ui_Screen_SwitchDetails);
    lv_timer_pump(0);
    save_screenshot("SwitchDetail", "en");

    /* Fan Detail screen */
    lv_disp_load_scr(ui_Screen_FanDetails);
    lv_timer_pump(0);
    save_screenshot("FanDetail", "en");

    /* Device Setting screen */
    lv_disp_load_scr(ui_Screen_Device_Setting);
    lv_timer_pump(0);
    save_screenshot("DeviceSetting", "en");

    if (s_capture_both_langs) {
        /* Test each screen in Chinese */
        printf("\n--- Testing Chinese UI ---\n");
        ui_i18n_set_lang(UI_LANG_ZH);
        ui_refresh_all_texts();
        lv_timer_pump(0);

        lv_disp_load_scr(ui_Screen_Login);
        lv_timer_pump(0);
        save_screenshot("Login", "zh");

        ui_Screen_Home_reload_device_cards();
        lv_disp_load_scr(ui_Screen_Home);
        lv_timer_pump(0);
        save_screenshot("Home", "zh");

        lv_disp_load_scr(ui_Screen_Schedules);
        lv_timer_pump(0);
        save_screenshot("Schedules", "zh");

        lv_disp_load_scr(ui_Screen_CreateSchedule);
        lv_timer_pump(0);
        save_screenshot("CreateSchedule", "zh");

        lv_disp_load_scr(ui_Screen_Scenes);
        lv_timer_pump(0);
        save_screenshot("Scenes", "zh");

        lv_disp_load_scr(ui_Screen_CreateScene);
        lv_timer_pump(0);
        save_screenshot("CreateScene", "zh");

        ui_Screen_SelectDevices_show(NULL);
        lv_timer_pump(0);
        save_screenshot("SelectDevices", "zh");

        lv_disp_load_scr(ui_Screen_SelectActions);
        lv_timer_pump(0);
        save_screenshot("SelectActions", "zh");

        lv_disp_load_scr(ui_Screen_User);
        lv_timer_pump(0);
        save_screenshot("User", "zh");

        lv_disp_load_scr(ui_Screen_LightDetail);
        lv_timer_pump(0);
        save_screenshot("LightDetail", "zh");

        lv_disp_load_scr(ui_Screen_SwitchDetails);
        lv_timer_pump(0);
        save_screenshot("SwitchDetail", "zh");

        lv_disp_load_scr(ui_Screen_FanDetails);
        lv_timer_pump(0);
        save_screenshot("FanDetail", "zh");

        lv_disp_load_scr(ui_Screen_Device_Setting);
        lv_timer_pump(0);
        save_screenshot("DeviceSetting", "zh");
    }

    ui_deinit_all_screens();
    rm_device_group_manager_deinit();

    printf("\n=== Visual test complete ===\n");
    printf("Screenshots saved to: %s\n", s_output_dir);
}

int main(int argc, char **argv)
{
    int pr = parse_args(argc, argv);
    if (pr == 2) {
        return 0;
    }
    if (pr != 0) {
        return 1;
    }

    lv_init();
    (void)setvbuf(stdout, NULL, _IONBF, 0);
    (void)setvbuf(stderr, NULL, _IONBF, 0);

    if (s_mode == TEST_MODE_COMPARE) {
        if (!headless_disp_init()) {
            fprintf(stderr, "headless_disp_init failed\n");
            headless_disp_free();
            return 1;
        }
        /* Comparison mode - write comparison results */
        printf("Comparison mode not implemented yet. Use Python script.\n");
        printf("Baseline dir: %s\n", s_baseline_dir);
        printf("Output dir: %s\n", s_output_dir);
        headless_disp_free();
        return 0;
    }

    if (s_mode == TEST_MODE_CAPTURE || s_mode == TEST_MODE_CREATE_BASELINE) {
        printf("Clearing output directory: %s\n", s_output_dir);
        prepare_fresh_output_dir();
    }

    if (visual_test_multi_res_default()) {
        if (s_mode == TEST_MODE_CREATE_BASELINE) {
            printf("Create baseline: %d resolutions (override with -w/-H or RM_PC_LCD_HOR / RM_PC_LCD_VER)\n",
                   BASELINE_RES_COUNT);
        } else {
            printf("Capture: %d resolutions (override with -w/-H or RM_PC_LCD_HOR / RM_PC_LCD_VER)\n",
                   BASELINE_RES_COUNT);
        }
        for (int i = 0; i < BASELINE_RES_COUNT; i++) {
            s_hor = s_baseline_resolutions[i].w;
            s_ver = s_baseline_resolutions[i].h;
            if (!headless_disp_init()) {
                fprintf(stderr, "headless_disp_init failed at %dx%d\n", s_hor, s_ver);
                headless_disp_free();
                return 1;
            }
            run_visual_tests();
            headless_disp_free();
        }
        return 0;
    }

    if (!headless_disp_init()) {
        fprintf(stderr, "headless_disp_init failed\n");
        headless_disp_free();
        return 1;
    }

    run_visual_tests();

    headless_disp_free();
    return 0;
}
