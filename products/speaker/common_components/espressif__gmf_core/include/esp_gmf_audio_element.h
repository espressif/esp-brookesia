/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "esp_gmf_element.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  GMF audio element structure
 */
typedef struct _esp_gmf_audio_element {
    struct esp_gmf_element  base;       /*!< Base element structure */
    esp_gmf_info_sound_t    snd_info;   /*!< Sound information */
    esp_gmf_info_file_t     file_info;  /*!< File information */
    void                   *lock;       /*!< Lock for thread safety */
} esp_gmf_audio_element_t;

/** GMF audio element handle */
typedef void *esp_gmf_audio_element_handle_t;

/**
 * @brief  Initialize a GMF audio element with the given configuration
 *
 * @param[in]  handle  GMF audio element handle to initialize
 * @param[in]  config  Pointer to the configuration structure
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 *       - ESP_GMF_ERR_MEMORY_LACK  Memory allocate failed
 */
esp_gmf_err_t esp_gmf_audio_el_init(esp_gmf_audio_element_handle_t handle, esp_gmf_element_cfg_t *config);

/**
 * @brief  Get sound information from a GMF audio element
 *
 * @param[in]   handle  GMF audio element handle
 * @param[out]  info    Pointer to store the sound information
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_get_snd_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_sound_t *info);

/**
 * @brief  Set sound information for a GMF audio element
 *
 * @param[in]  handle  GMF audio element handle
 * @param[in]  info    Pointer to the sound information to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_set_snd_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_sound_t *info);

/**
 * @brief  Get file information from a GMF audio element
 *
 * @param[in]   handle  GMF audio element handle
 * @param[out]  info    Pointer to store the file information
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_get_file_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_file_t *info);

/**
 * @brief  Set file information for a GMF audio element
 *
 * @param[in]  handle  GMF audio element handle
 * @param[in]  info    Pointer to the file information to set
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_set_file_info(esp_gmf_audio_element_handle_t handle, esp_gmf_info_file_t *info);

/**
 * @brief  Set the file size for a GMF audio element
 *
 * @param[in]  handle  GMF audio element handle
 * @param[in]  size    New file size to update
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_set_file_size(esp_gmf_audio_element_handle_t handle, uint64_t size);

/**
 * @brief  Update the file position for a GMF audio element
 *
 * @param[in]  handle  GMF audio element handle
 * @param[in]  pos     New file position to update
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_update_file_pos(esp_gmf_audio_element_handle_t handle, uint64_t pos);

/**
 * @brief  Deinitialize a GMF audio element, freeing associated resources
 *
 * @param[in]  handle  GMF audio element handle to deinitialize
 *
 * @return
 *       - ESP_GMF_ERR_OK           On success
 *       - ESP_GMF_ERR_INVALID_ARG  Invalid argument
 */
esp_gmf_err_t esp_gmf_audio_el_deinit(esp_gmf_audio_element_handle_t handle);

#ifdef __cplusplus
}
#endif  /* __cplusplus */
