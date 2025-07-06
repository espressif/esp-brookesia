/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO., LTD
 * SPDX-License-Identifier: LicenseRef-Espressif-Modified-MIT
 *
 * See LICENSE file for details.
 */

#pragma once

#include "stdint.h"
#include "esp_gmf_err.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

/**
 * @brief  Structure representing metadata information for a GMF element
 */
typedef struct {
    void *content;  /*!< Pointer to the content */
    int   length;   /*!< Length of the content */
    void *ctx;      /*!< User context */
} esp_gmf_info_metadata_t;

/**
 * @brief  Structure representing sound-related information for a GMF element
 */
typedef struct {
    uint32_t  format_id;     /*!< Audio format FourCC representation refer to `esp_fourcc.h` for more details */
    int       sample_rates;  /*!< Sample rate */
    int       bitrate;       /*!< Bits per second */
    uint16_t  channels : 8;  /*!< Number of channels */
    uint16_t  bits     : 8;  /*!< Bit depth */
} esp_gmf_info_sound_t;

/**
 * @brief  Structure representing sound-related information for a GMF element
 */
typedef struct {
    uint32_t  format_id;  /*!< Video format FourCC representation refer to `esp_fourcc.h` for more details */
    uint16_t  height;     /*!< Height of the picture */
    uint16_t  width;      /*!< Width of the picture */
    uint16_t  fps;        /*!< Frame per sample */
    uint32_t  bitrate;    /*!< Bits per second */
} esp_gmf_info_video_t;

/**
 * @brief  Structure representing picture-related information for a GMF element
 */
typedef struct {
    uint16_t  height;  /*!< Height of the picture */
    uint16_t  width;   /*!< Width of the picture */
} esp_gmf_info_pic_t;

/**
 * @brief  Structure representing file-related information for a GMF element
 */
typedef struct {
    const char *uri;   /*!< Uniform Resource Identifier (URI) of the file */
    uint64_t    size;  /*!< Total size of the file */
    uint64_t    pos;   /*!< Byte position in the file */
} esp_gmf_info_file_t;

/**
 * @brief  Enum representing the type of information for a GMF element
 */
typedef enum {
    ESP_GMF_INFO_SOUND = 1,  /*!< Sound-related information */
    ESP_GMF_INFO_VIDEO = 2,  /*!< Video-related information */
    ESP_GMF_INFO_FILE  = 3,  /*!< File-related information */
    ESP_GMF_INFO_PIC   = 4,  /*!< Picture-related information */
} esp_gmf_info_type_t;

#ifdef __cplusplus
}
#endif  /* __cplusplus */
