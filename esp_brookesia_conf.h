/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// *INDENT-OFF*

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// Systems /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Core related
 */
// Enable/Disable the log output of the core modules
#define ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG              (1)
#if ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG

    #define ESP_BROOKESIA_CONF_CORE_APP_ENABLE_DEBUG_LOG      (1)
    #define ESP_BROOKESIA_CONF_CORE_HOME_ENABLE_DEBUG_LOG     (1)
    #define ESP_BROOKESIA_CONF_CORE_MANAGER_ENABLE_DEBUG_LOG  (1)
    #define ESP_BROOKESIA_CONF_CORE_CORE_ENABLE_DEBUG_LOG     (1)

#endif // ESP_BROOKESIA_CONF_CORE_ENABLE_DEBUG_LOG

/**
 * Phone related
 */
// Enable/Disable the log output of the phone modules
#define ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG                     (1)
#if ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG

    #define ESP_BROOKESIA_CONF_PHONE_APP_ENABLE_DEBUG_LOG             (1)
    #define ESP_BROOKESIA_CONF_PHONE_HOME_ENABLE_DEBUG_LOG            (1)
    #define ESP_BROOKESIA_CONF_PHONE_MANAGER_ENABLE_DEBUG_LOG         (1)
    #define ESP_BROOKESIA_CONF_PHONE_LAUNCHER_ENABLE_DEBUG_LOG        (1)
    #define ESP_BROOKESIA_CONF_PHONE_GESTURE_ENABLE_DEBUG_LOG         (1)
    #define ESP_BROOKESIA_CONF_PHONE_RECENTS_SCREEN_ENABLE_DEBUG_LOG  (1)
    #define ESP_BROOKESIA_CONF_PHONE_NAVIGATION_ENABLE_DEBUG_LOG      (1)
    #define ESP_BROOKESIA_CONF_PHONE_STATUS_BAR_ENABLE_DEBUG_LOG      (1)
    #define ESP_BROOKESIA_CONF_PHONE_PHONE_ENABLE_DEBUG_LOG           (1)

#endif // ESP_BROOKESIA_CONF_PHONE_ENABLE_DEBUG_LOG

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////// GUI ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * LVGL related
 */
// Enable/Disable the log output of the LVGL modules
#define ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG                    (1)
#if ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG

    #define ESP_BROOKESIA_CONF_LVGL_CANVAS_ENABLE_DEBUG_LOG         (1)
    #define ESP_BROOKESIA_CONF_LVGL_CONTAINER_ENABLE_DEBUG_LOG      (1)
    #define ESP_BROOKESIA_CONF_LVGL_OBJECT_ENABLE_DEBUG_LOG         (1)
    #define ESP_BROOKESIA_CONF_LVGL_SCREEN_ENABLE_DEBUG_LOG         (1)
    #define ESP_BROOKESIA_CONF_LVGL_TIMER_ENABLE_DEBUG_LOG          (1)

#endif // ESP_BROOKESIA_CONF_LVGL_ENABLE_DEBUG_LOG

/**
 * Squareline related
 */
/**
 * Use the internal "ui_helpers.c" and "ui_helpers.h" instead of the ones exported from Squareline Studio.
 * 1: enable, 0: disable
 *
 * This configuration is to avoid function redefinition errors caused by multiple UIs exported from Squareline Studio
 * that include duplicate "ui_helpers.c" and "ui_helpers.h".
 */
#define ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_HELPERS     (1)

/**
 * Use the internal general APIs of "ui_comp.c" and "ui_comp.h" instead of the ones exported from Squareline Studio.
 * 1: enable, 0: disable
 *
 * This configuration is to avoid function redefinition errors caused by multiple UIs exported from Squareline Studio
 * that include duplicate APIs of "ui_comp.c" and "ui_comp.h".
 */
#define ESP_BROOKESIA_CONF_GUI_SQUARELINE_ENABLE_UI_COMP        (1)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// File Version ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Do not change the following versions. These version numbers are used to check compatibility between this
 * configuration file and the library. Rules for version numbers:
 * 1. Major version mismatch: Configurations are incompatible, must use library version
 * 2. Minor version mismatch: May be missing new configurations, recommended to update
 * 3. Patch version mismatch: No impact on functionality
 */
#define ESP_BROOKESIA_CONF_FILE_VER_MAJOR  1
#define ESP_BROOKESIA_CONF_FILE_VER_MINOR  0
#define ESP_BROOKESIA_CONF_FILE_VER_PATCH  0

// *INDENT-ON*
