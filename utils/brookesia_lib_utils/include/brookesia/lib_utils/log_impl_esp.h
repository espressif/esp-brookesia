/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_log.h"

/** @def BROOKESIA_LOGT_IMPL_FUNC
 *  @brief Emit a trace-level log message through the ESP-IDF logging backend.
 */
#if !defined(BROOKESIA_LOGT_IMPL_FUNC)
#   define BROOKESIA_LOGT_IMPL_FUNC(TAG, format, ...) ESP_LOGV(TAG, format, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGD_IMPL_FUNC
 *  @brief Emit a debug-level log message through the ESP-IDF logging backend.
 */
#if !defined(BROOKESIA_LOGD_IMPL_FUNC)
#   define BROOKESIA_LOGD_IMPL_FUNC(TAG, format, ...) ESP_LOGD(TAG, format, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGI_IMPL_FUNC
 *  @brief Emit an info-level log message through the ESP-IDF logging backend.
 */
#if !defined(BROOKESIA_LOGI_IMPL_FUNC)
#   define BROOKESIA_LOGI_IMPL_FUNC(TAG, format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGW_IMPL_FUNC
 *  @brief Emit a warning-level log message through the ESP-IDF logging backend.
 */
#if !defined(BROOKESIA_LOGW_IMPL_FUNC)
#   define BROOKESIA_LOGW_IMPL_FUNC(TAG, format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGE_IMPL_FUNC
 *  @brief Emit an error-level log message through the ESP-IDF logging backend.
 */
#if !defined(BROOKESIA_LOGE_IMPL_FUNC)
#   define BROOKESIA_LOGE_IMPL_FUNC(TAG, format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#endif
