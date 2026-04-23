/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Select Devices: scrollable list of current home devices.
 * Each row = device icon + name, clickable to open device detail (Light/Switch/Fan).
 * Done button returns to Create Schedule or Create Scene (or previous screen).
 * Selected devices and actions are stored in s_item->node_actions_list (rm_node_action_list_t).
 */

#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_device_group_manager.h"
#include "ui_Screen_CreateSchedule.h"
#include "ui_Screen_CreateScene.h"
#include "ui_Screen_SelectActions.h"
#include "ui_Screen_SelectDevices.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_standard_device.h"
#include <stdlib.h>
#include <string.h>

lv_obj_t *ui_Screen_SelectDevices = NULL;

/** Picker row + selected cards: cap icon on narrow displays so text column keeps width; avoids clipping. */
static int32_t select_devices_icon_size_px(int32_t design_px)
{
    int32_t sz = ui_display_scale_px(design_px);
    int32_t hor = ui_display_get_hor_res();
    if (hor > 0 && hor < 400) {
        int32_t cap = hor / 12;
        if (cap < 20) {
            cap = 20;
        }
        if (sz > cap) {
            sz = cap;
        }
    }
    return sz;
}

static rm_list_item_t *s_item = NULL;
/** True when s_item is a heap copy (e.g. edit flow) and must not be replaced by find(id) canonical pointer. */
static bool s_s_item_is_detached_copy = false;
/** Copy of schedule/scene list id while Select Devices is active; used to re-resolve s_item after list manager replaces the struct. */
static char s_item_id_buf[64];

static void select_devices_sync_s_item_from_id(void)
{
    if (s_item_id_buf[0] == '\0') {
        return;
    }
    if (s_s_item_is_detached_copy) {
        return;
    }
    rm_list_item_t *live = rm_list_manager_find_item_by_id(s_item_id_buf);
    if (live != NULL) {
        s_item = live;
    }
}

static lv_obj_t *s_list = NULL;
static lv_obj_t *s_selected_list = NULL;
static lv_obj_t *s_selected_label = NULL;
static lv_obj_t *s_select_more_label = NULL;
static lv_obj_t *s_header_title_label = NULL;
static lv_obj_t *s_done_label = NULL;

static void select_devices_row_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    rm_device_item_handle_t handle = (rm_device_item_handle_t)lv_event_get_user_data(e);
    if (!handle) {
        return;
    }
    ui_Screen_SelectActions_show(handle);
}

static void select_devices_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_devices_sync_s_item_from_id();
    if (s_item && s_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE && ui_Screen_CreateScene) {
        rm_list_manager_revert_node_action_item(s_item);
        ui_Screen_CreateScene_refresh_actions();
        lv_scr_load(ui_Screen_CreateScene);
    } else if (ui_Screen_CreateSchedule) {
        rm_list_manager_revert_node_action_item(s_item);
        ui_Screen_CreateSchedule_refresh_actions();
        lv_scr_load(ui_Screen_CreateSchedule);
    } else if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
    }
}

static void select_devices_done_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_devices_sync_s_item_from_id();
    if (s_item && s_item->entity_type == RM_LIST_ENTITY_TYPE_SCENE && ui_Screen_CreateScene) {
        ui_Screen_CreateScene_refresh_actions();
        lv_scr_load(ui_Screen_CreateScene);
    } else if (ui_Screen_CreateSchedule) {
        ui_Screen_CreateSchedule_refresh_actions();
        lv_scr_load(ui_Screen_CreateSchedule);
    } else if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
    }
}

void ui_Screen_SelectDevices_set_selected_summary(rm_device_item_handle_t handle, const char *summary)
{
    select_devices_sync_s_item_from_id();
    if (handle == NULL || s_item == NULL || handle->node_id == NULL) {
        return;
    }

    if (summary == NULL || summary[0] == '\0') {
        rm_list_manager_remove_node_action_from_list_item(s_item, handle->node_id);
        return;
    }

    rm_node_action_item_t *item = rm_list_manager_create_new_node_action_item(handle->node_id);
    if (item == NULL) {
        return;
    }
    char *action_json = NULL;
    size_t action_json_size = 0;
    rm_list_manager_summary_to_action_json(handle->type, summary, &action_json, &action_json_size);
    if (action_json) {
        item->action = action_json;
    } else {
        item->action = strdup(ui_str(UI_STR_NO_ACTIONS_SELECTED));
    }
    esp_err_t err = rm_list_manager_add_node_action_to_list_item(s_item, item);
    if (err != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_LIST_ACTION));
        rm_list_manager_clean_node_action_item_memory(item);
    }
}

static void select_devices_remove_selected_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    select_devices_sync_s_item_from_id();
    rm_device_item_handle_t handle = (rm_device_item_handle_t)lv_event_get_user_data(e);
    if (handle && handle->node_id) {
        /* Set the node action item type to delete and no need to remove it from the list */
        rm_node_action_item_t *node_action_item = rm_list_manager_find_node_action_item_by_id(s_item, handle->node_id);
        if (node_action_item) {
            rm_list_manager_set_node_action_item_type(node_action_item, RM_NODE_ACTION_DELETE);
        }
    }
    ui_Screen_SelectDevices_refresh();
}

static void select_devices_add_selected_card(lv_obj_t *parent, rm_device_item_handle_t handle, const char *summary)
{
    if (!parent || !handle || !summary) {
        return;
    }

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0xE0E8F0), 0);
    lv_obj_set_style_pad_all(card, ui_display_scale_px(6), 0);
    lv_obj_set_style_pad_right(card, ui_display_scale_px(24), 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, ui_display_scale_px(8), 0);

#if LVGL_VERSION_MAJOR >= 9
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(handle->type, true));
#else
    lv_obj_t *icon = lv_img_create(card);
    lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(handle->type, true));
#endif
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    {
        int32_t iz = select_devices_icon_size_px(32);
        lv_obj_set_size(icon, iz, iz);
    }
    lv_obj_set_style_flex_grow(icon, 0, 0);

    /* Text column: flex-grow only — do not use lv_pct(100) width or it covers the icon */
    lv_obj_t *text_col = lv_obj_create(card);
    lv_obj_clear_flag(text_col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(text_col, LV_OPA_0, 0);
    lv_obj_set_style_border_width(text_col, 0, 0);
    lv_obj_set_flex_flow(text_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(text_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(text_col, ui_display_scale_px(2), 0);
    lv_obj_set_style_pad_left(text_col, 0, 0);
    lv_obj_set_height(text_col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(text_col, 1);
    lv_obj_set_style_min_width(text_col, 0, 0);

    lv_obj_t *name = lv_label_create(text_col);
    lv_label_set_text(name, ui_device_name_for_display(handle->name && handle->name[0] ? handle->name : ""));
    lv_obj_set_style_text_font(name, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_width(name, lv_pct(100));
    /* LONG_CLIP: LONG_DOT can corrupt UTF-8 (cf. ui_Screen_Home device cards). */
    lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

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

    /* Delete button: fixed on the right, always visible */
    lv_obj_t *close_btn = lv_btn_create(card);
    lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(close_btn, ui_display_scale_px(28), ui_display_scale_px(28));
    lv_obj_set_style_radius(close_btn, ui_display_scale_px(14), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFECEF), 0);
    lv_obj_set_style_bg_color(close_btn, lv_color_hex(0xFFDDE3), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(close_btn, 0, 0);
    lv_obj_set_style_shadow_width(close_btn, 0, 0);
    lv_obj_add_event_cb(close_btn, select_devices_remove_selected_cb, LV_EVENT_CLICKED, handle);
    lv_obj_t *close_lab = lv_label_create(close_btn);
    lv_label_set_text(close_lab, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(close_lab, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(close_lab, lv_color_hex(0xDC2626), 0);
    lv_obj_center(close_lab);
}

static lv_obj_t *create_header(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, ui_display_scale_px(56));
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, ui_display_scale_px(8), 0);
    lv_obj_set_style_pad_right(bar, ui_display_scale_px(8), 0);

    lv_obj_t *back = lv_btn_create(bar);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(back, ui_display_scale_px(40), ui_display_scale_px(40));
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, select_devices_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *arrow = lv_label_create(back);
    lv_label_set_text(arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(arrow, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(arrow);

    lv_obj_t *title = lv_label_create(bar);
    s_header_title_label = title;
    lv_label_set_text(title, ui_str(UI_STR_SELECT_DEVICES));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_width(title, lv_pct(68));
    lv_label_set_long_mode(title, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    return bar;
}

void ui_Screen_SelectDevices_show(rm_list_item_t *schedule_item)
{
    s_item = schedule_item;
    s_s_item_is_detached_copy = false;
    s_item_id_buf[0] = '\0';
    if (schedule_item != NULL && schedule_item->id != NULL) {
        strncpy(s_item_id_buf, schedule_item->id, sizeof(s_item_id_buf) - 1);
        s_item_id_buf[sizeof(s_item_id_buf) - 1] = '\0';
        rm_list_item_t *live = rm_list_manager_find_item_by_id(schedule_item->id);
        if (live != NULL && live != schedule_item) {
            s_s_item_is_detached_copy = true;
        }
    }
    ui_Screen_SelectDevices_refresh();
    lv_scr_load(ui_Screen_SelectDevices);
}

void ui_Screen_SelectDevices_refresh(void)
{
    if (ui_Screen_SelectDevices == NULL || s_list == NULL || s_selected_list == NULL) {
        return;
    }
    select_devices_sync_s_item_from_id();
    lv_obj_clean(s_list);
    lv_obj_clean(s_selected_list);

    int selected_count = 0;
    if (s_item != NULL) {
        int count = rm_list_manager_get_node_actions_count(s_item);
        for (int i = 0; i < count; i++) {
            rm_node_action_item_t *item = rm_list_manager_get_node_action_item_by_index(s_item, i);
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
                select_devices_add_selected_card(s_selected_list, handle, summary_buf);
                free(summary_buf);
            } else {
                select_devices_add_selected_card(s_selected_list, handle, ui_str(UI_STR_NO_ACTIONS_SELECTED));
            }
            selected_count++;
        }
    }

    if (s_selected_label) {
        if (selected_count == 0) {
            lv_obj_add_flag(s_selected_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_selected_list, LV_OBJ_FLAG_HIDDEN);
            if (s_select_more_label) {
                lv_label_set_text(s_select_more_label, ui_str(UI_STR_SELECT_DEVICES));
            }
        } else {
            lv_obj_clear_flag(s_selected_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_selected_list, LV_OBJ_FLAG_HIDDEN);
            if (s_select_more_label) {
                lv_label_set_text(s_select_more_label, ui_str(UI_STR_SELECT_MORE));
            }
        }
    }

    int n = rm_app_backend_get_current_home_devices_count();
    for (int i = 0; i < n; i++) {
        rm_app_device_t dev;
        rm_device_item_handle_t handle = rm_app_backend_get_current_home_device_by_index(i, &dev);
        if (!handle) {
            continue;
        }
        rm_node_action_item_t *item = rm_list_manager_find_node_action_item_by_id(s_item, dev.node_id);
        if (item && (item->type != RM_NODE_ACTION_DELETE)) {
            /* Device is already selected, so no need show it in the list */
            continue;
        }
        lv_obj_t *row = lv_btn_create(s_list);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(row, lv_pct(100));
        lv_obj_set_style_radius(row, ui_display_scale_px(12), 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xF0F4F8), 0);
        lv_obj_set_style_bg_color(row, lv_color_hex(0xE0E8F0), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(row, 0, 0);
        lv_obj_set_style_pad_left(row, ui_display_scale_px(10), 0);
        lv_obj_set_style_pad_right(row, ui_display_scale_px(10), 0);
        lv_obj_set_style_pad_top(row, ui_display_scale_px(6), 0);
        lv_obj_set_style_pad_bottom(row, ui_display_scale_px(6), 0);
        lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(row, ui_display_scale_px(8), 0);
        lv_obj_add_event_cb(row, select_devices_row_click_cb, LV_EVENT_CLICKED, (void *)handle);

        const int32_t row_icon_sz = select_devices_icon_size_px(32);

#if LVGL_VERSION_MAJOR >= 9
        lv_obj_t *icon = lv_img_create(row);
        lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(dev.type, true));
#else
        lv_obj_t *icon = lv_img_create(row);
        lv_img_set_src(icon, rainmaker_standard_device_get_device_icon_src(dev.type, true));
#endif
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(icon, row_icon_sz, row_icon_sz);
        lv_obj_set_style_flex_grow(icon, 0, 0);
        lv_obj_add_flag(icon, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        /* Middle column only: never lv_pct(100) on a label direct child of row — it spans full row and overlaps icon. */
        lv_obj_t *text_slot = lv_obj_create(row);
        lv_obj_clear_flag(text_slot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(text_slot, LV_OPA_0, 0);
        lv_obj_set_style_border_width(text_slot, 0, 0);
        lv_obj_set_style_pad_all(text_slot, 0, 0);
        lv_obj_set_flex_grow(text_slot, 1);
        lv_obj_set_style_min_width(text_slot, 0, 0);
        lv_obj_set_height(text_slot, LV_SIZE_CONTENT);

        lv_obj_t *name = lv_label_create(text_slot);
        lv_label_set_text(name, ui_device_name_for_display(dev.name && dev.name[0] ? dev.name : ""));
        lv_obj_set_style_text_font(name, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
        lv_obj_set_style_text_color(name, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_width(name, lv_pct(100));
        lv_label_set_long_mode(name, LV_LABEL_LONG_CLIP);

        {
            int32_t min_h = row_icon_sz + ui_display_scale_px(14);
            if (min_h < ui_display_scale_px(48)) {
                min_h = ui_display_scale_px(48);
            }
            lv_obj_set_height(row, min_h);
        }
    }
}

void ui_Screen_SelectDevices_screen_init(void)
{
    ui_Screen_SelectDevices = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_SelectDevices, lv_color_hex(0xFFFFFF), 0);
    lv_obj_clear_flag(ui_Screen_SelectDevices, LV_OBJ_FLAG_SCROLLABLE);

    create_header(ui_Screen_SelectDevices);

    const int32_t top_h = ui_display_scale_px(56);
    lv_coord_t scr_h = lv_disp_get_ver_res(lv_disp_get_default());
    if (scr_h <= 0) {
        scr_h = 480;
    }

    /* Outer column: scroll area + Done pinned at bottom */
    lv_obj_t *content = lv_obj_create(ui_Screen_SelectDevices);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(content, lv_pct(100));
    lv_obj_set_height(content, LV_MAX(40, scr_h - top_h));
    lv_obj_align_to(content, ui_Screen_SelectDevices, LV_ALIGN_TOP_LEFT, 0, top_h);
    lv_obj_set_style_bg_opa(content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(content, ui_display_scale_px(16), 0);
    lv_obj_set_style_pad_row(content, ui_display_scale_px(12), 0);

    lv_obj_t *scroll_mid = lv_obj_create(content);
    lv_obj_add_flag(scroll_mid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroll_mid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll_mid, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_width(scroll_mid, lv_pct(100));
    lv_obj_set_flex_grow(scroll_mid, 1);
    lv_obj_set_style_bg_opa(scroll_mid, LV_OPA_0, 0);
    lv_obj_set_style_border_width(scroll_mid, 0, 0);
    lv_obj_set_flex_flow(scroll_mid, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(scroll_mid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(scroll_mid, ui_display_scale_px(12), 0);

    s_selected_label = lv_label_create(scroll_mid);
    lv_label_set_text(s_selected_label, ui_str(UI_STR_SELECTED_DEVICES));
    lv_obj_set_style_text_font(s_selected_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_selected_label, lv_color_hex(0x4A5568), 0);
    lv_obj_set_width(s_selected_label, lv_pct(100));
    lv_label_set_long_mode(s_selected_label, LV_LABEL_LONG_CLIP);

    s_selected_list = lv_obj_create(scroll_mid);
    lv_obj_clear_flag(s_selected_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_selected_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_selected_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_selected_list, ui_display_scale_px(10), 0);
    lv_obj_set_style_pad_bottom(s_selected_list, ui_display_scale_px(4), 0);
    lv_obj_set_width(s_selected_list, lv_pct(100));
    lv_obj_set_height(s_selected_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_selected_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_selected_list, 0, 0);

    s_select_more_label = lv_label_create(scroll_mid);
    lv_label_set_text(s_select_more_label, ui_str(UI_STR_SELECT_MORE));
    lv_obj_set_style_text_font(s_select_more_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_select_more_label, lv_color_hex(0x4A5568), 0);
    lv_obj_set_width(s_select_more_label, lv_pct(100));
    lv_label_set_long_mode(s_select_more_label, LV_LABEL_LONG_CLIP);

    s_list = lv_obj_create(scroll_mid);
    lv_obj_clear_flag(s_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(s_list, ui_display_scale_px(10), 0);
    lv_obj_set_width(s_list, lv_pct(100));
    lv_obj_set_height(s_list, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(s_list, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_list, 0, 0);

    /* Done button at bottom */
    lv_obj_t *done_btn = lv_btn_create(content);
    lv_obj_clear_flag(done_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(done_btn, lv_pct(100));
    lv_obj_set_height(done_btn, ui_display_scale_px(48));
    lv_obj_set_style_radius(done_btn, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0xE0E8F0), 0);
    lv_obj_set_style_bg_color(done_btn, lv_color_hex(0xD0D8E0), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(done_btn, 0, 0);
    lv_obj_add_event_cb(done_btn, select_devices_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *done_lab = lv_label_create(done_btn);
    s_done_label = done_lab;
    lv_label_set_text(done_lab, ui_str(UI_STR_DONE));
    lv_obj_set_style_text_font(done_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(done_lab, lv_color_hex(0x4A5568), 0);
    lv_obj_set_width(done_lab, lv_pct(100));
    lv_label_set_long_mode(done_lab, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(done_lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(done_lab);
}

void ui_Screen_SelectDevices_screen_destroy(void)
{
    if (ui_Screen_SelectDevices) {
        lv_obj_del(ui_Screen_SelectDevices);
    }
    ui_Screen_SelectDevices = NULL;
    s_item = NULL;
    s_item_id_buf[0] = '\0';
    s_list = NULL;
    s_selected_list = NULL;
    s_selected_label = NULL;
    s_select_more_label = NULL;
    s_header_title_label = NULL;
    s_done_label = NULL;
}

void ui_Screen_SelectDevices_apply_language(void)
{
    if (s_header_title_label) {
        lv_label_set_text(s_header_title_label, ui_str(UI_STR_SELECT_DEVICES));
        lv_obj_set_style_text_font(s_header_title_label, ui_font_title(), 0);
    }
    if (s_selected_label) {
        lv_label_set_text(s_selected_label, ui_str(UI_STR_SELECTED_DEVICES));
        lv_obj_set_style_text_font(s_selected_label, ui_font_body(), 0);
    }
    if (s_done_label) {
        lv_label_set_text(s_done_label, ui_str(UI_STR_DONE));
        lv_obj_set_style_text_font(s_done_label, ui_font_body(), 0);
    }
    ui_Screen_SelectDevices_refresh();
}

#ifdef RM_HOST_BUILD
bool ui_Screen_SelectDevices_layout_test_get_first_picker_row(lv_obj_t **row_out, lv_obj_t **icon_out,
        lv_obj_t **name_label_out,
        lv_obj_t **text_slot_out)
{
    if (!s_list || !row_out || !icon_out || !name_label_out || !text_slot_out) {
        return false;
    }
    if (lv_obj_get_child_cnt(s_list) < 1) {
        return false;
    }
    lv_obj_t *row = lv_obj_get_child(s_list, 0);
    if (lv_obj_get_child_cnt(row) < 2) {
        return false;
    }
    lv_obj_t *icon = lv_obj_get_child(row, 0);
    lv_obj_t *text_slot = lv_obj_get_child(row, 1);
    if (!lv_obj_check_type(icon, &lv_image_class)) {
        return false;
    }
    if (lv_obj_get_child_cnt(text_slot) < 1) {
        return false;
    }
    lv_obj_t *name = lv_obj_get_child(text_slot, 0);
    if (!lv_obj_check_type(name, &lv_label_class)) {
        return false;
    }
    *row_out = row;
    *icon_out = icon;
    *text_slot_out = text_slot;
    *name_label_out = name;
    return true;
}
#endif /* RM_HOST_BUILD */
