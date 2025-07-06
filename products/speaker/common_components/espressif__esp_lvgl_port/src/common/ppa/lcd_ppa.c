/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */


#include <string.h>
#include "esp_err.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "soc/soc_caps.h"
#include "lcd_ppa.h"

#define PPA_LCD_ENABLE_CB   0

#if SOC_PPA_SUPPORTED
#define ALIGN_UP(num, align)    (((num) + ((align) - 1)) & ~((align) - 1))

struct lvgl_port_ppa_t {
    uint8_t             *buffer;
    uint32_t            buffer_size;
    ppa_client_handle_t srm_handle;
    uint32_t            color_type_id;
};

static const char *TAG = "PPA";
/*******************************************************************************
* Function definitions
*******************************************************************************/
#if PPA_LCD_ENABLE_CB
static bool _lvgl_port_ppa_callback(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data);
#endif
/*******************************************************************************
* Public API functions
*******************************************************************************/

lvgl_port_ppa_handle_t lvgl_port_ppa_create(const lvgl_port_ppa_cfg_t *cfg)
{
    esp_err_t ret = ESP_OK;
    assert(cfg != NULL);

    lvgl_port_ppa_t *ppa_ctx = malloc(sizeof(lvgl_port_ppa_t));
    ESP_GOTO_ON_FALSE(ppa_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for PPA context allocation!");
    memset(ppa_ctx, 0, sizeof(lvgl_port_ppa_t));

    uint32_t buffer_caps = 0;
    if (cfg->flags.buff_dma) {
        buffer_caps |= MALLOC_CAP_DMA;
    }
    if (cfg->flags.buff_spiram) {
        buffer_caps |= MALLOC_CAP_SPIRAM;
    }
    if (buffer_caps == 0) {
        buffer_caps |= MALLOC_CAP_DEFAULT;
    }

    ppa_ctx->buffer_size = ALIGN_UP(cfg->buffer_size, CONFIG_CACHE_L2_CACHE_LINE_SIZE);
    ppa_ctx->buffer = heap_caps_aligned_calloc(CONFIG_CACHE_L2_CACHE_LINE_SIZE, ppa_ctx->buffer_size, sizeof(uint8_t), buffer_caps);
    assert(ppa_ctx->buffer != NULL);

    ppa_client_config_t ppa_client_config = {
        .oper_type = PPA_OPERATION_SRM,
    };
    ESP_GOTO_ON_ERROR(ppa_register_client(&ppa_client_config, &ppa_ctx->srm_handle), err, TAG, "Error when registering PPA client!");

#if PPA_LCD_ENABLE_CB
    ppa_event_callbacks_t ppa_cbs = {
        .on_trans_done = _lvgl_port_ppa_callback,
    };
    ESP_GOTO_ON_ERROR(ppa_client_register_event_callbacks(ppa_ctx->srm_handle, &ppa_cbs), err, TAG, "Error when registering PPA callbacks!");
#endif

    ppa_ctx->color_type_id = COLOR_TYPE_ID(cfg->color_space, cfg->pixel_format);

err:
    if (ret != ESP_OK) {
        if (ppa_ctx->buffer) {
            free(ppa_ctx->buffer);
        }
        if (ppa_ctx) {
            free(ppa_ctx);
        }
    }

    return ppa_ctx;
}

void lvgl_port_ppa_delete(lvgl_port_ppa_handle_t handle)
{
    lvgl_port_ppa_t *ppa_ctx = (lvgl_port_ppa_t *)handle;
    assert(ppa_ctx != NULL);

    if (ppa_ctx->buffer) {
        free(ppa_ctx->buffer);
    }

    ppa_unregister_client(ppa_ctx->srm_handle);

    free(ppa_ctx);
}

uint8_t *lvgl_port_ppa_get_output_buffer(lvgl_port_ppa_handle_t handle)
{
    lvgl_port_ppa_t *ppa_ctx = (lvgl_port_ppa_t *)handle;
    assert(ppa_ctx != NULL);
    return ppa_ctx->buffer;
}

esp_err_t lvgl_port_ppa_rotate(lvgl_port_ppa_handle_t handle, lvgl_port_ppa_disp_rotate_t *rotate_cfg)
{
    lvgl_port_ppa_t *ppa_ctx = (lvgl_port_ppa_t *)handle;
    assert(ppa_ctx != NULL);
    assert(rotate_cfg != NULL);
    const int w = rotate_cfg->area.x2 - rotate_cfg->area.x1 + 1;
    const int h = rotate_cfg->area.y2 - rotate_cfg->area.y1 + 1;

    /* Set dimension by screen size and rotation */
    int out_w = w;
    int out_h = h;

    int x1 = rotate_cfg->area.x1;
    int x2 = rotate_cfg->area.x2;
    int y1 = rotate_cfg->area.y1;
    int y2 = rotate_cfg->area.y2;

    /* Rotate coordinates */
    switch (rotate_cfg->rotation) {
    case PPA_SRM_ROTATION_ANGLE_0:
        break;
    case PPA_SRM_ROTATION_ANGLE_90:
        out_w = h;
        out_h = w;
        x1 = rotate_cfg->area.y1;
        x2 = rotate_cfg->area.y2;
        y1 = rotate_cfg->disp_size.hres - rotate_cfg->area.x2 - 1;
        y2 = rotate_cfg->disp_size.hres - rotate_cfg->area.x1 - 1;
        break;
    case PPA_SRM_ROTATION_ANGLE_180:
        x1 = rotate_cfg->disp_size.hres - rotate_cfg->area.x2 - 1;
        x2 = rotate_cfg->disp_size.hres - rotate_cfg->area.x1 - 1;
        y1 = rotate_cfg->disp_size.vres - rotate_cfg->area.y2 - 1;
        y2 = rotate_cfg->disp_size.vres - rotate_cfg->area.y1 - 1;
        break;
    case PPA_SRM_ROTATION_ANGLE_270:
        out_w = h;
        out_h = w;
        x1 = rotate_cfg->disp_size.vres - rotate_cfg->area.y2 - 1;
        x2 = rotate_cfg->disp_size.vres - rotate_cfg->area.y1 - 1;
        y1 = rotate_cfg->area.x1;
        y2 = rotate_cfg->area.x2;
        break;
    }
    /* Return new coordinates */
    rotate_cfg->area.x1 = x1;
    rotate_cfg->area.x2 = x2;
    rotate_cfg->area.y1 = y1;
    rotate_cfg->area.y2 = y2;

    /* Prepare Operation     */
    ppa_srm_oper_config_t srm_oper_config = {
        .in.buffer = rotate_cfg->in_buff,
        .in.pic_w = w,
        .in.pic_h = h,
        .in.block_w = w,
        .in.block_h = h,
        .in.block_offset_x = 0,
        .in.block_offset_y = 0,
        .in.srm_cm = ppa_ctx->color_type_id,

        .out.buffer = ppa_ctx->buffer,
        .out.buffer_size = ppa_ctx->buffer_size,
        .out.pic_w = out_w,
        .out.pic_h = out_h,
        .out.block_offset_x = 0,
        .out.block_offset_y = 0,
        .out.srm_cm = ppa_ctx->color_type_id,

        .rotation_angle = rotate_cfg->rotation,
        .scale_x = 1.0,
        .scale_y = 1.0,

        .byte_swap = rotate_cfg->swap_bytes,

        .mode = rotate_cfg->ppa_mode,
        .user_data = rotate_cfg->user_data,
    };

    return ppa_do_scale_rotate_mirror(ppa_ctx->srm_handle, &srm_oper_config);
}

#if PPA_LCD_ENABLE_CB
static bool _lvgl_port_ppa_callback(ppa_client_handle_t ppa_client, ppa_event_data_t *event_data, void *user_data)
{
    return false;
}
#endif

#endif
