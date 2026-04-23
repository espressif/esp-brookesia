/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Select Devices screen: list of current home devices, each row clickable to device detail.
 * Shown from Create Schedule when user taps Actions (+).
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "rainmaker_device_group_manager.h"
#include "rainmaker_list_manager.h"

extern lv_obj_t *ui_Screen_SelectDevices;

void ui_Screen_SelectDevices_screen_init(void);
void ui_Screen_SelectDevices_screen_destroy(void);
/** Show Select Devices with the schedule item whose node_actions_list stores selected devices/actions. */
void ui_Screen_SelectDevices_show(rm_list_item_t *schedule_item);
/** Update selected device summary text (NULL/empty to remove). */
void ui_Screen_SelectDevices_set_selected_summary(rm_device_item_handle_t handle, const char *summary);
/** Refresh UI with existing selection state. */
void ui_Screen_SelectDevices_refresh(void);
void ui_Screen_SelectDevices_apply_language(void);

#ifdef RM_HOST_BUILD
/**
 * Layout regression: first device row in the picker list (icon + text_slot + name label).
 * Returns false if the list is empty or structure does not match.
 */
bool ui_Screen_SelectDevices_layout_test_get_first_picker_row(lv_obj_t **row_out, lv_obj_t **icon_out,
        lv_obj_t **name_label_out,
        lv_obj_t **text_slot_out);
#endif

#ifdef __cplusplus
}
#endif
