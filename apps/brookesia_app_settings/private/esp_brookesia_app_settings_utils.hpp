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
#define ESP_UTILS_LOG_TAG "BS:App:Settings"
#include "esp_lib_utils.h"
#if defined(ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG)
#   undef ESP_UTILS_LOGD
#   define ESP_UTILS_LOGD(...)
#endif
