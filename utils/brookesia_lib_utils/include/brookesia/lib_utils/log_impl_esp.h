/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "esp_log.h"

#if !defined(BROOKESIA_LOGT_IMPL_FUNC)
#   define BROOKESIA_LOGT_IMPL_FUNC(TAG, format, ...) ESP_LOGV(TAG, format, ##__VA_ARGS__)
#endif
#if !defined(BROOKESIA_LOGD_IMPL_FUNC)
#   define BROOKESIA_LOGD_IMPL_FUNC(TAG, format, ...) ESP_LOGD(TAG, format, ##__VA_ARGS__)
#endif
#if !defined(BROOKESIA_LOGI_IMPL_FUNC)
#   define BROOKESIA_LOGI_IMPL_FUNC(TAG, format, ...) ESP_LOGI(TAG, format, ##__VA_ARGS__)
#endif
#if !defined(BROOKESIA_LOGW_IMPL_FUNC)
#   define BROOKESIA_LOGW_IMPL_FUNC(TAG, format, ...) ESP_LOGW(TAG, format, ##__VA_ARGS__)
#endif
#if !defined(BROOKESIA_LOGE_IMPL_FUNC)
#   define BROOKESIA_LOGE_IMPL_FUNC(TAG, format, ...) ESP_LOGE(TAG, format, ##__VA_ARGS__)
#endif
