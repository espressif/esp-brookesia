/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

template <typename... Args>
constexpr void host_log_discard(Args &&...)
{
}

#define BROOKESIA_LOG_TRACE_GUARD() host_log_discard(0)
#define BROOKESIA_LOGI(format, ...) host_log_discard(format __VA_OPT__(, ) __VA_ARGS__)
#define BROOKESIA_LOGE(format, ...) host_log_discard(format __VA_OPT__(, ) __VA_ARGS__)
#define BROOKESIA_LOGW(format, ...) host_log_discard(format __VA_OPT__(, ) __VA_ARGS__)

#define BROOKESIA_CHECK_FALSE_RETURN(value, ret, fmt, ...) \
    do {                                                   \
        if (!(value)) {                                    \
            return (ret);                                  \
        }                                                  \
    } while (0)

#define BROOKESIA_CHECK_NULL_RETURN(value, ret, fmt, ...) \
    do {                                                  \
        if ((value) == nullptr) {                         \
            return (ret);                                 \
        }                                                 \
    } while (0)

#define BROOKESIA_CHECK_ESP_ERR_RETURN(value, ret, fmt, ...) \
    do {                                                     \
        if ((value) != ESP_OK) {                             \
            return (ret);                                    \
        }                                                    \
    } while (0)
