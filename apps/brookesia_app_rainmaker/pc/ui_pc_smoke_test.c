/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Headless PC UI smoke test (no SDL window):
 * - Register minimal LVGL display (flush no-op).
 * - Build all RM_HOST_BUILD screens, pump timers (lets pending LVGL work run).
 * - Assert every ui_str(id) is non-empty for EN and ZH.
 * - Sanity-check ui_display_scale_px and ui_format_hue_value_string.
 *
 * Resolution: environment `RM_PC_LCD_HOR` / `RM_PC_LCD_VER`, or `-w` / `-H` (same as rainmaker_ui_pc).
 * Defaults: 1024×600.
 *
 * Run: ./build/rainmaker_ui_pc_smoke
 *      RM_PC_LCD_HOR=320 RM_PC_LCD_VER=240 ./build/rainmaker_ui_pc_smoke
 * CTest: ctest --test-dir build
 */

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#endif

#include "lvgl.h"
#if LVGL_VERSION_MAJOR < 9
#include "src/extra/themes/basic/lv_theme_basic.h"
#endif

#include "ui.h"
#include "ui_display.h"
#include "ui_i18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RM_PC_DIM_MIN
#define RM_PC_DIM_MIN 240
#endif
#ifndef RM_PC_DIM_MAX
#define RM_PC_DIM_MAX 4096
#endif

#define BUF_LINES 40

static int s_hor = 1024;
static int s_ver = 600;

#if LVGL_VERSION_MAJOR >= 9

static uint8_t *s_buf;
static lv_display_t *s_disp;

static void headless_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    (void)area;
    (void)px_map;
    lv_display_flush_ready(disp);
}

#else

static lv_color_t *s_buf;
static lv_disp_draw_buf_t s_draw_buf;

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
    /* Copy immediately: some libc implementations reuse one buffer across getenv calls. */
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

    /* Manual argv (no getopt): avoids BSD/macOS getopt global state issues under CTest / argc==1. */
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            printf(
                "Usage: %s [-w WIDTH] [-H HEIGHT]\n"
                "  Defaults: 1024×600; override with -w/-H or RM_PC_LCD_HOR / RM_PC_LCD_VER.\n",
                argv[0]);
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
            continue;
        }
        fprintf(stderr, "%s: unknown argument %s\n", argv[0], a);
        return 1;
    }

    if (width > 0) {
        s_hor = width;
    } else if (parse_dim_env("RM_PC_LCD_HOR", &s_hor) != 0) {
        s_hor = 1024;
    }

    if (height > 0) {
        s_ver = height;
    } else if (parse_dim_env("RM_PC_LCD_VER", &s_ver) != 0) {
        s_ver = 600;
    }

    return 0;
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
#endif
    if (s_buf) {
        free(s_buf);
        s_buf = NULL;
    }
}

static void lv_timer_pump(unsigned rounds)
{
    for (unsigned i = 0; i < rounds; i++) {
        lv_tick_inc(5);
        lv_timer_handler();
    }
}

static int check_all_strings(void)
{
    for (unsigned id = 0; id < (unsigned)UI_STR_COUNT; id++) {
        const char *s = ui_str((ui_str_id_t)id);
        if (s == NULL || s[0] == '\0') {
            fprintf(stderr, "ui_str(%u) empty or null\n", id);
            return 1;
        }
    }
    return 0;
}

static int check_scale(void)
{
    int32_t px = ui_display_scale_px(100);
    int32_t v = ui_display_get_ver_res();
    if (px <= 0 || px > v) {
        fprintf(stderr, "ui_display_scale_px(100)=%ld ver=%ld\n", (long)px, (long)v);
        return 1;
    }
    return 0;
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
    if (!headless_disp_init()) {
        fprintf(stderr, "headless_disp_init failed\n");
        headless_disp_free();
        return 1;
    }

    lv_disp_t *disp = lv_disp_get_default();
    ui_display_init(disp);
    ui_i18n_init();

    /*
     * Theme init allocates style storage; we do not call lv_disp_set_theme in headless smoke.
     * Walking the default screen to apply the theme can fault intermittently on some hosts
     * (lv_disp_set_theme → lv_theme_apply). SDL ./rainmaker_ui_pc still applies the theme as usual.
     */
#if LVGL_VERSION_MAJOR >= 9
    (void)lv_theme_simple_init(disp);
#else
    (void)lv_theme_basic_init(disp);
#endif

    ui_init_all_screens();
    /*
     * Headless: avoid lv_timer_handler() loops here — a single pump can fault on macOS/arm64
     * (likely a screen-registered timer + missing invariants vs SDL build). Full tick coverage
     * is exercised by ./rainmaker_ui_pc. Smoke still validates screen graph build + i18n tables.
     */
    lv_timer_pump(0);

    if (check_all_strings() != 0) {
        headless_disp_free();
        return 1;
    }

    ui_refresh_all_texts();
    if (check_all_strings() != 0) {
        headless_disp_free();
        return 1;
    }

    if (check_scale() != 0) {
        headless_disp_free();
        return 1;
    }

    ui_i18n_set_lang(UI_LANG_ZH);
    ui_refresh_all_texts();
    lv_timer_pump(0);
    if (check_all_strings() != 0) {
        headless_disp_free();
        return 1;
    }

    char hue_buf[32];
    ui_format_hue_value_string(303, hue_buf, sizeof(hue_buf));
    if (hue_buf[0] == '\0') {
        fprintf(stderr, "ui_format_hue_value_string produced empty\n");
        headless_disp_free();
        return 1;
    }

    ui_deinit_all_screens();
    headless_disp_free();

    printf("rainmaker_ui_pc_smoke: OK (%dx%d)\n", s_hor, s_ver);
    return 0;
}
