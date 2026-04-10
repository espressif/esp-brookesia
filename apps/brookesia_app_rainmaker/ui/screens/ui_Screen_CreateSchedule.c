/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
* Create Schedule detail screen: name, time, repeat days, actions, save.
*/

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../ui.h"
#include "rainmaker_app_backend.h"
#include "ui_Screen_SelectDevices.h"
#include "ui_Screen_SelectActions.h"
#include "ui_Screen_Schedules.h"
#include "rainmaker_standard_device.h"
#include "rainmaker_list_manager.h"
#include "rainmaker_app_backend_util.h"

lv_obj_t *ui_Screen_CreateSchedule = NULL;

static lv_obj_t *s_content = NULL;
static lv_obj_t *s_name_ta = NULL;
static lv_obj_t *s_time_btn = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_day_btns[7] = { NULL };
static lv_obj_t *s_actions_plus_btn = NULL;
static lv_obj_t *s_actions_empty_panel = NULL;
static lv_obj_t *s_actions_list = NULL;
static lv_obj_t *s_save_btn = NULL;
static lv_obj_t *s_delete_btn = NULL;
static lv_obj_t *s_create_schedule_kb = NULL;
static lv_obj_t *s_header_title = NULL;
static lv_obj_t *s_primary_btn_label = NULL;
static lv_obj_t *s_name_title_lab = NULL;
static lv_obj_t *s_time_row_lab = NULL;
static lv_obj_t *s_repeat_row_lab = NULL;
static lv_obj_t *s_actions_row_lab = NULL;
static lv_obj_t *s_no_act_lab = NULL;
static lv_obj_t *s_actions_hint_lab = NULL;
static lv_obj_t *s_del_lab = NULL;
static lv_obj_t *s_time_picker_cancel_lab = NULL;
static lv_obj_t *s_time_picker_done_lab = NULL;

static const char *const s_day_labels[] = { "M", "T", "W", "Th", "F", "S", "Su" };

typedef struct {
    struct rm_list_item *schedule_item;
    int time_hour;
    int time_minute;
    bool time_pm;
    uint8_t repeat_days;
    uint8_t action_count;
    bool is_edit;
} create_schedule_ctx_t;

static create_schedule_ctx_t s_create_schedule_ctx = {NULL, 0, 0, false, 0, 0, false};

static void ui_create_schedule_update_time_display(void)
{
    if (s_time_label == NULL) {
        return;
    }
    char buf[24];
    snprintf(buf, sizeof(buf), "%d:%02d %s", s_create_schedule_ctx.time_hour, s_create_schedule_ctx.time_minute,
             s_create_schedule_ctx.time_pm ? ui_str(UI_STR_PM) : ui_str(UI_STR_AM));
    if (s_create_schedule_ctx.schedule_item) {
        s_create_schedule_ctx.schedule_item->trigger.days_of_week.minutes = s_create_schedule_ctx.time_hour * 60 + s_create_schedule_ctx.time_minute + (s_create_schedule_ctx.time_pm ? 12 * 60 : 0);
    }
    lv_label_set_text(s_time_label, buf);
}

static void ui_create_schedule_refresh_repeat_style(void)
{
    for (int i = 0; i < 7; i++) {
        if (s_day_btns[i] == NULL) {
            continue;
        }
        bool on = (s_create_schedule_ctx.repeat_days & (1u << i)) != 0;
        if (on) {
            lv_obj_set_style_bg_color(s_day_btns[i], lv_color_hex(0x2E6BE6), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_day_btns[i], 0), lv_color_hex(0xFFFFFF), 0);
            if (s_create_schedule_ctx.schedule_item) {
                s_create_schedule_ctx.schedule_item->trigger.days_of_week.days_of_week |= (1u << i);
            }
        } else {
            lv_obj_set_style_bg_color(s_day_btns[i], lv_color_hex(0xE8EEFA), 0);
            lv_obj_set_style_text_color(lv_obj_get_child(s_day_btns[i], 0), lv_color_hex(0x6B7A99), 0);
            if (s_create_schedule_ctx.schedule_item) {
                s_create_schedule_ctx.schedule_item->trigger.days_of_week.days_of_week &= ~(1u << i);
            }
        }
    }
}

static void ui_create_schedule_refresh_actions_empty(void)
{
    if (s_actions_empty_panel) {
        if (s_create_schedule_ctx.action_count > 0) {
            lv_obj_add_flag(s_actions_empty_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_actions_empty_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_actions_list) {
        if (s_create_schedule_ctx.action_count > 0) {
            lv_obj_clear_flag(s_actions_list, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_actions_list, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

/** User data for an action card when clicking to edit */
typedef struct {
    rm_device_item_handle_t handle;
    int action_index;
} create_schedule_action_card_ctx_t;

static void ui_create_schedule_action_card_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DELETE) {
        return;
    }
    create_schedule_action_card_ctx_t *ctx = (create_schedule_action_card_ctx_t *)lv_event_get_user_data(e);
    if (ctx) {
        free(ctx);
    }
}

static void ui_create_schedule_action_card_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    create_schedule_action_card_ctx_t *ctx = (create_schedule_action_card_ctx_t *)lv_event_get_user_data(e);
    if (!ctx || !s_create_schedule_ctx.schedule_item) {
        return;
    }
    ui_Screen_SelectActions_show_for_edit(ctx->handle, s_create_schedule_ctx.schedule_item, ctx->action_index);
}

static void ui_create_schedule_add_action_card(lv_obj_t *parent, rm_device_item_handle_t handle, const char *summary, int action_index)
{
    if (!parent || !handle || !summary || summary[0] == '\0') {
        return;
    }

    create_schedule_action_card_ctx_t *ctx = (create_schedule_action_card_ctx_t *)malloc(sizeof(create_schedule_action_card_ctx_t));
    if (!ctx) {
        return;
    }
    ctx->handle = handle;
    ctx->action_index = action_index;

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card, ui_display_scale_px(14), 0);
    lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card, ui_create_schedule_action_card_click_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(card, ui_create_schedule_action_card_delete_cb, LV_EVENT_DELETE, ctx);

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
    /* Gap icon ↔ text: LVGL8 PC sim has no lv_obj_set_style_margin_right; pad_left on text_col is equivalent. */
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

void ui_Screen_CreateSchedule_refresh_actions(void)
{
    if (!s_actions_list) {
        return;
    }
    lv_obj_clean(s_actions_list);
    int added = 0;
    if (s_create_schedule_ctx.schedule_item ) {
        int count = rm_list_manager_get_node_actions_count(s_create_schedule_ctx.schedule_item);
        for (int i = 0; i < count; i++) {
            rm_node_action_item_t *item = rm_list_manager_get_node_action_item_by_index(s_create_schedule_ctx.schedule_item, i);
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
                ui_create_schedule_add_action_card(s_actions_list, handle, summary_buf, i);
                free(summary_buf);
            } else {
                ui_create_schedule_add_action_card(s_actions_list, handle, ui_str_by_id(UI_STR_NO_ACTIONS_SELECTED), i);
            }
            added++;
        }
    }
    s_create_schedule_ctx.action_count = added;
    ui_create_schedule_refresh_actions_empty();
}

/* ---------- Time picker overlay ---------- */
static lv_obj_t *s_time_picker_overlay = NULL;
static lv_obj_t *s_time_roller_h = NULL;
static lv_obj_t *s_time_roller_m = NULL;
static lv_obj_t *s_time_roller_ap = NULL;

static void ui_create_schedule_time_picker_done_cb(lv_event_t *e);
static void ui_create_schedule_time_picker_cancel_cb(lv_event_t *e);
static void ui_create_schedule_delete_cb(lv_event_t *e);
static void ui_create_schedule_apply_mode();

static void ui_clean_create_schedule_ctx(void)
{
    if (s_create_schedule_ctx.schedule_item) {
        rm_list_manager_clean_item_memory(s_create_schedule_ctx.schedule_item);
    }
    s_create_schedule_ctx.schedule_item = NULL;
    s_create_schedule_ctx.time_hour = 0;
    s_create_schedule_ctx.time_minute = 0;
    s_create_schedule_ctx.time_pm = false;
    s_create_schedule_ctx.repeat_days = 0;
    s_create_schedule_ctx.action_count = 0;
    s_create_schedule_ctx.is_edit = false;
}

static void ui_create_schedule_show_time_picker(void)
{
    if (s_time_picker_overlay) {
        lv_obj_clear_flag(s_time_picker_overlay, LV_OBJ_FLAG_HIDDEN);
        lv_roller_set_selected(s_time_roller_h, (uint32_t)(s_create_schedule_ctx.time_hour - 1), LV_ANIM_OFF);
        lv_roller_set_selected(s_time_roller_m, (uint32_t)s_create_schedule_ctx.time_minute, LV_ANIM_OFF);
        lv_roller_set_selected(s_time_roller_ap, s_create_schedule_ctx.time_pm ? 1 : 0, LV_ANIM_OFF);
        return;
    }

    lv_obj_t *scr = ui_Screen_CreateSchedule;
    s_time_picker_overlay = lv_obj_create(scr);
    lv_obj_clear_flag(s_time_picker_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_time_picker_overlay, lv_pct(100), lv_pct(100));
    lv_obj_align(s_time_picker_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(s_time_picker_overlay, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_time_picker_overlay, 0, 0);

    lv_obj_t *panel = lv_obj_create(s_time_picker_overlay);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(panel, lv_pct(90));
    lv_obj_set_style_min_height(panel, 220, 0);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 12, 0);
    lv_obj_set_style_pad_top(panel, 0, 0);

    /* Top bar: Cancel / Done (darker bar like reference) */
    lv_obj_t *btn_row = lv_obj_create(panel);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_row, lv_pct(100), 44);
    lv_obj_set_style_radius(btn_row, 0, 0);
    lv_obj_set_style_bg_color(btn_row, lv_color_hex(0xF0F4F8), 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_left(btn_row, 12, 0);
    lv_obj_set_style_pad_right(btn_row, 12, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *cancel_btn = lv_btn_create(btn_row);
    lv_obj_set_style_bg_opa(cancel_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_add_event_cb(cancel_btn, ui_create_schedule_time_picker_cancel_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *cancel_lab = lv_label_create(cancel_btn);
    s_time_picker_cancel_lab = cancel_lab;
    lv_label_set_text(cancel_lab, ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(cancel_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(cancel_lab, lv_color_hex(0x4A5568), 0);
    lv_obj_center(cancel_lab);

    lv_obj_t *done_btn = lv_btn_create(btn_row);
    lv_obj_set_style_bg_opa(done_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(done_btn, 0, 0);
    lv_obj_add_event_cb(done_btn, ui_create_schedule_time_picker_done_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *done_lab = lv_label_create(done_btn);
    s_time_picker_done_lab = done_lab;
    lv_label_set_text(done_lab, ui_str(UI_STR_DONE));
    lv_obj_set_style_text_font(done_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(done_lab, lv_color_hex(0x4A5568), 0);
    lv_obj_center(done_lab);

    /* Rollers row: hour : minute AM/PM */
    lv_obj_t *roller_cont = lv_obj_create(panel);
    lv_obj_clear_flag(roller_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(roller_cont, lv_pct(100));
    lv_obj_set_height(roller_cont, 140);
    lv_obj_set_style_bg_opa(roller_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(roller_cont, 0, 0);
    lv_obj_set_flex_flow(roller_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(roller_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(roller_cont, 16, 0);
    lv_obj_align_to(roller_cont, btn_row, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    s_time_roller_h = lv_roller_create(roller_cont);
    lv_obj_set_width(s_time_roller_h, 56);
    lv_obj_set_height(s_time_roller_h, 120);
    lv_roller_set_options(s_time_roller_h, "1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_font(s_time_roller_h, ui_font_roller(), 0);
    lv_obj_set_style_text_font(s_time_roller_h, ui_font_roller(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(s_time_roller_h, lv_color_hex(0xE8ECF0), LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_time_roller_h, lv_color_hex(0x4A5568), LV_PART_SELECTED);

    lv_obj_t *colon = lv_label_create(roller_cont);
    lv_label_set_text(colon, ":");
    lv_obj_set_style_text_font(colon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(colon, lv_color_hex(0x4A5568), 0);
    lv_obj_align(colon, LV_ALIGN_CENTER, 0, 0);

    s_time_roller_m = lv_roller_create(roller_cont);
    lv_obj_set_width(s_time_roller_m, 56);
    lv_obj_set_height(s_time_roller_m, 120);
    lv_roller_set_options(s_time_roller_m, "00\n01\n02\n03\n04\n05\n06\n07\n08\n09\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59", LV_ROLLER_MODE_NORMAL);
    lv_obj_set_style_text_font(s_time_roller_m, ui_font_roller(), 0);
    lv_obj_set_style_text_font(s_time_roller_m, ui_font_roller(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(s_time_roller_m, lv_color_hex(0xE8ECF0), LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_time_roller_m, lv_color_hex(0x4A5568), LV_PART_SELECTED);

    s_time_roller_ap = lv_roller_create(roller_cont);
    lv_obj_set_width(s_time_roller_ap, 52);
    lv_obj_set_height(s_time_roller_ap, 120);
    {
        char ap_opts[48];
        snprintf(ap_opts, sizeof(ap_opts), "%s\n%s", ui_str(UI_STR_AM), ui_str(UI_STR_PM));
        lv_roller_set_options(s_time_roller_ap, ap_opts, LV_ROLLER_MODE_NORMAL);
    }
    lv_obj_set_style_text_font(s_time_roller_ap, ui_font_roller(), 0);
    lv_obj_set_style_text_font(s_time_roller_ap, ui_font_roller(), LV_PART_SELECTED);
    lv_obj_set_style_bg_color(s_time_roller_ap, lv_color_hex(0xE8ECF0), LV_PART_SELECTED);
    lv_obj_set_style_text_color(s_time_roller_ap, lv_color_hex(0x4A5568), LV_PART_SELECTED);

    lv_roller_set_selected(s_time_roller_h, (uint32_t)(s_create_schedule_ctx.time_hour - 1), LV_ANIM_OFF);
    lv_roller_set_selected(s_time_roller_m, (uint32_t)s_create_schedule_ctx.time_minute, LV_ANIM_OFF);
    lv_roller_set_selected(s_time_roller_ap, s_create_schedule_ctx.time_pm ? 1 : 0, LV_ANIM_OFF);
    lv_obj_move_foreground(s_time_picker_overlay);
}

static void ui_create_schedule_time_picker_done_cb(lv_event_t *e)
{
    (void)e;
    if (s_time_roller_h && s_time_roller_m && s_time_roller_ap) {
        s_create_schedule_ctx.time_hour = (int)lv_roller_get_selected(s_time_roller_h) + 1;
        s_create_schedule_ctx.time_minute = (int)lv_roller_get_selected(s_time_roller_m);
        s_create_schedule_ctx.time_pm = lv_roller_get_selected(s_time_roller_ap) == 1;
        ui_create_schedule_update_time_display();
    }
    if (s_time_picker_overlay) {
        lv_obj_add_flag(s_time_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_create_schedule_time_picker_cancel_cb(lv_event_t *e)
{
    (void)e;
    if (s_time_picker_overlay) {
        lv_obj_add_flag(s_time_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

/* ---------- Back / Save / Time / Repeat / Actions ---------- */
static void ui_create_schedule_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (ui_Screen_Schedules) {
        lv_scr_load(ui_Screen_Schedules);
        ui_Screen_Schedules_show();
    }
}

static void ui_create_schedule_time_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_create_schedule_show_time_picker();
}

static void ui_create_schedule_day_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *target = lv_event_get_target(e);
    for (int i = 0; i < 7; i++) {
        if (s_day_btns[i] == target) {
            s_create_schedule_ctx.repeat_days ^= (1u << i);
            ui_create_schedule_refresh_repeat_style();
            return;
        }
    }
}

static void ui_create_schedule_actions_plus_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_Screen_SelectDevices_show(s_create_schedule_ctx.schedule_item);
}

#define CREATE_SCHEDULE_HEADER_H  56
#define CREATE_SCHEDULE_KB_H      130

static void ui_create_schedule_keyboard_show(void)
{
    if (!s_create_schedule_kb || !s_content || !s_name_ta) {
        return;
    }
    lv_obj_t *body = lv_obj_get_parent(s_content);
    if (!body || body == ui_Screen_CreateSchedule) {
        return;
    }
    /* Shrink scrollable area so it ends above the keyboard */
    int screen_h = lv_obj_get_height(ui_Screen_CreateSchedule);
    lv_obj_set_height(body, screen_h - CREATE_SCHEDULE_HEADER_H - CREATE_SCHEDULE_KB_H);
    lv_obj_update_layout(s_create_schedule_kb);
    lv_obj_update_layout(body);
    /* Scroll form area (s_content) to top so name block is in view */
    lv_obj_scroll_to_y(s_content, 0, LV_ANIM_OFF);
    lv_obj_update_layout(body);

    /* If input is still covered by keyboard, scroll up by the overlap amount */
    lv_area_t kb_a;
    lv_area_t ta_a;
    lv_obj_get_coords(s_create_schedule_kb, &kb_a);
    lv_obj_get_coords(s_name_ta, &ta_a);
    const int32_t margin = 12;
    int32_t overlap = ta_a.y2 - (kb_a.y1 - margin);
    if (overlap > 0) {
        int32_t cur = lv_obj_get_scroll_y(s_content);
        lv_obj_scroll_to_y(s_content, cur + overlap, LV_ANIM_OFF);
    }
}

static void ui_create_schedule_keyboard_hide(void)
{
    if (!s_content || !ui_Screen_CreateSchedule) {
        return;
    }
    lv_obj_t *body = lv_obj_get_parent(s_content);
    if (body && body != ui_Screen_CreateSchedule) {
        lv_coord_t sh = lv_obj_get_height(ui_Screen_CreateSchedule);
        lv_obj_set_height(body, LV_MAX(40, sh - CREATE_SCHEDULE_HEADER_H));
    }
}

static void ui_create_schedule_kb_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (s_create_schedule_kb) {
            /* On READY, sync name from textarea to s_create_schedule_ctx.schedule_item->name before detach */
            if (code == LV_EVENT_READY && s_name_ta && s_create_schedule_ctx.schedule_item) {
                lv_obj_t *bound_ta = lv_keyboard_get_textarea(s_create_schedule_kb);
                if (bound_ta == s_name_ta) {
                    const char *name = lv_textarea_get_text(s_name_ta);
                    char *new_name = (name && name[0] != '\0') ? strdup(name) : strdup("");
                    if (new_name) {
                        rainmaker_app_backend_util_safe_free((void *)s_create_schedule_ctx.schedule_item->name);
                        s_create_schedule_ctx.schedule_item->name = new_name;
                    }
                }
            }
            lv_keyboard_set_textarea(s_create_schedule_kb, NULL);
            lv_obj_add_flag(s_create_schedule_kb, LV_OBJ_FLAG_HIDDEN);
            ui_create_schedule_keyboard_hide();
        }
    }
}

static void ui_create_schedule_name_ta_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    if (code == LV_EVENT_FOCUSED && s_create_schedule_kb) {
        lv_event_stop_bubbling(e);
        lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
        lv_keyboard_set_textarea(s_create_schedule_kb, ta);
        lv_obj_clear_flag(s_create_schedule_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_align(s_create_schedule_kb, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(s_create_schedule_kb, -16);
        lv_obj_move_foreground(s_create_schedule_kb);
        ui_create_schedule_keyboard_show();
    }
}

static void ui_create_schedule_pencil_btn_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_name_ta && s_create_schedule_kb) {
        lv_obj_add_state(s_name_ta, LV_STATE_FOCUSED);
        lv_textarea_set_cursor_pos(s_name_ta, LV_TEXTAREA_CURSOR_LAST);
        lv_keyboard_set_textarea(s_create_schedule_kb, s_name_ta);
        lv_obj_clear_flag(s_create_schedule_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_align(s_create_schedule_kb, LV_ALIGN_BOTTOM_MID);
        lv_obj_set_y(s_create_schedule_kb, -16);
        lv_obj_move_foreground(s_create_schedule_kb);
        ui_create_schedule_keyboard_show();
    }
}

static void ui_create_schedule_errbuf_append(char *buf, size_t buf_sz, const char *line)
{
    if (!line || !line[0] || buf_sz == 0) {
        return;
    }
    size_t len = strlen(buf);
    if (len > 0 && len + 1 < buf_sz) {
        buf[len++] = '\n';
        buf[len] = '\0';
    }
    strncat(buf, line, buf_sz - len - 1);
    buf[buf_sz - 1] = '\0';
}

static void ui_create_schedule_save_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_create_schedule_ctx.schedule_item) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCHEDULE_ITEM_EMPTY));
        return;
    }
    struct rm_list_item *schedule_item = s_create_schedule_ctx.schedule_item;

    const char *name = s_name_ta ? lv_textarea_get_text(s_name_ta) : NULL;
    if (!name || name[0] == '\0') {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCHEDULE_NAME_EMPTY));
        return;
    }
    char *new_name = strdup(name);
    if (!new_name) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_NO_MEMORY));
        return;
    }
    rainmaker_app_backend_util_safe_free((void *)schedule_item->name);
    schedule_item->name = new_name;

    int candidates = 0;
    bool any_unavailable = false;
    bool any_real_action = false;
    int count = rm_list_manager_get_node_actions_count(schedule_item);
    for (int i = 0; i < count; i++) {
        rm_node_action_item_t *item = rm_list_manager_get_node_action_item_by_index(schedule_item, i);
        if (!item || !item->node_id || item->type == RM_NODE_ACTION_DELETE) {
            continue;
        }
        candidates++;
        rm_device_item_handle_t handle = rm_app_backend_get_device_handle_by_node_id(item->node_id);
        if (!handle) {
            any_unavailable = true;
            continue;
        }
        const char *act = item->action;
        if (act && act[0] && !rm_list_manager_action_is_placeholder(act)) {
            any_real_action = true;
        }
    }

    const bool fail_no_device = (candidates == 0);
    const bool fail_unavail = any_unavailable;
    const bool fail_no_action = (candidates > 0 && !any_real_action);

    if (fail_no_device || fail_unavail || fail_no_action) {
        char errbuf[768];
        errbuf[0] = '\0';
        if (fail_no_device) {
            ui_create_schedule_errbuf_append(errbuf, sizeof(errbuf), ui_str(UI_STR_SCHEDULE_ERR_NO_DEVICE));
        }
        if (fail_unavail) {
            ui_create_schedule_errbuf_append(errbuf, sizeof(errbuf), ui_str(UI_STR_SCHEDULE_ERR_DEVICE_UNAVAILABLE));
        }
        if (fail_no_action) {
            ui_create_schedule_errbuf_append(errbuf, sizeof(errbuf), ui_str(UI_STR_SCHEDULE_ERR_NO_VALID_ACTION));
        }
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), errbuf);
        return;
    }

    /* Create and edit schedule both will enter this function */
    esp_err_t err = rm_list_manager_update_item_to_backend(schedule_item);
    if (err != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_SCHEDULE_SAVE));
        return;
    }
    /* Refresh the schedule list */
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    /* GET /nodes may still return stale schedule name after successful PUT */
    rm_list_manager_merge_saved_item_into_canonical(s_create_schedule_ctx.schedule_item);
    ui_clean_create_schedule_ctx();
    if (ui_Screen_Schedules) {
        lv_scr_load(ui_Screen_Schedules);
        ui_Screen_Schedules_show();
    }
}

static void ui_create_schedule_delete_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    esp_err_t err = rm_list_manager_delete_item_from_backend(s_create_schedule_ctx.schedule_item);
    if (err != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_SCHEDULE_DELETE_FROM));
        return;
    }
    rainmaker_app_backend_util_vTaskDelay(500);
    rm_app_backend_refresh_home_screen(false);
    ui_Screen_Home_reload_device_cards();
    ui_clean_create_schedule_ctx();
    if (ui_Screen_Schedules) {
        lv_scr_load(ui_Screen_Schedules);
        ui_Screen_Schedules_show();
    }
}

static void ui_create_schedule_set_primary_btn_text(void)
{
    if (!s_primary_btn_label) {
        return;
    }
    /* Plain text: LV_SYMBOL_OK + body/CJK font shows □ */
    lv_label_set_text(s_primary_btn_label,
                      ui_str(s_create_schedule_ctx.is_edit ? UI_STR_UPDATE : UI_STR_SAVE));
    lv_obj_set_style_text_font(s_primary_btn_label, ui_font_cjk_always(), 0);
}

static void ui_create_schedule_apply_mode(void)
{
    if (s_header_title) {
        lv_label_set_text(s_header_title,
                          ui_str(s_create_schedule_ctx.is_edit ? UI_STR_EDIT_SCHEDULE : UI_STR_CREATE_SCHEDULE));
        lv_obj_set_style_text_font(s_header_title, ui_font_title(), 0);
    }
    ui_create_schedule_set_primary_btn_text();
    if (s_delete_btn) {
        if (s_create_schedule_ctx.is_edit) {
            lv_obj_clear_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);
        }
    }
    /* Refresh layout so scroll range includes Save/Delete when in edit mode */
    if (s_content) {
        lv_obj_update_layout(s_content);
        lv_obj_t *body = lv_obj_get_parent(s_content);
        if (body && body != ui_Screen_CreateSchedule) {
            lv_obj_update_layout(body);
        }
    }
}

/* ---------- Header ---------- */
static lv_obj_t *ui_create_schedule_create_header(lv_obj_t *parent)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(bar, lv_pct(100));
    lv_obj_set_height(bar, 56);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xF5F8FF), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_left(bar, 8, 0);
    lv_obj_set_style_pad_right(bar, 8, 0);

    lv_obj_t *back = lv_btn_create(bar);
    lv_obj_clear_flag(back, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(back, 40, 40);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_bg_opa(back, LV_OPA_0, 0);
    lv_obj_set_style_border_width(back, 0, 0);
    lv_obj_add_event_cb(back, ui_create_schedule_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_arrow = lv_label_create(back);
    lv_label_set_text(back_arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(back_arrow);

    s_header_title = lv_label_create(bar);
    lv_label_set_text(s_header_title, ui_str(UI_STR_CREATE_SCHEDULE));
    lv_obj_set_style_text_font(s_header_title, ui_font_title(), 0);
    lv_obj_set_style_text_color(s_header_title, lv_color_hex(0x1A1A1A), 0);
    lv_obj_align(s_header_title, LV_ALIGN_CENTER, 0, 0);

    return bar;
}

/* ---------- Content: scroll_mid (form) + Save/Delete fixed at bottom of body ---------- */
static void ui_create_schedule_create_content(lv_obj_t *body_outer)
{
    lv_obj_t *scroll_mid = lv_obj_create(body_outer);
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

    /* Schedule Name card: container with border; pencil opens keyboard */
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
    lv_label_set_text(name_lab, ui_str(UI_STR_SCHEDULE_NAME));
    lv_obj_set_style_text_font(name_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(name_lab, lv_color_hex(0x4A5568), 0);
    lv_obj_align(name_lab, LV_ALIGN_TOP_LEFT, 0, 0);

    /* Divider between Schedule Name title and name input */
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
    lv_textarea_set_placeholder_text(s_name_ta, ui_str(UI_STR_SCHEDULE_NAME));
    lv_textarea_set_max_length(s_name_ta, 64);
    lv_obj_align_to(s_name_ta, sep_inside_name, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 4);
    lv_obj_set_style_bg_opa(s_name_ta, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_name_ta, 0, 0);
    lv_obj_add_flag(s_name_ta, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_name_ta, ui_create_schedule_name_ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t *pencil_btn = lv_btn_create(name_card);
    lv_obj_clear_flag(pencil_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(pencil_btn, 28, 28);
    lv_obj_align_to(pencil_btn, s_name_ta, LV_ALIGN_OUT_RIGHT_MID, -36, 0);
    lv_obj_set_style_bg_opa(pencil_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(pencil_btn, 0, 0);
    lv_obj_add_event_cb(pencil_btn, ui_create_schedule_pencil_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pencil = lv_label_create(pencil_btn);
    lv_label_set_text(pencil, LV_SYMBOL_EDIT);
    lv_obj_set_style_text_color(pencil, lv_color_hex(0x8AA0C8), 0);
    lv_obj_center(pencil);

    /* Time row (no divider between name card and time) */
    lv_obj_t *time_row = lv_obj_create(s_content);
    lv_obj_clear_flag(time_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(time_row, lv_pct(100), 36);
    lv_obj_set_style_bg_opa(time_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(time_row, 0, 0);
    lv_obj_set_flex_flow(time_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *time_lab = lv_label_create(time_row);
    s_time_row_lab = time_lab;
    lv_label_set_text(time_lab, ui_str(UI_STR_TIME));
    lv_obj_set_style_text_font(time_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(time_lab, lv_color_hex(0x1A1A1A), 0);

    s_time_btn = lv_btn_create(time_row);
    lv_obj_clear_flag(s_time_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(s_time_btn, 32);
    lv_obj_set_style_min_width(s_time_btn, 88, 0);
    lv_obj_set_style_radius(s_time_btn, 18, 0);
    lv_obj_set_style_bg_color(s_time_btn, lv_color_hex(0xE0ECFA), 0);
    lv_obj_set_style_bg_color(s_time_btn, lv_color_hex(0xD0DCE8), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_time_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_time_btn, 0, 0);
    lv_obj_add_event_cb(s_time_btn, ui_create_schedule_time_btn_cb, LV_EVENT_CLICKED, NULL);
    s_time_label = lv_label_create(s_time_btn);
    lv_obj_set_style_text_color(s_time_label, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(s_time_label);

    /* Repeat row */
    lv_obj_t *sep2 = lv_obj_create(s_content);
    lv_obj_clear_flag(sep2, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sep2, lv_pct(100), 1);
    lv_obj_set_style_bg_color(sep2, lv_color_hex(0xE0E8F0), 0);
    lv_obj_set_style_border_width(sep2, 0, 0);

    lv_obj_t *repeat_row = lv_obj_create(s_content);
    lv_obj_clear_flag(repeat_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(repeat_row, lv_pct(100), 38);
    lv_obj_set_style_bg_opa(repeat_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(repeat_row, 0, 0);
    lv_obj_set_flex_flow(repeat_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(repeat_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *repeat_lab = lv_label_create(repeat_row);
    s_repeat_row_lab = repeat_lab;
    lv_label_set_text(repeat_lab, ui_str(UI_STR_REPEAT));
    lv_obj_set_style_text_font(repeat_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(repeat_lab, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t *days_cont = lv_obj_create(repeat_row);
    lv_obj_clear_flag(days_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_height(days_cont, 24);
    lv_obj_set_style_bg_opa(days_cont, LV_OPA_0, 0);
    lv_obj_set_style_border_width(days_cont, 0, 0);
    lv_obj_set_flex_flow(days_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(days_cont, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(days_cont, 2, 0);
    lv_obj_set_flex_grow(days_cont, 1);

    for (int i = 0; i < 7; i++) {
        s_day_btns[i] = lv_btn_create(days_cont);
        lv_obj_clear_flag(s_day_btns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_day_btns[i], 28, 28);
        lv_obj_set_style_radius(s_day_btns[i], 14, 0);
        lv_obj_set_style_bg_color(s_day_btns[i], lv_color_hex(0xE8EEFA), 0);
        lv_obj_set_style_border_width(s_day_btns[i], 0, 0);
        lv_obj_set_style_shadow_width(s_day_btns[i], 0, 0);
        lv_obj_add_event_cb(s_day_btns[i], ui_create_schedule_day_btn_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *day_lab = lv_label_create(s_day_btns[i]);
        lv_label_set_text(day_lab, s_day_labels[i]);
        lv_obj_set_style_text_font(day_lab, ui_font_nav(), 0);
        lv_obj_set_style_text_color(day_lab, lv_color_hex(0x6B7A99), 0);
        lv_obj_center(day_lab);
    }
    ui_create_schedule_refresh_repeat_style();

    /* Actions row */
    lv_obj_t *sep3 = lv_obj_create(s_content);
    lv_obj_clear_flag(sep3, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(sep3, lv_pct(100), 1);
    lv_obj_set_style_bg_color(sep3, lv_color_hex(0xE0E8F0), 0);
    lv_obj_set_style_border_width(sep3, 0, 0);

    lv_obj_t *actions_row = lv_obj_create(s_content);
    lv_obj_clear_flag(actions_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(actions_row, lv_pct(100), 36);
    lv_obj_set_style_bg_opa(actions_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(actions_row, 0, 0);
    lv_obj_set_flex_flow(actions_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(actions_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *actions_lab = lv_label_create(actions_row);
    s_actions_row_lab = actions_lab;
    lv_label_set_text(actions_lab, ui_str(UI_STR_ACTIONS));
    lv_obj_set_style_text_font(actions_lab, ui_font_body(), 0);
    lv_obj_set_style_text_color(actions_lab, lv_color_hex(0x1A1A1A), 0);

    s_actions_plus_btn = lv_btn_create(actions_row);
    lv_obj_clear_flag(s_actions_plus_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_actions_plus_btn, 32, 32);
    lv_obj_set_style_radius(s_actions_plus_btn, 16, 0);
    lv_obj_set_style_bg_color(s_actions_plus_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(s_actions_plus_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_actions_plus_btn, 0, 0);
    lv_obj_add_event_cb(s_actions_plus_btn, ui_create_schedule_actions_plus_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *plus_lab = lv_label_create(s_actions_plus_btn);
    lv_label_set_text(plus_lab, LV_SYMBOL_PLUS);
    lv_obj_set_style_text_color(plus_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(plus_lab, &lv_font_montserrat_18, 0);
    lv_obj_center(plus_lab);

    /* No Actions Selected placeholder */
    s_actions_empty_panel = lv_obj_create(s_content);
    lv_obj_clear_flag(s_actions_empty_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_actions_empty_panel, lv_pct(100));
    lv_obj_set_style_min_height(s_actions_empty_panel, 140, 0);
    lv_obj_set_style_bg_opa(s_actions_empty_panel, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_actions_empty_panel, 0, 0);
    lv_obj_set_style_pad_top(s_actions_empty_panel, 20, 0);
    lv_obj_set_flex_flow(s_actions_empty_panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_actions_empty_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(s_actions_empty_panel, 10, 0);

    lv_obj_t *gear_circle = lv_obj_create(s_actions_empty_panel);
    lv_obj_clear_flag(gear_circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(gear_circle, 56, 56);
    lv_obj_set_style_radius(gear_circle, 28, 0);
    lv_obj_set_style_bg_color(gear_circle, lv_color_hex(0xE8EEFA), 0);
    lv_obj_set_style_border_width(gear_circle, 0, 0);
    lv_obj_t *gear_icon = lv_label_create(gear_circle);
    lv_label_set_text(gear_icon, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_icon, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_text_font(gear_icon, &lv_font_montserrat_24, 0);
    lv_obj_center(gear_icon);

    lv_obj_t *no_act = lv_label_create(s_actions_empty_panel);
    s_no_act_lab = no_act;
    lv_label_set_text(no_act, ui_str(UI_STR_NO_ACTIONS_SELECTED));
    lv_obj_set_style_text_font(no_act, ui_font_body(), 0);
    lv_obj_set_style_text_color(no_act, lv_color_hex(0x1A1A1A), 0);

    lv_obj_t *hint = lv_label_create(s_actions_empty_panel);
    s_actions_hint_lab = hint;
    lv_label_set_text(hint, ui_str(UI_STR_SCHEDULE_ACTIONS_HINT));
    lv_obj_set_style_text_font(hint, ui_font_body(), 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6B7A99), 0);
    lv_obj_set_width(hint, lv_pct(90));
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);

    ui_create_schedule_refresh_actions_empty();

    /* Actions list (cards) */
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

    /* Save/Delete inside scroll (same flow as Create Scene — not fixed to screen bottom) */
    s_save_btn = lv_btn_create(s_content);
    lv_obj_clear_flag(s_save_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(s_save_btn, lv_pct(100));
    lv_obj_set_height(s_save_btn, ui_display_scale_px(48));
    lv_obj_set_style_radius(s_save_btn, 12, 0);
    lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(s_save_btn, 0, 0);
    lv_obj_set_style_shadow_width(s_save_btn, 0, 0);
    lv_obj_add_event_cb(s_save_btn, ui_create_schedule_save_cb, LV_EVENT_CLICKED, NULL);
    s_primary_btn_label = lv_label_create(s_save_btn);
    ui_create_schedule_set_primary_btn_text();
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
    lv_obj_add_event_cb(s_delete_btn, ui_create_schedule_delete_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *del_lab = lv_label_create(s_delete_btn);
    s_del_lab = del_lab;
    lv_label_set_text(del_lab, ui_str(UI_STR_DELETE));
    lv_obj_set_style_text_color(del_lab, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(del_lab, ui_font_cjk_always(), 0);
    lv_obj_center(del_lab);
    lv_obj_add_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);
}

/* ---------- Public show ---------- */
static void ui_create_schedule_set_time_from_minutes(uint16_t minutes)
{
    int h24 = minutes / 60;
    s_create_schedule_ctx.time_pm = (h24 >= 12);
    if (h24 == 0) {
        s_create_schedule_ctx.time_hour = 12;
    } else if (h24 > 12) {
        s_create_schedule_ctx.time_hour = h24 - 12;
    } else {
        s_create_schedule_ctx.time_hour = h24;
    }
    s_create_schedule_ctx.time_minute = minutes % 60;
}

void ui_Screen_CreateSchedule_show(const char *schedule_name)
{
    if (ui_Screen_CreateSchedule == NULL || schedule_name == NULL || schedule_name[0] == '\0') {
        return;
    }

    ui_clean_create_schedule_ctx();
    s_create_schedule_ctx.schedule_item = rm_list_manager_create_new_item_with_name(NULL, schedule_name, RM_LIST_ENTITY_TYPE_SCHEDULE);
    if (s_create_schedule_ctx.schedule_item == NULL) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_SCHEDULE_CREATE_NEW));
        return;
    }

    if (s_name_ta) {
        lv_textarea_set_text(s_name_ta, schedule_name);
    }

    /* Set initial time from system (once per show) */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t) {
        int h24 = t->tm_hour;
        s_create_schedule_ctx.time_pm = (h24 >= 12);
        if (h24 == 0) {
            s_create_schedule_ctx.time_hour = 12;
        } else if (h24 > 12) {
            s_create_schedule_ctx.time_hour = h24 - 12;
        } else {
            s_create_schedule_ctx.time_hour = h24;
        }
        s_create_schedule_ctx.time_minute = t->tm_min;
    }
    ui_create_schedule_update_time_display();
    ui_create_schedule_refresh_repeat_style();
    ui_Screen_CreateSchedule_refresh_actions();
    if (s_time_picker_overlay) {
        lv_obj_add_flag(s_time_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    ui_create_schedule_apply_mode();
    if (s_delete_btn) {
        lv_obj_add_flag(s_delete_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_save_btn) {
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2E6BE6), 0);
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    }
    lv_scr_load(ui_Screen_CreateSchedule);
}

void ui_Screen_CreateSchedule_show_edit(const char *schedule_id)
{
    if (ui_Screen_CreateSchedule == NULL || schedule_id == NULL || schedule_id[0] == '\0') {
        return;
    }

    ui_clean_create_schedule_ctx();
    s_create_schedule_ctx.schedule_item = rm_list_manager_get_copied_item_by_id(schedule_id);
    if (s_create_schedule_ctx.schedule_item == NULL) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_SCHEDULE_COPY_FAILED));
        return;
    }
    s_create_schedule_ctx.is_edit = true;

    if (s_name_ta) {
        lv_textarea_set_text(s_name_ta, (s_create_schedule_ctx.schedule_item->name ? s_create_schedule_ctx.schedule_item->name : ""));
    }
    ui_create_schedule_set_time_from_minutes(s_create_schedule_ctx.schedule_item->trigger.days_of_week.minutes);
    ui_create_schedule_update_time_display();
    s_create_schedule_ctx.repeat_days = s_create_schedule_ctx.schedule_item->trigger.days_of_week.days_of_week;
    ui_create_schedule_refresh_repeat_style();
    ui_Screen_CreateSchedule_refresh_actions();
    if (s_time_picker_overlay) {
        lv_obj_add_flag(s_time_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    ui_create_schedule_apply_mode();
    if (s_save_btn) {
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2E6BE6), 0);
        lv_obj_set_style_bg_color(s_save_btn, lv_color_hex(0x2559C4), LV_STATE_PRESSED);
    }
    lv_scr_load(ui_Screen_CreateSchedule);
}

void ui_Screen_CreateSchedule_screen_init(void)
{
    ui_Screen_CreateSchedule = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_CreateSchedule, lv_color_hex(0xF5F8FF), 0);
    lv_obj_clear_flag(ui_Screen_CreateSchedule, LV_OBJ_FLAG_SCROLLABLE);

    (void)ui_create_schedule_create_header(ui_Screen_CreateSchedule);

    lv_coord_t scr_h = lv_disp_get_ver_res(lv_disp_get_default());
    if (scr_h <= 0) {
        scr_h = 480;
    }
    /* Outer column: scroll_mid fills area; Save/Delete are inside scroll (not pinned to screen bottom). */
    lv_obj_t *body = lv_obj_create(ui_Screen_CreateSchedule);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_height(body, LV_MAX(40, scr_h - CREATE_SCHEDULE_HEADER_H));
    lv_obj_align(body, LV_ALIGN_TOP_MID, 0, CREATE_SCHEDULE_HEADER_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_0, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_bottom(body, 0, 0);

    ui_create_schedule_create_content(body);

    /* Keyboard for Schedule Name (pencil / focus on textarea) */
    s_create_schedule_kb = lv_keyboard_create(ui_Screen_CreateSchedule);
    lv_obj_set_width(s_create_schedule_kb, lv_pct(100));
    lv_obj_set_height(s_create_schedule_kb, ui_display_scale_px(130));
    lv_obj_add_flag(s_create_schedule_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_create_schedule_kb, ui_create_schedule_kb_event_cb, LV_EVENT_ALL, NULL);

    ui_create_schedule_update_time_display();
}

void ui_Screen_CreateSchedule_screen_destroy(void)
{
    if (ui_Screen_CreateSchedule) {
        lv_obj_del(ui_Screen_CreateSchedule);
    }
    ui_Screen_CreateSchedule = NULL;
    s_content = NULL;
    s_name_ta = NULL;
    s_time_btn = NULL;
    s_time_label = NULL;
    for (int i = 0; i < 7; i++) {
        s_day_btns[i] = NULL;
    }
    s_actions_plus_btn = NULL;
    s_actions_empty_panel = NULL;
    s_actions_list = NULL;
    s_save_btn = NULL;
    s_delete_btn = NULL;
    s_header_title = NULL;
    s_primary_btn_label = NULL;
    s_name_title_lab = NULL;
    s_time_row_lab = NULL;
    s_repeat_row_lab = NULL;
    s_actions_row_lab = NULL;
    s_no_act_lab = NULL;
    s_actions_hint_lab = NULL;
    s_del_lab = NULL;
    s_time_picker_cancel_lab = NULL;
    s_time_picker_done_lab = NULL;
    s_time_picker_overlay = NULL;
    s_time_roller_h = NULL;
    s_time_roller_m = NULL;
    s_time_roller_ap = NULL;
    s_create_schedule_kb = NULL;
    ui_clean_create_schedule_ctx();
}

void ui_Screen_CreateSchedule_apply_language(void)
{
    if (s_name_title_lab) {
        lv_label_set_text(s_name_title_lab, ui_str(UI_STR_SCHEDULE_NAME));
        lv_obj_set_style_text_font(s_name_title_lab, ui_font_body(), 0);
    }
    if (s_name_ta) {
        lv_textarea_set_placeholder_text(s_name_ta, ui_str(UI_STR_SCHEDULE_NAME));
    }
    if (s_time_row_lab) {
        lv_label_set_text(s_time_row_lab, ui_str(UI_STR_TIME));
        lv_obj_set_style_text_font(s_time_row_lab, ui_font_body(), 0);
    }
    if (s_repeat_row_lab) {
        lv_label_set_text(s_repeat_row_lab, ui_str(UI_STR_REPEAT));
        lv_obj_set_style_text_font(s_repeat_row_lab, ui_font_body(), 0);
    }
    if (s_actions_row_lab) {
        lv_label_set_text(s_actions_row_lab, ui_str(UI_STR_ACTIONS));
        lv_obj_set_style_text_font(s_actions_row_lab, ui_font_body(), 0);
    }
    if (s_no_act_lab) {
        lv_label_set_text(s_no_act_lab, ui_str(UI_STR_NO_ACTIONS_SELECTED));
        lv_obj_set_style_text_font(s_no_act_lab, ui_font_body(), 0);
    }
    if (s_actions_hint_lab) {
        lv_label_set_text(s_actions_hint_lab, ui_str(UI_STR_SCHEDULE_ACTIONS_HINT));
        lv_obj_set_style_text_font(s_actions_hint_lab, ui_font_body(), 0);
    }
    if (s_del_lab) {
        lv_label_set_text(s_del_lab, ui_str(UI_STR_DELETE));
        lv_obj_set_style_text_font(s_del_lab, ui_font_cjk_always(), 0);
    }
    if (s_time_picker_cancel_lab) {
        lv_label_set_text(s_time_picker_cancel_lab, ui_str(UI_STR_CANCEL));
        lv_obj_set_style_text_font(s_time_picker_cancel_lab, ui_font_body(), 0);
    }
    if (s_time_picker_done_lab) {
        lv_label_set_text(s_time_picker_done_lab, ui_str(UI_STR_DONE));
        lv_obj_set_style_text_font(s_time_picker_done_lab, ui_font_body(), 0);
    }
    if (s_time_roller_ap) {
        char ap_opts[48];
        snprintf(ap_opts, sizeof(ap_opts), "%s\n%s", ui_str(UI_STR_AM), ui_str(UI_STR_PM));
        uint32_t sel = lv_roller_get_selected(s_time_roller_ap);
        lv_roller_set_options(s_time_roller_ap, ap_opts, LV_ROLLER_MODE_NORMAL);
        lv_roller_set_selected(s_time_roller_ap, sel > 1 ? 1 : sel, LV_ANIM_OFF);
    }
    ui_create_schedule_apply_mode();
    ui_create_schedule_update_time_display();
}
