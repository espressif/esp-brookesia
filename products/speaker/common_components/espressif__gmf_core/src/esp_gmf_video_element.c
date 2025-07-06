/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#include <string.h>
#include "esp_gmf_video_element.h"
#include "esp_gmf_oal_mem.h"
#include "esp_gmf_oal_mutex.h"
#include "esp_log.h"

static const char *TAG = "ESP_GMF_VID_ELEMENT";

esp_gmf_err_t esp_gmf_video_el_init(esp_gmf_video_element_handle_t handle, esp_gmf_element_cfg_t *config)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, config, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_video_element_t *vid = (esp_gmf_video_element_t *)handle;
    config->ctx = (void *)vid;
    esp_gmf_element_init(&vid->base, config);
    vid->lock = esp_gmf_oal_mutex_create();
    ESP_GMF_MEM_CHECK(TAG, vid->lock, {
        esp_gmf_element_deinit(&vid->base);
        esp_gmf_oal_free(vid);
        return ESP_GMF_ERR_MEMORY_LACK;
    });
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_video_el_get_src_info(esp_gmf_video_element_handle_t handle, esp_gmf_info_video_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_video_element_t *vid = (esp_gmf_video_element_t *)handle;
    esp_gmf_oal_mutex_lock(vid->lock);
    memcpy(info, &vid->src_info, sizeof(esp_gmf_info_video_t));
    esp_gmf_oal_mutex_unlock(vid->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_video_el_set_src_info(esp_gmf_video_element_handle_t handle, esp_gmf_info_video_t *info)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    ESP_GMF_NULL_CHECK(TAG, info, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_video_element_t *vid = (esp_gmf_video_element_t *)handle;
    esp_gmf_oal_mutex_lock(vid->lock);
    memcpy(&vid->src_info, info, sizeof(esp_gmf_info_video_t));
    esp_gmf_oal_mutex_unlock(vid->lock);
    return ESP_GMF_ERR_OK;
}

esp_gmf_err_t esp_gmf_video_el_deinit(esp_gmf_video_element_handle_t handle)
{
    ESP_GMF_NULL_CHECK(TAG, handle, return ESP_GMF_ERR_INVALID_ARG);
    esp_gmf_video_element_t *vid = (esp_gmf_video_element_t *)handle;
    esp_gmf_oal_mutex_lock(vid->lock);
    esp_gmf_element_deinit(&vid->base);
    esp_gmf_oal_mutex_unlock(vid->lock);
    esp_gmf_oal_mutex_destroy(vid->lock);
    return ESP_GMF_ERR_OK;
}
