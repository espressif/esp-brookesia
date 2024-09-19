/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>
#include "lvgl.h"
#include "esp_brookesia_conf_internal.h"
#include "esp_brookesia_core_type.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef unlikely
#define unlikely(x)  (x)
#endif

#if ESP_BROOKESIA_LOG_STYLE == ESP_BROOKESIA_LOG_STYLE_STD
#define LOGD(format, ...) printf("[DEBUG][%s:%d](%s): " format "\n", esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                 __LINE__, __func__,  ##__VA_ARGS__)
#define LOGI(format, ...) printf("[INFO] [%s:%d](%s): " format "\n", esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                 __LINE__, __func__,  ##__VA_ARGS__)
#define LOGW(format, ...) printf("[WARN] [%s:%d](%s): " format "\n", esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                 __LINE__, __func__,  ##__VA_ARGS__)
#define LOGE(format, ...) printf("[ERROR][%s:%d](%s): " format "\n", esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                 __LINE__, __func__,  ##__VA_ARGS__)
#elif ESP_BROOKESIA_LOG_STYLE == ESP_BROOKESIA_LOG_STYLE_ESP
#include "esp_log.h"
#define ESP_BROOKESIA_TAG      "esp-brookesia"
#define LOGD(format, ...) ESP_LOGD(ESP_BROOKESIA_TAG, "[%s:%d](%s)" format, esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                   __LINE__, __func__, ##__VA_ARGS__)
#define LOGI(format, ...) ESP_LOGI(ESP_BROOKESIA_TAG, "[%s:%d](%s)" format, esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                   __LINE__, __func__, ##__VA_ARGS__)
#define LOGW(format, ...) ESP_LOGW(ESP_BROOKESIA_TAG, "[%s:%d](%s)" format, esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                   __LINE__, __func__, ##__VA_ARGS__)
#define LOGE(format, ...) ESP_LOGE(ESP_BROOKESIA_TAG, "[%s:%d](%s)" format, esp_brookesia_core_utils_path_to_file_name(__FILE__), \
                                   __LINE__, __func__, ##__VA_ARGS__)
#elif ESP_BROOKESIA_LOG_STYLE == ESP_BROOKESIA_LOG_STYLE_LVGL
#include "lvgl.h"
#define LOGD(format, ...) LV_LOG_TRACE(format, ##__VA_ARGS__)
#define LOGI(format, ...) LV_LOG_INFO(format, ##__VA_ARGS__)
#define LOGW(format, ...) LV_LOG_WARN(format, ##__VA_ARGS__)
#define LOGE(format, ...) LV_LOG_ERROR(format, ##__VA_ARGS__)
#else
#error "Unknown log style, please check ESP_BROOKESIA_LOG_STYLE in file `esp_brookesia_conf_internal.h`"
#endif

#define LOG_LEVEL(level, format, ...) do {                                          \
        if      (level == ESP_BROOKESIA_LOG_LEVEL_DEBUG) { LOGD(format, ##__VA_ARGS__); }  \
        else if (level == ESP_BROOKESIA_LOG_LEVEL_INFO)  { LOGI(format, ##__VA_ARGS__); }  \
        else if (level == ESP_BROOKESIA_LOG_LEVEL_WARN)  { LOGW(format, ##__VA_ARGS__); }  \
        else if (level == ESP_BROOKESIA_LOG_LEVEL_ERROR) { LOGE(format, ##__VA_ARGS__); }  \
        else { }                                                                    \
    } while(0)

#define LOG_LEVEL_LOCAL(level, format, ...) do {                                \
        if (level >= ESP_BROOKESIA_LOG_LEVEL) LOG_LEVEL(level, format, ##__VA_ARGS__); \
    } while(0)

#define ESP_BROOKESIA_LOGD(format, ...) LOG_LEVEL_LOCAL(ESP_BROOKESIA_LOG_LEVEL_DEBUG, format, ##__VA_ARGS__)
#define ESP_BROOKESIA_LOGI(format, ...) LOG_LEVEL_LOCAL(ESP_BROOKESIA_LOG_LEVEL_INFO, format, ##__VA_ARGS__)
#define ESP_BROOKESIA_LOGW(format, ...) LOG_LEVEL_LOCAL(ESP_BROOKESIA_LOG_LEVEL_WARN, format, ##__VA_ARGS__)
#define ESP_BROOKESIA_LOGE(format, ...) LOG_LEVEL_LOCAL(ESP_BROOKESIA_LOG_LEVEL_ERROR, format, ##__VA_ARGS__)

#if ESP_BROOKESIA_CHECK_RESULT_ASSERT
#define ESP_BROOKESIA_CHECK_NULL_RETURN(x, ...)    assert(x)
#define ESP_BROOKESIA_CHECK_FALSE_RETURN(x, ...)   assert(x)
#define ESP_BROOKESIA_CHECK_VALUE_RETURN(x, min, max, ...) do { \
            __typeof__(x) _x = (x);                      \
            assert((_x >= min) && (_x <= max));          \
        } while(0)
#define ESP_BROOKESIA_CHECK_NULL_GOTO(x,  ...)     assert(x)
#define ESP_BROOKESIA_CHECK_FALSE_GOTO(x, ...)     assert(x)
#define ESP_BROOKESIA_CHECK_VALUE_GOTO(x, min, max, ...)   do { \
            __typeof__(x) _x = (x);                      \
            assert((_x >= min) && (_x <= max));          \
        } while(0)
#define ESP_BROOKESIA_CHECK_NULL_EXIT(x, ...)      assert(x)
#define ESP_BROOKESIA_CHECK_FALSE_EXIT(x, ...)     assert(x)
#define ESP_BROOKESIA_CHECK_VALUE_EXIT(x, min, max, ...) do {   \
            __typeof__(x) _x = (x);                      \
            assert((_x >= min) && (_x <= max));          \
        } while(0)
#else
/**
 * @brief Check if the pointer is NULL; if NULL, log an error and return the specified value.
 *
 * @param x Pointer to check
 * @param ret Value to return if the pointer is NULL
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_NULL_RETURN(x, ret, fmt, ...) do { \
            if ((x) == NULL) {                          \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);        \
                return ret;                             \
            }                                           \
        } while(0)

/**
 * @brief Check if the value is false; if false, log an error and return the specified value.
 *
 * @param x Value to check
 * @param ret Value to return if the value is false
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_FALSE_RETURN(x, ret, fmt, ...) do { \
            if ((x) == false) {                          \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);         \
                return ret;                              \
            }                                            \
        } while(0)

/**
 * @brief Check if the value is within the range [min, max]; if not, log an error and return the specified value.
 *
 * @param x Value to check
 * @param min Minimum acceptable value
 * @param max Maximum acceptable value
 * @param ret Value to return if the value is out of range
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_VALUE_RETURN(x, min, max, ret, fmt, ...) do { \
            __typeof__(x) _x = (x);                                \
            if ((_x < min) || (_x > max)) {                        \
                ESP_BROOKESIA_LOGE("Invalid value: %d, should be in range [%d, %d]", _x, min, max); \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);                   \
                return ret;                                        \
            }                                                      \
        } while(0)

/**
 * @brief Check if the pointer is NULL; if NULL, log an error and goto the specified label.
 *
 * @param x Pointer to check
 * @param goto_tag Label to jump to if the pointer is NULL
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_NULL_GOTO(x, goto_tag, fmt, ...) do { \
            if ((x) == NULL) {                             \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);           \
                goto goto_tag;                             \
            }                                              \
        } while(0)

/**
 * @brief Check if the value is false; if false, log an error and goto the specified label.
 *
 * @param x Value to check
 * @param goto_tag Label to jump to if the value is false
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_FALSE_GOTO(x, goto_tag, fmt, ...) do { \
            if (unlikely((x) == false)) {                   \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);            \
                goto goto_tag;                              \
            }                                               \
        } while(0)

/**
 * @brief Check if the value is within the range [min, max]; if not, log an error and goto the specified label.
 *
 * @param x Value to check
 * @param min Minimum acceptable value
 * @param max Maximum acceptable value
 * @param goto_tag Label to jump to if the value is out of range
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_VALUE_GOTO(x, min, max, goto_tag, fmt, ...) do { \
            __typeof__(x) _x = (x);                                   \
            if ((_x < min) || (_x > max)) {                           \
                ESP_BROOKESIA_LOGE("Invalid value: %d, should be in range [%d, %d]", _x, min, max); \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);                      \
                goto goto_tag;                                        \
            }                                                         \
        } while(0)

/**
 * @brief Check if the pointer is NULL; if NULL, log an error and return without a value.
 *
 * @param x Pointer to check
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_NULL_EXIT(x, fmt, ...) do { \
            if ((x) == NULL) {                   \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__); \
                return;                          \
            }                                    \
        } while(0)

/**
 * @brief Check if the value is false; if false, log an error and return without a value.
 *
 * @param x Value to check
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_FALSE_EXIT(x, fmt, ...) do { \
            if ((x) == false) {                   \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);  \
                return;                           \
            }                                     \
        } while(0)

/**
 * @brief Check if the value is within the range [min, max]; if not, log an error and return without a value.
 *
 * @param x Value to check
 * @param min Minimum acceptable value
 * @param max Maximum acceptable value
 * @param fmt Format string for the error message
 * @param ... Additional arguments for the format string
 */
#define ESP_BROOKESIA_CHECK_VALUE_EXIT(x, min, max, fmt, ...) do { \
            __typeof__(x) _x = (x);                         \
            if ((_x < min) || (_x > max)) {                 \
                ESP_BROOKESIA_LOGE("Invalid value: %d, should be in range [%d, %d]", _x, min, max); \
                ESP_BROOKESIA_LOGE(fmt, ##__VA_ARGS__);            \
                return;                                     \
            }                                               \
        } while(0)

#endif /* ESP_BROOKESIA_CHECK_RESULT_ASSERT */

const char *esp_brookesia_core_utils_path_to_file_name(const char *path);

bool esp_brookesia_core_utils_get_internal_font_by_size(uint8_t size_px, const lv_font_t **font);

lv_color_t esp_brookesia_core_utils_get_random_color(void);

bool esp_brookesia_core_utils_check_obj_out_of_parent(lv_obj_t *obj);

bool esp_brookesia_core_utils_check_event_code_valid(lv_event_code_t code);

lv_indev_t *esp_brookesia_core_utils_get_input_dev(const lv_disp_t *display, lv_indev_type_t type);

lv_anim_path_cb_t esp_brookesia_core_utils_get_anim_path_cb(ESP_Brookesia_LvAnimationPathType_t type);

#ifdef __cplusplus
}
#endif
