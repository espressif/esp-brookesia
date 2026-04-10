/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include <SDL.h>
#include <stdbool.h>

#ifndef RM_PC_LCD_HOR_DEFAULT
#define RM_PC_LCD_HOR_DEFAULT 1024
#endif
#ifndef RM_PC_LCD_VER_DEFAULT
#define RM_PC_LCD_VER_DEFAULT 600
#endif

/**
 * Create SDL window, register LVGL display + mouse.
 *
 * @param hor_res Horizontal resolution in pixels; if <= 0, uses RM_PC_LCD_HOR_DEFAULT (1024).
 * @param ver_res Vertical resolution in pixels; if <= 0, uses RM_PC_LCD_VER_DEFAULT (600).
 */
bool lv_sdl_port_init(SDL_Window **out_window, int hor_res, int ver_res);

void lv_sdl_port_deinit(void);

/** Call each frame after lv_timer_handler() to present the framebuffer. */
void lv_sdl_port_present(void);
