/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "ui_nav.h"
#include "ui.h"
#include "ui_i18n.h"
#include "ui_display.h"

/* Design-time px; scaled at runtime via ui_display_scale_px() */
#define UI_NAV_HOTSPOT_H_DESIGN 28
#define UI_NAV_PANEL_H_DESIGN 56
/* Auto hide timeout */
#define UI_NAV_AUTO_HIDE_MS 5000

typedef struct {
    lv_obj_t *nav;
    lv_obj_t *hotspot;
    lv_timer_t *hide_timer;
} ui_nav_ctx_t;

static lv_obj_t *ui_nav_get_screen_for_tab(ui_nav_tab_t tab)
{
    switch (tab) {
    case UI_NAV_TAB_HOME:
        return ui_Screen_Home;
    case UI_NAV_TAB_SCHEDULES:
        return ui_Screen_Schedules;
    case UI_NAV_TAB_SCENES:
        return ui_Screen_Scenes;
    case UI_NAV_TAB_USER:
        return ui_Screen_User;
    default:
        return NULL;
    }
}

static void ui_nav_hide_timer_cb(lv_timer_t *t)
{
#if LVGL_VERSION_MAJOR >= 9
    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)lv_timer_get_user_data(t);
#else
    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)t->user_data;
#endif
    if (!ctx || !ctx->nav) {
        return;
    }
    lv_obj_add_flag(ctx->nav, LV_OBJ_FLAG_HIDDEN);
    if (ctx->hotspot) {
        lv_obj_move_foreground(ctx->hotspot);
    }
    /* one-shot */
    if (ctx->hide_timer) {
        lv_timer_del(ctx->hide_timer);
        ctx->hide_timer = NULL;
    }
}

static void ui_nav_show(ui_nav_ctx_t *ctx)
{
    if (!ctx || !ctx->nav) {
        return;
    }
    lv_obj_clear_flag(ctx->nav, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(ctx->nav);

    if (ctx->hide_timer) {
        lv_timer_reset(ctx->hide_timer);
    } else {
        ctx->hide_timer = lv_timer_create(ui_nav_hide_timer_cb, UI_NAV_AUTO_HIDE_MS, ctx);
    }
}

static void ui_nav_hide(ui_nav_ctx_t *ctx)
{
    if (!ctx || !ctx->nav) {
        return;
    }
    lv_obj_add_flag(ctx->nav, LV_OBJ_FLAG_HIDDEN);
    if (ctx->hotspot) {
        lv_obj_move_foreground(ctx->hotspot);
    }
    if (ctx->hide_timer) {
        lv_timer_del(ctx->hide_timer);
        ctx->hide_timer = NULL;
    }
}

static void ui_nav_hotspot_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)lv_event_get_user_data(e);
    ui_nav_show(ctx);
}

static void ui_nav_nav_event_cb(lv_event_t *e)
{
    /* Any interaction on nav should restart auto-hide timer */
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)lv_event_get_user_data(e);
    ui_nav_show(ctx);
}

static void ui_nav_ctx_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) {
        return;
    }
    if (ctx->hide_timer) {
        lv_timer_del(ctx->hide_timer);
        ctx->hide_timer = NULL;
    }
    UI_LV_FREE(ctx);
}

static void ui_nav_set_active_btn(lv_obj_t *btn)
{
    if (!btn) {
        return;
    }
    lv_obj_t *nav = lv_obj_get_parent(btn);
    if (!nav) {
        return;
    }
    uint32_t cnt = lv_obj_get_child_cnt(nav);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(nav, i);
        if (child) {
            lv_obj_set_style_bg_color(child, lv_color_hex(0xFFFFFF), 0);
        }
    }
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xE8F0FF), 0);
}

static void ui_nav_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_current_target(e);
    if (!btn) {
        return;
    }
    /* Buttons are created in the same order as ui_nav_tab_t enum */
    ui_nav_tab_t tab = (ui_nav_tab_t)lv_obj_get_index(btn);
    lv_obj_t *target = ui_nav_get_screen_for_tab(tab);
    if (target) {
        ui_nav_set_active_btn(lv_event_get_current_target(e));
        /* Refresh dynamic data before screen switch */
        if (tab == UI_NAV_TAB_HOME) {
            ui_Screen_Home_refresh_from_backend();
        } else if (tab == UI_NAV_TAB_SCHEDULES) {
            ui_Screen_Schedules_show();
        } else if (tab == UI_NAV_TAB_SCENES) {
            ui_Screen_Scenes_show();
        } else if (tab == UI_NAV_TAB_USER) {
            ui_Screen_User_refresh_from_backend();
        }
        /* Hide immediately after selection */
        ui_nav_hide(ctx);
        lv_disp_load_scr(target);
    }
}

lv_obj_t *ui_nav_create(lv_obj_t *parent, ui_nav_tab_t active)
{
    ui_nav_ctx_t *ctx = (ui_nav_ctx_t *)UI_LV_MALLOC(sizeof(ui_nav_ctx_t));
    if (!ctx) {
        return NULL;
    }
    ctx->nav = NULL;
    ctx->hotspot = NULL;
    ctx->hide_timer = NULL;

    /* Invisible hotspot at bottom: click anywhere here to show the nav */
    lv_obj_t *hotspot = lv_btn_create(parent);
    lv_obj_clear_flag(hotspot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_align(hotspot, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_width(hotspot, lv_pct(100));
    lv_obj_set_height(hotspot, ui_display_scale_px(UI_NAV_HOTSPOT_H_DESIGN));
    lv_obj_set_style_bg_opa(hotspot, LV_OPA_0, 0);
    lv_obj_set_style_border_width(hotspot, 0, 0);
    lv_obj_set_style_shadow_width(hotspot, 0, 0);
    lv_obj_add_event_cb(hotspot, ui_nav_hotspot_event_cb, LV_EVENT_CLICKED, ctx);
    ctx->hotspot = hotspot;

    /* Nav panel (hidden by default, shows when tapping handle) */
    lv_obj_t *nav = lv_obj_create(parent);
    lv_obj_clear_flag(nav, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_align(nav, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_width(nav, lv_pct(100));
    lv_obj_set_height(nav, ui_display_scale_px(UI_NAV_PANEL_H_DESIGN));
    lv_obj_set_style_pad_left(nav, ui_display_scale_px(12), 0);
    lv_obj_set_style_pad_right(nav, ui_display_scale_px(12), 0);
    lv_obj_set_style_pad_top(nav, ui_display_scale_px(6), 0);
    lv_obj_set_style_pad_bottom(nav, ui_display_scale_px(6), 0);
    lv_obj_set_style_pad_column(nav, ui_display_scale_px(6), 0);
    lv_obj_set_style_radius(nav, ui_display_scale_px(18), 0);
    lv_obj_set_style_bg_color(nav, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_color(nav, lv_color_hex(0xD7E1F5), 0);
    lv_obj_set_style_border_width(nav, 2, 0);
    lv_obj_set_style_shadow_width(nav, 0, 0);
    lv_obj_add_flag(nav, LV_OBJ_FLAG_HIDDEN);

    ctx->nav = nav;
    /* Any click on the nav area restarts the 5s timer */
    lv_obj_add_event_cb(nav, ui_nav_nav_event_cb, LV_EVENT_CLICKED, ctx);
    /* Free ctx when nav is deleted (screen destroyed) */
    lv_obj_add_event_cb(nav, ui_nav_ctx_delete_cb, LV_EVENT_DELETE, ctx);

    lv_obj_set_flex_flow(nav, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(nav, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static const ui_str_id_t label_ids[UI_NAV_TAB_MAX] = {
        UI_STR_NAV_HOME,
        UI_STR_NAV_SCHED,
        UI_STR_NAV_SCENES,
        UI_STR_NAV_USER,
    };

    for (int i = 0; i < (int)UI_NAV_TAB_MAX; i++) {
        lv_obj_t *btn = lv_btn_create(nav);
        lv_obj_set_size(btn, ui_display_scale_px(56), ui_display_scale_px(40));
        lv_obj_set_style_radius(btn, ui_display_scale_px(12), 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        /* Determine tab by child index; pass ctx as user_data for hide/timer */
        lv_obj_add_event_cb(btn, ui_nav_btn_event_cb, LV_EVENT_CLICKED, ctx);
        lv_obj_set_style_bg_color(btn, (i == (int)active) ? lv_color_hex(0xE8F0FF) : lv_color_hex(0xFFFFFF), 0);

        lv_obj_t *lab = lv_label_create(btn);
        lv_label_set_text(lab, ui_str(label_ids[i]));
        lv_obj_set_style_text_font(lab, ui_font_nav(), 0);
        lv_obj_center(lab);

        /* Also restart timer on tab area click */
        lv_obj_add_event_cb(btn, ui_nav_nav_event_cb, LV_EVENT_CLICKED, ctx);
    }

    /* Ensure the hotspot stays on top when nav is hidden */
    lv_obj_move_foreground(hotspot);

    return nav;
}
