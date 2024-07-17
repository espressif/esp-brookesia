/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "core_data.h"
#include "app_launcher_data.h"
#include "recents_screen_data.h"
#include "gesture_data.h"
#include "navigation_bar_data.h"
#include "status_bar_data.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Home */
#define ESP_UI_PHONE_DEFAULT_DARK_HOME_DATA()                              \
    {                                                                      \
        .status_bar = ESP_UI_PHONE_DEFAULT_DARK_STATUS_BAR_DATA(),         \
        .navigation_bar = ESP_UI_PHONE_DEFAULT_DARK_NAVIGATION_BAR_DATA(), \
        .app_launcher = ESP_UI_PHONE_DEFAULT_DARK_APP_LAUNCHER_DATA(),     \
        .recents_screen = ESP_UI_PHONE_DEFAULT_DARK_RECENTS_SCREEN_DATA(), \
        .flags = {                                                         \
            .enable_status_bar = 1,                                        \
            .enable_navigation_bar = 1,                                    \
            .enable_app_launcher_flex = 1,                                 \
            .enable_recents_screen = 1,                                    \
            .enable_recents_screen_flex = 1,                               \
            .enable_recents_screen_show_status_bar = 0,                    \
            .enable_recents_screen_show_navigation_bar = 0,                \
        },                                                                 \
    }

/* Manager */
#define ESP_UI_PHONE_DEFAULT_DARK_MANAGER_DATA()             \
    {                                                        \
        .gesture = ESP_UI_PHONE_DEFAULT_DARK_GESTURE_DATA(), \
        .recents_screen = {                                  \
            .drag_snapshot_y_step = 10,                      \
            .drag_snapshot_y_threshold = 50,                 \
            .drag_snapshot_angle_threshold = 60,             \
            .delete_snapshot_y_threshold = 50,               \
        },                                                   \
        .flags = {                                           \
            .enable_gesture = 1,                             \
            .enable_recents_screen_snapshot_drag = 1,        \
        },                                                   \
    }

/* Phone */
#define ESP_UI_PHONE_DEFAULT_DARK_STYLESHEET()               \
    {                                                        \
        .core = ESP_UI_PHONE_DEFAULT_DARK_CORE_DATA(),       \
        .home = ESP_UI_PHONE_DEFAULT_DARK_HOME_DATA(),       \
        .manager = ESP_UI_PHONE_DEFAULT_DARK_MANAGER_DATA(), \
    }

#ifdef __cplusplus
}
#endif
