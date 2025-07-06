/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_element.h"
#include "esp_gmf_info.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  GMF video element structure
 */
typedef struct _esp_gmf_video_element {
    struct esp_gmf_element  base;      /*!< Base element structure */
    esp_gmf_info_video_t    src_info;  /*!< Video input information */
    void                   *lock;      /*!< Lock for thread safety */
} esp_gmf_video_element_t;

/** GMF video element handle */
typedef void *esp_gmf_video_element_handle_t;

/**
 * @brief  Initialize a GMF video element with the given configuration
 *
 * @param[in]  handle  GMF video element handle to initialize
 * @param[in]  config  Pointer to the configuration structure
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocate failed
 */
esp_gmf_err_t esp_gmf_video_el_init(esp_gmf_video_element_handle_t handle, esp_gmf_element_cfg_t *config);

/**
 * @brief  Get video source information from a GMF video element
 *
 * @param[in]   handle  GMF video element handle
 * @param[out]  info    Pointer to store the video source information
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_video_el_get_src_info(esp_gmf_video_element_handle_t handle, esp_gmf_info_video_t *info);

/**
 * @brief  Set video source information from a GMF video element
 *
 * @param[in]   handle  GMF video element handle
 * @param[out]  info    Pointer to the video source information to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_video_el_set_src_info(esp_gmf_video_element_handle_t handle, esp_gmf_info_video_t *info);

/**
 * @brief  Deinitialize a GMF video element, freeing associated resources
 *
 * @param[in]  handle  GMF video element handle to deinitialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_video_el_deinit(esp_gmf_video_element_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
