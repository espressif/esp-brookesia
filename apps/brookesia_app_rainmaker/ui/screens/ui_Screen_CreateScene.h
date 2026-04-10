/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Create Scene screen: name + actions + save.
 */

#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern lv_obj_t *ui_Screen_CreateScene;

void ui_Screen_CreateScene_show(const char *scene_name);
void ui_Screen_CreateScene_show_edit(const char *scene_id);
void ui_Screen_CreateScene_refresh_actions(void);
void ui_Screen_CreateScene_screen_init(void);
void ui_Screen_CreateScene_screen_destroy(void);
void ui_Screen_CreateScene_apply_language(void);

#ifdef __cplusplus
}
#endif
