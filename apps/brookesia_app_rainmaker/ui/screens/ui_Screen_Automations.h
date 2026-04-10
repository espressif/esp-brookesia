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

extern lv_obj_t *ui_Screen_Automations;

void ui_Screen_Automations_screen_init(void);
void ui_Screen_Automations_screen_destroy(void);
void ui_Screen_Automations_apply_language(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
