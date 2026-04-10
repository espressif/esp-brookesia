/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_display.h"
#include "ui_i18n.h"
#include "lvgl.h"

/* Generated bitmap font: see ui/fonts/README.md */
extern const lv_font_t lv_font_ui_zh_16;

static lv_disp_t *s_disp;
static int32_t s_hor;
static int32_t s_ver;
static uint16_t s_scale_q8 = 256;

static const lv_font_t *pick_cjk_or(const lv_font_t *latin)
{
    if (ui_i18n_uses_cjk()) {
        return &lv_font_ui_zh_16;
    }
    return latin;
}

void ui_display_init(lv_disp_t *disp)
{
    s_disp = disp ? disp : lv_disp_get_default();
    if (!s_disp) {
        return;
    }
    s_hor = lv_disp_get_hor_res(s_disp);
    s_ver = lv_disp_get_ver_res(s_disp);
    int32_t short_side = (s_hor < s_ver) ? s_hor : s_ver;
    s_scale_q8 = (uint16_t)((short_side * 256) / UI_DISPLAY_DESIGN_SHORT);
    if (s_scale_q8 < 180) {
        s_scale_q8 = 180;
    }
    if (s_scale_q8 > 384) {
        s_scale_q8 = 384;
    }
}

lv_disp_t *ui_display_get(void)
{
    return s_disp ? s_disp : lv_disp_get_default();
}

int32_t ui_display_get_hor_res(void)
{
    return s_hor;
}

int32_t ui_display_get_ver_res(void)
{
    return s_ver;
}

uint16_t ui_display_scale_q8(void)
{
    return s_scale_q8;
}

int32_t ui_display_scale_px(int32_t design_px)
{
    return (int32_t)(((int64_t)design_px * (int64_t)s_scale_q8) / 256);
}

int32_t ui_display_top_bar_height(void)
{
    return ui_display_scale_px(64);
}

int32_t ui_display_content_height_below_top_bar(int32_t top_bar_design_px, int32_t bottom_margin_design_px)
{
    int32_t top = ui_display_scale_px(top_bar_design_px);
    int32_t margin = ui_display_scale_px(bottom_margin_design_px);
    int32_t h = s_ver > 0 ? s_ver : 240;
    int32_t ch = h - top - margin;
    return (ch > 40) ? ch : 40;
}

const lv_font_t *ui_font_montserrat_14(void)
{
    return &lv_font_montserrat_14;
}

const lv_font_t *ui_font_montserrat_16(void)
{
    return &lv_font_montserrat_16;
}

const lv_font_t *ui_font_montserrat_18(void)
{
    return &lv_font_montserrat_18;
}

const lv_font_t *ui_font_nav(void)
{
    return pick_cjk_or(&lv_font_montserrat_14);
}

const lv_font_t *ui_font_body(void)
{
    if (s_ver > 0 && s_ver < 360) {
        return pick_cjk_or(&lv_font_montserrat_14);
    }
    return pick_cjk_or(&lv_font_montserrat_16);
}

const lv_font_t *ui_font_title(void)
{
    if (s_ver > 0 && s_ver < 360) {
        return pick_cjk_or(&lv_font_montserrat_16);
    }
    /* P4-class landscape (e.g. 600px height): slightly larger title */
    if (s_ver >= 540) {
        return pick_cjk_or(&lv_font_montserrat_20);
    }
    return pick_cjk_or(&lv_font_montserrat_18);
}

const lv_font_t *ui_font_roller(void)
{
    /* Custom 16px CJK font includes digits; use it when UI is Chinese. */
    return pick_cjk_or(&lv_font_montserrat_20);
}

const lv_font_t *ui_font_cjk_always(void)
{
    return &lv_font_ui_zh_16;
}

const lv_font_t *ui_font_welcome_card(void)
{
    int32_t hor = ui_display_get_hor_res();
    int32_t ver = ui_display_get_ver_res();
    int32_t short_side = (hor > 0 && ver > 0) ? ((hor < ver) ? hor : ver) : UI_DISPLAY_DESIGN_SHORT;
    if (short_side < 400) {
        return pick_cjk_or(&lv_font_montserrat_14);
    }
    return pick_cjk_or(&lv_font_montserrat_16);
}

int32_t ui_display_icon_size_small(void)
{
    return ui_display_scale_px(18);
}

int32_t ui_display_icon_size_medium(void)
{
    return ui_display_scale_px(22);
}
