/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include "rainmaker_app_backend.h"

extern lv_obj_t *ui_Screen_FanDetails;

void ui_Screen_FanDetails_screen_init(void);
void ui_Screen_FanDetails_screen_destroy(void);

/** Open fan detail screen for the given device (must be FAN type). */
void ui_Screen_FanDetails_show(rm_device_item_handle_t device);
void ui_Screen_FanDetails_apply_language(void);

#ifdef __cplusplus
}
#endif
