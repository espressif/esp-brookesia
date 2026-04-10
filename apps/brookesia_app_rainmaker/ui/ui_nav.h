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

typedef enum {
    UI_NAV_TAB_HOME = 0,
    UI_NAV_TAB_SCHEDULES,
    UI_NAV_TAB_SCENES,
    UI_NAV_TAB_USER,
    UI_NAV_TAB_MAX,
} ui_nav_tab_t;

/**
 * @brief Create the bottom navigation bar.
 *
 * @param parent Parent object
 * @param active Which tab to highlight
 * @return The nav container object
 */
lv_obj_t *ui_nav_create(lv_obj_t *parent, ui_nav_tab_t active);

#ifdef __cplusplus
} /* extern "C" */
#endif
