/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Shared UI helpers used by both firmware and PC host build.
 */

#include "ui.h"
#include <stdint.h>
#include <string.h>

/** Full-screen overlay for language picker (avoid lv_msgbox btnmatrix flex quirks on PC/SDL). */
static lv_obj_t *s_lang_picker_root = NULL;

/** Full-screen busy overlay (text-only dialog, no animation) on lv_layer_top(). */
static lv_obj_t *s_busy_root = NULL;
static lv_obj_t *s_busy_label = NULL;
/** Which i18n string the busy overlay is showing (for ui_refresh_all_texts after language change). */
static ui_str_id_t s_busy_msg_id = UI_STR_LOGIN_PLEASE_WAIT;

static void ui_language_picker_close_internal(void);

/** Do not delete the picker tree from inside CLICKED on a child/backdrop — defer until after the event returns. */
static void ui_language_picker_close_async_cb(void *user_data)
{
    (void)user_data;
    ui_language_picker_close_internal();
}

#ifndef RM_HOST_BUILD

static void ui_language_soft_restart_async_cb(void *user_data)
{
    (void)user_data;
    phone_app_rainmaker_ui_soft_restart();
}

static void ui_language_restart_msgbox_btn_cb(lv_event_t *e)
{
#if LVGL_VERSION_MAJOR >= 9
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
#else
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
#endif
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    if (mbox) {
        lv_msgbox_close_async(mbox);
    }
    (void)lv_async_call(ui_language_soft_restart_async_cb, NULL);
}

static void ui_show_language_restart_hint_dialog(void)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, ui_str(UI_STR_MSGBOX_TITLE_INFO));
    lv_msgbox_add_text(mbox, ui_str(UI_STR_LANGUAGE_RESTART_HINT));
    lv_msgbox_add_close_button(mbox);
    lv_obj_t *ok_btn = lv_msgbox_add_footer_button(mbox, ui_str(UI_STR_OK));
    ui_msgbox_apply_common_layout(mbox);
    if (ok_btn) {
        lv_obj_add_event_cb(ok_btn, ui_language_restart_msgbox_btn_cb, LV_EVENT_CLICKED, mbox);
    }
#else
    const char *btns[] = { ui_str(UI_STR_OK), "" };
    lv_obj_t *mbox = lv_msgbox_create(NULL, ui_str(UI_STR_MSGBOX_TITLE_INFO), ui_str(UI_STR_LANGUAGE_RESTART_HINT), btns, true);
    ui_msgbox_apply_common_layout(mbox);
    lv_obj_t *btnm = lv_msgbox_get_btns(mbox);
    if (btnm) {
        lv_obj_add_event_cb(btnm, ui_language_restart_msgbox_btn_cb, LV_EVENT_VALUE_CHANGED, mbox);
    }
#endif
}

#endif /* !RM_HOST_BUILD */

#ifdef RM_HOST_BUILD
/**
 * Run after ui_language_picker_close_internal() in a *later* LVGL task slot.
 * On PC/SDL, updating the entire screen (ui_refresh_all_texts) in the same async
 * callback that deletes the full-screen picker can fault; defer language apply.
 */
static void ui_language_pc_set_lang_async_cb(void *user_data)
{
    intptr_t idx = (intptr_t)user_data;
    ui_lang_t lang = (idx == 0) ? UI_LANG_EN : UI_LANG_ZH;
    phone_app_rainmaker_ui_set_language(lang);
}
#endif

static void ui_language_picker_apply_lang_async_cb(void *user_data)
{
    intptr_t idx = (intptr_t)user_data;
    ui_lang_t lang = (idx == 0) ? UI_LANG_EN : UI_LANG_ZH;
    ui_language_picker_close_internal();
#ifdef RM_HOST_BUILD
    (void)lv_async_call(ui_language_pc_set_lang_async_cb, (void *)idx);
#else
    ui_i18n_set_lang(lang);
    ui_refresh_all_texts();
    ui_show_language_restart_hint_dialog();
#endif
}

static void ui_language_picker_lang_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    intptr_t idx = (intptr_t)lv_event_get_user_data(e);
    (void)lv_async_call(ui_language_picker_apply_lang_async_cb, (void *)idx);
}

static void ui_language_picker_backdrop_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (lv_event_get_target(e) != lv_event_get_current_target(e)) {
        return;
    }
    (void)lv_async_call(ui_language_picker_close_async_cb, NULL);
}

static void ui_language_picker_close_internal(void)
{
    if (s_lang_picker_root) {
        lv_obj_del(s_lang_picker_root);
        s_lang_picker_root = NULL;
    }
}

/** lv_theme_basic defaults can make buttons nearly invisible on a white panel (same fill as background). */
static void ui_language_picker_style_choice_btn(lv_obj_t *btn, lv_obj_t *lbl)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xE8EEF8), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(btn, lv_color_hex(0x3B82F6), LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(btn, 0, LV_PART_MAIN);
    if (lbl) {
        lv_obj_set_style_text_color(lbl, lv_color_hex(0x1A1A1A), 0);
    }
}

void ui_show_language_picker(void)
{
    if (s_lang_picker_root) {
        return;
    }

    lv_disp_t *disp = lv_disp_get_default();
    lv_coord_t aw = lv_disp_get_hor_res(disp);
    lv_coord_t ah = lv_disp_get_ver_res(disp);

    lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
    s_lang_picker_root = backdrop;
    lv_obj_set_size(backdrop, aw, ah);
    lv_obj_align(backdrop, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(backdrop, ui_language_picker_backdrop_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *panel = lv_obj_create(backdrop);
    lv_obj_set_width(panel, lv_pct(78));
    lv_obj_set_style_max_width(panel, ui_display_scale_px(420), 0);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, ui_display_scale_px(12), 0);
    lv_obj_set_style_pad_all(panel, ui_display_scale_px(20), 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_radius(panel, ui_display_scale_px(12), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xDDDDDD), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, ui_str(UI_STR_LANGUAGE));
    lv_obj_set_style_text_font(title, ui_font_body(), 0);
    lv_obj_set_width(title, lv_pct(100));

    lv_obj_t *hint = lv_label_create(panel);
    lv_label_set_text(hint, ui_str(UI_STR_LANGUAGE_PICK_HINT));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, lv_pct(100));
    lv_obj_set_style_text_font(hint, ui_font_body(), 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);

    lv_obj_t *btn_en = lv_btn_create(panel);
    lv_obj_set_width(btn_en, lv_pct(100));
    lv_obj_set_height(btn_en, ui_display_scale_px(44));
    lv_obj_add_event_cb(btn_en, ui_language_picker_lang_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)0);
    lv_obj_t *lbl_en = lv_label_create(btn_en);
    lv_label_set_text(lbl_en, ui_str(UI_STR_LANGUAGE_BTN_EN));
    lv_obj_set_style_text_font(lbl_en, ui_font_body(), 0);
    lv_obj_center(lbl_en);
    ui_language_picker_style_choice_btn(btn_en, lbl_en);

    lv_obj_t *btn_zh = lv_btn_create(panel);
    lv_obj_set_width(btn_zh, lv_pct(100));
    lv_obj_set_height(btn_zh, ui_display_scale_px(44));
    lv_obj_add_event_cb(btn_zh, ui_language_picker_lang_btn_cb, LV_EVENT_CLICKED, (void *)(intptr_t)1);
    lv_obj_t *lbl_zh = lv_label_create(btn_zh);
    lv_label_set_text(lbl_zh, ui_str(UI_STR_LANGUAGE_BTN_ZH));
    lv_obj_set_style_text_font(lbl_zh, ui_font_cjk_always(), 0);
    lv_obj_center(lbl_zh);
    ui_language_picker_style_choice_btn(btn_zh, lbl_zh);
}

void ui_language_picker_close(void)
{
    ui_language_picker_close_internal();
}

void ui_hide_busy(void)
{
    if (s_busy_root) {
        lv_obj_del(s_busy_root);
        s_busy_root = NULL;
        s_busy_label = NULL;
    }
}

void ui_show_busy(const char *message)
{
    ui_hide_busy();

    s_busy_msg_id = UI_STR_LOGIN_PLEASE_WAIT;
    const char *msg = message ? message : ui_str(UI_STR_LOGIN_PLEASE_WAIT);
    if (message && strcmp(message, ui_str(UI_STR_HOME_REFRESHING)) == 0) {
        s_busy_msg_id = UI_STR_HOME_REFRESHING;
    } else if (message && strcmp(message, ui_str(UI_STR_SCENE_ACTIVATING)) == 0) {
        s_busy_msg_id = UI_STR_SCENE_ACTIVATING;
    } else if (message && strcmp(message, ui_str(UI_STR_DELETING)) == 0) {
        s_busy_msg_id = UI_STR_DELETING;
    }

    lv_disp_t *disp = ui_display_get();
    if (!disp) {
        disp = lv_disp_get_default();
    }
    if (!disp) {
        return;
    }
    lv_coord_t aw = lv_disp_get_hor_res(disp);
    lv_coord_t ah = lv_disp_get_ver_res(disp);

    lv_obj_t *backdrop = lv_obj_create(lv_layer_top());
    s_busy_root = backdrop;
    lv_obj_set_size(backdrop, aw, ah);
    lv_obj_align(backdrop, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_move_foreground(backdrop);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_50, 0);
    lv_obj_clear_flag(backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(backdrop, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *panel = lv_obj_create(backdrop);
    lv_obj_set_width(panel, lv_pct(72));
    lv_obj_set_style_max_width(panel, ui_display_scale_px(360), 0);
    lv_obj_set_height(panel, LV_SIZE_CONTENT);
    lv_obj_center(panel);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(panel, ui_display_scale_px(16), 0);
    lv_obj_set_style_pad_all(panel, ui_display_scale_px(24), 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_radius(panel, ui_display_scale_px(12), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xDDDDDD), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    /* CJK: set font before set_text; LONG_CLIP avoids long modes that can corrupt UTF-8 (cf. ui_Screen_Home dropdown). */
    s_busy_label = lv_label_create(panel);
    lv_label_set_long_mode(s_busy_label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(s_busy_label, lv_pct(100));
    lv_obj_set_style_text_font(s_busy_label, ui_font_body(), 0);
    lv_label_set_text(s_busy_label, msg);
    lv_obj_set_style_text_align(s_busy_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_busy_label, lv_color_hex(0x1A1A1A), 0);

    lv_obj_update_layout(backdrop);
    /* Event handler may call blocking work next; force one full refresh so the dialog is visible. */
    lv_refr_now(disp);
}

static void ui_msgbox_set_font_on_labels(lv_obj_t *o, const lv_font_t *f)
{
    if (!o || !f) {
        return;
    }
    if (lv_obj_check_type(o, &lv_label_class)) {
        lv_obj_set_style_text_font(o, f, 0);
    }
    uint32_t n = lv_obj_get_child_cnt(o);
    for (uint32_t i = 0; i < n; i++) {
        ui_msgbox_set_font_on_labels(lv_obj_get_child(o, i), f);
    }
}

void ui_msgbox_apply_font(lv_obj_t *mbox, const lv_font_t *font)
{
    if (!mbox || !font) {
        return;
    }
    ui_msgbox_set_font_on_labels(mbox, font);
}

void ui_msgbox_apply_body_font(lv_obj_t *mbox)
{
    ui_msgbox_apply_font(mbox, ui_font_body());
}

void ui_msgbox_apply_common_layout(lv_obj_t *mbox)
{
    if (mbox == NULL) {
        return;
    }

    const lv_font_t *mbox_body_font = ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body();

    /* Default lv_msgbox is top-left of full-screen parent; center on backdrop. */
    lv_obj_center(mbox);

    lv_obj_set_width(mbox, lv_pct(85));
    lv_obj_set_style_max_width(mbox, ui_display_scale_px(560), 0);
    lv_obj_set_style_pad_all(mbox, ui_display_scale_px(20), 0);
    lv_obj_set_style_pad_row(mbox, ui_display_scale_px(12), 0);
    lv_obj_set_style_radius(mbox, ui_display_scale_px(14), 0);
    lv_obj_set_style_shadow_width(mbox, ui_display_scale_px(16), 0);
    lv_obj_set_style_shadow_opa(mbox, LV_OPA_30, 0);
    lv_obj_set_style_shadow_color(mbox, lv_color_hex(0x000000), 0);
    lv_obj_set_style_shadow_ofs_y(mbox, ui_display_scale_px(4), 0);

    lv_obj_t *title = lv_msgbox_get_title(mbox);
    if (title) {
        lv_obj_set_style_text_font(title, ui_font_title(), 0);
    }
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *content_for_txt = lv_msgbox_get_content(mbox);
    lv_obj_t *txt = NULL;
    if (content_for_txt && lv_obj_get_child_count(content_for_txt) > 0) {
        txt = lv_obj_get_child(content_for_txt, 0);
    }
#else
    lv_obj_t *txt = lv_msgbox_get_text(mbox);
#endif
    if (txt) {
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_width(txt, lv_pct(100));
        lv_obj_set_style_text_font(txt, mbox_body_font, 0);
    }

    lv_obj_t *content = lv_msgbox_get_content(mbox);
    if (content) {
        lv_obj_set_style_pad_all(content, 0, 0);
    }

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *footer = lv_msgbox_get_footer(mbox);
    if (footer) {
        lv_obj_set_width(footer, lv_pct(100));
        uint32_t n = lv_obj_get_child_count(footer);
        for (uint32_t i = 0; i < n; i++) {
            lv_obj_t *btn = lv_obj_get_child(footer, i);
            if (btn) {
                ui_msgbox_set_font_on_labels(btn, mbox_body_font);
            }
        }
    }
#else
    lv_obj_t *btns = lv_msgbox_get_btns(mbox);
    if (btns) {
        lv_obj_set_style_text_font(btns, mbox_body_font, LV_PART_ITEMS);
        lv_obj_set_width(btns, lv_pct(100));
    }
#endif

    lv_obj_update_layout(mbox);
    lv_obj_center(mbox);
}

static void ui_msgbox_auto_close_timer_cb(lv_timer_t *t)
{
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *mbox = (lv_obj_t *)lv_timer_get_user_data(t);
#else
    lv_obj_t *mbox = (lv_obj_t *)t->user_data;
#endif
    if (mbox) {
        lv_msgbox_close_async(mbox);
    }
    lv_timer_del(t);
}

void ui_show_msgbox(const char *title, const char *text)
{
    if (title == NULL) {
        title = ui_str(UI_STR_MSGBOX_TITLE_INFO);
    }
    if (text == NULL || text[0] == '\0') {
        text = ui_str(UI_STR_MSGBOX_TEXT_NA);
    }
#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *mbox = lv_msgbox_create(NULL);
    lv_msgbox_add_title(mbox, title);
    lv_msgbox_add_text(mbox, text);
#else
    lv_obj_t *mbox = lv_msgbox_create(NULL, title, text, NULL, false);
#endif
    ui_msgbox_apply_common_layout(mbox);
    lv_timer_t *timer = lv_timer_create(ui_msgbox_auto_close_timer_cb, 2000, mbox);
    (void)timer;
}

void ui_init_all_screens(void)
{
    ui_Screen_Login_screen_init();
    ui_Screen_Home_screen_init();
    ui_Screen_Schedules_screen_init();
    ui_Screen_CreateSchedule_screen_init();
    ui_Screen_CreateScene_screen_init();
    ui_Screen_SelectDevices_screen_init();
    ui_Screen_SelectActions_screen_init();
    ui_Screen_Scenes_screen_init();
    ui_Screen_User_screen_init();
    ui_Screen_LightDetail_screen_init();
    ui_Screen_SwitchDetails_screen_init();
    ui_Screen_FanDetails_screen_init();
    ui_Screen_Device_Setting_screen_init();
#ifndef RM_HOST_BUILD
    ui_Screen_Automations_screen_init();
#endif
}

void ui_deinit_all_screens(void)
{
    ui_hide_busy();
    ui_language_picker_close();
#ifndef RM_HOST_BUILD
    ui_Screen_Automations_screen_destroy();
#endif
    ui_Screen_LightDetail_screen_destroy();
    ui_Screen_SwitchDetails_screen_destroy();
    ui_Screen_FanDetails_screen_destroy();
    ui_Screen_Device_Setting_screen_destroy();
    ui_Screen_User_screen_destroy();
    ui_Screen_Scenes_screen_destroy();
    ui_Screen_CreateSchedule_screen_destroy();
    ui_Screen_CreateScene_screen_destroy();
    ui_Screen_SelectDevices_screen_destroy();
    ui_Screen_SelectActions_screen_destroy();
    ui_Screen_Schedules_screen_destroy();
    ui_Screen_Home_screen_destroy();
    ui_Screen_Login_screen_destroy();
}

void ui_refresh_all_texts(void)
{
    if (s_busy_root && s_busy_label) {
        lv_obj_set_style_text_font(s_busy_label, ui_font_body(), 0);
        lv_label_set_text(s_busy_label, ui_str(s_busy_msg_id));
    }
    ui_Screen_User_apply_language();
    ui_Screen_Home_apply_language();
    ui_Screen_Login_apply_language();
    ui_Screen_Schedules_apply_language();
    ui_Screen_Scenes_apply_language();
    ui_Screen_CreateSchedule_apply_language();
    ui_Screen_CreateScene_apply_language();
    ui_Screen_SelectDevices_apply_language();
    ui_Screen_SelectActions_apply_language();
    ui_Screen_LightDetail_apply_language();
    ui_Screen_SwitchDetails_apply_language();
    ui_Screen_FanDetails_apply_language();
    ui_Screen_Device_Setting_apply_language();
#ifndef RM_HOST_BUILD
    ui_Screen_Automations_apply_language();
#endif
}
