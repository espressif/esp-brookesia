/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "screens/ui_Screen_Light_Detail.h"
#include "screens/ui_Screen_Fan_Details.h"
#include "screens/ui_Screen_Switch_Details.h"
#include "ui.h"

lv_obj_t *ui_Screen_LightDetail = NULL;
lv_obj_t *ui_Screen_FanDetails = NULL;
lv_obj_t *ui_Screen_SwitchDetails = NULL;

void ui_Screen_LightDetail_show(rm_device_item_handle_t device)
{
    (void)device;
    ui_show_msgbox(ui_str(UI_STR_MSGBOX_TITLE_INFO), ui_str(UI_STR_HOME_PLUS_PLACEHOLDER));
}

void ui_Screen_FanDetails_show(rm_device_item_handle_t device)
{
    (void)device;
    ui_show_msgbox(ui_str(UI_STR_MSGBOX_TITLE_INFO), ui_str(UI_STR_HOME_PLUS_PLACEHOLDER));
}

void ui_Screen_SwitchDetails_show(rm_device_item_handle_t device)
{
    (void)device;
    ui_show_msgbox(ui_str(UI_STR_MSGBOX_TITLE_INFO), ui_str(UI_STR_HOME_PLUS_PLACEHOLDER));
}

void ui_Screen_LightDetail_screen_init(void)
{
}

void ui_Screen_LightDetail_screen_destroy(void)
{
}

void ui_Screen_LightDetail_apply_language(void)
{
}

void ui_Screen_FanDetails_screen_init(void)
{
}

void ui_Screen_FanDetails_screen_destroy(void)
{
}

void ui_Screen_FanDetails_apply_language(void)
{
}

void ui_Screen_SwitchDetails_screen_init(void)
{
}

void ui_Screen_SwitchDetails_screen_destroy(void)
{
}

void ui_Screen_SwitchDetails_apply_language(void)
{
}
