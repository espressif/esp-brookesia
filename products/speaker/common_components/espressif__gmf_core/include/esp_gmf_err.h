/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

#define ESP_GMF_ERR_BASE      (-0x2000)
#define ESP_GMF_ERR_CORE_BASE (ESP_GMF_ERR_BASE + 0x0)

/**
 * @brief  Error codes for GMF IO operations
 */
typedef enum {
    ESP_GMF_IO_OK      = ESP_OK,    /*!< Operation successful */
    ESP_GMF_IO_FAIL    = ESP_FAIL,  /*!< Operation failed */
    ESP_GMF_IO_TIMEOUT = -2,        /*!< Operation timed out */
    ESP_GMF_IO_ABORT   = -3,        /*!< Operation aborted */
} esp_gmf_err_io_t;

typedef enum {
    ESP_GMF_ERR_OK             = ESP_OK,
    ESP_GMF_ERR_FAIL           = ESP_FAIL,
    ESP_GMF_ERR_TIMEOUT        = ESP_GMF_IO_TIMEOUT,
    ESP_GMF_ERR_UNKNOWN        = ESP_GMF_ERR_CORE_BASE - 0,
    ESP_GMF_ERR_ALREADY_EXISTS = ESP_GMF_ERR_CORE_BASE - 1,
    ESP_GMF_ERR_MEMORY_LACK    = ESP_GMF_ERR_CORE_BASE - 2,
    ESP_GMF_ERR_INVALID_URI    = ESP_GMF_ERR_CORE_BASE - 3,
    ESP_GMF_ERR_INVALID_PATH   = ESP_GMF_ERR_CORE_BASE - 4,
    ESP_GMF_ERR_INVALID_ARG    = ESP_GMF_ERR_CORE_BASE - 5,
    ESP_GMF_ERR_INVALID_STATE  = ESP_GMF_ERR_CORE_BASE - 6,
    ESP_GMF_ERR_OUT_OF_RANGE   = ESP_GMF_ERR_CORE_BASE - 7,
    ESP_GMF_ERR_NOT_READY      = ESP_GMF_ERR_CORE_BASE - 8,
    ESP_GMF_ERR_NOT_SUPPORT    = ESP_GMF_ERR_CORE_BASE - 9,
    ESP_GMF_ERR_NOT_FOUND      = ESP_GMF_ERR_CORE_BASE - 10,
    ESP_GMF_ERR_NOT_ENOUGH     = ESP_GMF_ERR_CORE_BASE - 12,
    ESP_GMF_ERR_NO_DATA        = ESP_GMF_ERR_CORE_BASE - 13,
} esp_gmf_err_t;

#define ESP_GMF_CHECK(TAG, a, action, msg) if (!(a)) {                           \
    ESP_LOGE(TAG, "%s:%d (%s): %s", __FILENAME__, __LINE__, __FUNCTION__, msg);  \
    action;                                                                      \
}

#define ESP_GMF_RET_ON_FAIL(TAG, a, action, msg) if (unlikely(a == ESP_FAIL)) {  \
    ESP_LOGE(TAG, "%s:%d (%s): %s", __FILENAME__, __LINE__, __FUNCTION__, msg);  \
    action;                                                                      \
}

#define ESP_GMF_RET_ON_NOT_OK(TAG, a, action, msg) if (unlikely(a != ESP_OK)) {  \
    ESP_LOGE(TAG, "%s:%d (%s): %s", __FILENAME__, __LINE__, __FUNCTION__, msg);  \
    action;                                                                      \
}

#define ESP_GMF_RET_ON_ERROR(log_tag, a, action, format, ...) do {                                \
    esp_err_t err_rc_ = (a);                                                                      \
    if (unlikely(err_rc_ != ESP_OK)) {                                                            \
        ESP_LOGE(log_tag, "%s(%d): " format, __FUNCTION__, __LINE__ __VA_OPT__(, ) __VA_ARGS__);  \
        action;                                                                                   \
    }                                                                                             \
} while (0)

#define ESP_GMF_MEM_VERIFY(TAG, a, action, name, size) if (!(a)) {                                       \
    ESP_LOGE(TAG, "%s(%d): Failed to allocate memory for %s(%d).", __FUNCTION__, __LINE__, name, size);  \
    action;                                                                                              \
}

#define ESP_GMF_MEM_CHECK(TAG, a, action) ESP_GMF_CHECK(TAG, a, action, "Memory exhausted")

#define ESP_GMF_NULL_CHECK(TAG, a, action) ESP_GMF_CHECK(TAG, a, action, "Got NULL Pointer")

#ifdef __cplusplus
}
#endif  /* __cplusplus */
