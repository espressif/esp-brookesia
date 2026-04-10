/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Scenes screen: list scene cards + create/edit scene entry.

#include <stdio.h>

#include "../ui.h"
#include "../ui_gesture_nav.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_list_manager.h"
#include "ui_Screen_CreateScene.h"

lv_obj_t *ui_Screen_Scenes = NULL;

static lv_obj_t *s_content = NULL;
static lv_obj_t *s_list_cont = NULL;
static lv_obj_t *s_add_btn = NULL;
static lv_obj_t *s_edit_btn = NULL;
static lv_obj_t *s_edit_btn_label = NULL;
static lv_obj_t *s_scenes_top_title = NULL;
static lv_obj_t *s_scenes_add_label = NULL;
static bool s_is_edit_mode = false;
static void ui_scenes_refresh_list(void);

static void ui_scenes_scene_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    const char *scene_id = (const char *)lv_event_get_user_data(e);
    if (!scene_id || scene_id[0] == '\0') {
        return;
    }
    ui_Screen_CreateScene_show_edit(scene_id);
}

static bool ui_scenes_request_delete_scene_from_backend(const char *scene_id)
{
    rm_list_item_t *scene_item = rm_list_manager_find_item_by_id(scene_id);
    if (!scene_item) {
        return false;
    }
    esp_err_t err = rm_list_manager_delete_item_from_backend(scene_item);
    if (err != ESP_OK) {
        return false;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    return true;
}

static void ui_scenes_delete_scene_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *scene_id = (const char *)lv_event_get_user_data(e);
    if (!scene_id || scene_id[0] == '\0') {
        return;
    }
    ui_show_busy(ui_str(UI_STR_DELETING));
    bool ok = ui_scenes_request_delete_scene_from_backend(scene_id);
    ui_hide_busy();
    if (!ok) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_DELETE_SCENE));
        return;
    }
    ui_Screen_Scenes_show();
}

static void ui_scenes_activate_scene_cb(lv_event_t *e)
{
    const char *scene_id_raw = (const char *)lv_event_get_user_data(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *scene_id = scene_id_raw;
    if (!scene_id || scene_id[0] == '\0') {
        return;
    }

    ui_show_busy(ui_str(UI_STR_SCENE_ACTIVATING));

    rm_list_item_t *scene = rm_list_manager_get_copied_item_by_id(scene_id);
    if (!scene) {
        ui_hide_busy();
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_FIND_SCENE));
        return;
    }
    esp_err_t err = rm_list_manager_enable_item_to_backend(scene, true);
    rm_list_manager_clean_item_memory(scene);
    if (err != ESP_OK) {
        ui_hide_busy();
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_ACTIVATE_SCENE));
        return;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    ui_scenes_refresh_list();
    ui_hide_busy();
    ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_SCENE_ACTIVATE_SUCCESS));
}

static void ui_scenes_add_card(lv_obj_t *parent, rm_list_item_t *scene)
{
    if (!parent || !scene) {
        return;
    }
    int action_count = rm_list_manager_get_node_actions_count(scene);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_pad_top(card, 4, 0);
    lv_obj_set_style_pad_bottom(card, 4, 0);
    lv_obj_set_style_pad_left(card, 10, 0);
    lv_obj_set_style_pad_right(card, 10, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 2, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xF8FBFF), LV_STATE_PRESSED);
        lv_obj_add_event_cb(card, ui_scenes_scene_click_cb, LV_EVENT_LONG_PRESSED, (void *)scene->id);
    }

    lv_obj_t *row1 = lv_obj_create(card);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row1, lv_pct(100));
    lv_obj_set_height(row1, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row1, 0, 0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(row1, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    lv_obj_t *name = lv_label_create(row1);
    lv_label_set_text(name, scene->name && scene->name[0] ? scene->name : (scene->id ? scene->id : ui_str(UI_STR_SCENE)));
    lv_obj_set_style_text_font(name, ui_font_body(), 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0x1A1A1A), 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(name, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    lv_obj_t *spacer = lv_obj_create(row1);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_pad_all(spacer, 0, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(spacer, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    if (s_is_edit_mode) {
        const lv_coord_t del_sz = ui_display_scale_px(44);
        lv_obj_t *del_btn = lv_btn_create(row1);
        lv_obj_clear_flag(del_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(del_btn, del_sz, del_sz);
        lv_obj_set_style_radius(del_btn, del_sz / 2, 0);
        lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xFFECEF), 0);
        lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xFFDDE3), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(del_btn, 0, 0);
        lv_obj_set_style_shadow_width(del_btn, 0, 0);
        lv_obj_add_event_cb(del_btn, ui_scenes_delete_scene_cb, LV_EVENT_CLICKED, (void *)scene->id);
        lv_obj_t *del_lab = lv_label_create(del_btn);
        lv_label_set_text(del_lab, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_font(del_lab, ui_font_montserrat_18(), 0);
        lv_obj_set_style_text_color(del_lab, lv_color_hex(0xDC2626), 0);
        lv_obj_center(del_lab);
    } else {
        lv_obj_t *act_btn = lv_btn_create(row1);
        lv_obj_clear_flag(act_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_height(act_btn, ui_display_scale_px(40));
        lv_obj_set_style_radius(act_btn, ui_display_scale_px(20), 0);
        lv_obj_set_style_bg_color(act_btn, lv_color_hex(0xE8EEFA), 0);
        lv_obj_set_style_bg_color(act_btn, lv_color_hex(0xDCE6FC), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(act_btn, 0, 0);
        lv_obj_set_style_shadow_width(act_btn, 0, 0);
        lv_obj_set_style_pad_left(act_btn, ui_display_scale_px(16), 0);
        lv_obj_set_style_pad_right(act_btn, ui_display_scale_px(16), 0);
        lv_obj_set_style_pad_top(act_btn, ui_display_scale_px(4), 0);
        lv_obj_set_style_pad_bottom(act_btn, ui_display_scale_px(4), 0);
        lv_obj_set_ext_click_area(act_btn, ui_display_scale_px(12));
        lv_obj_add_event_cb(act_btn, ui_scenes_activate_scene_cb, LV_EVENT_CLICKED, (void *)scene->id);
        lv_obj_t *act_lab = lv_label_create(act_btn);
        lv_label_set_text(act_lab, ui_str(UI_STR_ACTIVATE));
        lv_obj_set_style_text_color(act_lab, lv_color_hex(0x2E6BE6), 0);
        lv_obj_set_style_text_font(act_lab, ui_font_body(), 0);
        lv_obj_center(act_lab);
    }

    lv_obj_t *bottom_row = lv_obj_create(card);
    lv_obj_clear_flag(bottom_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bottom_row, lv_pct(100));
    lv_obj_set_height(bottom_row, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(bottom_row, 0, 0);
    lv_obj_set_style_bg_opa(bottom_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(bottom_row, 0, 0);
    lv_obj_set_style_pad_all(bottom_row, 0, 0);
    lv_obj_set_flex_flow(bottom_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom_row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(bottom_row, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d %s", action_count,
             (action_count == 1) ? ui_str(UI_STR_DEVICE_SINGULAR) : ui_str(UI_STR_DEVICE_PLURAL));
    lv_obj_t *count_lab = lv_label_create(bottom_row);
    lv_label_set_text(count_lab, count_buf);
    lv_obj_set_style_text_font(count_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(count_lab, lv_color_hex(0x6B7A99), 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(count_lab, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

static void ui_scenes_refresh_list(void)
{
    if (!s_list_cont) {
        return;
    }
    lv_obj_clean(s_list_cont);

    int total = rm_list_manager_get_lists_count();
    int added = 0;
    for (int i = 0; i < total; i++) {
        rm_list_item_t *item = rm_list_manager_get_item_by_index(i);
        if (!item || item->entity_type != RM_LIST_ENTITY_TYPE_SCENE) {
            continue;
        }
        ui_scenes_add_card(s_list_cont, item);
        added++;
    }

    if (added == 0) {
        lv_obj_t *empty = lv_obj_create(s_list_cont);
        lv_obj_clear_flag(empty, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_height(empty, lv_pct(100));
        lv_obj_set_style_bg_opa(empty, LV_OPA_0, 0);
        lv_obj_set_style_border_width(empty, 0, 0);
        lv_obj_t *title = lv_label_create(empty);
        lv_label_set_text(title, ui_str(UI_STR_SCENE_EMPTY));
        lv_obj_set_style_text_color(title, lv_color_hex(0x4A5568), 0);
        lv_obj_set_style_text_font(title, ui_font_title(), 0);
        lv_obj_set_width(title, lv_pct(100));
        lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(title, LV_LABEL_LONG_WRAP);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(24));
        lv_obj_t *sub = lv_label_create(empty);
        lv_label_set_text(sub, ui_str(UI_STR_SCENE_EMPTY_SUB));
        lv_obj_set_style_text_font(sub, ui_font_body(), 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x6B7A99), 0);
        lv_obj_set_width(sub, lv_pct(100));
        lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(sub, title, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(8));
    }

    if (s_edit_btn) {
        if (added > 0) {
            lv_obj_clear_flag(s_edit_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_edit_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_Screen_Scenes_show(void)
{
    s_is_edit_mode = false;
    if (s_edit_btn_label) {
        lv_label_set_text(s_edit_btn_label, ui_str(UI_STR_EDIT));
        lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    }
    ui_scenes_refresh_list();
}

static void ui_scenes_add_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_Screen_CreateScene_show(NULL);
}

static void ui_scenes_edit_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    s_is_edit_mode = !s_is_edit_mode;
    if (s_edit_btn_label) {
        lv_label_set_text(s_edit_btn_label, s_is_edit_mode ? ui_str(UI_STR_DONE) : ui_str(UI_STR_EDIT));
        lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    }
    ui_scenes_refresh_list();
}

static lv_obj_t *ui_scenes_create_top_bar(lv_obj_t *parent)
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
    s_scenes_top_title = title;
    lv_label_set_text(title, ui_str(UI_STR_SCENES_TITLE));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(4));

    s_edit_btn = lv_btn_create(bar);
    lv_obj_clear_flag(s_edit_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_edit_btn, ui_display_scale_px(56), ui_display_scale_px(34));
    lv_obj_align(s_edit_btn, LV_ALIGN_TOP_RIGHT, 0, ui_display_scale_px(2));
    lv_obj_set_style_bg_opa(s_edit_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_edit_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_edit_btn, 0, 0);
    lv_obj_add_event_cb(s_edit_btn, ui_scenes_edit_btn_cb, LV_EVENT_CLICKED, NULL);
    s_edit_btn_label = lv_label_create(s_edit_btn);
    lv_label_set_text(s_edit_btn_label, ui_str(UI_STR_EDIT));
    lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_edit_btn_label, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(s_edit_btn_label);
    return bar;
}

void ui_Screen_Scenes_screen_init(void)
{
    ui_Screen_Scenes = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen_Scenes, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen_Scenes, lv_color_hex(0xF5F8FF), 0);
    ui_gesture_nav_enable(ui_Screen_Scenes);

    int32_t yres = 240;
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) {
        yres = lv_disp_get_ver_res(disp);
    }
    const int32_t top_h = ui_display_top_bar_height();

    (void)ui_scenes_create_top_bar(ui_Screen_Scenes);
    s_content = lv_obj_create(ui_Screen_Scenes);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_content, lv_pct(100));
    lv_obj_set_height(s_content, LV_MAX(40, yres - top_h));
    lv_obj_align_to(s_content, ui_Screen_Scenes, LV_ALIGN_TOP_LEFT, 0, top_h);
    lv_obj_set_style_bg_opa(s_content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_content, 0, 0);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_base_dir(s_content, LV_BASE_DIR_LTR, 0);
    lv_obj_set_style_pad_all(s_content, ui_display_scale_px(16), 0);
    lv_obj_set_style_pad_row(s_content, ui_display_scale_px(12), 0);

    s_list_cont = lv_obj_create(s_content);
    lv_obj_add_flag(s_list_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_list_cont, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_list_cont, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_list_cont, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_set_width(s_list_cont, lv_pct(100));
    lv_obj_set_height(s_list_cont, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(s_list_cont, 1);
    lv_obj_set_style_bg_opa(s_list_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_list_cont, 0, 0);
    lv_obj_set_flex_flow(s_list_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list_cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_list_cont, ui_display_scale_px(10), 0);

    s_add_btn = lv_btn_create(s_content);
    lv_obj_clear_flag(s_add_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_add_btn, lv_pct(100));
    lv_obj_set_height(s_add_btn, ui_display_scale_px(48));
    lv_obj_set_style_radius(s_add_btn, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(s_add_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_bg_color(s_add_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_add_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_add_btn, 0, 0);
    lv_obj_add_event_cb(s_add_btn, ui_scenes_add_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *add_label = lv_label_create(s_add_btn);
    s_scenes_add_label = add_label;
    lv_label_set_text(add_label, ui_str(UI_STR_ADD_SCENE));
    lv_obj_set_style_text_font(add_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(add_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(add_label);

    ui_scenes_refresh_list();
}

void ui_Screen_Scenes_screen_destroy(void)
{
    if (ui_Screen_Scenes) {
        lv_obj_del(ui_Screen_Scenes);
    }
    ui_Screen_Scenes = NULL;
    s_content = NULL;
    s_list_cont = NULL;
    s_add_btn = NULL;
    s_edit_btn = NULL;
    s_edit_btn_label = NULL;
    s_scenes_top_title = NULL;
    s_scenes_add_label = NULL;
    s_is_edit_mode = false;
}

void ui_Screen_Scenes_apply_language(void)
{
    if (s_scenes_top_title) {
        lv_label_set_text(s_scenes_top_title, ui_str(UI_STR_SCENES_TITLE));
        lv_obj_set_style_text_font(s_scenes_top_title, ui_font_title(), 0);
    }
    if (s_edit_btn_label) {
        lv_label_set_text(s_edit_btn_label, s_is_edit_mode ? ui_str(UI_STR_DONE) : ui_str(UI_STR_EDIT));
        lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    }
    if (s_scenes_add_label) {
        lv_label_set_text(s_scenes_add_label, ui_str(UI_STR_ADD_SCENE));
        lv_obj_set_style_text_font(s_scenes_add_label, ui_font_body(), 0);
    }
    ui_scenes_refresh_list();
}
