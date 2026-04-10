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

extern lv_obj_t *ui_Screen_User;

void ui_Screen_User_screen_init(void);
void ui_Screen_User_screen_destroy(void);
void ui_Screen_User_refresh_from_backend(void);
void ui_Screen_User_apply_language(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
