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

extern lv_obj_t *ui_Screen_CreateSchedule;

/**
 * @brief Show Create Schedule screen with pre-filled schedule name
 * @param schedule_name Name from previous step (can be NULL for empty)
 */
void ui_Screen_CreateSchedule_show(const char *schedule_name);
/**
 * @brief Show Edit Schedule screen by schedule id
 * @param schedule_id Existing schedule id
 */
void ui_Screen_CreateSchedule_show_edit(const char *schedule_id);
/** Refresh Actions list from selected devices. */
void ui_Screen_CreateSchedule_refresh_actions(void);

void ui_Screen_CreateSchedule_screen_init(void);
void ui_Screen_CreateSchedule_screen_destroy(void);
void ui_Screen_CreateSchedule_apply_language(void);

#ifdef __cplusplus
} /* extern "C" */
#endif
