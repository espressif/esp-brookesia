/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_err.h"
#include "esp_gmf_info.h"
#include "esp_gmf_oal_mem.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Tag of esp_gmf_info_file
 */
static const char *INFO_TAG = "ESP_GMF_INFO_FILE";

/**
 * @brief  Initialize the file information by given handle
 *
 * @param[in]  handle  Pointer to the file information handle to initialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_init(esp_gmf_info_file_t *handle)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if (handle->uri) {
        esp_gmf_oal_free((char *)handle->uri);
        handle->uri = NULL;
    }
    handle->pos = 0;
    handle->size = 0;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Deinitialize the file information by given handle
 *
 * @param[in]  handle  Pointer to the file information handle to deinitialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_deinit(esp_gmf_info_file_t *handle)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if (handle->uri) {
        esp_gmf_oal_free((char *)handle->uri);
    }
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Update the file position of the specific handle
 *
 * @param[in]  handle    Pointer to the file information handle
 * @param[in]  byte_pos  Byte position to update by
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_update_pos(esp_gmf_info_file_t *handle, uint64_t byte_pos)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    handle->pos += byte_pos;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Set the file position of the specific handle
 *
 * @param[in]  handle    Pointer to the file information handle
 * @param[in]  byte_pos  Byte position to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_set_pos(esp_gmf_info_file_t *handle, uint64_t byte_pos)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    handle->pos = byte_pos;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Get the position of the specific handle
 *
 * @param[in]   handle    Pointer to the file information handle
 * @param[out]  byte_pos  Pointer to store the byte position
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_get_pos(esp_gmf_info_file_t *handle, uint64_t *byte_pos)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    *byte_pos = handle->pos;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Set the total size of the specific handle
 *
 * @param[in]  handle      Pointer to the file information handle
 * @param[in]  total_size  Total size to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_set_size(esp_gmf_info_file_t *handle, uint64_t total_size)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    handle->size = total_size;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Get the total size of the specific handle
 *
 * @param[in]   handle      Pointer to the file information handle
 * @param[out]  total_size  Pointer to store the total size
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_get_size(esp_gmf_info_file_t *handle, uint64_t *total_size)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    *total_size = handle->size;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Set the URI of the specific handle
 *
 * @param[in]  handle  Pointer to the file information handle
 * @param[in]  uri     URI to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocation failed
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_set_uri(esp_gmf_info_file_t *handle, const char *uri)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    if (handle->uri) {
        esp_gmf_oal_free((void *)handle->uri);
        handle->uri = NULL;
    }
    if (uri) {
        handle->uri = esp_gmf_oal_strdup(uri);
        if (handle->uri == NULL) {
            return ESP_GMF_ERR_MEMORY_LACK;
        }
    }
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Get the URI of the specific handle
 *
 * @param[in]   handle  Pointer to the file information handle
 * @param[out]  uri     Pointer to store the URI
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid configuration provided
 */
static inline esp_gmf_err_t esp_gmf_info_file_get_uri(esp_gmf_info_file_t *handle, char **uri)
{
    ESP_GMF_NULL_CHECK(INFO_TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    *uri = (char *)handle->uri;
    return ESP_GMF_ERR_OK;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
