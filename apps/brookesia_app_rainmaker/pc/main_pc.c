/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
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
#include "lv_sdl_port.h"

#include "ui.h"
#include "ui_display.h"
#include "ui_i18n.h"
#include "ui_gesture_nav.h"
#include "rainmaker_app_backend.h"

#include <SDL.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef RM_PC_DIM_MIN
#define RM_PC_DIM_MIN 240
#endif
#ifndef RM_PC_DIM_MAX
#define RM_PC_DIM_MAX 4096
#endif

static void usage(FILE *fp, const char *prog)
{
    fprintf(fp, "Usage: %s [options]\n", prog);
    fprintf(fp, "  -w, --width <px>   Horizontal resolution (default %d, or RM_PC_LCD_HOR)\n", RM_PC_LCD_HOR_DEFAULT);
    fprintf(fp, "  -H, --height <px>  Vertical resolution (default %d, or RM_PC_LCD_VER)\n", RM_PC_LCD_VER_DEFAULT);
    fprintf(fp, "  -h, --help         Show this help\n");
    fprintf(fp, "Environment: RM_PC_LCD_HOR, RM_PC_LCD_VER (used when the matching option is omitted)\n");
}

static int parse_dim_env(const char *name, int *out)
{
    const char *v = getenv(name);
    if (!v || !v[0]) {
        return -1;
    }
    char *end = NULL;
    long x = strtol(v, &end, 10);
    if (end == v || *end != '\0' || x < RM_PC_DIM_MIN || x > RM_PC_DIM_MAX) {
        return -1;
    }
    *out = (int)x;
    return 0;
}

static void rainmaker_pc_ui_init(void)
{
    lv_disp_t *dispp = lv_disp_get_default();
    ui_display_init(dispp);
    ui_i18n_init();

#if LVGL_VERSION_MAJOR >= 9
    lv_theme_t *theme = lv_theme_simple_init(dispp);
#else
    lv_theme_t *theme = lv_theme_basic_init(dispp);
#endif
    lv_disp_set_theme(dispp, theme);

    ui_init_all_screens();
    ui_refresh_all_texts();

    esp_err_t ret = rm_app_backend_init();
    if (ret != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_INIT_RM));
        lv_disp_load_scr(ui_Screen_Login);
        return;
    }

    if (rm_app_backend_is_logged_in()) {
        ui_Screen_Home_refresh_from_backend();
        lv_disp_load_scr(ui_Screen_Home);
    } else {
        lv_disp_load_scr(ui_Screen_Login);
    }
}

int main(int argc, char **argv)
{
    int width = 0;
    int height = 0;

    static struct option long_opts[] = {
        { "width", required_argument, NULL, 'w' },
        { "height", required_argument, NULL, 'H' },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 },
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "w:H:h", long_opts, NULL)) != -1) {
        switch (opt) {
        case 'w': {
            char *end = NULL;
            long v = strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || v < RM_PC_DIM_MIN || v > RM_PC_DIM_MAX) {
                fprintf(stderr, "%s: invalid width %s (use %d-%d)\n", argv[0], optarg, RM_PC_DIM_MIN, RM_PC_DIM_MAX);
                return 1;
            }
            width = (int)v;
            break;
        }
        case 'H': {
            char *end = NULL;
            long v = strtol(optarg, &end, 10);
            if (end == optarg || *end != '\0' || v < RM_PC_DIM_MIN || v > RM_PC_DIM_MAX) {
                fprintf(stderr, "%s: invalid height %s (use %d-%d)\n", argv[0], optarg, RM_PC_DIM_MIN, RM_PC_DIM_MAX);
                return 1;
            }
            height = (int)v;
            break;
        }
        case 'h':
            usage(stdout, argv[0]);
            return 0;
        default:
            usage(stderr, argv[0]);
            return 1;
        }
    }

    if (width <= 0) {
        if (parse_dim_env("RM_PC_LCD_HOR", &width) != 0) {
            width = RM_PC_LCD_HOR_DEFAULT;
        }
    }
    if (height <= 0) {
        if (parse_dim_env("RM_PC_LCD_VER", &height) != 0) {
            height = RM_PC_LCD_VER_DEFAULT;
        }
    }

    lv_init();
    if (!lv_sdl_port_init(NULL, width, height)) {
        return 1;
    }

    rainmaker_pc_ui_init();

    bool running = true;
    uint32_t last = SDL_GetTicks();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
            } else if (e.type == SDL_KEYDOWN) {
                /* Fallback when mouse swipe does not generate LV_EVENT_GESTURE (e.g. focus on SDL). */
                if (e.key.keysym.sym == SDLK_LEFT) {
                    ui_gesture_nav_feed_dir(LV_DIR_LEFT);
                } else if (e.key.keysym.sym == SDLK_RIGHT) {
                    ui_gesture_nav_feed_dir(LV_DIR_RIGHT);
                }
            }
        }

        uint32_t now = SDL_GetTicks();
        uint32_t dt = now - last;
        if (dt > 0) {
            lv_tick_inc(dt);
            last = now;
        }

        lv_timer_handler();
        lv_sdl_port_present();
        SDL_Delay(1);
    }

    lv_sdl_port_deinit();
    return 0;
}
