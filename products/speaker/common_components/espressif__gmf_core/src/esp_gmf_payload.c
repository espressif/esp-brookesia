/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include "stdlib.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_payload.h"
#include "esp_log.h"
#include "string.h"

static const char *TAG = "ESP_GMF_PAYLOAD";

esp_gmf_err_t esp_gmf_payload_new(esp_gmf_payload_t **instance)
{
    if (instance == NULL) {
        ESP_LOGE(TAG, "Invalid parameters on %s, h:%p", __func__, instance);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *instance = (esp_gmf_payload_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_payload_t));
    ESP_LOGD(TAG, "New a payload, h:%p, called:0x%08x", *instance, (intptr_t)__builtin_return_address(0) - 2);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_payload_new_with_len(uint32_t buf_length, esp_gmf_payload_t **instance)
{
    if ((instance == NULL) || (buf_length == 0)) {
        ESP_LOGE(TAG, "Invalid parameters on %s, h:%p, l:%ld", __func__, instance, buf_length);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    *instance = (esp_gmf_payload_t *)esp_gmf_oal_calloc(1, sizeof(esp_gmf_payload_t));
    ESP_GMF_NULL_CHECK(TAG, *instance, return ESP_GMF_ERR_MEMORY_LACK);
    (*instance)->buf = esp_gmf_oal_calloc(1, buf_length);
    ESP_GMF_NULL_CHECK(TAG, (*instance)->buf, {
        esp_gmf_oal_free(*instance);
        return ESP_GMF_ERR_MEMORY_LACK;
    });
    (*instance)->buf_length = buf_length;
    (*instance)->needs_free = 1;
    ESP_LOGD(TAG, "New a payload with buffer, h:%p, buf:%p, l:%ld, called:0x%08x", *instance, (*instance)->buf, buf_length, (intptr_t)__builtin_return_address(0) - 2);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_payload_copy_data(esp_gmf_payload_t *src, esp_gmf_payload_t *dest)
{
    if ((src == NULL) || (dest == 0) || (src->buf == NULL) || (dest->buf == 0)) {
        ESP_LOGE(TAG, "Invalid parameters, src:%p-buf:%p, dest:%p-buf:%p", src, src->buf, dest, dest == NULL ? NULL : dest->buf);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    if (src->valid_size > dest->buf_length) {
        ESP_LOGE(TAG, "The valid size large than target buffer length, src:%p-l:%d, dest:%p-l:%d",
                 src, src->valid_size, dest, dest->buf_length);
        return ESP_GMF_ERR_OUT_OF_RANGE;
    }
    if (src->valid_size > 0) {
        memcpy(dest->buf, src->buf, src->valid_size);
    }
    dest->valid_size = src->valid_size;
    dest->is_done = src->is_done;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_payload_realloc_aligned_buf(esp_gmf_payload_t *instance, uint8_t align, uint32_t new_length)
{
    if ((instance == NULL) || (new_length == 0)) {
        ESP_LOGE(TAG, "Invalid parameters on %s, h:%p, l:%ld", __func__, instance, new_length);
        return ESP_GMF_ERR_INVALID_ARG;
    }
    // `buf` allocates by others
    if (instance->buf && (instance->needs_free == 0)) {
        ESP_LOGW(TAG, "Does not support reallocation of payload buffer that were allocated externally, p:%p, buf:%p, l:%d, new_l:%ld", instance, instance->buf, instance->buf_length, new_length);
        return ESP_GMF_ERR_NOT_SUPPORT;
    }
    uint8_t *buf = esp_gmf_oal_malloc_align(align, new_length);
    ESP_GMF_NULL_CHECK(TAG, buf, { return ESP_GMF_ERR_MEMORY_LACK;});
    if (instance->buf) {
        // There was no operation of memcpy valid size. Because this function is to expand the buffer size.
        ESP_LOGD(TAG, "Free payload:%p, buf:%p-%d, needs_free:%d", instance, instance->buf, instance->buf_length, instance->needs_free);
        esp_gmf_oal_free(instance->buf);
    }
    instance->needs_free = 1;
    instance->buf = buf;
    instance->buf_length = new_length;
    ESP_LOGD(TAG, "Realloc payload, h:%p, new_buf:%p-%ld, called:0x%08x", instance, instance->buf, new_length, (intptr_t)__builtin_return_address(0) - 2);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_payload_realloc_buf(esp_gmf_payload_t *instance, uint32_t new_length)
{
    return esp_gmf_payload_realloc_aligned_buf(instance, 1, new_length);
}

esp_gmf_err_t esp_gmf_payload_set_done(esp_gmf_payload_t *instance)
{
    ESP_GMF_NULL_CHECK(TAG, instance, return ESP_GMF_ERR_INVALID_ARG);
    instance->is_done = true;
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_payload_clean_done(esp_gmf_payload_t *instance)
{
    ESP_GMF_NULL_CHECK(TAG, instance, return ESP_GMF_ERR_INVALID_ARG);
    instance->is_done = false;
    return ESP_GMF_ERR_OK;
}

void esp_gmf_payload_delete(esp_gmf_payload_t *instance)
{
    ESP_LOGD(TAG, "Delete a payload, h:%p, needs_free:%d, buf:%p, l:%d", instance, instance != NULL ? instance->needs_free : -1,
             instance != NULL ? instance->buf : NULL, instance != NULL ? instance->buf_length : -1);
    if (instance) {
        if (instance->needs_free) {
            esp_gmf_oal_free(instance->buf);
            instance->needs_free = 0;
        }
        instance->buf = NULL;
        instance->buf_length = 0;
        esp_gmf_oal_free(instance);
    }
}
