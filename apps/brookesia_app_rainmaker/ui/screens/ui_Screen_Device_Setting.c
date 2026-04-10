/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Device settings screen: device name, node info, OTA, remove device.
 */

#include "../ui.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_app_backend_util.h"
#include "rainmaker_standard_device.h"
#include "rainmaker_api_handle.h"
#include "rm_platform.h"
#include <string.h>
#include <stdlib.h>

lv_obj_t *ui_Screen_Device_Setting = NULL;

static rm_device_item_handle_t s_device_setting_device = NULL;
static rainmaker_ota_update_info_t s_ota_update_info = {0};
static lv_obj_t *s_device_setting_prev_screen = NULL;
static lv_obj_t *s_device_name_ta = NULL;
static lv_obj_t *s_device_setting_kb = NULL;
static lv_obj_t *s_device_setting_content = NULL;
static int32_t s_device_setting_content_base_y = 56;
static lv_obj_t *s_node_id_value_label = NULL;
static lv_obj_t *s_version_value_label = NULL;
static lv_obj_t *s_timezone_btn = NULL;
static lv_obj_t *s_timezone_value_label = NULL;
static lv_obj_t *s_ota_current_value_label = NULL;
static lv_obj_t *s_ota_new_value_label = NULL;
static lv_obj_t *s_ota_check_btn = NULL;       /* Check for Updates; hidden when Update Now is shown */
static lv_obj_t *s_ota_update_now_btn = NULL;
static lv_obj_t *s_edit_name_btn = NULL;
static lv_obj_t *s_operations_section = NULL;  /* Operations card; visibility by whether_has_system_service */
static lv_obj_t *s_op_reboot_btn = NULL;
static lv_obj_t *s_op_wifi_btn = NULL;
static lv_obj_t *s_op_factory_btn = NULL;
static void ui_device_setting_show_keyboard(lv_obj_t *ta);
static void ui_device_setting_hide_keyboard(void);

/* Timezone picker overlay */
static lv_obj_t *s_tz_picker_overlay = NULL;
static lv_obj_t *s_tz_picker_search_ta = NULL;
static lv_obj_t *s_tz_picker_list = NULL;
static lv_obj_t *s_tz_picker_kb = NULL;
static lv_obj_t *s_devset_title_label = NULL;
static lv_obj_t *s_sec_title_device_name = NULL;
static lv_obj_t *s_sec_title_node_info = NULL;
static lv_obj_t *s_sec_title_operations = NULL;
static lv_obj_t *s_sec_title_ota = NULL;
static lv_obj_t *s_lbl_node_id_caption = NULL;
static lv_obj_t *s_lbl_version_caption = NULL;
static lv_obj_t *s_lbl_timezone_caption = NULL;
static lv_obj_t *s_lbl_reboot = NULL;
static lv_obj_t *s_lbl_wifi_reset = NULL;
static lv_obj_t *s_lbl_factory_reset = NULL;
static lv_obj_t *s_lbl_ota_current = NULL;
static lv_obj_t *s_lbl_ota_new = NULL;
static lv_obj_t *s_ota_check_btn_label = NULL;
static lv_obj_t *s_ota_update_now_label = NULL;
static lv_obj_t *s_remove_device_label = NULL;
static const rainmaker_app_backend_util_tz_db_pair_t *s_tz_db = NULL;
static size_t s_tz_db_count = 0;

typedef void (*ui_device_setting_confirm_fn_t)(void *user_data);

static struct {
    ui_device_setting_confirm_fn_t on_confirm;
    void *user_data;
} s_confirm_ctx;

static void ui_device_setting_confirm_cancel_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *dialog = (lv_obj_t *)lv_event_get_user_data(e);
    if (dialog) {
        lv_obj_del(dialog);
    }
}

static void ui_device_setting_confirm_ok_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    lv_obj_t *dialog = (lv_obj_t *)lv_event_get_user_data(e);
    ui_device_setting_confirm_fn_t fn = s_confirm_ctx.on_confirm;
    void *ud = s_confirm_ctx.user_data;
    if (dialog) {
        lv_obj_del(dialog);
    }
    if (fn) {
        fn(ud);
    }
}

static void ui_device_setting_show_confirm(const char *title, const char *message,
        ui_device_setting_confirm_fn_t on_confirm, void *user_data)
{
    if (title == NULL) {
        title = ui_str(UI_STR_CONFIRM);
    }
    if (message == NULL || message[0] == '\0') {
        message = ui_str(UI_STR_CONTINUE);
    }
    s_confirm_ctx.on_confirm = on_confirm;
    s_confirm_ctx.user_data = user_data;

    /* Full-screen overlay (dimmed) */
    lv_obj_t *dialog = lv_obj_create(lv_scr_act());
    lv_obj_clear_flag(dialog, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(dialog, lv_pct(100), lv_pct(100));
    lv_obj_align(dialog, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(dialog, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(dialog, LV_OPA_50, 0);
    lv_obj_set_style_border_width(dialog, 0, 0);
    lv_obj_set_style_pad_all(dialog, 0, 0);
    lv_obj_move_foreground(dialog);

    /* Content box: white, rounded — use % width so 1024×600 panels are not stuck at 320px */
    lv_obj_t *content = lv_obj_create(dialog);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(content, lv_pct(88));
    lv_obj_set_style_max_width(content, ui_display_scale_px(400), 0);
    lv_obj_set_height(content, LV_SIZE_CONTENT);
    lv_obj_align(content, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(content, ui_display_scale_px(20), 0);
    lv_obj_set_style_bg_color(content, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(content, 0, 0);
    lv_obj_set_style_pad_all(content, ui_display_scale_px(24), 0);
    lv_obj_set_style_pad_row(content, ui_display_scale_px(16), 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Title: centered */
    lv_obj_t *title_label = lv_label_create(content);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x2D3748), 0);
    lv_obj_set_style_text_font(title_label, ui_font_title(), 0);

    /* Message: centered, wrap */
    lv_obj_t *msg_label = lv_label_create(content);
    lv_label_set_text(msg_label, message);
    lv_label_set_long_mode(msg_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(msg_label, lv_pct(100));
    lv_obj_set_style_text_color(msg_label, lv_color_hex(0x4A5568), 0);
    lv_obj_set_style_text_font(msg_label, ui_font_body(), 0);

    /* Button row: two containers, Cancel (left) + Confirm (right) */
    lv_obj_t *btn_row = lv_obj_create(content);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, ui_display_scale_px(44));
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_0, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_style_pad_column(btn_row, ui_display_scale_px(12), 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Cancel: container + button (light grey-blue) */
    lv_obj_t *cancel_cont = lv_obj_create(btn_row);
    lv_obj_clear_flag(cancel_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_grow(cancel_cont, 1);
    lv_obj_set_height(cancel_cont, ui_display_scale_px(44));
    lv_obj_set_style_radius(cancel_cont, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(cancel_cont, lv_color_hex(0xC9D6EE), 0);
    lv_obj_set_style_border_width(cancel_cont, 0, 0);
    lv_obj_set_style_pad_all(cancel_cont, 0, 0);

    lv_obj_t *cancel_btn = lv_btn_create(cancel_cont);
    lv_obj_clear_flag(cancel_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(cancel_btn, lv_pct(100), lv_pct(100));
    lv_obj_align(cancel_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(cancel_btn, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(cancel_btn, lv_color_hex(0xC9D6EE), 0);
    lv_obj_set_style_border_width(cancel_btn, 0, 0);
    lv_obj_set_style_shadow_width(cancel_btn, 0, 0);
    lv_obj_add_event_cb(cancel_btn, ui_device_setting_confirm_cancel_cb, LV_EVENT_CLICKED, dialog);
    lv_obj_t *cancel_txt = lv_label_create(cancel_btn);
    lv_label_set_text(cancel_txt, ui_str(UI_STR_CANCEL));
    lv_obj_set_style_text_font(cancel_txt, ui_font_body(), 0);
    lv_obj_set_style_text_color(cancel_txt, lv_color_hex(0x2D3748), 0);
    lv_obj_center(cancel_txt);

    /* Confirm: container + button (dark blue) */
    lv_obj_t *confirm_cont = lv_obj_create(btn_row);
    lv_obj_clear_flag(confirm_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_grow(confirm_cont, 1);
    lv_obj_set_height(confirm_cont, ui_display_scale_px(44));
    lv_obj_set_style_radius(confirm_cont, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(confirm_cont, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(confirm_cont, 0, 0);
    lv_obj_set_style_pad_all(confirm_cont, 0, 0);

    lv_obj_t *confirm_btn = lv_btn_create(confirm_cont);
    lv_obj_clear_flag(confirm_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(confirm_btn, lv_pct(100), lv_pct(100));
    lv_obj_align(confirm_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(confirm_btn, ui_display_scale_px(12), 0);
    lv_obj_set_style_bg_color(confirm_btn, lv_color_hex(0x2E6BE6), 0);
    lv_obj_set_style_border_width(confirm_btn, 0, 0);
    lv_obj_set_style_shadow_width(confirm_btn, 0, 0);
    lv_obj_add_event_cb(confirm_btn, ui_device_setting_confirm_ok_cb, LV_EVENT_CLICKED, dialog);
    lv_obj_t *confirm_txt = lv_label_create(confirm_btn);
    lv_label_set_text(confirm_txt, ui_str(UI_STR_CONFIRM));
    lv_obj_set_style_text_font(confirm_txt, ui_font_body(), 0);
    lv_obj_set_style_text_color(confirm_txt, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(confirm_txt);
}

static void ui_device_setting_refresh_from_device(void)
{
    if (!s_device_setting_device) {
        return;
    }
    if (!rm_device_group_manager_is_device_in_list((const rm_device_item_t *)s_device_setting_device)) {
        s_device_setting_device = NULL;
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_device_setting_device;
    if (s_device_name_ta) {
        lv_textarea_set_text(s_device_name_ta, dev->name ? dev->name : ui_str(UI_STR_DEVICE));
        lv_obj_set_style_text_font(s_device_name_ta, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
    }
    if (s_node_id_value_label) {
        lv_label_set_text(s_node_id_value_label, dev->node_id ? dev->node_id : "--");
    }
    if (s_version_value_label) {
        lv_label_set_text(s_version_value_label, dev->fw_version ? dev->fw_version : "--");
    }
    if (s_ota_current_value_label) {
        lv_label_set_text(s_ota_current_value_label, dev->fw_version ? dev->fw_version : "--");
    }
    if (s_ota_new_value_label) {
        lv_label_set_text(s_ota_new_value_label, "--");
    }
    if (s_ota_check_btn) {
        lv_obj_clear_flag(s_ota_check_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_ota_update_now_btn) {
        lv_obj_add_flag(s_ota_update_now_btn, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_operations_section) {
        if (dev->whether_has_system_service) {
            lv_obj_clear_flag(s_operations_section, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_operations_section, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_timezone_value_label) {
        lv_label_set_text(s_timezone_value_label, dev->timezone ? dev->timezone : ui_str(UI_STR_SELECT_TIMEZONE));
    }

    /* Apply offline state: disable and gray out Device Name edit, Timezone, and 3 Operations buttons */
    bool online = dev->online;
    if (s_edit_name_btn) {
        if (online) {
            lv_obj_add_flag(s_edit_name_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *icon = lv_obj_get_child(s_edit_name_btn, 0);
            if (icon) {
                lv_obj_set_style_text_color(icon, lv_color_hex(0x8AA0C8), 0);
            }
        } else {
            lv_obj_clear_flag(s_edit_name_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_t *icon = lv_obj_get_child(s_edit_name_btn, 0);
            if (icon) {
                lv_obj_set_style_text_color(icon, lv_color_hex(0xB0BEC5), 0);
            }
        }
    }
    if (s_timezone_btn) {
        if (online) {
            lv_obj_add_flag(s_timezone_btn, LV_OBJ_FLAG_CLICKABLE);
            if (s_timezone_value_label) {
                lv_obj_set_style_text_color(s_timezone_value_label, lv_color_hex(0x4A5568), 0);
            }
            lv_obj_t *arrow = lv_obj_get_child(s_timezone_btn, 1);
            if (arrow) {
                lv_obj_set_style_text_color(arrow, lv_color_hex(0x8AA0C8), 0);
            }
        } else {
            lv_obj_clear_flag(s_timezone_btn, LV_OBJ_FLAG_CLICKABLE);
            if (s_timezone_value_label) {
                lv_obj_set_style_text_color(s_timezone_value_label, lv_color_hex(0x9CA3AF), 0);
            }
            lv_obj_t *arrow = lv_obj_get_child(s_timezone_btn, 1);
            if (arrow) {
                lv_obj_set_style_text_color(arrow, lv_color_hex(0x9CA3AF), 0);
            }
        }
    }
    if (s_op_reboot_btn) {
        if (online) {
            lv_obj_add_flag(s_op_reboot_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_op_reboot_btn, lv_color_hex(0x2C5AA0), 0);
            lv_obj_t *lab = lv_obj_get_child(s_op_reboot_btn, 0);
            if (lab) {
                lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
            }
        } else {
            lv_obj_clear_flag(s_op_reboot_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_op_reboot_btn, lv_color_hex(0x9CA3AF), 0);
            lv_obj_t *lab = lv_obj_get_child(s_op_reboot_btn, 0);
            if (lab) {
                lv_obj_set_style_text_color(lab, lv_color_hex(0x6B7280), 0);
            }
        }
    }
    if (s_op_wifi_btn) {
        if (online) {
            lv_obj_add_flag(s_op_wifi_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_op_wifi_btn, lv_color_hex(0xE74C3C), 0);
            lv_obj_set_style_bg_opa(s_op_wifi_btn, LV_OPA_COVER, 0);
            lv_obj_t *lab = lv_obj_get_child(s_op_wifi_btn, 0);
            if (lab) {
                lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
            }
        } else {
            lv_obj_clear_flag(s_op_wifi_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_op_wifi_btn, lv_color_hex(0x9CA3AF), 0);
            lv_obj_t *lab = lv_obj_get_child(s_op_wifi_btn, 0);
            if (lab) {
                lv_obj_set_style_text_color(lab, lv_color_hex(0x6B7280), 0);
            }
        }
    }
    if (s_op_factory_btn) {
        if (online) {
            lv_obj_add_flag(s_op_factory_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_op_factory_btn, lv_color_hex(0xE74C3C), 0);
            lv_obj_set_style_bg_opa(s_op_factory_btn, LV_OPA_COVER, 0);
            lv_obj_t *lab = lv_obj_get_child(s_op_factory_btn, 0);
            if (lab) {
                lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
            }
        } else {
            lv_obj_clear_flag(s_op_factory_btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(s_op_factory_btn, lv_color_hex(0x9CA3AF), 0);
            lv_obj_t *lab = lv_obj_get_child(s_op_factory_btn, 0);
            if (lab) {
                lv_obj_set_style_text_color(lab, lv_color_hex(0x6B7280), 0);
            }
        }
    }
}

static bool ui_device_setting_tz_match(const char *tz, const char *q)
{
    if (!tz) {
        return false;
    }
    if (!q || q[0] == '\0') {
        return true;
    }
    /* case-insensitive substring match */
    size_t tz_len = strlen(tz);
    size_t q_len = strlen(q);
    for (size_t i = 0; i + q_len <= tz_len; i++) {
        size_t j = 0;
        for (; j < q_len; j++) {
            char a = tz[i + j];
            char b = q[j];
            if (a >= 'A' && a <= 'Z') {
                a = (char)(a - 'A' + 'a');
            }
            if (b >= 'A' && b <= 'Z') {
                b = (char)(b - 'A' + 'a');
            }
            if (a != b) {
                break;
            }
        }
        if (j == q_len) {
            return true;
        }
    }
    return false;
}

static void ui_device_setting_tz_picker_close(void)
{
    if (s_tz_picker_overlay) {
        lv_obj_add_flag(s_tz_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_tz_picker_kb) {
        lv_keyboard_set_textarea(s_tz_picker_kb, NULL);
        lv_obj_add_flag(s_tz_picker_kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void ui_device_setting_tz_picker_close_cb(lv_event_t *e)
{
    (void)e;
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_device_setting_tz_picker_close();
}

static void ui_device_setting_tz_picker_item_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    const char *tz = (const char *)lv_event_get_user_data(e);
    if (!tz || !s_device_setting_device) {
        return;
    }
    rm_device_item_t *dev = (rm_device_item_t *)s_device_setting_device;
    /* Update local device timezone string for UI */
    if (dev->timezone) {
        rainmaker_app_backend_util_safe_free(dev->timezone);
        dev->timezone = NULL;
    }
    dev->timezone = strdup(tz);
    if (s_timezone_value_label) {
        lv_label_set_text(s_timezone_value_label, tz);
    }
    const char *posix = rainmaker_app_backend_util_get_tz_posix_str(tz);
    if (posix) {
        const char *err = NULL;
        if (rainmaker_standard_device_set_timezone((rm_device_item_t *)s_device_setting_device, tz, posix, &err) != ESP_OK) {
            ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_SET_TZ));
            free((void *)err);
        } else {
            ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_TZ_SET_OK));
        }
    } else {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_FAILED_SET_TZ));
    }
    ui_device_setting_tz_picker_close();
}

static void ui_device_setting_tz_picker_rebuild(const char *query)
{
    if (!s_tz_picker_list) {
        return;
    }
    lv_obj_clean(s_tz_picker_list);
    if (!s_tz_db) {
        rainmaker_app_backend_util_get_tz_db(&s_tz_db, &s_tz_db_count);
    }
    for (size_t i = 0; i < s_tz_db_count; i++) {
        const char *tz = s_tz_db[i].name;
        if (!ui_device_setting_tz_match(tz, query)) {
            continue;
        }
        lv_obj_t *btn = lv_btn_create(s_tz_picker_list);
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, ui_display_scale_px(36));
        lv_obj_set_style_bg_opa(btn, LV_OPA_0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_pad_left(btn, 8, 0);
        lv_obj_set_style_pad_right(btn, 8, 0);
        lv_obj_add_event_cb(btn, ui_device_setting_tz_picker_item_cb, LV_EVENT_CLICKED, (void *)tz);
        lv_obj_t *lab = lv_label_create(btn);
        lv_label_set_text(lab, tz);
        lv_obj_set_style_text_font(lab, ui_font_body(), 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(0x4A5568), 0);
        lv_obj_align(lab, LV_ALIGN_LEFT_MID, 0, 0);
    }
}

static void ui_device_setting_tz_picker_search_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }
    const char *q = lv_textarea_get_text(s_tz_picker_search_ta);
    ui_device_setting_tz_picker_rebuild(q);
}

static void ui_device_setting_tz_picker_search_focus_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_FOCUSED) {
        return;
    }
    if (!s_tz_picker_kb || !s_tz_picker_search_ta) {
        return;
    }
    lv_keyboard_set_textarea(s_tz_picker_kb, s_tz_picker_search_ta);
    lv_obj_clear_flag(s_tz_picker_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_align(s_tz_picker_kb, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(s_tz_picker_kb, -30);
    lv_obj_move_foreground(s_tz_picker_kb);
}

static void ui_device_setting_tz_picker_kb_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        if (s_tz_picker_kb) {
            lv_keyboard_set_textarea(s_tz_picker_kb, NULL);
            lv_obj_add_flag(s_tz_picker_kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void ui_device_setting_timezone_open_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_device_setting_device) {
        return;
    }
    if (!((rm_device_item_t *)s_device_setting_device)->online) {
        return;
    }
    if (!ui_Screen_Device_Setting) {
        return;
    }
    if (!s_tz_picker_overlay) {
        /* Fullscreen overlay */
        s_tz_picker_overlay = lv_obj_create(ui_Screen_Device_Setting);
        lv_obj_clear_flag(s_tz_picker_overlay, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_tz_picker_overlay, lv_pct(100), lv_pct(100));
        lv_obj_align(s_tz_picker_overlay, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_bg_color(s_tz_picker_overlay, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_border_width(s_tz_picker_overlay, 0, 0);
        lv_obj_move_foreground(s_tz_picker_overlay);

        /* Close button */
        lv_obj_t *close_btn = lv_btn_create(s_tz_picker_overlay);
        lv_obj_clear_flag(close_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(close_btn, ui_display_scale_px(44), ui_display_scale_px(44));
        lv_obj_align(close_btn, LV_ALIGN_TOP_LEFT, 6, 6);
        lv_obj_set_style_bg_opa(close_btn, LV_OPA_0, 0);
        lv_obj_set_style_border_width(close_btn, 0, 0);
        lv_obj_add_event_cb(close_btn, ui_device_setting_tz_picker_close_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *close_lab = lv_label_create(close_btn);
        lv_label_set_text(close_lab, LV_SYMBOL_LEFT);
        lv_obj_set_style_text_font(close_lab, ui_font_montserrat_18(), 0);
        lv_obj_set_style_text_color(close_lab, lv_color_hex(0x2E6BE6), 0);
        lv_obj_center(close_lab);

        /* Search */
        s_tz_picker_search_ta = lv_textarea_create(s_tz_picker_overlay);
        lv_obj_set_width(s_tz_picker_search_ta, lv_pct(92));
        lv_textarea_set_one_line(s_tz_picker_search_ta, true);
        lv_textarea_set_placeholder_text(s_tz_picker_search_ta, ui_str(UI_STR_SEARCH_TIMEZONE));
        lv_obj_set_style_text_font(s_tz_picker_search_ta, ui_font_body(), 0);
        lv_obj_align(s_tz_picker_search_ta, LV_ALIGN_TOP_MID, 0, 18);
        lv_obj_add_event_cb(s_tz_picker_search_ta, ui_device_setting_tz_picker_search_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_event_cb(s_tz_picker_search_ta, ui_device_setting_tz_picker_search_focus_cb, LV_EVENT_FOCUSED, NULL);

        /* List */
        s_tz_picker_list = lv_obj_create(s_tz_picker_overlay);
        lv_obj_add_flag(s_tz_picker_list, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(s_tz_picker_list, lv_pct(100));
        lv_obj_align(s_tz_picker_list, LV_ALIGN_TOP_MID, 0, 54);
        lv_obj_set_height(s_tz_picker_list, lv_obj_get_height(s_tz_picker_overlay) - 54);
        lv_obj_set_style_bg_opa(s_tz_picker_list, LV_OPA_0, 0);
        lv_obj_set_style_border_width(s_tz_picker_list, 0, 0);
        lv_obj_set_style_pad_all(s_tz_picker_list, 12, 0);
        lv_obj_set_style_pad_row(s_tz_picker_list, 8, 0);
        lv_obj_set_flex_flow(s_tz_picker_list, LV_FLEX_FLOW_COLUMN);

        /* Keyboard for search */
        s_tz_picker_kb = lv_keyboard_create(s_tz_picker_overlay);
        lv_obj_add_flag(s_tz_picker_kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_event_cb(s_tz_picker_kb, ui_device_setting_tz_picker_kb_cb, LV_EVENT_ALL, NULL);
    }

    lv_obj_clear_flag(s_tz_picker_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_tz_picker_overlay);
    if (s_tz_picker_list) {
        lv_obj_set_height(s_tz_picker_list, lv_obj_get_height(s_tz_picker_overlay) - 54);
    }
    if (s_tz_picker_search_ta) {
        lv_textarea_set_text(s_tz_picker_search_ta, "");
    }
    ui_device_setting_tz_picker_rebuild(NULL);
}

static void ui_device_setting_back_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_device_setting_prev_screen) {
        lv_scr_load(s_device_setting_prev_screen);
    } else if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
    }
}

static void ui_device_setting_edit_name_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_device_setting_device || !((rm_device_item_t *)s_device_setting_device)->online) {
        return;
    }
    ui_device_setting_show_keyboard(s_device_name_ta);
}

static void ui_device_setting_check_updates_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_ota_new_value_label) {
        memset(&s_ota_update_info, 0, sizeof(rainmaker_ota_update_info_t));
        const char *err = NULL;
        if (rainmaker_api_check_ota_update(((rm_device_item_t *)s_device_setting_device)->node_id, &s_ota_update_info, &err) != ESP_OK) {
            ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_CHECK_UPDATE));
            free((void *)err);
            return;
        }
        if (s_ota_update_info.ota_available) {
            lv_label_set_text(s_ota_new_value_label, s_ota_update_info.new_firmware_version);
            if (s_ota_check_btn) {
                lv_obj_add_flag(s_ota_check_btn, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_ota_update_now_btn) {
                lv_obj_clear_flag(s_ota_update_now_btn, LV_OBJ_FLAG_HIDDEN);
            }
        } else {
            lv_label_set_text(s_ota_new_value_label, ui_str(UI_STR_NO_UPDATES));
            if (s_ota_check_btn) {
                lv_obj_clear_flag(s_ota_check_btn, LV_OBJ_FLAG_HIDDEN);
            }
            if (s_ota_update_now_btn) {
                lv_obj_add_flag(s_ota_update_now_btn, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

static void ui_device_setting_ota_update_now_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (!s_device_setting_device) {
        return;
    }
    if (s_ota_update_info.ota_available) {
        const char *err = NULL;
        if (rainmaker_api_start_ota_update(((rm_device_item_t *)s_device_setting_device)->node_id, s_ota_update_info.ota_job_id, &err) != ESP_OK) {
            ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_OTA));
            free((void *)err);
            return;
        }
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_OTA_STARTED));
    }
}

static void ui_device_setting_do_remove_device(void *unused)
{
    (void)unused;
    if (!s_device_setting_device) {
        return;
    }
    const char *err = NULL;
    /* Factory reset if the device has a system service */
    if (((rm_device_item_t *)s_device_setting_device)->online && ((rm_device_item_t *)s_device_setting_device)->whether_has_system_service) {
        if (rainmaker_standard_device_do_system_service((rm_device_item_t *)s_device_setting_device,
                RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_FACTORY_RESET,
                &err) != ESP_OK) {
            ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_FACTORY));
            free((void *)err);
            return;
        }
    }
    /* Remove the device from the Rainmaker */
    if (rainmaker_api_remove_device(((rm_device_item_t *)s_device_setting_device)->node_id, &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_REMOVE_DEV));
        free((void *)err);
        return;
    }
    ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_DEV_REMOVED_OK));
    const char *node_id = ((rm_device_item_t *)s_device_setting_device)->node_id;
    /* Remove from g_devices_list and all groups' nodes_list (no refresh API) */
    if (rm_device_group_manager_remove_device_by_node_id(node_id) != ESP_OK) {
        /* Still go home; list may be out of sync */
    }
    s_device_setting_device = NULL;
    if (ui_Screen_Home) {
        lv_scr_load(ui_Screen_Home);
        ui_Screen_Home_reload_device_cards();
    }
}

static void ui_device_setting_remove_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_device_setting_show_confirm(
        ui_str(UI_STR_REMOVE_DEVICE),
        ui_str(UI_STR_REMOVE_DEVICE_CONFIRM),
        ui_device_setting_do_remove_device,
        NULL);
}

static void ui_device_setting_hide_keyboard(void)
{
    if (s_device_setting_kb) {
        lv_keyboard_set_textarea(s_device_setting_kb, NULL);
        lv_obj_add_flag(s_device_setting_kb, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_device_setting_content) {
        lv_obj_set_y(s_device_setting_content, s_device_setting_content_base_y);
        lv_obj_add_flag(s_device_setting_content, LV_OBJ_FLAG_SCROLLABLE);
        /* Restore vertical elastic scrolling after keyboard hides */
        lv_obj_set_scroll_dir(s_device_setting_content, LV_DIR_VER);
        lv_obj_set_scroll_snap_y(s_device_setting_content, LV_SCROLL_SNAP_START);
        lv_obj_scroll_to_y(s_device_setting_content, 0, LV_ANIM_OFF);
    }
}

static void ui_device_setting_adjust_for_keyboard(lv_obj_t *ta)
{
    if (!s_device_setting_kb || !s_device_setting_content || !ta) {
        return;
    }
    lv_obj_update_layout(s_device_setting_kb);
    lv_obj_update_layout(ta);
    lv_area_t kb_a;
    lv_area_t ta_a;
    lv_obj_get_coords(s_device_setting_kb, &kb_a);
    lv_obj_get_coords(ta, &ta_a);
    int32_t margin = 6;
    int32_t overlap = ta_a.y2 - (kb_a.y1 - margin);
    if (overlap > 0) {
        lv_obj_set_y(s_device_setting_content, s_device_setting_content_base_y - overlap);
    } else {
        lv_obj_set_y(s_device_setting_content, s_device_setting_content_base_y);
    }
}

static void ui_device_setting_show_keyboard(lv_obj_t *ta)
{
    if (!s_device_setting_kb || !ta) {
        return;
    }
    lv_keyboard_set_textarea(s_device_setting_kb, ta);
    lv_obj_clear_flag(s_device_setting_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_align(s_device_setting_kb, LV_ALIGN_BOTTOM_MID);
    lv_obj_set_y(s_device_setting_kb, -ui_display_scale_px(30));
    lv_obj_move_foreground(s_device_setting_kb);
    lv_textarea_set_cursor_pos(ta, LV_TEXTAREA_CURSOR_LAST);
    lv_obj_add_state(ta, LV_STATE_FOCUSED);
    if (s_device_setting_content) {
        lv_obj_add_flag(s_device_setting_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_scroll_to_view(ta, LV_ANIM_ON);
        /* Disable manual scroll while keyboard is visible */
        lv_obj_clear_flag(s_device_setting_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scroll_dir(s_device_setting_content, LV_DIR_NONE);
    }
    ui_device_setting_adjust_for_keyboard(ta);
}

static void ui_device_setting_keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY) {
        const char *name = s_device_name_ta ? lv_textarea_get_text(s_device_name_ta) : "";
        ui_device_setting_hide_keyboard();
        if (name) {
            const char *err = NULL;
            if (rainmaker_standard_device_set_name((rm_device_item_t *)s_device_setting_device, name, &err) != ESP_OK) {
                ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_DEV_NAME));
                free((void *)err);
                return;
            } else {
                ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_DEV_NAME_OK));
            }
        } else {
            ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), ui_str(UI_STR_INVALID_DEV_NAME));
        }
    } else if (code == LV_EVENT_CANCEL) {
        ui_device_setting_hide_keyboard();
    }
}

static lv_obj_t *ui_device_setting_create_header(lv_obj_t *parent)
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
    lv_obj_add_event_cb(back, ui_device_setting_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_arrow = lv_label_create(back);
    lv_label_set_text(back_arrow, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_font(back_arrow, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(back_arrow, lv_color_hex(0x2E6BE6), 0);
    lv_obj_center(back_arrow);

    lv_obj_t *title = lv_label_create(bar);
    s_devset_title_label = title;
    lv_label_set_text(title, ui_str(UI_STR_DEVICE_SETTINGS));
    lv_obj_set_style_text_font(title, ui_font_title(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    return bar;
}

typedef struct {
    lv_obj_t *body;
    lv_obj_t *arrow_label;
    bool expanded;
} ui_device_setting_section_ctx_t;

static void ui_device_setting_section_delete_cb(lv_event_t *e)
{
    ui_device_setting_section_ctx_t *ctx = (ui_device_setting_section_ctx_t *)lv_event_get_user_data(e);
    if (ctx) {
        UI_LV_FREE(ctx);
    }
}

static void ui_device_setting_section_apply_state(ui_device_setting_section_ctx_t *ctx)
{
    if (!ctx || !ctx->body || !ctx->arrow_label) {
        return;
    }
    if (ctx->expanded) {
        lv_obj_clear_flag(ctx->body, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ctx->arrow_label, LV_SYMBOL_UP);
    } else {
        lv_obj_add_flag(ctx->body, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(ctx->arrow_label, LV_SYMBOL_DOWN);
    }
}

static void ui_device_setting_section_toggle_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_device_setting_section_ctx_t *ctx = (ui_device_setting_section_ctx_t *)lv_event_get_user_data(e);
    if (!ctx) {
        return;
    }
    ctx->expanded = !ctx->expanded;
    ui_device_setting_section_apply_state(ctx);
    /* Recalculate sizes after hide/show */
    if (ctx->body) {
        lv_obj_update_layout(lv_obj_get_parent(ctx->body));
    }
}

static lv_obj_t *ui_device_setting_create_card(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(card, lv_pct(90));
    lv_obj_set_height(card, LV_SIZE_CONTENT);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_shadow_width(card, 12, 0);
    lv_obj_set_style_shadow_color(card, lv_color_hex(0xB0C4EE), 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, 8, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    return card;
}

static lv_obj_t *ui_device_setting_create_section(lv_obj_t *parent, const char *title, bool expanded, lv_obj_t **out_body,
        lv_obj_t **out_title_label)
{
    lv_obj_t *card = ui_device_setting_create_card(parent);

    /* Header row */
    lv_obj_t *hdr = lv_obj_create(card);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(hdr, lv_pct(100));
    lv_obj_set_height(hdr, ui_display_scale_px(28));
    lv_obj_set_style_bg_opa(hdr, LV_OPA_0, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *t = lv_label_create(hdr);
    lv_label_set_text(t, title ? title : "");
    lv_obj_set_style_text_font(t, ui_font_body(), 0);
    lv_obj_set_style_text_color(t, lv_color_hex(0x4A5568), 0);
    if (out_title_label) {
        *out_title_label = t;
    }

    ui_device_setting_section_ctx_t *ctx = (ui_device_setting_section_ctx_t *)UI_LV_MALLOC(sizeof(*ctx));
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
        ctx->expanded = expanded;
    }

    lv_obj_t *toggle_btn = lv_btn_create(hdr);
    lv_obj_clear_flag(toggle_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(toggle_btn, ui_display_scale_px(32), ui_display_scale_px(28));
    lv_obj_set_style_bg_opa(toggle_btn, LV_OPA_0, 0);
    lv_obj_set_style_border_width(toggle_btn, 0, 0);
    lv_obj_set_style_shadow_width(toggle_btn, 0, 0);
    lv_obj_add_event_cb(toggle_btn, ui_device_setting_section_toggle_cb, LV_EVENT_CLICKED, ctx);
    lv_obj_add_event_cb(toggle_btn, ui_device_setting_section_delete_cb, LV_EVENT_DELETE, ctx);

    lv_obj_t *arrow = lv_label_create(toggle_btn);
    lv_obj_set_style_text_font(arrow, ui_font_montserrat_18(), 0);
    lv_obj_set_style_text_color(arrow, lv_color_hex(0x8AA0C8), 0);
    lv_obj_center(arrow);

    /* Body */
    lv_obj_t *body = lv_obj_create(card);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_height(body, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(body, LV_OPA_0, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_pad_row(body, 10, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);

    if (ctx) {
        ctx->body = body;
        ctx->arrow_label = arrow;
        ui_device_setting_section_apply_state(ctx);
    } else {
        /* If alloc failed, keep expanded to avoid hiding content permanently */
        lv_label_set_text(arrow, LV_SYMBOL_UP);
    }

    if (out_body) {
        *out_body = body;
    }
    return card;
}

static void ui_device_setting_do_reboot(void *unused)
{
    (void)unused;
    const char *err = NULL;
    if (rainmaker_standard_device_do_system_service((rm_device_item_t *)s_device_setting_device,
            RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_REBOOT,
            &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_REBOOT));
        free((void *)err);
    } else {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_REBOOT_OK));
    }
}

static void ui_device_setting_op_reboot_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_device_setting_show_confirm(
        ui_str(UI_STR_REBOOT),
        ui_str(UI_STR_REBOOT_CONFIRM),
        ui_device_setting_do_reboot,
        NULL);
}

static void ui_device_setting_do_wifi_reset(void *unused)
{
    (void)unused;
    const char *err = NULL;
    if (rainmaker_standard_device_do_system_service((rm_device_item_t *)s_device_setting_device,
            RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_WIFI_RESET,
            &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_WIFI_RESET));
        free((void *)err);
    } else {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_WIFI_RESET_OK));
    }
}

static void ui_device_setting_op_wifi_reset_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_device_setting_show_confirm(
        ui_str(UI_STR_WIFI_RESET),
        ui_str(UI_STR_WIFI_RESET_CONFIRM),
        ui_device_setting_do_wifi_reset,
        NULL);
}

static void ui_device_setting_do_factory_reset(void *unused)
{
    (void)unused;
    const char *err = NULL;
    if (rainmaker_standard_device_do_system_service((rm_device_item_t *)s_device_setting_device,
            RM_STANDARD_DEVICE_SYSTEM_SERVICE_TYPE_FACTORY_RESET,
            &err) != ESP_OK) {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_ERROR), err ? err : ui_str(UI_STR_FAILED_FACTORY_RESET));
        free((void *)err);
    } else {
        ui_show_msgbox(ui_str(UI_STR_MSGBOX_SUCCESS), ui_str(UI_STR_FACTORY_RESET_OK));
        const char *node_id = ((rm_device_item_t *)s_device_setting_device)->node_id;
        /* Remove from g_devices_list and all groups' nodes_list (no refresh API) */
        if (rm_device_group_manager_remove_device_by_node_id(node_id) != ESP_OK) {
            /* Still go home; list may be out of sync */
        }
        s_device_setting_device = NULL;
        if (ui_Screen_Home) {
            lv_scr_load(ui_Screen_Home);
            ui_Screen_Home_reload_device_cards();
        }
    }
}

static void ui_device_setting_op_factory_reset_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    ui_device_setting_show_confirm(
        ui_str(UI_STR_FACTORY_RESET),
        ui_str(UI_STR_FACTORY_RESET_CONFIRM),
        ui_device_setting_do_factory_reset,
        NULL);
}

static void ui_device_setting_create_content(lv_obj_t *parent)
{
    /* Layout for cards */
    lv_obj_set_style_pad_top(parent, 16, 0);
    lv_obj_set_style_pad_bottom(parent, 16, 0);
    lv_obj_set_style_pad_left(parent, 0, 0);
    lv_obj_set_style_pad_right(parent, 0, 0);
    lv_obj_set_style_pad_row(parent, 12, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    /* Device Name (collapsed by default) */
    lv_obj_t *name_body = NULL;
    (void)ui_device_setting_create_section(parent, ui_str(UI_STR_DEVICE_NAME_SECTION), false, &name_body, &s_sec_title_device_name);
    if (name_body) {
        lv_obj_t *name_divider = lv_obj_create(name_body);
        lv_obj_clear_flag(name_divider, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(name_divider, lv_pct(100), ui_display_scale_px(1));
        lv_obj_set_style_bg_color(name_divider, lv_color_hex(0xE4E7EC), 0);
        lv_obj_set_style_bg_opa(name_divider, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(name_divider, 0, 0);

        lv_obj_t *name_row = lv_obj_create(name_body);
        lv_obj_clear_flag(name_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(name_row, lv_pct(100));
        lv_obj_set_height(name_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(name_row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(name_row, 0, 0);
        lv_obj_set_style_pad_all(name_row, 0, 0);
        lv_obj_set_flex_flow(name_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(name_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        s_device_name_ta = lv_textarea_create(name_row);
        lv_obj_set_width(s_device_name_ta, lv_pct(82));
        lv_textarea_set_one_line(s_device_name_ta, true);
        lv_textarea_set_text(s_device_name_ta, ui_str(UI_STR_DEVICE));
        lv_obj_set_style_text_font(s_device_name_ta, ui_i18n_uses_cjk() ? ui_font_cjk_always() : ui_font_body(), 0);
        /* Disable direct tap editing; only pencil icon opens keyboard */
        lv_obj_clear_flag(s_device_name_ta, LV_OBJ_FLAG_CLICKABLE);

        lv_obj_t *edit_btn = lv_btn_create(name_row);
        lv_obj_clear_flag(edit_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(edit_btn, ui_display_scale_px(28), ui_display_scale_px(28));
        lv_obj_set_style_bg_opa(edit_btn, LV_OPA_0, 0);
        lv_obj_set_style_border_width(edit_btn, 0, 0);
        lv_obj_set_style_shadow_width(edit_btn, 0, 0);
        s_edit_name_btn = edit_btn;
        lv_obj_add_event_cb(edit_btn, ui_device_setting_edit_name_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *edit_icon = lv_label_create(edit_btn);
        lv_label_set_text(edit_icon, LV_SYMBOL_EDIT);
        lv_obj_set_style_text_font(edit_icon, ui_font_montserrat_18(), 0);
        lv_obj_set_style_text_color(edit_icon, lv_color_hex(0x8AA0C8), 0);
        lv_obj_center(edit_icon);
    }

    /* Node Information (collapsed by default) */
    lv_obj_t *node_body = NULL;
    (void)ui_device_setting_create_section(parent, ui_str(UI_STR_NODE_INFORMATION), false, &node_body, &s_sec_title_node_info);
    if (node_body) {
        /* Node ID row: label and value on same line, value wraps only when needed */
        lv_obj_t *node_id_row = lv_obj_create(node_body);
        lv_obj_clear_flag(node_id_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(node_id_row, lv_pct(100));
        lv_obj_set_height(node_id_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(node_id_row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(node_id_row, 0, 0);
        lv_obj_set_style_pad_all(node_id_row, 0, 0);
        lv_obj_set_style_pad_column(node_id_row, 6, 0);
        lv_obj_set_flex_flow(node_id_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(node_id_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

        lv_obj_t *node_id_label = lv_label_create(node_id_row);
        s_lbl_node_id_caption = node_id_label;
        lv_label_set_text(node_id_label, ui_str(UI_STR_NODE_ID));
        lv_obj_set_style_text_font(node_id_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(node_id_label, lv_color_hex(0xA0AEC0), 0);

        s_node_id_value_label = lv_label_create(node_id_row);
        lv_label_set_text(s_node_id_value_label, "--");
        lv_obj_set_style_text_font(s_node_id_value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(s_node_id_value_label, lv_color_hex(0x4A5568), 0);
        lv_label_set_long_mode(s_node_id_value_label, LV_LABEL_LONG_WRAP);
        lv_obj_set_flex_grow(s_node_id_value_label, 1);

        /* Version row */
        lv_obj_t *ver_row = lv_obj_create(node_body);
        lv_obj_clear_flag(ver_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(ver_row, lv_pct(100));
        lv_obj_set_height(ver_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(ver_row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(ver_row, 0, 0);
        lv_obj_set_style_pad_all(ver_row, 0, 0);
        lv_obj_set_flex_flow(ver_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(ver_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *ver_label = lv_label_create(ver_row);
        s_lbl_version_caption = ver_label;
        lv_label_set_text(ver_label, ui_str(UI_STR_VERSION_SHORT));
        lv_obj_set_style_text_font(ver_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(ver_label, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_flex_grow(ver_label, 1, 0);
        lv_obj_set_style_min_width(ver_label, 0, 0);
        lv_obj_set_width(ver_label, lv_pct(100));
        lv_label_set_long_mode(ver_label, LV_LABEL_LONG_DOT);

        s_version_value_label = lv_label_create(ver_row);
        lv_label_set_text(s_version_value_label, "1.9");
        lv_obj_set_style_text_font(s_version_value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(s_version_value_label, lv_color_hex(0x4A5568), 0);
        lv_label_set_long_mode(s_version_value_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_version_value_label, ui_display_scale_px(128));

        /* Timezone row */
        lv_obj_t *tz_row = lv_obj_create(node_body);
        lv_obj_clear_flag(tz_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(tz_row, lv_pct(100));
        lv_obj_set_height(tz_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(tz_row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(tz_row, 0, 0);
        lv_obj_set_style_pad_all(tz_row, 0, 0);
        lv_obj_set_flex_flow(tz_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(tz_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *tz_label = lv_label_create(tz_row);
        s_lbl_timezone_caption = tz_label;
        lv_label_set_text(tz_label, ui_str(UI_STR_TIMEZONE_COLON));
        lv_obj_set_style_text_font(tz_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(tz_label, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_flex_grow(tz_label, 1, 0);
        lv_obj_set_style_min_width(tz_label, 0, 0);
        lv_obj_set_width(tz_label, lv_pct(100));
        lv_label_set_long_mode(tz_label, LV_LABEL_LONG_DOT);

        s_timezone_btn = lv_btn_create(tz_row);
        lv_obj_clear_flag(s_timezone_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_timezone_btn, ui_display_scale_px(160), ui_display_scale_px(28));
        lv_obj_set_style_bg_opa(s_timezone_btn, LV_OPA_0, 0);
        lv_obj_set_style_bg_opa(s_timezone_btn, LV_OPA_20, LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(s_timezone_btn, lv_color_hex(0x2E6BE6), LV_STATE_PRESSED);
        lv_obj_set_style_border_width(s_timezone_btn, 0, 0);
        lv_obj_set_style_shadow_width(s_timezone_btn, 0, 0);
        lv_obj_add_event_cb(s_timezone_btn, ui_device_setting_timezone_open_cb, LV_EVENT_CLICKED, NULL);

        s_timezone_value_label = lv_label_create(s_timezone_btn);
        lv_label_set_text(s_timezone_value_label, ui_str(UI_STR_SELECT_TIMEZONE));
        lv_obj_set_style_text_font(s_timezone_value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(s_timezone_value_label, lv_color_hex(0x4A5568), 0);
        lv_label_set_long_mode(s_timezone_value_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_timezone_value_label, ui_display_scale_px(132));
        lv_obj_align(s_timezone_value_label, LV_ALIGN_LEFT_MID, 0, 0);

        lv_obj_t *tz_arrow = lv_label_create(s_timezone_btn);
        lv_label_set_text(tz_arrow, LV_SYMBOL_RIGHT);
        lv_obj_set_style_text_font(tz_arrow, ui_font_montserrat_18(), 0);
        lv_obj_set_style_text_color(tz_arrow, lv_color_hex(0x8AA0C8), 0);
        lv_obj_align(tz_arrow, LV_ALIGN_RIGHT_MID, 0, 0);
    }

    /* Operations (collapsed by default, above OTA); visibility set in refresh by whether_has_system_service */
    lv_obj_t *op_body = NULL;
    s_operations_section =
        ui_device_setting_create_section(parent, ui_str(UI_STR_OPERATIONS), false, &op_body, &s_sec_title_operations);
    if (op_body) {
        lv_obj_t *reboot_btn = lv_btn_create(op_body);
        s_op_reboot_btn = reboot_btn;
        lv_obj_clear_flag(reboot_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(reboot_btn, lv_pct(100), ui_display_scale_px(28));
        lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0x2C5AA0), 0);
        lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0x234a82), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(reboot_btn, lv_color_hex(0x9DB3DA), LV_STATE_DISABLED);
        lv_obj_set_style_border_width(reboot_btn, 0, 0);
        lv_obj_set_style_radius(reboot_btn, ui_display_scale_px(8), 0);
        lv_obj_add_event_cb(reboot_btn, ui_device_setting_op_reboot_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *reboot_label = lv_label_create(reboot_btn);
        s_lbl_reboot = reboot_label;
        lv_label_set_text(reboot_label, ui_str(UI_STR_REBOOT));
        lv_obj_set_style_text_font(reboot_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(reboot_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(reboot_label);

        lv_obj_t *wifi_btn = lv_btn_create(op_body);
        s_op_wifi_btn = wifi_btn;
        lv_obj_clear_flag(wifi_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(wifi_btn, lv_pct(100), ui_display_scale_px(28));
        lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0xE74C3C), 0);
        lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0xc93d2f), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(wifi_btn, lv_color_hex(0xEAB2AC), LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(wifi_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(wifi_btn, 0, 0);
        lv_obj_set_style_radius(wifi_btn, ui_display_scale_px(8), 0);
        lv_obj_add_event_cb(wifi_btn, ui_device_setting_op_wifi_reset_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *wifi_label = lv_label_create(wifi_btn);
        s_lbl_wifi_reset = wifi_label;
        lv_label_set_text(wifi_label, ui_str(UI_STR_WIFI_RESET));
        lv_obj_set_style_text_font(wifi_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(wifi_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(wifi_label);

        lv_obj_t *factory_btn = lv_btn_create(op_body);
        s_op_factory_btn = factory_btn;
        lv_obj_clear_flag(factory_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(factory_btn, lv_pct(100), ui_display_scale_px(28));
        lv_obj_set_style_bg_color(factory_btn, lv_color_hex(0xE74C3C), 0);
        lv_obj_set_style_bg_color(factory_btn, lv_color_hex(0xc93d2f), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(factory_btn, lv_color_hex(0xEAB2AC), LV_STATE_DISABLED);
        lv_obj_set_style_bg_opa(factory_btn, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(factory_btn, 0, 0);
        lv_obj_set_style_radius(factory_btn, ui_display_scale_px(8), 0);
        lv_obj_add_event_cb(factory_btn, ui_device_setting_op_factory_reset_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *factory_label = lv_label_create(factory_btn);
        s_lbl_factory_reset = factory_label;
        lv_label_set_text(factory_label, ui_str(UI_STR_FACTORY_RESET));
        lv_obj_set_style_text_font(factory_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(factory_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(factory_label);
    }

    /* OTA (collapsed by default) */
    lv_obj_t *ota_body = NULL;
    (void)ui_device_setting_create_section(parent, ui_str(UI_STR_OTA_SECTION), false, &ota_body, &s_sec_title_ota);
    if (ota_body) {
        /* Current Version row */
        lv_obj_t *cur_row = lv_obj_create(ota_body);
        lv_obj_clear_flag(cur_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(cur_row, lv_pct(100));
        lv_obj_set_height(cur_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(cur_row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(cur_row, 0, 0);
        lv_obj_set_style_pad_all(cur_row, 0, 0);
        lv_obj_set_flex_flow(cur_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cur_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *ota_cur_label = lv_label_create(cur_row);
        s_lbl_ota_current = ota_cur_label;
        lv_label_set_text(ota_cur_label, ui_str(UI_STR_CURRENT_VERSION_COLON));
        lv_obj_set_style_text_font(ota_cur_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(ota_cur_label, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_flex_grow(ota_cur_label, 1, 0);
        lv_obj_set_style_min_width(ota_cur_label, 0, 0);
        lv_obj_set_width(ota_cur_label, lv_pct(100));
        lv_label_set_long_mode(ota_cur_label, LV_LABEL_LONG_DOT);

        s_ota_current_value_label = lv_label_create(cur_row);
        lv_label_set_text(s_ota_current_value_label, "1.9");
        lv_obj_set_style_text_font(s_ota_current_value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(s_ota_current_value_label, lv_color_hex(0x4A5568), 0);
        lv_label_set_long_mode(s_ota_current_value_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_ota_current_value_label, ui_display_scale_px(128));

        /* New Version row */
        lv_obj_t *new_row = lv_obj_create(ota_body);
        lv_obj_clear_flag(new_row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(new_row, lv_pct(100));
        lv_obj_set_height(new_row, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(new_row, LV_OPA_0, 0);
        lv_obj_set_style_border_width(new_row, 0, 0);
        lv_obj_set_style_pad_all(new_row, 0, 0);
        lv_obj_set_flex_flow(new_row, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(new_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        lv_obj_t *ota_new_label = lv_label_create(new_row);
        s_lbl_ota_new = ota_new_label;
        lv_label_set_text(ota_new_label, ui_str(UI_STR_NEW_VERSION_COLON));
        lv_obj_set_style_text_font(ota_new_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(ota_new_label, lv_color_hex(0xA0AEC0), 0);
        lv_obj_set_style_flex_grow(ota_new_label, 1, 0);
        lv_obj_set_style_min_width(ota_new_label, 0, 0);
        lv_obj_set_width(ota_new_label, lv_pct(100));
        lv_label_set_long_mode(ota_new_label, LV_LABEL_LONG_DOT);

        s_ota_new_value_label = lv_label_create(new_row);
        lv_label_set_text(s_ota_new_value_label, "--");
        lv_obj_set_style_text_font(s_ota_new_value_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(s_ota_new_value_label, lv_color_hex(0x4A5568), 0);
        lv_label_set_long_mode(s_ota_new_value_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(s_ota_new_value_label, ui_display_scale_px(128));

        s_ota_check_btn = lv_btn_create(ota_body);
        lv_obj_clear_flag(s_ota_check_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_ota_check_btn, lv_pct(100), ui_display_scale_px(32));
        lv_obj_set_style_bg_color(s_ota_check_btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_color(s_ota_check_btn, lv_color_hex(0xEDF3FF), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(s_ota_check_btn, lv_color_hex(0xF4F6FA), LV_STATE_DISABLED);
        lv_obj_set_style_border_color(s_ota_check_btn, lv_color_hex(0xC9D6EE), 0);
        lv_obj_set_style_border_width(s_ota_check_btn, 1, 0);
        lv_obj_set_style_radius(s_ota_check_btn, ui_display_scale_px(10), 0);
        lv_obj_add_event_cb(s_ota_check_btn, ui_device_setting_check_updates_cb, LV_EVENT_CLICKED, NULL);
        lv_obj_t *ota_btn_label = lv_label_create(s_ota_check_btn);
        s_ota_check_btn_label = ota_btn_label;
        lv_label_set_text(ota_btn_label, ui_str(UI_STR_CHECK_UPDATES));
        lv_obj_set_style_text_font(ota_btn_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(ota_btn_label, lv_color_hex(0x4A6CB3), 0);
        lv_obj_center(ota_btn_label);

        /* Update Now button: replaces Check for Updates when upgrade is detected; pill style, blue, white text */
        s_ota_update_now_btn = lv_btn_create(ota_body);
        lv_obj_clear_flag(s_ota_update_now_btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_width(s_ota_update_now_btn, lv_pct(100));
        lv_obj_set_height(s_ota_update_now_btn, ui_display_scale_px(44));
        lv_obj_set_style_bg_color(s_ota_update_now_btn, lv_color_hex(0x2159C4), 0);
        lv_obj_set_style_bg_color(s_ota_update_now_btn, lv_color_hex(0x1a47a0), LV_STATE_PRESSED);
        lv_obj_set_style_bg_color(s_ota_update_now_btn, lv_color_hex(0x9DB3DA), LV_STATE_DISABLED);
        lv_obj_set_style_border_width(s_ota_update_now_btn, 0, 0);
        lv_obj_set_style_radius(s_ota_update_now_btn, ui_display_scale_px(22), 0); /* pill: half of height */
        lv_obj_add_flag(s_ota_update_now_btn, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *update_now_label = lv_label_create(s_ota_update_now_btn);
        s_ota_update_now_label = update_now_label;
        lv_label_set_text(update_now_label, ui_str(UI_STR_UPDATE_NOW));
        lv_obj_set_style_text_font(update_now_label, ui_font_body(), 0);
        lv_obj_set_style_text_color(update_now_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(update_now_label);
        lv_obj_add_event_cb(s_ota_update_now_btn, ui_device_setting_ota_update_now_cb, LV_EVENT_CLICKED, NULL);
    }

    /* Remove Device (always visible) */
    lv_obj_t *remove_btn = lv_btn_create(parent);
    lv_obj_clear_flag(remove_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(remove_btn, lv_pct(90), ui_display_scale_px(40));
    lv_obj_set_style_bg_color(remove_btn, lv_color_hex(0xE74C3C), 0);
    lv_obj_set_style_bg_color(remove_btn, lv_color_hex(0xc93d2f), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(remove_btn, lv_color_hex(0xEAB2AC), LV_STATE_DISABLED);
    lv_obj_set_style_border_width(remove_btn, 0, 0);
    lv_obj_set_style_radius(remove_btn, ui_display_scale_px(12), 0);
    lv_obj_add_event_cb(remove_btn, ui_device_setting_remove_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *remove_label = lv_label_create(remove_btn);
    s_remove_device_label = remove_label;
    /* Do not prefix LV_SYMBOL_TRASH: ui_font_body() is CJK and has no symbol glyphs (shows tofu). */
    lv_label_set_text(remove_label, ui_str(UI_STR_REMOVE_DEVICE));
    lv_obj_set_style_text_font(remove_label, ui_font_body(), 0);
    lv_obj_set_style_text_color(remove_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(remove_label);

    /* Bottom spacer */
    lv_obj_t *bottom_spacer = lv_obj_create(parent);
    lv_obj_clear_flag(bottom_spacer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bottom_spacer, lv_pct(100), ui_display_scale_px(24));
    lv_obj_set_style_bg_opa(bottom_spacer, LV_OPA_0, 0);
    lv_obj_set_style_border_width(bottom_spacer, 0, 0);
}

void ui_Screen_Device_Setting_show(rm_device_item_handle_t device)
{
    if (device == NULL || ui_Screen_Device_Setting == NULL) {
        return;
    }
    s_device_setting_prev_screen = lv_scr_act();
    s_device_setting_device = device;
    ui_device_setting_refresh_from_device();
    if (s_device_setting_content) {
        lv_obj_add_flag(s_device_setting_content, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_scroll_to_y(s_device_setting_content, 0, LV_ANIM_OFF);
    }
    lv_scr_load(ui_Screen_Device_Setting);
}

void ui_Screen_Device_Setting_screen_init(void)
{
    ui_Screen_Device_Setting = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_Screen_Device_Setting, lv_color_hex(0xF5F8FF), 0);
    (void)ui_device_setting_create_header(ui_Screen_Device_Setting);
    /* Header + scrollable content */
    s_device_setting_content = lv_obj_create(ui_Screen_Device_Setting);
    lv_obj_add_flag(s_device_setting_content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_device_setting_content, lv_pct(100), lv_pct(100));
    lv_obj_align(s_device_setting_content, LV_ALIGN_TOP_MID, 0, ui_display_scale_px(56));
    lv_obj_set_style_bg_opa(s_device_setting_content, LV_OPA_0, 0);
    lv_obj_set_style_border_width(s_device_setting_content, 0, 0);
    s_device_setting_content_base_y = ui_display_scale_px(56);
    lv_obj_set_scroll_dir(s_device_setting_content, LV_DIR_VER);
    lv_obj_set_scroll_snap_y(s_device_setting_content, LV_SCROLL_SNAP_START);
    lv_obj_set_scrollbar_mode(s_device_setting_content, LV_SCROLLBAR_MODE_OFF);
    ui_device_setting_create_content(s_device_setting_content);

    s_device_setting_kb = lv_keyboard_create(ui_Screen_Device_Setting);
    lv_obj_set_width(s_device_setting_kb, lv_pct(100));
    lv_obj_set_height(s_device_setting_kb, ui_display_scale_px(140));
    lv_obj_add_flag(s_device_setting_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_device_setting_kb, ui_device_setting_keyboard_event_cb, LV_EVENT_ALL, NULL);
}

void ui_Screen_Device_Setting_screen_destroy(void)
{
    if (ui_Screen_Device_Setting) {
        lv_obj_del(ui_Screen_Device_Setting);
    }
    ui_Screen_Device_Setting = NULL;
    s_device_setting_device = NULL;
    s_device_setting_prev_screen = NULL;
    s_device_name_ta = NULL;
    s_device_setting_kb = NULL;
    s_device_setting_content = NULL;
    s_node_id_value_label = NULL;
    s_version_value_label = NULL;
    s_timezone_btn = NULL;
    s_timezone_value_label = NULL;
    s_ota_current_value_label = NULL;
    s_ota_new_value_label = NULL;
    s_edit_name_btn = NULL;
    s_op_reboot_btn = NULL;
    s_op_wifi_btn = NULL;
    s_op_factory_btn = NULL;
    s_tz_picker_overlay = NULL;
    s_tz_picker_search_ta = NULL;
    s_tz_picker_list = NULL;
    s_tz_picker_kb = NULL;
    s_tz_db = NULL;
    s_tz_db_count = 0;
    s_devset_title_label = NULL;
    s_sec_title_device_name = NULL;
    s_sec_title_node_info = NULL;
    s_sec_title_operations = NULL;
    s_sec_title_ota = NULL;
    s_lbl_node_id_caption = NULL;
    s_lbl_version_caption = NULL;
    s_lbl_timezone_caption = NULL;
    s_lbl_reboot = NULL;
    s_lbl_wifi_reset = NULL;
    s_lbl_factory_reset = NULL;
    s_lbl_ota_current = NULL;
    s_lbl_ota_new = NULL;
    s_ota_check_btn_label = NULL;
    s_ota_update_now_label = NULL;
    s_remove_device_label = NULL;
}

void ui_Screen_Device_Setting_apply_language(void)
{
    if (s_devset_title_label) {
        lv_label_set_text(s_devset_title_label, ui_str(UI_STR_DEVICE_SETTINGS));
        lv_obj_set_style_text_font(s_devset_title_label, ui_font_title(), 0);
    }
    if (s_sec_title_device_name) {
        lv_label_set_text(s_sec_title_device_name, ui_str(UI_STR_DEVICE_NAME_SECTION));
    }
    if (s_sec_title_node_info) {
        lv_label_set_text(s_sec_title_node_info, ui_str(UI_STR_NODE_INFORMATION));
    }
    if (s_sec_title_operations) {
        lv_label_set_text(s_sec_title_operations, ui_str(UI_STR_OPERATIONS));
    }
    if (s_sec_title_ota) {
        lv_label_set_text(s_sec_title_ota, ui_str(UI_STR_OTA_SECTION));
    }
    if (s_lbl_node_id_caption) {
        lv_label_set_text(s_lbl_node_id_caption, ui_str(UI_STR_NODE_ID));
    }
    if (s_lbl_version_caption) {
        lv_label_set_text(s_lbl_version_caption, ui_str(UI_STR_VERSION_SHORT));
    }
    if (s_lbl_timezone_caption) {
        lv_label_set_text(s_lbl_timezone_caption, ui_str(UI_STR_TIMEZONE_COLON));
    }
    if (s_lbl_reboot) {
        lv_label_set_text(s_lbl_reboot, ui_str(UI_STR_REBOOT));
    }
    if (s_lbl_wifi_reset) {
        lv_label_set_text(s_lbl_wifi_reset, ui_str(UI_STR_WIFI_RESET));
    }
    if (s_lbl_factory_reset) {
        lv_label_set_text(s_lbl_factory_reset, ui_str(UI_STR_FACTORY_RESET));
    }
    if (s_lbl_ota_current) {
        lv_label_set_text(s_lbl_ota_current, ui_str(UI_STR_CURRENT_VERSION_COLON));
    }
    if (s_lbl_ota_new) {
        lv_label_set_text(s_lbl_ota_new, ui_str(UI_STR_NEW_VERSION_COLON));
    }
    if (s_ota_check_btn_label) {
        lv_label_set_text(s_ota_check_btn_label, ui_str(UI_STR_CHECK_UPDATES));
    }
    if (s_ota_update_now_label) {
        lv_label_set_text(s_ota_update_now_label, ui_str(UI_STR_UPDATE_NOW));
    }
    if (s_remove_device_label) {
        lv_label_set_text(s_remove_device_label, ui_str(UI_STR_REMOVE_DEVICE));
    }
    ui_device_setting_refresh_from_device();
}
