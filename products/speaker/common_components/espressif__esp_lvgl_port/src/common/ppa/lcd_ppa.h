/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief LCD PPA
 */

#pragma once
#include "driver/ppa.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct lvgl_port_ppa_t lvgl_port_ppa_t;
typedef lvgl_port_ppa_t *lvgl_port_ppa_handle_t;

/**
 * @brief Init configuration structure
 */
typedef struct {
    uint32_t        buffer_size;  /*!< Size of the buffer for the PPA */
    color_space_t   color_space;  /*!< Color space of input/output data */
    uint32_t        pixel_format; /*!< Pixel format of input/output data */
    struct {
        unsigned int buff_dma: 1;    /*!< Allocated buffer will be DMA capable */
        unsigned int buff_spiram: 1; /*!< Allocated buffer will be in PSRAM */
    } flags;
}  lvgl_port_ppa_cfg_t;

/**
 * @brief Display area structure
 */
typedef struct {
    uint16_t x1;
    uint16_t x2;
    uint16_t y1;
    uint16_t y2;
} lvgl_port_ppa_disp_area_t;

/**
 * @brief Display size structure
 */
typedef struct {
    uint32_t hres;
    uint32_t vres;
} lvgl_port_ppa_disp_size_t;

/**
 * @brief Rotation configuration
 */
typedef struct {
    uint8_t                   *in_buff;     /*!< Input buffer for rotation */
    lvgl_port_ppa_disp_area_t area;         /*!< Coordinates of area */
    lvgl_port_ppa_disp_size_t disp_size;    /*!< Display size */
    ppa_srm_rotation_angle_t  rotation;     /*!< Output rotation */
    ppa_trans_mode_t          ppa_mode;     /*!< Blocking or non-blocking mode */
    bool                      swap_bytes;   /*!< SWAP bytes  */
    void                      *user_data;
} lvgl_port_ppa_disp_rotate_t;


/**
 * @brief Initialize PPA
 *
 * @note This function initialize PPA SRM Client and create buffer for process.
 *
 * @param cfg          Configuration structure
 *
 * @return
 *      - PPA LCD handle
 */
lvgl_port_ppa_handle_t lvgl_port_ppa_create(const lvgl_port_ppa_cfg_t *cfg);

/**
 * @brief Remove PPA
 *
 * @param handle   PPA LCD handle
 *
 * @note This function free buffer and deinitialize PPA.
 */
void lvgl_port_ppa_delete(lvgl_port_ppa_handle_t handle);

/**
 * @brief Get output buffer
 *
 * @param handle   PPA LCD handle
 *
 * @note This function get allocated buffer for output of PPA operation.
 */
uint8_t *lvgl_port_ppa_get_output_buffer(lvgl_port_ppa_handle_t handle);

/**
 * @brief Do rotation
 *
 * @param handle   PPA LCD handle
 * @param rotate_cfg   Rotation settings
 *
 * @return
 *      - ESP_OK                    on success
 *      - ESP_ERR_NO_MEM            if memory allocation fails
 */
esp_err_t lvgl_port_ppa_rotate(lvgl_port_ppa_handle_t handle, lvgl_port_ppa_disp_rotate_t *rotate_cfg);

#ifdef __cplusplus
}
#endif
