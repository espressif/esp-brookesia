/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Create Scene screen: scene name + actions + save.
 */

#include <stdlib.h>
#include <string.h>
#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_standard_device.h"
#include "ui_Screen_CreateScene.h"
#include "ui_Screen_Scenes.h"
#include "ui_Screen_SelectActions.h"
#include "ui_Screen_SelectDevices.h"

lv_obj_t *ui_Screen_CreateScene = NULL;

typedef struct {
    rm_list_item_t *scene_item;
    bool is_edit;
} create_scene_ctx_t;

typedef struct {
    rm_device_item_handle_t handle;
    int action_index;
} create_scene_action_card_ctx_t;

static create_scene_ctx_t s_create_scene_ctx = {NULL, false};

static lv_obj_t *s_header_title = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_name_ta = NULL;
static lv_obj_t *s_actions_empty_panel = NULL;
static lv_obj_t *s_actions_list = NULL;
static lv_obj_t *s_create_scene_kb = NULL;
static lv_obj_t *s_save_btn = NULL;
static lv_obj_t *s_primary_btn_label = NULL;
static lv_obj_t *s_delete_btn = NULL;
static lv_obj_t *s_name_title_lab = NULL;
static lv_obj_t *s_actions_row_lab = NULL;
static lv_obj_t *s_no_act_lab = NULL;
static lv_obj_t *s_del_lab = NULL;
static int s_action_count = 0;

#define CREATE_SCENE_HEADER_H  56
#define CREATE_SCENE_KB_H      130

static void ui_create_scene_update_save_btn_state(void)
{
    if (!s_save_btn) {
        return;
    }
    bool enabled = (s_action_count > 0);
    if (enabled) {
        lv_obj_clear_state(s_save_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2E6BE6), 0);
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    } else {
        lv_obj_add_state(s_save_btn, LV_STATE_DISABLED);
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x88A8D8), 0);
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x88A8D8), LV_STATE_PRESSED);
    }
}

static void ui_clean_create_scene_ctx(void)
{
    if (s_create_scene_ctx.scene_item) {
        rm_list_manager_clean_item_memory(s_create_scene_ctx.scene_item);
    }
    s_create_scene_ctx.scene_item = NULL;
    s_create_scene_ctx.is_edit = false;
}

static void ui_create_scene_refresh_actions_empty(void)
{
    if (s_actions_empty_panel) {
        if (s_action_count > 0) {
            lv_obj_add_flag(s_actions_empty_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_actions_empty_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_actions_list) {
        if (s_action_count > 0) {
            lv_obj_clear_flag(s_actions_list, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_actions_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
    ui_create_scene_update_save_btn_state();
}

static void ui_create_scene_keyboard_show(void)
{
    if (!s_create_scene_kb || !s_content || !s_name_ta) {
        return;
    }
    lv_obj_t *body = lv_obj_get_parent(s_content);
    if (!body || body == ui_Screen_CreateScene) {
        return;
    }
    int screen_h = lv_obj_get_height(ui_Screen_CreateScene);
    lv_obj_set_height(body, screen_h - CREATE_SCENE_HEADER_H - CREATE_SCENE_KB_H);
    lv_obj_update_layout(s_create_scene_kb);
    lv_obj_update_layout(body);
    lv_obj_scroll_to_y(body, 0, LV_ANIM_OFF);
    lv_obj_update_layout(body);

    lv_area_t kb_a;
    lv_area_t ta_a;
    lv_obj_get_coords(s_create_scene_kb, &kb_a);
    lv_obj_get_coords(s_name_ta, &ta_a);
    const int32_t margin = 12;
    int32_t overlap = ta_a.y2 - (kb_a.y1 - margin);
    if (overlap > 0) {
        int32_t cur = lv_obj_get_scroll_y(body);
        lv_obj_scroll_to_y(body, cur + overlap, LV_ANIM_OFF);
    }
}

static void ui_create_scene_keyboard_hide(void)
{
    if (!s_content || !ui_Screen_CreateScene) {
        return;
    }
    lv_obj_t *body = lv_obj_get_parent(s_content);
    if (body && body != ui_Screen_CreateScene) {
        lv_coord_t sh = lv_obj_get_height(ui_Screen_CreateScene);
        lv_obj_set_height(body, LV_MAX(40, sh - CREATE_SCENE_HEADER_H));
    }
}

static void ui_create_scene_action_card_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    create_scene_action_card_ctx_t *ctx = (create_scene_action_card_ctx_t *)lv_event_get_user_data(e);
    if (ctx) {
        free(ctx);
    }
}

static void ui_create_scene_action_card_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    create_scene_action_card_ctx_t *ctx = (create_scene_action_card_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !s_create_scene_ctx.scene_item) {
        return;
    }
    ui_Screen_SelectActions_show_for_edit(ctx->handle, s_create_scene_ctx.scene_item, ctx->action_index);
}

static void ui_create_scene_add_action_card(lv_obj_t *parent, rm_device_item_handle_t handle, const char *summary, int action_index)
{
    if (!parent || !handle || !summary || summary[0] == '\0') {
        return;
    }
    create_scene_action_card_ctx_t *ctx = (create_scene_action_card_ctx_t *)malloc(sizeof(create_scene_action_card_ctx_t));
    if (!ctx) {
        return;
    }
    ctx->handle = handle;
    ctx->action_index = action_index;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE0E8F0), 0);
    lv_obj_set_style_pad_all(card, ui_display_scale_px(6), 0);
    lv_obj_set_style_pad_right(card, ui_display_scale_px(24), 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    /* All START: avoids cross-axis centering that can clip the icon on narrow rows */
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    /* Gap between icon and text — without this, labels draw over the icon */
    lv_obj_set_style_pad_column(card, ui_display_scale_px(14), 0);
    lv_obj_add_event_cb(card, ui_create_scene_action_card_click_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(card, ui_create_scene_action_card_delete_cb, LV_EVENT_DELETE, ctx);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(handle->type, true));
#else
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(handle->type, true));
#endif
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(icon, ui_display_scale_px(32), ui_display_scale_px(32));
    lv_obj_set_style_flex_grow(icon, 0, 0);

    lv_obj_t *text_col = lv_obj_create(card);
    /* Match CreateSchedule: extra gap icon ↔ text when pad_column is tight on small screens */
    lv_obj_set_style_pad_left(text_col, ui_display_scale_px(4), 0);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(text_col, LV_OPA_0, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(text_col, ui_display_scale_px(2), 0);
    lv_obj_set_height(text_col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text_col, 1);
    lv_obj_set_style_min_width(text_col, 0, 0);

    lv_obj_t *name = lv_label_create(text_col);
    lv_label_set_text(name, ui_device_name_for_display(handle->name));
    lv_obj_set_style_text_font(name, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_width(name, lv_pct(100));

    lv_obj_t *details = lv_label_create(text_col);
    {
        char disp[512];
        ui_format_action_summary_for_display(summary, disp, sizeof(disp));
        lv_label_set_text(details, disp);
    }
    lv_obj_set_style_text_font(details, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    lv_obj_set_style_text_color(details, lv_color_hex(0x6B7A99), 0);
    lv_label_set_long_mode(details, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(details, lv_pct(100));
}

void ui_Screen_CreateScene_refresh_actions(void)
{
    if (!s_actions_list) {
        return;
    }
    lv_obj_clean(s_actions_list);
    int added = 0;
    if (s_create_scene_ctx.scene_item) {
        int count = rm_list_manager_get_node_actions_count(s_create_scene_ctx.scene_item);
        for (int i = 0; i < count; i++) {
            rm_node_action_item_t *item = rm_list_manager_get_node_action_item_by_index(s_create_scene_ctx.scene_item, i);
            if (!item || !item->node_id || item->type == RM_NODE_ACTION_DELETE) {
                continue;
            }
            rm_device_item_handle_t handle = rm_app_backend_get_device_handle_by_node_id(item->node_id);
            if (!handle) {
                continue;
            }
            char *summary_buf = NULL;
            size_t summary_size = 0;
            rm_list_manager_action_to_summary(item->action, &summary_buf, &summary_size);
            if (summary_buf) {
                ui_create_scene_add_action_card(s_actions_list, handle, summary_buf, i);
                free(summary_buf);
            } else {
                ui_create_scene_add_action_card(s_actions_list, handle, ui_str(UI_STR_NO_ACTIONS_SELECTED), i);
            }
            added++;
        }
    }
    s_action_count = added;
    ui_create_scene_refresh_actions_empty();
}

static void ui_create_scene_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (ui_Screen_Scenes) {
        if (s_create_scene_kb) {
            lv_keyboard_set_textarea(s_create_scene_kb, NULL);
            lv_obj_add_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
        }
        ui_create_scene_keyboard_hide();
        lv_scr_load(ui_Screen_Scenes);
        ui_Screen_Scenes_show();
    }
}

static void ui_create_scene_actions_plus_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_create_scene_ctx.scene_item) {
        return;
    }
    ui_Screen_SelectDevices_show(s_create_scene_ctx.scene_item);
}

static void ui_create_scene_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_create_scene_ctx.scene_item) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCENE_DATA_NOT_READY));
        return;
    }
    const char *name = s_name_ta ? lv_textarea_get_text(s_name_ta) : NULL;
    if (!name || name[0] == '\0') {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCENE_NAME_EMPTY));
        return;
    }
    char *new_name = strdup(name);
    if (!new_name) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_NO_MEMORY));
        return;
    }
    rainmaker_app_backend_util_safe_free((void *)s_create_scene_ctx.scene_item->name);
    s_create_scene_ctx.scene_item->name = new_name;

    if (rm_list_manager_get_node_actions_count(s_create_scene_ctx.scene_item) <= 0) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_ADD_ONE_ACTION));
        return;
    }

    esp_err_t err = rm_list_manager_update_item_to_backend(s_create_scene_ctx.scene_item);
    if (err != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_SAVE_SCENE_BACKEND));
        return;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    /* GET /nodes may still return stale scene name after successful PUT; keep UI aligned with user edit */
    rm_list_manager_merge_saved_item_into_canonical(s_create_scene_ctx.scene_item);
    if (ui_Screen_Scenes) {
        if (s_create_scene_kb) {
            lv_keyboard_set_textarea(s_create_scene_kb, NULL);
            lv_obj_add_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
        }
        ui_create_scene_keyboard_hide();
        lv_scr_load(ui_Screen_Scenes);
        ui_Screen_Scenes_show();
    }
}

static void ui_create_scene_name_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED && s_create_scene_kb) {
        lv_event_stop_bubbling(e);
        lv_obj_t *ta = lv_event_get_target(e);
        lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        lv_keyboard_set_textarea(s_create_scene_kb, ta);
        lv_obj_clear_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_align(s_create_scene_kb, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(s_create_scene_kb, -16);
        lv_obj_move_foreground(s_create_scene_kb);
        ui_create_scene_keyboard_show();
    }
}

static void ui_create_scene_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(s_create_scene_kb, NULL);
        lv_obj_add_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
        ui_create_scene_keyboard_hide();
    }
}

static void ui_create_scene_pencil_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_name_ta && s_create_scene_kb) {
        lv_obj_add_state(s_name_ta, LV_STATE_FOCUSED);
        lv_textarea_set_cursor_pos(s_name_ta, LV_TEXTAREA_CURSOR_LAST);
        lv_keyboard_set_textarea(s_create_scene_kb, s_name_ta);
        lv_obj_clear_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_align(s_create_scene_kb, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(s_create_scene_kb, -16);
        lv_obj_move_foreground(s_create_scene_kb);
        ui_create_scene_keyboard_show();
    }
}

static void ui_create_scene_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_create_scene_ctx.scene_item) {
        return;
    }
    esp_err_t err = rm_list_manager_delete_item_from_backend(s_create_scene_ctx.scene_item);
    if (err != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_DELETE_SCENE));
        return;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    ui_clean_create_scene_ctx();
    if (ui_Screen_Scenes) {
        lv_scr_load(ui_Screen_Scenes);
        ui_Screen_Scenes_show();
    }
}

static void ui_create_scene_set_primary_btn_text(void)
{
    if (!s_primary_btn_label) {
        return;
    }
    /* Plain text only: LV_SYMBOL_OK + ui_font_body() renders □ with CJK UI */
    lv_label_set_text(s_primary_btn_label,
                      ui_str(s_create_scene_ctx.is_edit ? UI_STR_UPDATE : UI_STR_SAVE));
    lv_obj_set_style_text_font(s_primary_btn_label, ui_font_cjk_always(), 0);
}

static void ui_create_scene_apply_mode(void)
{
    if (s_header_title) {
        lv_label_set_text(s_header_title,
                          ui_str(s_create_scene_ctx.is_edit ? UI_STR_EDIT_SCENE : UI_STR_CREATE_SCENE_SCREEN));
        lv_obj_set_style_text_font(s_header_title, ui_font_title(), 0);
    }
    ui_create_scene_set_primary_btn_text();
    if (s_name_ta && s_create_scene_ctx.scene_item) {
        lv_textarea_set_text(s_name_ta, s_create_scene_ctx.scene_item->name ? s_create_scene_ctx.scene_item->name : "");
        /* Create: empty text + "新场景" placeholder; Edit: real name + field hint */
        lv_textarea_set_placeholder_text(s_name_ta,
                                         ui_str(s_create_scene_ctx.is_edit ? UI_STR_SCENE_NAME : UI_STR_NEW_SCENE));
    }
    if (s_delete_btn) {
        if (s_create_scene_ctx.is_edit && s_create_scene_ctx.scene_item) {
            lv_obj_clear_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    /* Refresh scroll range when edit mode toggles Save/Delete visibility (CreateSchedule pattern) */
    if (s_content) {
        lv_obj_update_layout(s_content);
        lv_obj_t *body = lv_obj_get_parent(s_content);
        if (body && body != ui_Screen_CreateScene) {
            lv_obj_update_layout(body);
        }
    }
}

void ui_Screen_CreateScene_show(const char *scene_name)
{
    if (!ui_Screen_CreateScene) {
        return;
    }
    ui_clean_create_scene_ctx();
    const char *name = (scene_name && scene_name[0] != '\0') ? scene_name : "";
    s_create_scene_ctx.scene_item = rm_list_manager_create_new_item_with_name(NULL, name, RM_LIST_ENTITY_TYPE_SCENE);
    if (!s_create_scene_ctx.scene_item) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_CREATE_SCENE_NEW));
        return;
    }
    s_create_scene_ctx.is_edit = false;
    if (s_create_scene_kb) {
        lv_keyboard_set_textarea(s_create_scene_kb, NULL);
        lv_obj_add_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
    }
    ui_create_scene_keyboard_hide();
    ui_create_scene_apply_mode();
    ui_Screen_CreateScene_refresh_actions();
    lv_scr_load(ui_Screen_CreateScene);
}

void ui_Screen_CreateScene_show_edit(const char *scene_id)
{
    if (!ui_Screen_CreateScene || !scene_id || scene_id[0] == '\0') {
        return;
    }
    ui_clean_create_scene_ctx();
    rm_list_item_t *copied = rm_list_manager_get_copied_item_by_id(scene_id);
    if (!copied || copied->entity_type != RM_LIST_ENTITY_TYPE_SCENE) {
        if (copied) {
            rm_list_manager_clean_item_memory(copied);
        }
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCENE_NOT_FOUND));
        return;
    }
    s_create_scene_ctx.scene_item = copied;
    s_create_scene_ctx.is_edit = true;
    if (s_create_scene_kb) {
        lv_keyboard_set_textarea(s_create_scene_kb, NULL);
        lv_obj_add_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
    }
    ui_create_scene_keyboard_hide();
    ui_create_scene_apply_mode();
    ui_Screen_CreateScene_refresh_actions();
    lv_scr_load(ui_Screen_CreateScene);
}

void ui_Screen_CreateScene_screen_init(void)
{
    ui_Screen_CreateScene = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_CreateScene, lv_color_hex(0xF5F8FF), 0);
    lv_obj_clear_flag(ui_Screen_CreateScene, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *header = lv_obj_create(ui_Screen_CreateScene);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(header, lv_pct(100));
    lv_obj_set_height(header, 56);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(header, 0, 0);

    lv_obj_t *back = lv_btn_create(header);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(back, 40, 40);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, ui_create_scene_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arrow = lv_label_create(back);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(arrow);

    s_header_title = lv_label_create(header);
    lv_label_set_text(s_header_title, ui_str(UI_STR_CREATE_SCENE_SCREEN));
    lv_obj_set_style_text_font(s_header_title, ui_font_title(), 0);
    lv_obj_set_style_text_color(s_header_title, lv_color_hex(0x1A1A1A), 0);
    lv_obj_align(s_header_title, LV_ALIGN_CENTER, 0, 0);

    lv_coord_t scr_h_body = lv_disp_get_ver_res(lv_disp_get_default());
    if (scr_h_body <= 0) {
        scr_h_body = 480;
    }

    /* Outer column: scroll_mid fills area below header; Save/Delete live inside scroll (not pinned to screen bottom). */
    lv_obj_t *body = lv_obj_create(ui_Screen_CreateScene);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_height(body, LV_MAX(40, scr_h_body - CREATE_SCENE_HEADER_H));
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_opa(body, LV_OPA_0, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_bottom(body, 0, 0);

    lv_obj_t *scroll_mid = lv_obj_create(body);
    lv_obj_add_flag(scroll_mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll_mid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_mid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_width(scroll_mid, lv_pct(100));
    lv_obj_set_flex_grow(scroll_mid, 1);
    lv_obj_set_style_bg_opa(scroll_mid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(scroll_mid, 0, 0);
    lv_obj_set_flex_flow(scroll_mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    s_content = scroll_mid;
    lv_obj_set_style_pad_all(s_content, ui_display_scale_px(16), 0);
    lv_obj_set_style_pad_bottom(s_content, ui_display_scale_px(16), 0);
    lv_obj_set_flex_flow(s_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_content, ui_display_scale_px(8), 0);

    /* Scene Name card: match Schedule Name style */
    lv_obj_t *name_card = lv_obj_create(s_content);
    lv_obj_clear_flag(name_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(name_card, lv_pct(100));
    lv_obj_set_height(name_card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(name_card, 12, 0);
    lv_obj_set_style_bg_color(name_card, lv_color_hex(0xF0F4FA), 0);
    lv_obj_set_style_border_width(name_card, 1, 0);
    lv_obj_set_style_border_color(name_card, lv_color_hex(0xD0D8E8), 0);
    lv_obj_set_style_pad_all(name_card, 4, 0);
    lv_obj_set_style_pad_bottom(name_card, 2, 0);

    lv_obj_t *name_lab = lv_label_create(name_card);
    s_name_title_lab = name_lab;
    lv_label_set_text(name_lab, ui_str(UI_STR_SCENE_NAME));
    lv_obj_set_style_text_font(name_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(name_lab, lv_color_hex(0x4A5568), 0);
    lv_obj_align(name_lab, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *sep_inside_name = lv_obj_create(name_card);
    lv_obj_clear_flag(sep_inside_name, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(sep_inside_name, lv_pct(100));
    lv_obj_set_height(sep_inside_name, 1);
    lv_obj_set_style_bg_color(sep_inside_name, lv_color_hex(0xE0E8F0), 0);
    lv_obj_set_style_border_width(sep_inside_name, 0, 0);
    lv_obj_align_to(sep_inside_name, name_lab, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);

    s_name_ta = lv_textarea_create(name_card);
    lv_obj_set_width(s_name_ta, lv_pct(100));
    lv_obj_set_height(s_name_ta, 18);
    lv_textarea_set_one_line(s_name_ta, true);
    lv_textarea_set_placeholder_text(s_name_ta, ui_str(UI_STR_NEW_SCENE));
    lv_textarea_set_max_length(s_name_ta, 64);
    lv_obj_align_to(s_name_ta, sep_inside_name, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_bg_opa(s_name_ta, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_name_ta, 0, 0);
    lv_obj_add_flag(s_name_ta, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_name_ta, ui_create_scene_name_ta_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_set_style_text_font(s_name_ta, ui_font_cjk_always(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_name_ta, ui_font_cjk_always(), LV_PART_TEXTAREA_PLACEHOLDER);

    lv_obj_t *pencil_btn = lv_btn_create(name_card);
    lv_obj_clear_flag(pencil_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pencil_btn, 28, 28);
    lv_obj_align_to(pencil_btn, s_name_ta, LV_ALIGN_OUT_RIGHT_MID, -36, 0);
    lv_obj_set_style_bg_opa(pencil_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(pencil_btn, 0, 0);
    lv_obj_add_event_cb(pencil_btn, ui_create_scene_pencil_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pencil = lv_label_create(pencil_btn);
    lv_label_set_text(pencil, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(pencil, lv_color_hex(0x8AA0C8), 0);
    lv_obj_center(pencil);

    lv_obj_t *actions_row = lv_obj_create(s_content);
    lv_obj_clear_flag(actions_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(actions_row, lv_pct(100));
    lv_obj_set_height(actions_row, 40);
    lv_obj_set_style_bg_opa(actions_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(actions_row, 0, 0);
    lv_obj_t *actions_lab = lv_label_create(actions_row);
    s_actions_row_lab = actions_lab;
    lv_label_set_text(actions_lab, ui_str(UI_STR_ACTIONS));
    lv_obj_set_style_text_font(actions_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(actions_lab, lv_color_hex(0x1A1A1A), 0);
    lv_obj_align(actions_lab, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_t *plus_btn = lv_btn_create(actions_row);
    lv_obj_clear_flag(plus_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(plus_btn, 36, 36);
    lv_obj_align(plus_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_radius(plus_btn, 18, 0);
    lv_obj_set_style_bg_color(plus_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_add_event_cb(plus_btn, ui_create_scene_actions_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *plus_lab = lv_label_create(plus_btn);
    lv_label_set_text(plus_lab, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(plus_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(plus_lab);

    s_actions_empty_panel = lv_obj_create(s_content);
    lv_obj_clear_flag(s_actions_empty_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_actions_empty_panel, lv_pct(100));
    lv_obj_set_height(s_actions_empty_panel, 200);
    lv_obj_set_style_bg_color(s_actions_empty_panel, lv_color_hex(0xF7FAFF), 0);
    lv_obj_set_style_border_width(s_actions_empty_panel, 0, 0);
    lv_obj_set_style_radius(s_actions_empty_panel, 12, 0);
    lv_obj_t *no_act = lv_label_create(s_actions_empty_panel);
    s_no_act_lab = no_act;
    lv_label_set_text(no_act, ui_str(UI_STR_NO_ACTIONS_SELECTED));
    lv_obj_set_style_text_font(no_act, ui_font_body(), 0);
    lv_obj_set_style_text_color(no_act, lv_color_hex(0x6B7A99), 0);
    lv_obj_center(no_act);

    s_actions_list = lv_obj_create(s_content);
    lv_obj_clear_flag(s_actions_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_actions_list, lv_pct(100));
    lv_obj_set_height(s_actions_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_actions_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_actions_list, 0, 0);
    lv_obj_set_flex_flow(s_actions_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_actions_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_actions_list, 10, 0);
    lv_obj_add_flag(s_actions_list, LV_OBJ_FLAG_HIDDEN);

    /* Primary / Delete: same sizing as CreateSchedule (ui_display_scale_px, full width) */
    s_save_btn = lv_btn_create(s_content);
    lv_obj_clear_flag(s_save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_save_btn, lv_pct(100));
    lv_obj_set_height(s_save_btn, ui_display_scale_px(48));
    lv_obj_set_style_radius(s_save_btn, 12, 0);
    lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_save_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_save_btn, 0, 0);
    lv_obj_add_event_cb(s_save_btn, ui_create_scene_save_cb, LV_EVENT_CLICKED, NULL);
    s_primary_btn_label = lv_label_create(s_save_btn);
    ui_create_scene_set_primary_btn_text();
    lv_obj_set_style_text_color(s_primary_btn_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(s_primary_btn_label);

    /* Delete button only for edit mode */
    s_delete_btn = lv_btn_create(s_content);
    lv_obj_clear_flag(s_delete_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_delete_btn, lv_pct(100));
    lv_obj_set_height(s_delete_btn, ui_display_scale_px(48));
    lv_obj_set_style_radius(s_delete_btn, 12, 0);
    lv_obj_set_style_bg_color(s_delete_btn, lv_color_hex(0xEF4444), 0);
    lv_obj_set_style_bg_color(s_delete_btn, lv_color_hex(0xDC2626), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_delete_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_delete_btn, 0, 0);
    lv_obj_add_event_cb(s_delete_btn, ui_create_scene_delete_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *del_lab = lv_label_create(s_delete_btn);
    s_del_lab = del_lab;
    lv_label_set_text(del_lab, ui_str(UI_STR_DELETE));
    lv_obj_set_style_text_color(del_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(del_lab, ui_font_cjk_always(), 0);
    lv_obj_center(del_lab);
    lv_obj_add_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);

    s_create_scene_kb = lv_keyboard_create(ui_Screen_CreateScene);
    lv_obj_set_width(s_create_scene_kb, lv_pct(100));
    lv_obj_set_height(s_create_scene_kb, ui_display_scale_px(130));
    lv_obj_add_flag(s_create_scene_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_create_scene_kb, ui_create_scene_kb_event_cb, LV_EVENT_ALL, NULL);
    ui_create_scene_update_save_btn_state();
}

void ui_Screen_CreateScene_screen_destroy(void)
{
    ui_clean_create_scene_ctx();
    if (ui_Screen_CreateScene) {
        lv_obj_del(ui_Screen_CreateScene);
    }
    ui_Screen_CreateScene = NULL;
    s_header_title = NULL;
    s_content = NULL;
    s_name_ta = NULL;
    s_actions_empty_panel = NULL;
    s_actions_list = NULL;
    s_create_scene_kb = NULL;
    s_save_btn = NULL;
    s_primary_btn_label = NULL;
    s_delete_btn = NULL;
    s_name_title_lab = NULL;
    s_actions_row_lab = NULL;
    s_no_act_lab = NULL;
    s_del_lab = NULL;
}

void ui_Screen_CreateScene_apply_language(void)
{
    if (s_name_title_lab) {
        lv_label_set_text(s_name_title_lab, ui_str(UI_STR_SCENE_NAME));
        lv_obj_set_style_text_font(s_name_title_lab, ui_font_body(), 0);
    }
    if (s_name_ta) {
        lv_obj_set_style_text_font(s_name_ta, ui_font_cjk_always(), LV_PART_MAIN);
        lv_obj_set_style_text_font(s_name_ta, ui_font_cjk_always(), LV_PART_TEXTAREA_PLACEHOLDER);
    }
    if (s_actions_row_lab) {
        lv_label_set_text(s_actions_row_lab, ui_str(UI_STR_ACTIONS));
        lv_obj_set_style_text_font(s_actions_row_lab, ui_font_body(), 0);
    }
    if (s_no_act_lab) {
        lv_label_set_text(s_no_act_lab, ui_str(UI_STR_NO_ACTIONS_SELECTED));
        lv_obj_set_style_text_font(s_no_act_lab, ui_font_body(), 0);
    }
    if (s_del_lab) {
        lv_label_set_text(s_del_lab, ui_str(UI_STR_DELETE));
        lv_obj_set_style_text_font(s_del_lab, ui_font_cjk_always(), 0);
    }
    ui_create_scene_apply_mode();
    ui_Screen_CreateScene_refresh_actions();
}
