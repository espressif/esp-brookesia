/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

/**
 * @brief This file contains utility functions for Brookesia, for internal use only and
 *        should not be included by other files
 */

#ifdef ESP_UTILS_LOG_TAG
#   undef ESP_UTILS_LOG_TAG
#endif
#define ESP_UTILS_LOG_TAG "Brookesia"
#include "esp_lib_utils.h"
#if defined(ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG)
#   undef ESP_UTILS_LOGD
#   define ESP_UTILS_LOGD(...)
#endif

#define ESP_BROOKESIA_LOGD(format, ...) ESP_UTILS_LOGD(format, ##__VA_ARGS__)
#define ESP_BROOKESIA_LOGI(format, ...) ESP_UTILS_LOGI(format, ##__VA_ARGS__)
#define ESP_BROOKESIA_LOGW(format, ...) ESP_UTILS_LOGW(format, ##__VA_ARGS__)
#define ESP_BROOKESIA_LOGE(format, ...) ESP_UTILS_LOGE(format, ##__VA_ARGS__)

#define ESP_BROOKESIA_CHECK_NULL_RETURN(x, ret, ...)        ESP_UTILS_CHECK_NULL_RETURN(x, ret, ##__VA_ARGS__)
#define ESP_BROOKESIA_CHECK_FALSE_RETURN(x, ret, ...)       ESP_UTILS_CHECK_FALSE_RETURN(x, ret, ##__VA_ARGS__)
#define ESP_BROOKESIA_CHECK_NULL_GOTO(x, goto_tag, ...)     ESP_UTILS_CHECK_NULL_GOTO(x, goto_tag, ##__VA_ARGS__)
#define ESP_BROOKESIA_CHECK_FALSE_GOTO(x, goto_tag, ...)    ESP_UTILS_CHECK_FALSE_GOTO(x, goto_tag, ##__VA_ARGS__)
#define ESP_BROOKESIA_CHECK_NULL_EXIT(x, ...)               ESP_UTILS_CHECK_NULL_EXIT(x, ##__VA_ARGS__)
#define ESP_BROOKESIA_CHECK_FALSE_EXIT(x, ...)              ESP_UTILS_CHECK_FALSE_EXIT(x, ##__VA_ARGS__)

#define ESP_BROOKESIA_LOG_TRACE_ENTER() ESP_UTILS_LOG_TRACE_ENTER()
#define ESP_BROOKESIA_LOG_TRACE_EXIT()  ESP_UTILS_LOG_TRACE_EXIT()
#define ESP_BROOKESIA_LOG_TRACE_ENTER_WITH_THIS() ESP_UTILS_LOG_TRACE_ENTER_WITH_THIS()
#define ESP_BROOKESIA_LOG_TRACE_EXIT_WITH_THIS()  ESP_UTILS_LOG_TRACE_EXIT_WITH_THIS()

/**
 * @brief Check if the value is within the range [min, max]; if not, log an error and return the specified value.
 *
 * @param x Value to check
 * @param min Minimum acceptable value
 * @param max Maximum acceptable value
 */
#define ESP_BROOKESIA_CHECK_VALUE(x, min, max) do { \
            __typeof__(x) __x = (x);                                \
            if ((__x < min) || (__x > max)) {                        \
                ESP_BROOKESIA_LOGE("Invalid value: %d, should be in range [%d, %d]", __x, min, max); \
            }                                                      \
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
            ESP_BROOKESIA_CHECK_VALUE(_x, min, max); \
            ESP_BROOKESIA_CHECK_FALSE_RETURN((_x >= min) && (_x <= max), ret, fmt, ##__VA_ARGS__); \
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
            ESP_BROOKESIA_CHECK_VALUE(_x, min, max); \
            ESP_BROOKESIA_CHECK_FALSE_GOTO((_x >= min) && (_x <= max), goto_tag, fmt, ##__VA_ARGS__); \
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
            ESP_BROOKESIA_CHECK_VALUE(_x, min, max); \
            ESP_BROOKESIA_CHECK_FALSE_EXIT((_x >= min) && (_x <= max), fmt, ##__VA_ARGS__); \
        } while(0)
