/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

// Schedules screen: list from g_schedules_list, each schedule id as a card.

#include "../ui.h"
#include "../ui_nav.h"
#include "../ui_gesture_nav.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_app_backend_util.h"
#include "ui_Screen_CreateSchedule.h"
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

lv_obj_t *ui_Screen_Schedules = NULL;

static lv_obj_t *s_list_cont = NULL;
static lv_obj_t *s_add_btn = NULL;
static lv_obj_t *s_content = NULL;
static lv_obj_t *s_edit_btn = NULL;
static lv_obj_t *s_edit_btn_label = NULL;
static bool s_is_edit_mode = false;

static void ui_schedules_refresh(void);
static void ui_schedules_apply_edit_mode(bool enable);

/* Create Schedule modal */
static lv_obj_t *s_create_modal = NULL;
static lv_obj_t *s_create_modal_box = NULL;  /* white dialog box inside overlay, shifted up when keyboard shown */
static lv_obj_t *s_schedule_name_ta = NULL;
static lv_obj_t *s_create_kb = NULL;
static lv_obj_t *s_sched_top_title = NULL;
static lv_obj_t *s_sched_add_label = NULL;
static lv_obj_t *s_sched_modal_title = NULL;
static lv_obj_t *s_sched_cancel_lab = NULL;
static lv_obj_t *s_sched_next_lab = NULL;

static void ui_schedules_modal_reset_shift(void)
{
    if (s_create_modal_box) {
        lv_obj_set_style_translate_y(s_create_modal_box, 0, 0);
    }
}

static void ui_schedules_modal_adjust_for_keyboard(lv_obj_t *ta, lv_obj_t *kb)
{
    if (ta == NULL || kb == NULL || s_create_modal_box == NULL) {
        return;
    }
    lv_obj_update_layout(kb);
    lv_obj_update_layout(ta);

    lv_area_t kb_a;
    lv_area_t ta_a;
    lv_obj_get_coords(kb, &kb_a);
    lv_obj_get_coords(ta, &ta_a);

    /* If textarea bottom is below keyboard top, shift dialog box up */
    const int32_t margin = 12;
    int32_t overlap = ta_a.y2 - (kb_a.y1 - margin);
    if (overlap < 0) {
        overlap = 0;
    }
    lv_disp_t *disp = lv_disp_get_default();
    int32_t max_shift = 200;
    if (disp) {
        max_shift = lv_disp_get_ver_res(disp) / 2;
    }
    if (overlap > max_shift) {
        overlap = max_shift;
    }
    lv_obj_set_style_translate_y(s_create_modal_box, -overlap, 0);
}

static void ui_schedules_modal_close(void)
{
    if (s_create_kb) {
        lv_keyboard_set_textarea(s_create_kb, NULL);
        lv_obj_add_flag(s_create_kb, LV_OBJ_FLAG_HIDDEN);
    }
    ui_schedules_modal_reset_shift();
    if (s_create_modal) {
        lv_obj_add_flag(s_create_modal, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_schedules_modal_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_schedules_modal_close();
}

static void ui_schedules_modal_next_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *name = s_schedule_name_ta ? lv_textarea_get_text(s_schedule_name_ta) : "";
    ui_schedules_modal_close();
    if (name && name[0] == '\0') {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCHEDULE_NAME_EMPTY));
        return;
    }
    ui_Screen_CreateSchedule_show(name);
}

static void ui_schedules_create_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(s_create_kb, NULL);
        lv_obj_add_flag(s_create_kb, LV_OBJ_FLAG_HIDDEN);
        ui_schedules_modal_reset_shift();
    }
}

static void ui_schedules_name_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);

    if (code == LV_EVENT_FOCUSED && s_create_kb) {
        lv_event_stop_bubbling(e);
        lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        lv_keyboard_set_textarea(s_create_kb, ta);
        lv_obj_clear_flag(s_create_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_align(s_create_kb, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(s_create_kb, -20);
        lv_obj_move_foreground(s_create_kb);
        ui_schedules_modal_adjust_for_keyboard(ta, s_create_kb);
    }
}

static void ui_schedules_create_modal_build(void)
{
    if (s_create_modal != NULL) {
        return;
    }
    lv_obj_t *scr = ui_Screen_Schedules;

    /* Overlay: full screen, blocks touches to background */
    s_create_modal = lv_obj_create(scr);
    lv_obj_clear_flag(s_create_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_create_modal, lv_pct(100), lv_pct(100));
    lv_obj_align(s_create_modal, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_create_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_create_modal, LV_OPA_40, 0);
    lv_obj_set_style_border_width(s_create_modal, 0, 0);
    lv_obj_add_flag(s_create_modal, LV_OBJ_FLAG_HIDDEN);

    /* White dialog box (moved up when keyboard is shown).
     * Width must fit: pad + btn + gap + btn + pad (fixed 280 with pad 20 was 240 inner vs 252 needed). */
    const int32_t modal_pad = ui_display_scale_px(20);
    const int32_t btn_w = ui_display_scale_px(120);
    const int32_t btn_h = ui_display_scale_px(40);
    const int32_t btn_gap = ui_display_scale_px(12);
    const int32_t box_w = btn_w * 2 + btn_gap + modal_pad * 2;

    s_create_modal_box = lv_obj_create(s_create_modal);
    lv_obj_t *box = s_create_modal_box;
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(box, box_w);
    lv_obj_set_style_min_height(box, ui_display_scale_px(180), 0);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(box, ui_display_scale_px(16), 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(box, 0, 0);
    lv_obj_set_style_pad_all(box, modal_pad, 0);
    lv_obj_set_style_pad_row(box, ui_display_scale_px(14), 0);

    lv_obj_t *title = lv_label_create(box);
    s_sched_modal_title = title;
    lv_label_set_text(title, ui_str(UI_STR_CREATE_SCHEDULE));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1A1A1A), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    s_schedule_name_ta = lv_textarea_create(box);
    lv_obj_set_width(s_schedule_name_ta, lv_pct(100));
    lv_obj_set_height(s_schedule_name_ta, ui_display_scale_px(40));
    lv_textarea_set_one_line(s_schedule_name_ta, true);
    lv_textarea_set_placeholder_text(s_schedule_name_ta, ui_str(UI_STR_SCHEDULE_NAME));
    lv_textarea_set_max_length(s_schedule_name_ta, 64);
    lv_obj_align_to(s_schedule_name_ta, title, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(14));
    lv_obj_set_style_text_font(s_schedule_name_ta, ui_font_body(), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_schedule_name_ta, ui_font_cjk_always(), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_opa(s_schedule_name_ta, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_schedule_name_ta, 0, 0);
    lv_obj_set_style_border_side(s_schedule_name_ta, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(s_schedule_name_ta, 1, 0);
    lv_obj_set_style_border_color(s_schedule_name_ta, lv_color_hex(0xD0D8E8), 0);
    lv_obj_add_flag(s_schedule_name_ta, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_schedule_name_ta, ui_schedules_name_ta_event_cb, LV_EVENT_ALL, NULL);

    /* Buttons row */
    lv_obj_t *btn_cont = lv_obj_create(box);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_cont, lv_pct(100), btn_h);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(btn_cont, btn_gap, 0);
    lv_obj_align_to(btn_cont, s_schedule_name_ta, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(20));

    lv_obj_t *cancel_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(cancel_btn, btn_w, btn_h);
    lv_obj_set_style_radius(cancel_btn, ui_display_scale_px(10), 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xE8EEFA), 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xD0DCE8), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_set_style_shadow_width(cancel_btn, 0, 0);
    lv_obj_add_event_cb(cancel_btn, ui_schedules_modal_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_lab = lv_label_create(cancel_btn);
    s_sched_cancel_lab = cancel_lab;
    lv_label_set_text(cancel_lab, ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(cancel_lab, ui_font_cjk_always(), 0);
    lv_obj_set_style_text_color(cancel_lab, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(cancel_lab);

    lv_obj_t *next_btn = lv_btn_create(btn_cont);
    lv_obj_set_size(next_btn, btn_w, btn_h);
    lv_obj_set_style_radius(next_btn, ui_display_scale_px(10), 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_bg_color(next_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(next_btn, 0, 0);
    lv_obj_set_style_shadow_width(next_btn, 0, 0);
    lv_obj_add_event_cb(next_btn, ui_schedules_modal_next_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *next_lab = lv_label_create(next_btn);
    s_sched_next_lab = next_lab;
    lv_label_set_text(next_lab, ui_str(UI_STR_NEXT));
    lv_obj_set_style_text_font(next_lab, ui_font_cjk_always(), 0);
    lv_obj_set_style_text_color(next_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(next_lab);

    /* Keyboard (child of screen so it appears above modal) */
    s_create_kb = lv_keyboard_create(scr);
    lv_obj_set_width(s_create_kb, lv_pct(100));
    lv_obj_set_height(s_create_kb, ui_display_scale_px(140));
    lv_obj_add_flag(s_create_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_create_kb, ui_schedules_create_kb_event_cb, LV_EVENT_ALL, NULL);
}

static bool ui_schedules_request_delete_schedule_from_backend(const char *schedule_id)
{
    /* Reserved hook for future backend delete API call */
    rm_list_item_t *schedule_item = rm_list_manager_find_item_by_id(schedule_id);
    if (schedule_item == NULL) {
        return false;
    }
    esp_err_t err = rm_list_manager_delete_item_from_backend(schedule_item);
    if (err != ESP_OK) {
        return false;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    ui_schedules_refresh();
    return true;
}

static void ui_schedules_delete_schedule_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *schedule_id = (const char *)lv_event_get_user_data(e);
    if (!schedule_id) {
        return;
    }

    ui_show_busy(ui_str(UI_STR_DELETING));
    bool ok = ui_schedules_request_delete_schedule_from_backend(schedule_id);
    ui_hide_busy();
    if (!ok) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_DELETE_SCHEDULE));
        return;
    }

    ui_schedules_refresh();
}

static void ui_schedules_open_edit_schedule_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_LONG_PRESSED) {
        return;
    }
    const char *schedule_id = (const char *)lv_event_get_user_data(e);
    if (!schedule_id) {
        return;
    }
    ui_Screen_CreateSchedule_show_edit(schedule_id);
}

static void ui_schedules_edit_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_schedules_apply_edit_mode(!s_is_edit_mode);
}

static void ui_schedules_add_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    /* Scroll list to top so Create Schedule modal is visible at top */
    if (s_list_cont && lv_obj_has_flag(s_list_cont, LV_OBJ_FLAG_SCROLLABLE)) {
        lv_obj_scroll_to_y(s_list_cont, 0, LV_ANIM_ON);
    }
    ui_schedules_create_modal_build();
    lv_textarea_set_text(s_schedule_name_ta, "");
    lv_obj_clear_flag(s_create_modal, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_create_modal);
}

/** Callback for schedule card switch: both CHECKED and unchecked go through here (LV_EVENT_VALUE_CHANGED) */
static void ui_schedules_switch_enabled_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    lv_obj_t *sw = lv_event_get_target(e);
    const char *schedule_id = (const char *)lv_event_get_user_data(e);
    if (!sw || !schedule_id) {
        return;
    }
    bool enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    rm_list_item_t *schedule = rm_list_manager_get_copied_item_by_id(schedule_id);
    if (!schedule) {
        return;
    }
    esp_err_t err = rm_list_manager_enable_item_to_backend(schedule, enabled);
    rm_list_manager_clean_item_memory(schedule);
    if (err != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_UPDATE_SCHEDULE));
        return;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    ui_schedules_refresh();
}

/* Minutes since midnight (0-1439) -> "5:34 am" / "5:55 pm" */
static void minutes_to_time_str(uint16_t minutes, char *buf, size_t buf_size)
{
    int h = minutes / 60;
    int m = minutes % 60;
    bool pm = (h >= 12);
    if (h == 0) {
        h = 12;
    } else if (h > 12) {
        h -= 12;
    }
    snprintf(buf, buf_size, "%d:%02d %s", h, m, pm ? ui_str(UI_STR_PM) : ui_str(UI_STR_AM));
}

static const char *const s_day_labels[] = { "M", "T", "W", "Th", "F", "S", "Su" };

static void ui_schedules_add_card(lv_obj_t *parent, rm_list_item_t *schedule)
{
    if (!parent || !schedule) {
        return;
    }

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, lv_pct(100));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 0, 0);
    lv_obj_set_style_pad_top(card, 2, 0);
    lv_obj_set_style_pad_bottom(card, 2, 0);
    lv_obj_set_style_pad_left(card, 8, 0);
    lv_obj_set_style_pad_right(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(card, 1, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xF8FBFF), LV_STATE_PRESSED);
        lv_obj_add_event_cb(card, ui_schedules_open_edit_schedule_cb, LV_EVENT_LONG_PRESSED, (void *)schedule->id);
    }

    /* Row 1: name (left), time + switch (right) */
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
    lv_obj_set_style_pad_column(row1, 4, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(row1, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    lv_obj_t *name_lab = lv_label_create(row1);
    lv_label_set_text(name_lab, schedule->name && schedule->name[0] ? schedule->name : (schedule->id ? schedule->id : ui_str(UI_STR_SCHEDULE)));
    lv_obj_set_style_text_font(name_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(name_lab, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_text_align(name_lab, LV_TEXT_ALIGN_LEFT, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(name_lab, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    /* spacer pushes time/switch to the right, avoid extra nested container */
    lv_obj_t *spacer = lv_obj_create(row1);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(spacer, 1, 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_min_height(spacer, 0, 0);
    lv_obj_set_style_min_width(spacer, 0, 0);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_set_style_pad_all(spacer, 0, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(spacer, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    char time_buf[24];
    uint16_t mins = (schedule->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE)
                    ? schedule->trigger.days_of_week.minutes : 0;
    minutes_to_time_str(mins, time_buf, sizeof(time_buf));

    lv_obj_t *time_lab = lv_label_create(row1);
    lv_label_set_text(time_lab, time_buf);
    lv_obj_set_style_text_font(time_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(time_lab, schedule->enabled ? lv_color_hex(0x2E6BE6) : lv_color_hex(0x4A5568), 0);
    lv_obj_set_style_bg_opa(time_lab, schedule->enabled ? LV_OPA_COVER : LV_OPA_0, 0);
    lv_obj_set_style_bg_color(time_lab, lv_color_hex(0xE8EEFA), 0);
    lv_obj_set_style_radius(time_lab, 8, 0);
    lv_obj_set_style_pad_left(time_lab, 6, 0);
    lv_obj_set_style_pad_right(time_lab, 6, 0);
    lv_obj_set_style_pad_top(time_lab, 2, 0);
    lv_obj_set_style_pad_bottom(time_lab, 2, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(time_lab, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    if (!s_is_edit_mode) {
        lv_obj_t *sw = lv_switch_create(row1);
        lv_obj_set_size(sw, 40, 22);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
        lv_obj_set_style_radius(sw, LV_RADIUS_CIRCLE, LV_PART_KNOB);
        lv_obj_set_style_border_width(sw, 0, LV_PART_MAIN);
        lv_obj_set_style_border_width(sw, 0, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0xE0E8F5), LV_PART_MAIN | LV_STATE_DEFAULT);
        /* Keep the slider track neutral; only knob turns blue when enabled */
        lv_obj_set_style_bg_color(sw, lv_color_hex(0xE0E8F5), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_opa(sw, LV_OPA_0, LV_PART_INDICATOR | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0xFFFFFF), LV_PART_KNOB | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(sw, lv_color_hex(0x2E6BE6), LV_PART_KNOB | LV_STATE_CHECKED);
        lv_obj_set_style_pad_all(sw, 2, LV_PART_MAIN);
        if (schedule->enabled) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        }
        lv_obj_add_event_cb(sw, ui_schedules_switch_enabled_cb, LV_EVENT_VALUE_CHANGED, (void *)schedule->id);
    } else {
        const lv_coord_t del_sz = ui_display_scale_px(44);
        lv_obj_t *del_btn = lv_btn_create(row1);
        lv_obj_clear_flag(del_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(del_btn, del_sz, del_sz);
        lv_obj_set_style_radius(del_btn, del_sz / 2, 0);
        lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xFFECEF), 0);
        lv_obj_set_style_bg_color(del_btn, lv_color_hex(0xFFDDE3), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(del_btn, 0, 0);
        lv_obj_set_style_shadow_width(del_btn, 0, 0);
        lv_obj_add_event_cb(del_btn, ui_schedules_delete_schedule_cb, LV_EVENT_CLICKED, (void *)schedule->id);
        lv_obj_t *del_lab = lv_label_create(del_btn);
        lv_label_set_text(del_lab, LV_SYMBOL_TRASH);
        lv_obj_set_style_text_font(del_lab, ui_font_montserrat_18(), 0);
        lv_obj_set_style_text_color(del_lab, lv_color_hex(0xDC2626), 0);
        lv_obj_center(del_lab);
    }

    /* Row 2: "Repeat" */
    lv_obj_t *row2 = lv_obj_create(card);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(row2, lv_pct(100));
    lv_obj_set_height(row2, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(row2, 0, 0);
    lv_obj_set_style_bg_opa(row2, LV_OPA_0, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(row2, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    lv_obj_t *repeat_lab = lv_label_create(row2);
    lv_label_set_text(repeat_lab, ui_str(UI_STR_REPEAT));
    lv_obj_set_style_text_font(repeat_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(repeat_lab, lv_color_hex(0x6B7A99), 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(repeat_lab, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    int dev_count = rm_list_manager_get_node_actions_count(schedule);
    char count_buf[32];
    snprintf(count_buf, sizeof(count_buf), "%d %s", dev_count,
             (dev_count == 1) ? ui_str(UI_STR_DEVICE_SINGULAR) : ui_str(UI_STR_DEVICE_PLURAL));

    lv_obj_t *days_row = lv_obj_create(card);
    lv_obj_clear_flag(days_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(days_row, lv_pct(100));
    lv_obj_set_height(days_row, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(days_row, 0, 0);
    lv_obj_set_style_bg_opa(days_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(days_row, 0, 0);
    lv_obj_set_style_pad_all(days_row, 0, 0);
    lv_obj_set_flex_flow(days_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(days_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(days_row, 1, 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(days_row, LV_OBJ_FLAG_EVENT_BUBBLE);
    }

    uint8_t days = (schedule->trigger_type == RM_SCHEDULE_TRIGGER_TYPE_ABSOLUTE)
                   ? schedule->trigger.days_of_week.days_of_week : 0;
    if (days == 0) {
        /* No days selected: show "Once" as a single grey pill (reference UI) */
        lv_obj_t *once_btn = lv_btn_create(days_row);
        lv_obj_clear_flag(once_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_height(once_btn, 24);
        lv_obj_set_style_min_width(once_btn, 0, 0);
        lv_obj_set_style_radius(once_btn, 12, 0);
        lv_obj_set_style_bg_color(once_btn, lv_color_hex(0xE8EEFA), 0);
        lv_obj_set_style_border_width(once_btn, 0, 0);
        lv_obj_set_style_shadow_width(once_btn, 0, 0);
        lv_obj_set_style_pad_left(once_btn, 12, 0);
        lv_obj_set_style_pad_right(once_btn, 12, 0);
        lv_obj_set_style_pad_top(once_btn, 0, 0);
        lv_obj_set_style_pad_bottom(once_btn, 0, 0);
        if (!s_is_edit_mode) {
            lv_obj_add_flag(once_btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
        lv_obj_t *once_lab = lv_label_create(once_btn);
        lv_label_set_text(once_lab, ui_str(UI_STR_ONCE));
        lv_obj_set_style_text_font(once_lab, ui_font_body(), 0);
        lv_obj_set_style_text_color(once_lab, lv_color_hex(0x6B7A99), 0);
        lv_obj_center(once_lab);
    } else {
        for (int i = 0; i < 7; i++) {
            lv_obj_t *day_btn = lv_btn_create(days_row);
            lv_obj_clear_flag(day_btn, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_set_size(day_btn, 20, 20);
            lv_obj_set_style_min_width(day_btn, 0, 0);
            lv_obj_set_style_min_height(day_btn, 0, 0);
            lv_obj_set_style_radius(day_btn, 10, 0);
            bool on = (days & (1u << i)) != 0;
            lv_obj_set_style_bg_color(day_btn, on ? lv_color_hex(0x2E6BE6) : lv_color_hex(0xE8EEFA), 0);
            lv_obj_set_style_border_width(day_btn, 0, 0);
            lv_obj_set_style_shadow_width(day_btn, 0, 0);
            lv_obj_set_style_pad_all(day_btn, 0, 0);
            if (!s_is_edit_mode) {
                lv_obj_add_flag(day_btn, LV_OBJ_FLAG_EVENT_BUBBLE);
            }
            lv_obj_t *day_lab = lv_label_create(day_btn);
            lv_label_set_text(day_lab, s_day_labels[i]);
            lv_obj_set_style_text_font(day_lab, ui_font_nav(), 0);
            lv_obj_set_style_text_color(day_lab, on ? lv_color_hex(0xFFFFFF) : lv_color_hex(0x6B7A99), 0);
            lv_obj_center(day_lab);
        }
    }

    /* Row 3: device count at card bottom-right */
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

    lv_obj_t *count_lab = lv_label_create(bottom_row);
    lv_label_set_text(count_lab, count_buf);
    lv_obj_set_style_text_font(count_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(count_lab, lv_color_hex(0x6B7A99), 0);
    if (!s_is_edit_mode) {
        lv_obj_add_flag(count_lab, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

static void ui_schedules_refresh(void)
{
    if (!s_list_cont) {
        return;
    }
    lv_obj_clean(s_list_cont);

    int total = rm_list_manager_get_lists_count();
    int count = 0;
    for (int i = 0; i < total; i++) {
        rm_list_item_t *item = rm_list_manager_get_item_by_index(i);
        if (item && item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
            count++;
        }
    }
    if (count == 0) {
        lv_obj_t *empty = lv_label_create(s_list_cont);
        lv_label_set_text(empty, ui_str(UI_STR_SCHEDULE_EMPTY));
        lv_obj_set_style_text_font(empty, ui_font_title(), 0);
        lv_obj_set_style_text_color(empty, lv_color_hex(0x1A1A1A), 0);
        lv_obj_set_width(empty, lv_pct(100));
        lv_obj_set_style_text_align(empty, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_long_mode(empty, LV_LABEL_LONG_WRAP);
        lv_obj_align(empty, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(24));
        lv_obj_t *sub = lv_label_create(s_list_cont);
        lv_label_set_text(sub, ui_str(UI_STR_SCHEDULE_EMPTY_SUB));
        lv_obj_set_style_text_font(sub, ui_font_body(), 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x6B7A99), 0);
        lv_obj_set_width(sub, lv_pct(100));
        lv_label_set_long_mode(sub, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(sub, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align_to(sub, empty, LV_ALIGN_OUT_BOTTOM_MID, 0, ui_display_scale_px(8));
        return;
    }

    if (s_edit_btn) {
        if (count > 0) {
            lv_obj_clear_flag(s_edit_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_edit_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }

    for (int i = 0; i < total; i++) {
        rm_list_item_t *item = rm_list_manager_get_item_by_index(i);
        if (item && item->entity_type == RM_LIST_ENTITY_TYPE_SCHEDULE) {
            ui_schedules_add_card(s_list_cont, item);
        }
    }
}

static void ui_schedules_apply_edit_mode(bool enable)
{
    s_is_edit_mode = enable;
    if (s_edit_btn_label) {
        lv_label_set_text(s_edit_btn_label, s_is_edit_mode ? ui_str(UI_STR_DONE) : ui_str(UI_STR_EDIT));
        lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    }
    ui_schedules_refresh();
}

void ui_Screen_Schedules_apply_language(void)
{
    if (s_sched_top_title) {
        lv_label_set_text(s_sched_top_title, ui_str(UI_STR_SCHEDULES_TITLE));
        lv_obj_set_style_text_font(s_sched_top_title, ui_font_title(), 0);
    }
    if (s_edit_btn_label) {
        lv_label_set_text(s_edit_btn_label, s_is_edit_mode ? ui_str(UI_STR_DONE) : ui_str(UI_STR_EDIT));
        lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    }
    if (s_sched_add_label) {
        lv_label_set_text(s_sched_add_label, ui_str(UI_STR_ADD_SCHEDULE));
        lv_obj_set_style_text_font(s_sched_add_label, ui_font_body(), 0);
    }
    if (s_sched_modal_title) {
        lv_label_set_text(s_sched_modal_title, ui_str(UI_STR_CREATE_SCHEDULE));
        lv_obj_set_style_text_font(s_sched_modal_title, ui_font_title(), 0);
    }
    if (s_schedule_name_ta) {
        lv_textarea_set_placeholder_text(s_schedule_name_ta, ui_str(UI_STR_SCHEDULE_NAME));
        lv_obj_set_style_text_font(s_schedule_name_ta, ui_font_body(), LV_PART_MAIN);
        lv_obj_set_style_text_font(s_schedule_name_ta, ui_font_cjk_always(), LV_PART_TEXTAREA_PLACEHOLDER);
    }
    if (s_sched_cancel_lab) {
        lv_label_set_text(s_sched_cancel_lab, ui_str(UI_STR_CANCEL));
        lv_obj_set_style_text_font(s_sched_cancel_lab, ui_font_cjk_always(), 0);
    }
    if (s_sched_next_lab) {
        lv_label_set_text(s_sched_next_lab, ui_str(UI_STR_NEXT));
        lv_obj_set_style_text_font(s_sched_next_lab, ui_font_cjk_always(), 0);
    }
    ui_schedules_refresh();
}

void ui_Screen_Schedules_show(void)
{
    /* Always enter in normal mode so cards show switches by default */
    ui_schedules_apply_edit_mode(false);
}

static lv_obj_t *ui_schedules_create_top_bar(lv_obj_t *parent)
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
    s_sched_top_title = title;
    lv_label_set_text(title, ui_str(UI_STR_SCHEDULES_TITLE));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(4));

    /* Edit button (top-right) */
    s_edit_btn = lv_btn_create(bar);
    lv_obj_clear_flag(s_edit_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_edit_btn, ui_display_scale_px(56), ui_display_scale_px(34));
    lv_obj_align(s_edit_btn, LV_ALIGN_TOP_RIGHT, 0, ui_display_scale_px(2));
    lv_obj_set_style_bg_opa(s_edit_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_edit_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_edit_btn, 0, 0);
    lv_obj_add_event_cb(s_edit_btn, ui_schedules_edit_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_edit_btn_label = lv_label_create(s_edit_btn);
    lv_label_set_text(s_edit_btn_label, ui_str(UI_STR_EDIT));
    lv_obj_set_style_text_font(s_edit_btn_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(s_edit_btn_label, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(s_edit_btn_label);

    return bar;
}

void ui_Screen_Schedules_screen_init(void)
{
    ui_Screen_Schedules = lv_obj_create(NULL);
    /* Top bar fixed; list scrolls inside content so "Add" stays at bottom */
    lv_obj_clear_flag(ui_Screen_Schedules, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen_Schedules, lv_color_hex(0xF5F8FF), 0);
    ui_gesture_nav_enable(ui_Screen_Schedules);

    (void)ui_schedules_create_top_bar(ui_Screen_Schedules);

    int32_t yres = 240;
    lv_disp_t *disp = lv_disp_get_default();
    if (disp) {
        yres = lv_disp_get_ver_res(disp);
    }
    const int32_t top_h = ui_display_top_bar_height();

    /* Content below header: fixed height so list can flex-grow and push Add button down */
    s_content = lv_obj_create(ui_Screen_Schedules);
    lv_obj_clear_flag(s_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_content, lv_pct(100));
    lv_obj_set_height(s_content, LV_MAX(40, yres - top_h));
    lv_obj_align_to(s_content, ui_Screen_Schedules, LV_ALIGN_TOP_LEFT, 0, top_h);
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
    lv_obj_set_style_pad_row(s_list_cont, ui_display_scale_px(12), 0);

    s_add_btn = lv_btn_create(s_content);
    lv_obj_clear_flag(s_add_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_add_btn, lv_pct(100));
    lv_obj_set_height(s_add_btn, ui_display_scale_px(48));
    lv_obj_set_style_radius(s_add_btn, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(s_add_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_bg_color(s_add_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_add_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_add_btn, 0, 0);
    lv_obj_add_event_cb(s_add_btn, ui_schedules_add_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *add_label = lv_label_create(s_add_btn);
    s_sched_add_label = add_label;
    lv_label_set_text(add_label, ui_str(UI_STR_ADD_SCHEDULE));
    lv_obj_set_style_text_font(add_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(add_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(add_label);

    ui_schedules_refresh();
}

void ui_Screen_Schedules_screen_destroy(void)
{
    if (ui_Screen_Schedules) {
        lv_obj_del(ui_Screen_Schedules);
    }
    ui_Screen_Schedules = NULL;
    s_list_cont = NULL;
    s_add_btn = NULL;
    s_content = NULL;
    s_edit_btn = NULL;
    s_edit_btn_label = NULL;
    s_is_edit_mode = false;
    s_create_modal = NULL;
    s_create_modal_box = NULL;
    s_schedule_name_ta = NULL;
    s_create_kb = NULL;
    s_sched_top_title = NULL;
    s_sched_add_label = NULL;
    s_sched_modal_title = NULL;
    s_sched_cancel_lab = NULL;
    s_sched_next_lab = NULL;
}
