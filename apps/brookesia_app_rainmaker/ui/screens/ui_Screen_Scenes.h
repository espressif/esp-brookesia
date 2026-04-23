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

extern lv_obj_t *ui_Screen_Scenes;

void ui_Screen_Scenes_screen_init(void);
void ui_Screen_Scenes_screen_destroy(void);
void ui_Screen_Scenes_show(void);
void ui_Screen_Scenes_apply_language(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
