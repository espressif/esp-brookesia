/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_obj_t *ui_Screen_Schedules;

void ui_Screen_Schedules_screen_init(void);
void ui_Screen_Schedules_screen_destroy(void);
/** Refresh list from g_schedules_list; call when entering the page. */
void ui_Screen_Schedules_show(void);
void ui_Screen_Schedules_apply_language(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
