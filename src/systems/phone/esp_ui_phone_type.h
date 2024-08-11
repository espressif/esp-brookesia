/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "core/esp_ui_core_type.h"
#include "widgets/status_bar/esp_ui_status_bar_type.h"
#include "widgets/navigation_bar/esp_ui_navigation_bar_type.h"
#include "widgets/app_launcher/esp_ui_app_launcher_type.h"
#include "widgets/recents_screen/esp_ui_recents_screen_type.h"
#include "widgets/gesture/esp_ui_gesture_type.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// Home ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    ESP_UI_StatusBarData_t status_bar;
    ESP_UI_NavigationBarData_t navigation_bar;
    ESP_UI_AppLauncherData_t app_launcher;
    ESP_UI_RecentsScreenData_t recents_screen;
    struct {
        uint8_t enable_status_bar: 1;
        uint8_t enable_navigation_bar: 1;
        uint8_t enable_app_launcher_flex: 1;
        uint8_t enable_recents_screen: 1;
        uint8_t enable_recents_screen_flex: 1;
        uint8_t enable_recents_screen_show_status_bar: 1;
        uint8_t enable_recents_screen_show_navigation_bar: 1;
        uint8_t enable_recents_screen_hide_when_no_snapshot: 1;
    } flags;
} ESP_UI_PhoneHomeData_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// Manager ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    ESP_UI_GestureData_t gesture;
    struct {
        uint16_t drag_snapshot_y_step;
        uint16_t drag_snapshot_y_threshold;
        uint16_t drag_snapshot_angle_threshold;
        uint16_t delete_snapshot_y_threshold;
    } recents_screen;
    struct {
        uint8_t enable_gesture: 1;
        uint8_t enable_recents_screen_snapshot_drag: 1;
    } flags;
} ESP_UI_PhoneManagerData_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// App //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef enum {
    ESP_UI_PHONE_APP_VISUAL_MODE_HIDE = 0,
    ESP_UI_PHONE_APP_VISUAL_MODE_SHOW_FIXED,
    // ESP_UI_PHONE_APP_VISUAL_MODE_SHOW_FLEX,  // TODO: Implement this mode
} ESP_UI_PhoneAppVisualMode_t;

/**
 * @brief Phone app data structure
 */
typedef struct {
    uint8_t app_launcher_page_index;                    /*!< The index of the app launcher page where the icon is shown */
    uint8_t status_bar_area_index;                      /*!< The index of the status area where the icon is shown */
    ESP_UI_StatusBarIconData_t status_icon_data;        /*!< The status icon data. If the `enable_status_icon_common_size`
                                                             flag is set, the `size` in this value will be ignored */
    ESP_UI_PhoneAppVisualMode_t status_bar_visual_mode; /*!< The visual mode of the status bar */
    ESP_UI_PhoneAppVisualMode_t navigation_bar_visual_mode;
    /*!< The visual mode of the navigation bar */
    struct {
        uint8_t enable_status_icon_common_size: 1;      /*!< If set, the size of the status icon will be set to the
                                                             common size in the status bar data */
        uint8_t enable_navigation_gesture: 1;           /*!< If set and the gesture is enabled, the navigation gesture
                                                             will be enabled. */
    } flags;                                            /*!< The flags for the phone app data */
} ESP_UI_PhoneAppData_t;

/**
 * @brief The default initializer for phone app data structure
 *
 * @note  The `app_launcher_page_index` and `status_bar_area_index` is set to 0
 * @note  The `enable_status_icon_common_size` flag is set by default
 *
 * @param status_icon The status icon image. Set to `NULL` if no icon is needed
 * @param use_status_bar Flag to show the status bar
 * @param use_navigation_bar Flag to show the navigation bar. If not set, the `enable_navigation_gesture` flag will be set
 *
 */
#define ESP_UI_PHONE_APP_DATA_DEFAULT(status_icon, use_status_bar, use_navigation_bar)                 \
    {                                                                                                  \
        .app_launcher_page_index = 0,                                                                  \
        .status_bar_area_index = 0,                                                                    \
        .status_icon_data = {                                                                          \
            .size = {},                                                                                \
            .icon = {                                                                                  \
                .image_num = (uint8_t)((status_icon != NULL) ? 1 : 0),                                 \
                .images = {                                                                            \
                    ESP_UI_STYLE_IMAGE(status_icon),                                                   \
                },                                                                                     \
            },                                                                                         \
        },                                                                                             \
        .status_bar_visual_mode = (use_status_bar) ? ESP_UI_PHONE_APP_VISUAL_MODE_SHOW_FIXED :         \
                                                     ESP_UI_PHONE_APP_VISUAL_MODE_HIDE,                \
        .navigation_bar_visual_mode = (use_navigation_bar) ? ESP_UI_PHONE_APP_VISUAL_MODE_SHOW_FIXED : \
                                                             ESP_UI_PHONE_APP_VISUAL_MODE_HIDE,        \
        .flags = {                                                                                     \
            .enable_status_icon_common_size = 1,                                                       \
            .enable_navigation_gesture = !use_navigation_bar,                                          \
        },                                                                                             \
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// Phone ///////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    ESP_UI_CoreData_t core;
    ESP_UI_PhoneHomeData_t home;
    ESP_UI_PhoneManagerData_t manager;
} ESP_UI_PhoneStylesheet_t;

#ifdef __cplusplus
}
#endif
