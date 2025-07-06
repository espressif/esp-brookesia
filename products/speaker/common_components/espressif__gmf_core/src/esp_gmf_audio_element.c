/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_gmf_audio_element.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_mutex.h"
#include "esp_gmf_info_file.h"
#include "esp_log.h"

static const char *TAG = "ESP_GMF_AUD_ELEMENT";

esp_gmf_err_t esp_gmf_audio_el_init(esp_gmf_audio_element_handle_t handle, esp_gmf_element_cfg_t *config)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, config, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    config->ctx = (void *)aud;
    esp_gmf_element_init(&aud->base, config);
    esp_gmf_info_file_init(&aud->file_info);
    aud->lock = esp_gmf_oal_mutex_create();
    ESP_GMF_MEM_CHECK(TAG, aud->lock, {
        esp_gmf_element_deinit(&aud->base);
        esp_gmf_oal_free(aud);
        return ESP_GMF_ERR_MEMORY_LACK;
    });
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_audio_el_get_snd_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_sound_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    memcpy(info, &aud->snd_info, sizeof(aud->snd_info));
    esp_gmf_oal_mutex_unlock(aud->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_audio_el_set_snd_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_sound_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    memcpy(&aud->snd_info, info, sizeof(aud->snd_info));
    esp_gmf_oal_mutex_unlock(aud->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_audio_el_get_file_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_file_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    memcpy(info, &aud->file_info, sizeof(aud->file_info));
    esp_gmf_oal_mutex_unlock(aud->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_audio_el_set_file_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_file_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    memcpy(&aud->file_info, info, sizeof(aud->file_info));
    esp_gmf_oal_mutex_unlock(aud->lock);
    return ESP_GMF_ERR_OK;
}
esp_gmf_err_t esp_gmf_audio_el_set_file_size(esp_gmf_audio_element_handle_t handle, uint64_t size)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    int ret = esp_gmf_info_file_set_size(&aud->file_info, size);
    esp_gmf_oal_mutex_unlock(aud->lock);
    return ret;
}

esp_gmf_err_t esp_gmf_audio_el_update_file_pos(esp_gmf_audio_element_handle_t handle, uint64_t pos)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    int ret = esp_gmf_info_file_update_pos(&aud->file_info, pos);
    esp_gmf_oal_mutex_unlock(aud->lock);
    return ret;
}

esp_gmf_err_t esp_gmf_audio_el_deinit(esp_gmf_audio_element_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_audio_element_t *aud = (esp_gmf_audio_element_t *)handle;
    esp_gmf_oal_mutex_lock(aud->lock);
    esp_gmf_element_deinit(&aud->base);
    esp_gmf_info_file_deinit(&aud->file_info);
    esp_gmf_oal_mutex_unlock(aud->lock);
    esp_gmf_oal_mutex_destroy(aud->lock);
    return ESP_GMF_ERR_OK;
}
