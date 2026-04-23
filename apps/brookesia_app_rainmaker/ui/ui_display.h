/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "lvgl.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * SquareLine / layout reference: short side used for uniform scaling (portrait or square panels).
 * ESP32-P4 Function EV Board with this demo uses 1024x600; short side 600 → scale ≈ 1.25 vs 480 reference.
 * RainMaker hand-written screens use ui_display_scale_px() with values tuned for this baseline.
 *
 * UI code uses standard LVGL APIs only; resolution-dependent sizing goes through this module so the
 * same screens can be validated on a PC simulator (SDL + LVGL) with a different display size.
 */
#ifndef UI_DISPLAY_DESIGN_SHORT
#define UI_DISPLAY_DESIGN_SHORT 480
#endif

/**
 * Call once after lv_display is ready (e.g. at the start of phone_app_rainmaker_ui_init).
 * Computes scale factors from the active display resolution.
 */
void ui_display_init(lv_disp_t *disp);

lv_disp_t *ui_display_get(void);

int32_t ui_display_get_hor_res(void);
int32_t ui_display_get_ver_res(void);

/** ~256 = 100% when physical short side == UI_DISPLAY_DESIGN_SHORT */
uint16_t ui_display_scale_q8(void);

/**
 * Map a pixel value from the design coordinate system to the current screen.
 * Use for nav heights, corner radii, fixed button sizes. Prefer lv_pct() for widths when possible.
 */
int32_t ui_display_scale_px(int32_t design_px);

/** Typical top bar / header height after scaling */
int32_t ui_display_top_bar_height(void);

/** Scroll content area under a top bar (full width, remaining height minus optional margin). */
int32_t ui_display_content_height_below_top_bar(int32_t top_bar_design_px, int32_t bottom_margin_design_px);

/**
 * Fonts: when UI language is Chinese, body/title/nav use the app bitmap font
 * lv_font_ui_zh_16 (see ui/fonts/) so glyphs render (Montserrat has no CJK).
 */
const lv_font_t *ui_font_nav(void);

/** Body / list row text */
const lv_font_t *ui_font_body(void);

/** Screen titles */
const lv_font_t *ui_font_title(void);

/**
 * Time rollers / large pickers: uses lv_font_ui_zh_16 when UI language is Chinese (CJK),
 * otherwise Montserrat 20 (digits and AM/PM stay readable).
 */
const lv_font_t *ui_font_roller(void);

/**
 * Always lv_font_ui_zh_16 (contains ASCII + UI Chinese subset).
 * Use when the widget must show CJK glyphs while app language is still English
 * (e.g. language picker button label "中文").
 */
const lv_font_t *ui_font_cjk_always(void);

/**
 * Home welcome card first line ("Welcome to" / 欢迎) and home name row.
 * Uses smaller Latin/CJK pairing on very small panels (short side &lt; 400px) so two lines
 * fit without overlapping at 320×240-class resolutions.
 */
const lv_font_t *ui_font_welcome_card(void);

/** Keep Montserrat for Latin-only or symbol-heavy UI (optional). */
const lv_font_t *ui_font_montserrat_14(void);
const lv_font_t *ui_font_montserrat_16(void);
const lv_font_t *ui_font_montserrat_18(void);

/** Icon size in symbol labels (approximate px height) */
int32_t ui_display_icon_size_small(void);
int32_t ui_display_icon_size_medium(void);

#ifdef __cplusplus
}
#endif
