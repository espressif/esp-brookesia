/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// User/Profile screen (hand-written placeholder, match app UI)

#include "../ui.h"
#include "../ui_nav.h"
#include "../ui_gesture_nav.h"
#include "../ui_display.h"
#include "rainmaker_app_backend.h"
#include <stdint.h>
#include <stdio.h>

lv_obj_t *ui_Screen_User = NULL;
static lv_obj_t *s_user_email_label = NULL;
static lv_obj_t *s_user_avatar_label = NULL;
/** Labels that depend on ui_str() — refreshed on language change */
static lv_obj_t *s_lbl_title = NULL;
static lv_obj_t *s_lbl_row[4];
static lv_obj_t *s_lbl_logout = NULL;

static void ui_user_refresh_menu_labels(void);

static void ui_user_language_row_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    ui_show_language_picker();
}

static void ui_user_settings_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    ui_show_msgbox(ui_str(UI_STR_SETTINGS), ui_str(UI_STR_SETTINGS_PLACEHOLDER));
}

static void ui_user_menu_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    const char *name = (const char *)lv_event_get_user_data(e);
    ui_show_msgbox(ui_str(UI_STR_TITLE_MY_PROFILE), name ? name : ui_str(UI_STR_MENU));
}

static void ui_user_logout_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    bool ok = rm_app_backend_logout();
    if (!ok) {
        ui_show_msgbox(ui_str(UI_STR_LOGOUT), ui_str(UI_STR_LOGOUT_FAILED));
        return;
    }
    printf("Logout OK\n");
    if (ui_Screen_Login) {
        lv_disp_load_scr(ui_Screen_Login);
    }
}

static void ui_user_refresh_profile(void)
{
    const char *email = rm_app_backend_get_current_user_email();
    if (email == NULL || email[0] == '\0') {
        email = ui_str(UI_STR_UNKNOWN);
    }
    if (s_user_email_label) {
        lv_label_set_text(s_user_email_label, email);
    }
    if (s_user_avatar_label) {
        char avatar_ch[2] = {0};
        avatar_ch[0] = email[0] ? email[0] : 'U';
        lv_label_set_text(s_user_avatar_label, avatar_ch);
    }
}

static lv_obj_t *ui_user_create_top_bar(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, ui_display_top_bar_height());
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, ui_display_scale_px(18), 0);
    lv_obj_set_style_pad_right(bar, ui_display_scale_px(18), 0);
    lv_obj_set_style_pad_top(bar, ui_display_scale_px(14), 0);
    lv_obj_set_style_pad_bottom(bar, ui_display_scale_px(10), 0);

    lv_obj_t *title = lv_label_create(bar);
    s_lbl_title = title;
    lv_label_set_text(title, ui_str(UI_STR_TITLE_MY_PROFILE));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(4));

    /* Settings button (top-right) */
    lv_obj_t *settings_btn = lv_btn_create(bar);
    lv_obj_clear_flag(settings_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(settings_btn, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(settings_btn, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_set_style_bg_opa(settings_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(settings_btn, 0, 0);
    lv_obj_set_style_shadow_width(settings_btn, 0, 0);
    lv_obj_add_event_cb(settings_btn, ui_user_settings_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *settings_icon = lv_label_create(settings_btn);
    lv_label_set_text(settings_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_font(settings_icon, &lv_font_montserrat_22, 0); /* symbol font fixed size */
    lv_obj_set_style_text_color(settings_icon, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(settings_icon);

    return bar;
}

static lv_obj_t *ui_user_create_card(lv_obj_t *parent, int32_t w_pct)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, lv_pct(w_pct));
    lv_obj_set_style_radius(card, ui_display_scale_px(18), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xB0C4EE), 0);
    lv_obj_set_style_shadow_width(card, ui_display_scale_px(16), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, ui_display_scale_px(14), 0);
    return card;
}

static void ui_user_add_row(lv_obj_t *parent, const char *left_symbol, const char *text, const char *user_data,
                            bool is_last, lv_obj_t **out_main_label, lv_event_cb_t click_cb)
{
    lv_event_cb_t cb = click_cb ? click_cb : ui_user_menu_event_cb;

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn, lv_pct(100));
    /* 40px design was too tight with CJK body font + symbol icons; clip last rows in menu card */
    lv_obj_set_height(btn, ui_display_scale_px(48));
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_left(btn, ui_display_scale_px(4), 0);
    lv_obj_set_style_pad_right(btn, ui_display_scale_px(4), 0);
    /* Long-press avoids accidental open when user swipes horizontally to change page */
    lv_obj_add_event_cb(btn, cb, LV_EVENT_LONG_PRESSED, (void *)user_data);

    lv_obj_t *icon = lv_label_create(btn);
    lv_label_set_text(icon, left_symbol ? left_symbol : "");
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x2E6BE6), 0);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, ui_display_scale_px(10), 0);

    lv_obj_t *lab = lv_label_create(btn);
    lv_label_set_text(lab, text ? text : "");
    lv_obj_set_style_text_font(lab, ui_font_body(), 0);
    lv_obj_align(lab, LV_ALIGN_LEFT_MID, ui_display_scale_px(44), 0);
    if (out_main_label) {
        *out_main_label = lab;
    }

    lv_obj_t *arrow = lv_label_create(btn);
    lv_label_set_text(arrow, LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(arrow, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x8AA0C8), 0);
    lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -ui_display_scale_px(10), 0);

    if (!is_last) {
        lv_obj_t *line = lv_obj_create(parent);
        lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(line, lv_pct(100), 1);
        lv_obj_set_style_bg_color(line, lv_color_hex(0xE6EEF9), 0);
        lv_obj_set_style_border_width(line, 0, 0);
        lv_obj_set_style_bg_opa(line, LV_OPA_60, 0);
    }
}

static void ui_user_refresh_menu_labels(void)
{
    if (s_lbl_title) {
        lv_label_set_text(s_lbl_title, ui_str(UI_STR_TITLE_MY_PROFILE));
        lv_obj_set_style_text_font(s_lbl_title, ui_font_title(), 0);
    }
    if (s_lbl_row[0]) {
        lv_label_set_text(s_lbl_row[0], ui_str(UI_STR_LANGUAGE));
        lv_obj_set_style_text_font(s_lbl_row[0], ui_font_body(), 0);
    }
    if (s_lbl_row[1]) {
        lv_label_set_text(s_lbl_row[1], ui_str(UI_STR_NOTIFICATION_CENTER));
        lv_obj_set_style_text_font(s_lbl_row[1], ui_font_body(), 0);
    }
    if (s_lbl_row[2]) {
        lv_label_set_text(s_lbl_row[2], ui_str(UI_STR_PRIVACY_POLICY));
        lv_obj_set_style_text_font(s_lbl_row[2], ui_font_body(), 0);
    }
    if (s_lbl_row[3]) {
        lv_label_set_text(s_lbl_row[3], ui_str(UI_STR_TERMS_OF_USE));
        lv_obj_set_style_text_font(s_lbl_row[3], ui_font_body(), 0);
    }
    if (s_lbl_logout) {
        lv_label_set_text_fmt(s_lbl_logout, "%s", ui_str(UI_STR_LOGOUT));
        lv_obj_set_style_text_font(s_lbl_logout, ui_font_body(), 0);
    }
}

void ui_Screen_User_apply_language(void)
{
    ui_user_refresh_menu_labels();
}

void ui_Screen_User_screen_init(void)
{
    ui_Screen_User = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen_User, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen_User, lv_color_hex(0xF5F8FF), 0);
    ui_gesture_nav_enable(ui_Screen_User);

    (void)ui_user_create_top_bar(ui_Screen_User);

    /* Scrollable content area between top bar and bottom nav */
    lv_obj_t *content = lv_obj_create(ui_Screen_User);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, 0, 0);
    lv_obj_set_width(content, lv_pct(100));
    /* Nav is hidden by default and shown as an overlay; do not reserve its height */
    lv_obj_set_height(content, ui_display_content_height_below_top_bar(64, 8));
    lv_obj_align(content, LV_ALIGN_TOP_MID, 0, ui_display_top_bar_height());
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    /* Allow swipe gesture to bubble up to the screen for page switching */
    lv_obj_add_flag(content, LV_OBJ_FLAG_GESTURE_BUBBLE);

    /* Profile card */
    lv_obj_t *profile = ui_user_create_card(content, 92);
    lv_obj_set_height(profile, ui_display_scale_px(60));
    lv_obj_align(profile, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(10));

    lv_obj_t *avatar = lv_obj_create(profile);
    lv_obj_clear_flag(avatar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(avatar, ui_display_scale_px(44), ui_display_scale_px(44));
    lv_obj_set_style_radius(avatar, ui_display_scale_px(22), 0);
    lv_obj_set_style_bg_color(avatar, lv_color_hex(0xBBD0F2), 0);
    lv_obj_set_style_border_width(avatar, 0, 0);
    lv_obj_align(avatar, LV_ALIGN_LEFT_MID, ui_display_scale_px(6), 0);

    const char *email = rm_app_backend_get_current_user_email();
    if (email == NULL || email[0] == '\0') {
        email = ui_str(UI_STR_UNKNOWN);
    }

    lv_obj_t *avatar_txt = lv_label_create(avatar);
    char avatar_ch[2] = {0};
    avatar_ch[0] = email[0] ? email[0] : 'U';
    lv_label_set_text(avatar_txt, avatar_ch);
    lv_obj_set_style_text_font(avatar_txt, ui_font_body(), 0);
    lv_obj_set_style_text_color(avatar_txt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(avatar_txt);
    s_user_avatar_label = avatar_txt;

    lv_obj_t *email_lab = lv_label_create(profile);
    lv_label_set_text(email_lab, email);
    lv_obj_set_style_text_font(email_lab, ui_font_body(), 0);
    lv_obj_align(email_lab, LV_ALIGN_LEFT_MID, ui_display_scale_px(60), 0);
    s_user_email_label = email_lab;

    /* Menu list card — height fits 4 rows + dividers + padding (fixed 200 was too short → clipped rows) */
    lv_obj_t *menu = ui_user_create_card(content, 92);
    lv_obj_align_to(menu, profile, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(12));
    lv_obj_set_style_pad_all(menu, ui_display_scale_px(10), 0);

    lv_obj_t *menu_col = lv_obj_create(menu);
    lv_obj_set_width(menu_col, lv_pct(100));
    lv_obj_set_height(menu_col, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(menu_col, LV_OPA_0, 0);
    lv_obj_set_style_border_width(menu_col, 0, 0);
    lv_obj_set_style_pad_all(menu_col, 0, 0);
    lv_obj_set_style_pad_row(menu_col, 0, 0);
    lv_obj_set_flex_flow(menu_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(menu_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(menu_col, LV_OBJ_FLAG_SCROLLABLE);

    /* Removed "Voice Assistants and Integrations" and "Agents Settings" per UI spec */
    for (unsigned i = 0; i < sizeof(s_lbl_row) / sizeof(s_lbl_row[0]); i++) {
        s_lbl_row[i] = NULL;
    }
    ui_user_add_row(menu_col, LV_SYMBOL_LIST, ui_str(UI_STR_LANGUAGE), NULL, false, &s_lbl_row[0],
                    ui_user_language_row_cb);
    ui_user_add_row(menu_col, LV_SYMBOL_BELL, ui_str(UI_STR_NOTIFICATION_CENTER), ui_str(UI_STR_NOTIFICATION_CENTER),
                    false, &s_lbl_row[1], NULL);
    ui_user_add_row(menu_col, LV_SYMBOL_EYE_OPEN, ui_str(UI_STR_PRIVACY_POLICY), ui_str(UI_STR_PRIVACY_POLICY), false,
                    &s_lbl_row[2], NULL);
    ui_user_add_row(menu_col, LV_SYMBOL_FILE, ui_str(UI_STR_TERMS_OF_USE), ui_str(UI_STR_TERMS_OF_USE), true,
                    &s_lbl_row[3], NULL);

    lv_obj_set_height(menu, LV_SIZE_CONTENT);

    /* Logout button card */
    lv_obj_t *logout = ui_user_create_card(content, 92);
    lv_obj_set_height(logout, ui_display_scale_px(52));
    lv_obj_align_to(logout, menu, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(14));
    lv_obj_add_event_cb(logout, ui_user_logout_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *logout_lab = lv_label_create(logout);
    s_lbl_logout = logout_lab;
    /* LVGL doesn't provide LV_SYMBOL_LOGOUT in this version; use an available icon */
    lv_label_set_text_fmt(logout_lab, "%s", ui_str(UI_STR_LOGOUT));
    lv_obj_set_style_text_color(logout_lab, lv_color_hex(0xD9534F), 0);
    lv_obj_set_style_text_font(logout_lab, ui_font_body(), 0);
    lv_obj_center(logout_lab);

    /* Bottom nav removed: swipe left/right to switch screens */
}

void ui_Screen_User_refresh_from_backend(void)
{
    ui_user_refresh_profile();
}

void ui_Screen_User_screen_destroy(void)
{
    ui_language_picker_close();
    if (ui_Screen_User) {
        lv_obj_del(ui_Screen_User);
    }
    ui_Screen_User = NULL;
    s_user_email_label = NULL;
    s_user_avatar_label = NULL;
    s_lbl_title = NULL;
    s_lbl_logout = NULL;
    for (unsigned i = 0; i < sizeof(s_lbl_row) / sizeof(s_lbl_row[0]); i++) {
        s_lbl_row[i] = NULL;
    }
}
