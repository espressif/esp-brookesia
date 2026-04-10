/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdio.h>

/** @def BROOKESIA_LOGT_IMPL_FUNC
 *  @brief Emit a trace-level log message through the stdio backend.
 */
#if !defined(BROOKESIA_LOGT_IMPL_FUNC)
#   define BROOKESIA_LOGT_IMPL_FUNC(TAG, format, ...) printf("[T] [%s]\t" format "\n", TAG, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGD_IMPL_FUNC
 *  @brief Emit a debug-level log message through the stdio backend.
 */
#if !defined(BROOKESIA_LOGD_IMPL_FUNC)
#   define BROOKESIA_LOGD_IMPL_FUNC(TAG, format, ...) printf("[D] [%s]\t" format "\n", TAG, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGI_IMPL_FUNC
 *  @brief Emit an info-level log message through the stdio backend.
 */
#if !defined(BROOKESIA_LOGI_IMPL_FUNC)
#   define BROOKESIA_LOGI_IMPL_FUNC(TAG, format, ...) printf("[I] [%s]\t" format "\n", TAG, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGW_IMPL_FUNC
 *  @brief Emit a warning-level log message through the stdio backend.
 */
#if !defined(BROOKESIA_LOGW_IMPL_FUNC)
#   define BROOKESIA_LOGW_IMPL_FUNC(TAG, format, ...) printf("[W] [%s]\t" format "\n", TAG, ##__VA_ARGS__)
#endif
/** @def BROOKESIA_LOGE_IMPL_FUNC
 *  @brief Emit an error-level log message through the stdio backend.
 */
#if !defined(BROOKESIA_LOGE_IMPL_FUNC)
#   define BROOKESIA_LOGE_IMPL_FUNC(TAG, format, ...) printf("[E] [%s]\t" format "\n", TAG, ##__VA_ARGS__)
#endif
