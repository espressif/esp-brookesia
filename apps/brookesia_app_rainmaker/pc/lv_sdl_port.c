/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "lv_sdl_port.h"
#include <SDL.h>
#include <stdlib.h>
#include <string.h>

#ifndef RM_PC_LCD_HOR_DEFAULT
#define RM_PC_LCD_HOR_DEFAULT 1024
#endif
#ifndef RM_PC_LCD_VER_DEFAULT
#define RM_PC_LCD_VER_DEFAULT 600
#endif

#define BUF_LINES 40

static SDL_Window *s_win;
static SDL_Renderer *s_rend;
static SDL_Texture *s_tex;
/** Partial refresh buffer (RGB565 bytes; do not use sizeof(lv_color_t) on LVGL 9 — lv_color_t is RGB888). */
static uint8_t *s_buf1;

#if LVGL_VERSION_MAJOR >= 9

static lv_display_t *s_disp;
static lv_indev_t *s_indev;

static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) {
        lv_display_flush_ready(disp);
        return;
    }

    SDL_Rect r = { .x = area->x1, .y = area->y1, .w = w, .h = h };
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(s_tex, &r, &pixels, &pitch) != 0) {
        lv_display_flush_ready(disp);
        return;
    }

    const uint8_t *src = px_map;
    uint8_t *dst = (uint8_t *)pixels;
    int src_stride = w * (int)lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
    for (int row = 0; row < h; row++) {
        memcpy(dst + row * pitch, src + row * src_stride, (size_t)src_stride);
    }
    SDL_UnlockTexture(s_tex);
    lv_display_flush_ready(disp);
}

static void indev_mouse_read(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    int x = 0;
    int y = 0;
    Uint32 b = SDL_GetMouseState(&x, &y);
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = (b & SDL_BUTTON(SDL_BUTTON_LEFT)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#else /* LVGL 8 */

static lv_disp_drv_t s_disp_drv;
static lv_indev_drv_t s_indev_drv;
static lv_disp_draw_buf_t s_draw_buf;

static void disp_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void)disp_drv;

    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);
    if (w <= 0 || h <= 0) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    SDL_Rect r = { .x = area->x1, .y = area->y1, .w = w, .h = h };
    void *pixels = NULL;
    int pitch = 0;
    if (SDL_LockTexture(s_tex, &r, &pixels, &pitch) != 0) {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    const uint8_t *src = (const uint8_t *)color_p;
    uint8_t *dst = (uint8_t *)pixels;
    int src_stride = w * (int)sizeof(lv_color_t);
    for (int row = 0; row < h; row++) {
        memcpy(dst + row * pitch, src + row * src_stride, (size_t)src_stride);
    }
    SDL_UnlockTexture(s_tex);
    lv_disp_flush_ready(disp_drv);
}

static void indev_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;
    int x = 0;
    int y = 0;
    Uint32 b = SDL_GetMouseState(&x, &y);
    data->point.x = (lv_coord_t)x;
    data->point.y = (lv_coord_t)y;
    data->state = (b & SDL_BUTTON(SDL_BUTTON_LEFT)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

#endif /* LVGL_VERSION_MAJOR >= 9 */

bool lv_sdl_port_init(SDL_Window **out_window, int hor_res, int ver_res)
{
    int hor = (hor_res > 0) ? hor_res : RM_PC_LCD_HOR_DEFAULT;
    int ver = (ver_res > 0) ? ver_res : RM_PC_LCD_VER_DEFAULT;

    if (s_buf1) {
        free(s_buf1);
        s_buf1 = NULL;
    }

#if LVGL_VERSION_MAJOR >= 9
    if (s_indev) {
        lv_indev_delete(s_indev);
        s_indev = NULL;
    }
    if (s_disp) {
        lv_display_delete(s_disp);
        s_disp = NULL;
    }
#endif

#if LVGL_VERSION_MAJOR >= 9
    s_buf1 = (uint8_t *)malloc((size_t)hor * (size_t)BUF_LINES * (size_t)lv_color_format_get_size(LV_COLOR_FORMAT_RGB565));
#else
    s_buf1 = (uint8_t *)malloc((size_t)hor * (size_t)BUF_LINES * sizeof(lv_color_t));
#endif
    if (!s_buf1) {
        return false;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        free(s_buf1);
        s_buf1 = NULL;
        return false;
    }

    s_win = SDL_CreateWindow(
                "RainMaker (PC) — Login subset",
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED,
                hor,
                ver,
                SDL_WINDOW_SHOWN);
    if (!s_win) {
        free(s_buf1);
        s_buf1 = NULL;
        SDL_Quit();
        return false;
    }

    s_rend = SDL_CreateRenderer(s_win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s_rend) {
        SDL_DestroyWindow(s_win);
        s_win = NULL;
        free(s_buf1);
        s_buf1 = NULL;
        SDL_Quit();
        return false;
    }

    SDL_RenderSetLogicalSize(s_rend, hor, ver);

    s_tex = SDL_CreateTexture(
                s_rend,
                SDL_PIXELFORMAT_RGB565,
                SDL_TEXTUREACCESS_STREAMING,
                hor,
                ver);
    if (!s_tex) {
        SDL_DestroyRenderer(s_rend);
        s_rend = NULL;
        SDL_DestroyWindow(s_win);
        s_win = NULL;
        free(s_buf1);
        s_buf1 = NULL;
        SDL_Quit();
        return false;
    }

#if LVGL_VERSION_MAJOR >= 9
    {
        uint32_t buf_bytes = (uint32_t)hor * (uint32_t)BUF_LINES * (uint32_t)lv_color_format_get_size(LV_COLOR_FORMAT_RGB565);
        s_disp = lv_display_create(hor, ver);
        if (!s_disp) {
            goto fail_after_tex;
        }
        lv_display_set_color_format(s_disp, LV_COLOR_FORMAT_RGB565);
        lv_display_set_buffers(s_disp, (void *)s_buf1, NULL, buf_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
        lv_display_set_flush_cb(s_disp, disp_flush_cb);

        s_indev = lv_indev_create();
        if (!s_indev) {
            lv_display_delete(s_disp);
            s_disp = NULL;
            goto fail_after_tex;
        }
        lv_indev_set_type(s_indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_read_cb(s_indev, indev_mouse_read);
    }
#else
    lv_disp_draw_buf_init(&s_draw_buf, (lv_color_t *)s_buf1, NULL, (uint32_t)(hor * BUF_LINES));

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = hor;
    s_disp_drv.ver_res = ver;
    s_disp_drv.flush_cb = disp_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);

    lv_indev_drv_init(&s_indev_drv);
    s_indev_drv.type = LV_INDEV_TYPE_POINTER;
    s_indev_drv.read_cb = indev_mouse_read;
    lv_indev_drv_register(&s_indev_drv);
#endif

    if (out_window) {
        *out_window = s_win;
    }
    return true;

#if LVGL_VERSION_MAJOR >= 9
fail_after_tex:
    SDL_DestroyTexture(s_tex);
    s_tex = NULL;
    SDL_DestroyRenderer(s_rend);
    s_rend = NULL;
    SDL_DestroyWindow(s_win);
    s_win = NULL;
    free(s_buf1);
    s_buf1 = NULL;
    SDL_Quit();
    return false;
#endif
}

void lv_sdl_port_deinit(void)
{
#if LVGL_VERSION_MAJOR >= 9
    if (s_indev) {
        lv_indev_delete(s_indev);
        s_indev = NULL;
    }
    if (s_disp) {
        lv_display_delete(s_disp);
        s_disp = NULL;
    }
#endif
    if (s_tex) {
        SDL_DestroyTexture(s_tex);
        s_tex = NULL;
    }
    if (s_rend) {
        SDL_DestroyRenderer(s_rend);
        s_rend = NULL;
    }
    if (s_win) {
        SDL_DestroyWindow(s_win);
        s_win = NULL;
    }
    if (s_buf1) {
        free(s_buf1);
        s_buf1 = NULL;
    }
    SDL_Quit();
}

void lv_sdl_port_present(void)
{
    if (!s_rend || !s_tex) {
        return;
    }
    SDL_RenderClear(s_rend);
    SDL_RenderCopy(s_rend, s_tex, NULL, NULL);
    SDL_RenderPresent(s_rend);
}
