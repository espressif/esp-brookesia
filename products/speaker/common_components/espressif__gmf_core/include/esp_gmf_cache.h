/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include <string.h>
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_payload.h"

/**
 * @brief  This GMF payload cache module is designed to cache a data block of a specified size. Each call to acquire returns this
 *         fixed-size block. If the acquired data is smaller than the expected size, new data must be loaded. When sufficient data
 *         is available, release clears the cached block.
 *         It is commonly used in scenarios where an element can only process a fixed data size, but the input data length is variable.
 *         For example, if an element requires exactly 1350 bytes per processing cycle but receives input of varying lengths, you can
 *         create a cache with `esp_gmf_cache_new(1350, out_handle)`, and the element can then fetch data using `esp_gmf_cache_acquire`
 */

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Structure representing a cache for GMF payloads
 */
typedef struct {
    uint8_t           *buf;          /*!< Pointer to the cache buffer */
    uint32_t           buf_len;      /*!< Total allocated size of the cache buffer */
    uint32_t           buf_filled;   /*!< Amount of data currently stored in the buffer */
    esp_gmf_payload_t  origin_load;  /*!< Original payload from which data is copied */
    esp_gmf_payload_t  load;         /*!< Current payload structure for acquiring data */
} esp_gmf_cache_t;

/**
 * @brief  Create a new GMF cache instance
 *
 * @param[in]   len     The size of the cache buffer to allocate
 * @param[out]  handle  Pointer to the cache handle to be initialized
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, cache is initialized
 *       - ESP_GMF_ERR_INVALID_ARG  If `handle` is NULL
 *       - ESP_GMF_ERR_MEMORY_LACK  If memory allocation fails
 */
static inline esp_gmf_err_t esp_gmf_cache_new(uint32_t len, esp_gmf_cache_t **handle)
{
    if (handle == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *handle = (esp_gmf_cache_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_cache_t));
    if (*handle == NULL) {
        ESP_LOGE("GMF_CACHE", "Failed to allocate the cache instance");
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    (*handle)->buf = (uint8_t *)esp_gmf_oal_calloc(1, len);
    if ((*handle)->buf == NULL) {
        esp_gmf_oal_free(*handle);
        ESP_LOGE("GMF_CACHE", "Failed to allocate the cache buffer, size:%ld", len);
        *handle = NULL;
        return ESP_GMF_ERR_MEMORY_LACK;
    }
    (*handle)->buf_len = len;
    (*handle)->buf_filled = 0;
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Delete a GMF cache instance and free associated memory
 *
 * @param[in]  handle  Pointer to the cache handle
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, cache is deleted
 *       - ESP_GMF_ERR_INVALID_ARG  If `handle` is NULL
 */
static inline esp_gmf_err_t esp_gmf_cache_delete(esp_gmf_cache_t *handle)
{
    if (handle == NULL) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    if (handle->buf) {
        esp_gmf_oal_free(handle->buf);
    }
    esp_gmf_oal_free(handle);
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Check if the cache is ready to load new data
 *         If the return `is_ready` is true, `esp_gmf_cache_load` can be called to load a new payload
 *
 * @param[in]   handle    Pointer to the cache handle
 * @param[out]  is_ready  Pointer to a boolean flag indicating whether new data can be loaded
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, `is_ready` is set
 *       - ESP_GMF_ERR_INVALID_ARG  If `handle` or `is_ready` is NULL
 */
static inline esp_gmf_err_t esp_gmf_cache_ready_for_load(esp_gmf_cache_t *handle, bool *is_ready)
{
    if ((handle == NULL) || (is_ready == NULL)) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *is_ready = false;
    if (handle->origin_load.valid_size == 0) {
        *is_ready = true;
    }
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Load new payload data into the cache
 *         Call `esp_gmf_cache_ready_for_load` before use to check if it is ready to load a new payload
 *
 * @param[in]  handle   Pointer to the cache handle
 * @param[in]  load_in  Pointer to the payload data to be loaded
 *
 * @return
 *       - ESP_GMF_ERR_OK             Success, payload is loaded
 *       - ESP_GMF_ERR_INVALID_ARG    If `handle` or `load_in` is NULL
 *       - ESP_GMF_ERR_INVALID_STATE  If previous data has not been consumed
 */
static inline esp_gmf_err_t esp_gmf_cache_load(esp_gmf_cache_t *handle, const esp_gmf_payload_t *load_in)
{
    if ((handle == NULL) || (load_in == NULL)) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    if (handle->origin_load.valid_size != 0) {
        ESP_LOGE("GMF_CACHE", "Reloading while previous load underuse, call esp_gmf_cache_ready_for_load check firstly, filled: %ld,\
                orig_valid: %d", handle->buf_filled, handle->origin_load.valid_size);
        return ESP_GMF_ERR_INVALID_STATE;
    }
    memcpy(&handle->origin_load, load_in, sizeof(esp_gmf_payload_t));
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Acquire a data chunk of the expected size from the cache
 *
 * @note
 *        - This function must be used in conjunction with `esp_gmf_cache_release`
 *        - The `expected_size` should match the internal buffer length
 *        - If `expected_size` exceeds the current buffer length, the buffer will be reallocated
 *
 * @param[in]   handle         Pointer to the cache handle
 * @param[in]   expected_size  The requested data size in bytes
 * @param[out]  load_out       Pointer to store the acquired payload data
 *
 * @return
 *       - ESP_GMF_ERR_OK           Data successfully acquired
 *       - ESP_GMF_ERR_INVALID_ARG  If `handle` or `load_out` is NULL
 *       - ESP_GMF_ERR_MEMORY_LACK  Insufficient memory to allocate a new buffer
 */
static inline esp_gmf_err_t esp_gmf_cache_acquire(esp_gmf_cache_t *handle, uint32_t expected_size, esp_gmf_payload_t **load_out)
{
    if ((handle == NULL) || (load_out == NULL)) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    if (expected_size > handle->buf_len) {
        uint8_t *buf = (uint8_t *)esp_gmf_oal_realloc(handle->buf, expected_size);
        ESP_GMF_MEM_CHECK("GMF_CACHE", buf, return ESP_GMF_ERR_MEMORY_LACK;);
        handle->buf = buf;
        ESP_LOGI("GMF_CACHE", "Reallocate the cache buffer from %ld to %ld bytes, %p", handle->buf_len, expected_size, handle->buf);
        handle->buf_len = expected_size;
    }

    *load_out = &handle->load;
    ESP_LOGD("GMF_CACHE", "ACQ, filled: %ld, Origin_valid_size: %d", handle->buf_filled, handle->origin_load.valid_size);

    /**
     *  1. If the original buffer has sufficient data, return its address directly to the user
     *  2. Copy the remaining data from the original buffer to the cache buffer
     *  3. If the original buffer does not have enough data for the user, provide the cached buffer address instead
     */

    if (handle->buf_filled == 0) {
        if (handle->origin_load.valid_size >= expected_size) {
            (*load_out)->buf = handle->origin_load.buf;
            (*load_out)->buf_length = expected_size;
            (*load_out)->valid_size = expected_size;
            (*load_out)->is_done = (handle->origin_load.valid_size == expected_size) ? handle->origin_load.is_done : false;
            (*load_out)->pts = handle->origin_load.pts;
            handle->origin_load.valid_size -= expected_size;
            handle->origin_load.buf += expected_size;
        }
    }
    if (handle->buf_filled || (handle->origin_load.valid_size < expected_size)) {
        int n = (handle->buf_len - handle->buf_filled) > handle->origin_load.valid_size
            ? handle->origin_load.valid_size
            : (handle->buf_len - handle->buf_filled);
        memcpy(handle->buf + handle->buf_filled, handle->origin_load.buf, n);
        handle->origin_load.buf += n;
        handle->origin_load.valid_size -= n;
        handle->buf_filled += n;
        ESP_LOGD("GMF_CACHE", "ACQ, filled: %ld, used size: %d, origin_left_size: %d", handle->buf_filled, n, handle->origin_load.valid_size);
    }
    if ((*load_out)->valid_size == 0) {
        (*load_out)->buf = handle->buf;
        (*load_out)->buf_length = handle->buf_len;
        (*load_out)->valid_size = handle->buf_filled;
        // FIXME If the data is split into several pieces, the pts remains the same.
        (*load_out)->pts = handle->origin_load.pts;
        (*load_out)->is_done = (handle->origin_load.valid_size == 0) ? handle->origin_load.is_done : false;
    }

    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Release the payload data previously acquired with `esp_gmf_cache_acquire`
 *
 * @note  The filled portion of the buffer is cleared only if its size equals the buffer length
 *
 * @param[in]  handle  Pointer to the cache handle
 * @param[in]  load    Pointer to the payload data to be released
 *
 * @return
 *       - ESP_GMF_ERR_OK           Payload data successfully released
 *       - ESP_GMF_ERR_INVALID_ARG  If either `handle` or `load` is NULL
 */
static inline esp_gmf_err_t esp_gmf_cache_release(esp_gmf_cache_t *handle, esp_gmf_payload_t *load)
{
    if ((handle == NULL) || (load == NULL)) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    ESP_LOGD("GMF_CACHE", "RLS, buf:%p-%p, filled: %ld, origin_valid_size: %d", load->buf, handle->buf, handle->buf_filled, handle->origin_load.valid_size);
    if ((load->buf == handle->buf) && (handle->buf_filled == handle->buf_len)) {
        handle->buf_filled = 0;
    }
    memset(&handle->load, 0, sizeof(esp_gmf_payload_t));
    return ESP_GMF_ERR_OK;
}

/**
 * @brief  Get the total amount of cached data
 *
 * @param[in]   handle  Pointer to the cache handle
 * @param[out]  filled  Pointer to store the filled data size
 *
 * @return
 *       - ESP_GMF_ERR_OK           Success, `filled` is set
 *       - ESP_GMF_ERR_INVALID_ARG  If `handle` or `filled` is NULL
 */
static inline esp_gmf_err_t esp_gmf_cache_get_cached_size(esp_gmf_cache_t *handle, int *filled)
{
    if ((handle == NULL) || (filled == NULL)) {
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *filled = handle->buf_filled + handle->origin_load.valid_size;
    return ESP_GMF_ERR_OK;
}

#ifdef __cplusplus
}
#endif  /* __cplusplus */
