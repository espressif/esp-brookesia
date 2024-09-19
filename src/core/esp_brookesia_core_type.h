/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "esp_brookesia_style_type.h"
#include "esp_brookesia_lv_type.h"

#ifdef __cplusplus
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////// Utils /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* Log Style */
#define ESP_BROOKESIA_LOG_STYLE_STD    0
#define ESP_BROOKESIA_LOG_STYLE_ESP    1
#define ESP_BROOKESIA_LOG_STYLE_LVGL   2

/* Log Level */
#define ESP_BROOKESIA_LOG_LEVEL_DEBUG  0
#define ESP_BROOKESIA_LOG_LEVEL_INFO   1
#define ESP_BROOKESIA_LOG_LEVEL_WARN   2
#define ESP_BROOKESIA_LOG_LEVEL_ERROR  3
#define ESP_BROOKESIA_LOG_LEVEL_NONE   4

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// Home /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ESP_BROOKESIA_CORE_HOME_DATA_DEFAULT_FONTS_NUM_MAX  ((ESP_BROOKESIA_STYLE_FONT_SIZE_MAX - ESP_BROOKESIA_STYLE_FONT_SIZE_MIN) / 2 + 1)
#define ESP_BROOKESIA_CORE_HOME_DATA_CONTAINER_STYLES_NUM   6

typedef struct {
    struct {
        ESP_Brookesia_StyleColor_t color;
        ESP_Brookesia_StyleImage_t wallpaper_image_resource;
    } background;
    struct {
        uint8_t default_fonts_num;
        ESP_Brookesia_StyleFont_t default_fonts[ESP_BROOKESIA_CORE_HOME_DATA_DEFAULT_FONTS_NUM_MAX];
    } text;
    struct {
        struct {
            uint8_t outline_width;
            ESP_Brookesia_StyleColor_t outline_color;
        } styles[ESP_BROOKESIA_CORE_HOME_DATA_CONTAINER_STYLES_NUM];
    } container;
} ESP_Brookesia_CoreHomeData_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////// Manager ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    struct {
        uint16_t max_running_num;
    } app;
    struct {
        uint8_t enable_app_save_snapshot: 1;
    } flags;
} ESP_Brookesia_CoreManagerData_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////// App //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief Core app data structure
 *
 */
typedef struct {
    const char *name;                           /*!< App name string */
    ESP_Brookesia_StyleImage_t launcher_icon;          /*!< Launcher icon image, use `ESP_BROOKESIA_STYLE_IMAGE*` macros to set */
    ESP_Brookesia_StyleSize_t screen_size;             /*!< App screen size, use `ESP_BROOKESIA_STYLE_SIZE_*` macros to set */
    struct {
        uint8_t enable_default_screen: 1;       /*!< If this flag is enabled, when app starts, the core will create a
                                                     default screen which will be automatically loaded and cleaned up.
                                                     Otherwise, the app needs to create a new screen and load it
                                                     manually in app's `run()` function */
        uint8_t enable_recycle_resource: 1;     /*!< If this flag is enabled, when app closes, the core will cleaned up
                                                     all recorded resources(screens, timers, and animations) automatically.
                                                     These resources are recorded in app's `run()` and `pause()` functions,
                                                     or between the `startRecordResource()` and `stopRecordResource()`
                                                     functions.  Otherwise, the app needs to call `cleanRecordResource()`
                                                     function to clean manually */
        uint8_t enable_resize_visual_area: 1;   /*!< If this flag is enabled, the core will resize the visual area of
                                                     all recorded screens which are recorded in app's `run()` and `pause()`
                                                     functions, or between the `startRecordResource()` and `stopRecordResource()`
                                                     functions. This is useful when the screen displays floating UIs, such as a
                                                     status bar. Otherwise, the app's screens will be displayed in full screen,
                                                     but some areas might be not visible. The app can call the `getVisualArea()`
                                                     function to retrieve the final visual area */
    } flags;                                    /*!< Core app data flags */
} ESP_Brookesia_CoreAppData_t;

/**
 * @brief The default initializer for core app data structure
 *
 * @note The `enable_recycle_resource` and `enable_resize_visual_area` flags are enabled by default.
 * @note The `screen_size` is set to the full screen by default.
 *
 * @param app_name The name of the app
 * @param icon The icon image of the app
 * @param use_default_screen description
 *
 */
#define ESP_BROOKESIA_CORE_APP_DATA_DEFAULT(app_name, icon, use_default_screen) \
    {                                                                    \
        .name = app_name,                                                \
        .launcher_icon = ESP_BROOKESIA_STYLE_IMAGE(icon),                      \
        .screen_size = ESP_BROOKESIA_STYLE_SIZE_RECT_PERCENT(100, 100),         \
        .flags = {                                                       \
            .enable_default_screen = use_default_screen,                 \
            .enable_recycle_resource = 1,                                \
            .enable_resize_visual_area = 1,                              \
        },                                                               \
    }

typedef enum {
    ESP_BROOKESIA_CORE_APP_STATUS_UNINSTALLED = 0,
    ESP_BROOKESIA_CORE_APP_STATUS_RUNNING,
    ESP_BROOKESIA_CORE_APP_STATUS_PAUSED,
    ESP_BROOKESIA_CORE_APP_STATUS_CLOSED,
} ESP_Brookesia_CoreAppStatus_t;

typedef enum {
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_START = 0,
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_STOP,
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_OPERATION,
    ESP_BROOKESIA_CORE_APP_EVENT_TYPE_MAX,
} ESP_Brookesia_CoreAppEventType_t;

typedef struct {
    int id;
    ESP_Brookesia_CoreAppEventType_t type;
    void *data;
} ESP_Brookesia_CoreAppEventData_t;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// Core ////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    const char *name;
    ESP_Brookesia_StyleSize_t screen_size;
    ESP_Brookesia_CoreHomeData_t home;
    ESP_Brookesia_CoreManagerData_t manager;
} ESP_Brookesia_CoreData_t;

typedef enum {
    ESP_BROOKESIA_CORE_NAVIGATE_TYPE_BACK,
    ESP_BROOKESIA_CORE_NAVIGATE_TYPE_HOME,
    ESP_BROOKESIA_CORE_NAVIGATE_TYPE_RECENTS_SCREEN,
    ESP_BROOKESIA_CORE_NAVIGATE_TYPE_MAX,
} ESP_Brookesia_CoreNavigateType_t;

#ifdef __cplusplus
}
#endif
