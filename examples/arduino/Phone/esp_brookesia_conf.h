/*
 * SPDX-FileCopyrightText: 2023-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////// Debug /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Assert when check result failed. 0: disable, 1: enable */
#define ESP_BROOKESIA_CHECK_RESULT_ASSERT     (0)

/**
 * Log style. choose one of the following:
 *      - ESP_BROOKESIA_LOG_STYLE_STD:  Use printf to output log
 *      - ESP_BROOKESIA_LOG_STYLE_ESP:  Use ESP_LOGx to output log
 *      - ESP_BROOKESIA_LOG_STYLE_LVGL: Use LV_LOG_x to output log
 *
 */
#define ESP_BROOKESIA_LOG_STYLE        (ESP_BROOKESIA_LOG_STYLE_STD)

/**
 * Log level. Higher levels produce less log output. choose one of the following:
 *      - ESP_BROOKESIA_LOG_LEVEL_DEBUG: Output all logs (most verbose)
 *      - ESP_BROOKESIA_LOG_LEVEL_INFO:  Output info, warnings, and errors
 *      - ESP_BROOKESIA_LOG_LEVEL_WARN:  Output warnings and errors
 *      - ESP_BROOKESIA_LOG_LEVEL_ERROR: Output only errors
 *      - ESP_BROOKESIA_LOG_LEVEL_NONE:  No log output (least verbose)
 *
 */
#define ESP_BROOKESIA_LOG_LEVEL        (ESP_BROOKESIA_LOG_LEVEL_INFO)

/* Enable debug logs for modules */
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE                (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS             (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE               (1)
// Core
#if ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_APP            (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_HOME           (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_MANAGER        (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_CORE_CORE           (1)
#endif
// Widgets
#if ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_APP_LAUNCHER   (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_RECENTS_SCREEN (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_GESTURE        (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_NAVIGATION     (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_WIDGETS_STATUS_BAR     (1)
#endif
// Phone
#if ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_APP           (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_HOME          (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_MANAGER       (1)
#define ESP_BROOKESIA_LOG_ENABLE_DEBUG_PHONE_PHONE         (1)
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// Memory /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ESP_BROOKESIA_MEMORY_USE_CUSTOM    (0)
#if ESP_BROOKESIA_MEMORY_USE_CUSTOM == 0
#define ESP_BROOKESIA_MEMORY_INCLUDE       <stdlib.h>
#define ESP_BROOKESIA_MEMORY_MALLOC        malloc
#define ESP_BROOKESIA_MEMORY_FREE          free
#else
#define ESP_BROOKESIA_MEMORY_INCLUDE       "esp_heap_caps.h"
#define ESP_BROOKESIA_MEMORY_MALLOC(x)     heap_caps_aligned_alloc(1, x, MALLOC_CAP_SPIRAM)
#define ESP_BROOKESIA_MEMORY_FREE          free
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////// Squareline ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Use the internal "ui_helpers.c" and "ui_helpers.h" instead of the ones exported from Squareline Studio. 1: enable,
 * 0: disable
 *
 * This configuration is to avoid function redefinition errors caused by multiple UIs exported from Squareline Studio
 * that include duplicate "ui_helpers.c" and "ui_helpers.h".
 *
 */
#define ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS   (1)
#if ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS
/**
 * Please uncomment one of the options below based on the version of Squareline Studio you are using and the corresponding
 * configured version of LVGL. (If multiple options are uncommented, a compilation error will occur)
 *
 */
// | Squareline |  LVGL  |
// #define ESP_BROOKESIA_SQ1_3_4_LV8_3_3       // |   1.3.4    | 8.3.3  |
// #define ESP_BROOKESIA_SQ1_3_4_LV8_3_4       // |   1.3.4    | 8.3.4  |
// #define ESP_BROOKESIA_SQ1_3_4_LV8_3_6       // |   1.3.4    | 8.3.6  |
// #define ESP_BROOKESIA_SQ1_4_0_LV8_3_6       // |   1.4.0    | 8.3.6  |
// #define ESP_BROOKESIA_SQ1_4_0_LV8_3_11      // |   1.4.0    | 8.3.11 |
// #define ESP_BROOKESIA_SQ1_4_1_LV8_3_6       // |   1.4.1    | 8.3.6  |
#define ESP_BROOKESIA_SQ1_4_1_LV8_3_11      // |   1.4.1    | 8.3.11 |
#endif /* ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_HELPERS */

/**
 * Use the internal general APIs of "ui_comp.c" and "ui_comp.h" instead of the ones exported from Squareline Studio.
 * 1: enable, 0: disable
 *
 * This configuration is to avoid function redefinition errors caused by multiple UIs exported from Squareline Studio
 * that include duplicate APIs of "ui_comp.c" and "ui_comp.h".
 *
 */
#define ESP_BROOKESIA_SQUARELINE_USE_INTERNAL_UI_COMP   (1)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// File Version ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * Do not change the following versions, they are used to check if the configurations in this file are compatible with
 * the current version of `esp_brookesia_conf.h` in the library. The detailed rules are as follows:
 *
 *   1. If the major version is not consistent, then the configurations in this file are incompatible with the library
 *      and must be replaced with the file from the library.
 *   2. If the minor version is not consistent, this file might be missing some new configurations, which will be set to
 *      default values. It is recommended to replace it with the file from the library.
 *   3. Even if the patch version is not consistent, it will not affect normal functionality.
 *
 */
#define ESP_BROOKESIA_CONF_FILE_VER_MAJOR  0
#define ESP_BROOKESIA_CONF_FILE_VER_MINOR  2
#define ESP_BROOKESIA_CONF_FILE_VER_PATCH  0
