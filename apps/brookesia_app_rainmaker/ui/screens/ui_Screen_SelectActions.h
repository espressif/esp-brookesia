/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Select Actions: list of configurable actions for a device (Power, Brightness, etc.).
 * Options depend on device type and whether_has_power.
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "rainmaker_app_backend.h"
#include "rainmaker_list_manager.h"

extern lv_obj_t *ui_Screen_SelectActions;

void ui_Screen_SelectActions_screen_init(void);
void ui_Screen_SelectActions_screen_destroy(void);
/** Show Select Actions for the given device handle */
void ui_Screen_SelectActions_show(rm_device_item_handle_t device_handle);

/** Show Select Actions in edit mode: pre-fill from schedule action at index; Done updates that action and returns to CreateSchedule */
void ui_Screen_SelectActions_show_for_edit(rm_device_item_handle_t device_handle, rm_list_item_t *schedule_item, int action_index);
void ui_Screen_SelectActions_apply_language(void);

#ifdef __cplusplus
}
#endif
