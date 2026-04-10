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

/**
 * @brief Enable swipe left/right to switch screens on the given screen root.
 *
 * Gesture mapping:
 * - LV_DIR_LEFT  -> next screen
 * - LV_DIR_RIGHT -> previous screen
 *
 * @param screen Root screen object (lv_obj_create(NULL))
 */
void ui_gesture_nav_enable(lv_obj_t *screen);

/**
 * @brief Switch tab as if a horizontal gesture occurred (e.g. PC keyboard left/right).
 *
 * Uses the active screen to decide the current tab; no-op if the screen is not a main tab.
 */
void ui_gesture_nav_feed_dir(lv_dir_t dir);

#ifdef __cplusplus
} /* extern "C" */
#endif
