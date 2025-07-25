/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "esp_lvgl_port_priv.h"

#define LVGL_PORT_PPA   (CONFIG_LVGL_PORT_ENABLE_PPA)

#if LVGL_PORT_PPA
#include "../common/ppa/lcd_ppa.h"
#endif

#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include "esp_lcd_panel_rgb.h"
#endif

#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
#include "esp_lcd_mipi_dsi.h"
#endif

#if (ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 4)) || (ESP_IDF_VERSION == ESP_IDF_VERSION_VAL(5, 0, 0))
#define LVGL_PORT_HANDLE_FLUSH_READY 0
#else
#define LVGL_PORT_HANDLE_FLUSH_READY 1
#endif

#ifndef CONFIG_LV_DRAW_BUF_ALIGN
#define CONFIG_LV_DRAW_BUF_ALIGN 1
#endif

static const char *TAG = "LVGL";

/*******************************************************************************
* Types definitions
*******************************************************************************/

typedef struct {
    lvgl_port_disp_type_t     disp_type;    /* Display type */
    esp_lcd_panel_io_handle_t io_handle;      /* LCD panel IO handle */
    esp_lcd_panel_handle_t    panel_handle;   /* LCD panel handle */
    esp_lcd_panel_handle_t    control_handle; /* LCD panel control handle */
    lvgl_port_rotation_cfg_t  rotation;       /* Default values of the screen rotation */
    lv_color_t                *draw_buffs[3]; /* Display draw buffers */
    uint8_t                   *oled_buffer;
    lv_display_t              *disp_drv;      /* LVGL display driver */
    lv_display_rotation_t     current_rotation;
    SemaphoreHandle_t         trans_sem;      /* Idle transfer mutex */
#if LVGL_PORT_PPA
    lvgl_port_ppa_handle_t    ppa_handle;
#endif //LVGL_PORT_PPA
    struct {
        unsigned int monochrome: 1;  /* True, if display is monochrome and using 1bit for 1px */
        unsigned int swap_bytes: 1;  /* Swap bytes in RGB656 (16-bit) before send to LCD driver */
        unsigned int full_refresh: 1;   /* Always make the whole screen redrawn */
        unsigned int direct_mode: 1;    /* Use screen-sized buffers and draw to absolute coordinates */
        unsigned int sw_rotate: 1;    /* Use software rotation (slower) or PPA if available */
        unsigned int dummy_draw: 1;   /* Use dummy draw to bypass the display driver */
    } flags;
} lvgl_port_display_ctx_t;

/*******************************************************************************
* Function definitions
*******************************************************************************/
static lv_display_t *lvgl_port_add_disp_priv(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_disp_priv_cfg_t *priv_cfg);
#if LVGL_PORT_HANDLE_FLUSH_READY
static bool lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx);
#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static bool lvgl_port_flush_rgb_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx);
#endif
#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx);
static bool lvgl_port_flush_dpi_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx);
#endif
#endif
static void lvgl_port_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map);
static void lvgl_port_disp_size_update_callback(lv_event_t *e);
static void lvgl_port_disp_rotation_update(lvgl_port_display_ctx_t *disp_ctx);
static void lvgl_port_display_invalidate_callback(lv_event_t *e);

/*******************************************************************************
* Public API functions
*******************************************************************************/

lv_display_t *lvgl_port_add_disp(const lvgl_port_display_cfg_t *disp_cfg)
{
    lvgl_port_lock(0);
    lv_disp_t *disp = lvgl_port_add_disp_priv(disp_cfg, NULL);

    if (disp != NULL) {
        lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);
        /* Set display type */
        disp_ctx->disp_type = LVGL_PORT_DISP_TYPE_OTHER;

        assert(disp_cfg->io_handle != NULL);

#if LVGL_PORT_HANDLE_FLUSH_READY
        const esp_lcd_panel_io_callbacks_t cbs = {
            .on_color_trans_done = lvgl_port_flush_io_ready_callback,
        };
        /* Register done callback */
        esp_lcd_panel_io_register_event_callbacks(disp_ctx->io_handle, &cbs, disp);
#endif

        /* Apply rotation from initial display configuration */
        lvgl_port_disp_rotation_update(disp_ctx);
    }
    lvgl_port_unlock();

    return disp;
}

lv_display_t *lvgl_port_add_disp_dsi(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_display_dsi_cfg_t *dsi_cfg)
{
    assert(dsi_cfg != NULL);
    const lvgl_port_disp_priv_cfg_t priv_cfg = {
        .avoid_tearing = dsi_cfg->flags.avoid_tearing,
    };
    lvgl_port_lock(0);
    lv_disp_t *disp = lvgl_port_add_disp_priv(disp_cfg, &priv_cfg);

    if (disp != NULL) {
        lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);
        /* Set display type */
        disp_ctx->disp_type = LVGL_PORT_DISP_TYPE_DSI;

#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
        esp_lcd_dpi_panel_event_callbacks_t cbs = {0};
        if (dsi_cfg->flags.avoid_tearing) {
            cbs.on_refresh_done = lvgl_port_flush_dpi_vsync_ready_callback;
        } else {
            cbs.on_color_trans_done = lvgl_port_flush_dpi_panel_ready_callback;
        }
        /* Register done callback */
        esp_lcd_dpi_panel_register_event_callbacks(disp_ctx->panel_handle, &cbs, disp);

        /* Apply rotation from initial display configuration */
        lvgl_port_disp_rotation_update(disp_ctx);

#else
        ESP_RETURN_ON_FALSE(false, NULL, TAG, "MIPI-DSI is supported only on ESP32P4 and from IDF 5.3!");
#endif
    }
    lvgl_port_unlock();

    return disp;
}

lv_display_t *lvgl_port_add_disp_rgb(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_display_rgb_cfg_t *rgb_cfg)
{
    lvgl_port_lock(0);
    assert(rgb_cfg != NULL);
    const lvgl_port_disp_priv_cfg_t priv_cfg = {
        .avoid_tearing = rgb_cfg->flags.avoid_tearing,
    };
    lv_disp_t *disp = lvgl_port_add_disp_priv(disp_cfg, &priv_cfg);

    if (disp != NULL) {
        lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);
        /* Set display type */
        disp_ctx->disp_type = LVGL_PORT_DISP_TYPE_RGB;

#if (CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
        /* Register done callback */
        const esp_lcd_rgb_panel_event_callbacks_t vsync_cbs = {
            .on_vsync = lvgl_port_flush_rgb_vsync_ready_callback,
        };

        const esp_lcd_rgb_panel_event_callbacks_t bb_cbs = {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2)
            .on_bounce_frame_finish = lvgl_port_flush_rgb_vsync_ready_callback,
#endif
        };

        if (rgb_cfg->flags.bb_mode && (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2))) {
            ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(disp_ctx->panel_handle, &bb_cbs, disp_ctx->disp_drv));
        } else {
            ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(disp_ctx->panel_handle, &vsync_cbs, disp_ctx->disp_drv));
        }
#else
        ESP_RETURN_ON_FALSE(false, NULL, TAG, "RGB is supported only on ESP32S3 and from IDF 5.0!");
#endif

        /* Apply rotation from initial display configuration */
        lvgl_port_disp_rotation_update(disp_ctx);
    }
    lvgl_port_unlock();

    return disp;
}

esp_err_t lvgl_port_remove_disp(lv_display_t *disp)
{
    assert(disp);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);

    lvgl_port_lock(0);
    lv_disp_remove(disp);
    lvgl_port_unlock();

    if (disp_ctx->draw_buffs[0]) {
        free(disp_ctx->draw_buffs[0]);
    }

    if (disp_ctx->draw_buffs[1]) {
        free(disp_ctx->draw_buffs[1]);
    }

    if (disp_ctx->draw_buffs[2]) {
        free(disp_ctx->draw_buffs[2]);
    }

    if (disp_ctx->oled_buffer) {
        free(disp_ctx->oled_buffer);
    }

    if (disp_ctx->trans_sem) {
        vSemaphoreDelete(disp_ctx->trans_sem);
    }
#if LVGL_PORT_PPA
    if (disp_ctx->ppa_handle) {
        lvgl_port_ppa_delete(disp_ctx->ppa_handle);
    }
#endif //LVGL_PORT_PPA

    free(disp_ctx);

    return ESP_OK;
}

void lvgl_port_flush_ready(lv_display_t *disp)
{
    assert(disp);
    lv_disp_flush_ready(disp);
}

/*******************************************************************************
* Private functions
*******************************************************************************/

static lv_display_t *lvgl_port_add_disp_priv(const lvgl_port_display_cfg_t *disp_cfg, const lvgl_port_disp_priv_cfg_t *priv_cfg)
{
    esp_err_t ret = ESP_OK;
    lv_display_t *disp = NULL;
    lv_color_t *buf1 = NULL;
    lv_color_t *buf2 = NULL;
    uint32_t buffer_size = 0;
    SemaphoreHandle_t trans_sem = NULL;
    assert(disp_cfg != NULL);
    assert(disp_cfg->panel_handle != NULL);
    assert(disp_cfg->buffer_size > 0);
    assert(disp_cfg->hres > 0);
    assert(disp_cfg->vres > 0);

    buffer_size = disp_cfg->buffer_size;

    /* Check supported display color formats */
    ESP_RETURN_ON_FALSE(disp_cfg->color_format == 0 || disp_cfg->color_format == LV_COLOR_FORMAT_RGB565 || disp_cfg->color_format == LV_COLOR_FORMAT_RGB888 || disp_cfg->color_format == LV_COLOR_FORMAT_XRGB8888 || disp_cfg->color_format == LV_COLOR_FORMAT_ARGB8888 || disp_cfg->color_format == LV_COLOR_FORMAT_I1, NULL, TAG, "Not supported display color format!");

    lv_color_format_t display_color_format = (disp_cfg->color_format != 0 ? disp_cfg->color_format : LV_COLOR_FORMAT_RGB565);
    uint8_t color_bytes = lv_color_format_get_size(display_color_format);
    if (disp_cfg->flags.swap_bytes) {
        /* Swap bytes can be used only in RGB565 color format */
        ESP_RETURN_ON_FALSE(display_color_format == LV_COLOR_FORMAT_RGB565, NULL, TAG, "Swap bytes can be used only in display color format RGB565!");
    }

    if (disp_cfg->flags.buff_dma) {
        /* DMA buffer can be used only in RGB565 color format */
        ESP_RETURN_ON_FALSE(display_color_format == LV_COLOR_FORMAT_RGB565, NULL, TAG, "DMA buffer can be used only in display color format RGB565 (not aligned copy)!");
    }

    /* Display context */
    lvgl_port_display_ctx_t *disp_ctx = malloc(sizeof(lvgl_port_display_ctx_t));
    ESP_GOTO_ON_FALSE(disp_ctx, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for display context allocation!");
    memset(disp_ctx, 0, sizeof(lvgl_port_display_ctx_t));
    disp_ctx->io_handle = disp_cfg->io_handle;
    disp_ctx->panel_handle = disp_cfg->panel_handle;
    disp_ctx->control_handle = disp_cfg->control_handle;
    disp_ctx->rotation.swap_xy = disp_cfg->rotation.swap_xy;
    disp_ctx->rotation.mirror_x = disp_cfg->rotation.mirror_x;
    disp_ctx->rotation.mirror_y = disp_cfg->rotation.mirror_y;
    disp_ctx->flags.swap_bytes = disp_cfg->flags.swap_bytes;
    disp_ctx->flags.sw_rotate = disp_cfg->flags.sw_rotate;
    disp_ctx->current_rotation = LV_DISPLAY_ROTATION_0;
    disp_ctx->flags.dummy_draw = disp_cfg->flags.default_dummy_draw;

    uint32_t buff_caps = 0;
#if SOC_PSRAM_DMA_CAPABLE == 0
    if (disp_cfg->flags.buff_dma && disp_cfg->flags.buff_spiram) {
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "Alloc DMA capable buffer in SPIRAM is not supported!");
    }
#endif
    if (disp_cfg->flags.buff_dma) {
        buff_caps |= MALLOC_CAP_DMA;
    }
    if (disp_cfg->flags.buff_spiram) {
        buff_caps |= MALLOC_CAP_SPIRAM;
    }
    if (buff_caps == 0) {
        buff_caps |= MALLOC_CAP_DEFAULT;
    }

    /* Use RGB internal buffers for avoid tearing effect */
    if (priv_cfg && priv_cfg->avoid_tearing) {
#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        buffer_size = disp_cfg->hres * disp_cfg->vres;
        ESP_GOTO_ON_ERROR(esp_lcd_rgb_panel_get_frame_buffer(disp_cfg->panel_handle, 2, (void *)&buf1, (void *)&buf2), err, TAG, "Get RGB buffers failed");
#elif CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0)
        buffer_size = disp_cfg->hres * disp_cfg->vres;
        ESP_GOTO_ON_ERROR(esp_lcd_dpi_panel_get_frame_buffer(disp_cfg->panel_handle, 2, (void *)&buf1, (void *)&buf2), err, TAG, "Get RGB buffers failed");
#endif

        trans_sem = xSemaphoreCreateCounting(1, 0);
        ESP_GOTO_ON_FALSE(trans_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create transport counting Semaphore");
        disp_ctx->trans_sem = trans_sem;
    } else {
        /* alloc draw buffers used by LVGL */
        /* it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized */
        buf1 = heap_caps_aligned_alloc(CONFIG_LV_DRAW_BUF_ALIGN, buffer_size * color_bytes, buff_caps);
        ESP_GOTO_ON_FALSE(buf1, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (buf1) allocation!");
        if (disp_cfg->double_buffer) {
            buf2 = heap_caps_aligned_alloc(CONFIG_LV_DRAW_BUF_ALIGN, buffer_size * color_bytes, buff_caps);
            ESP_GOTO_ON_FALSE(buf2, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (buf2) allocation!");
        }

        disp_ctx->draw_buffs[0] = buf1;
        disp_ctx->draw_buffs[1] = buf2;

        trans_sem = xSemaphoreCreateCounting(1, 1);
        ESP_GOTO_ON_FALSE(trans_sem, ESP_ERR_NO_MEM, err, TAG, "Failed to create transport counting Semaphore");
        disp_ctx->trans_sem = trans_sem;
    }

    disp = lv_display_create(disp_cfg->hres, disp_cfg->vres);

    /* Set display color format */
    lv_display_set_color_format(disp, display_color_format);

    /* Monochrome display settings */
    if (disp_cfg->monochrome) {
#if CONFIG_LV_COLOR_DEPTH_1
#error please disable LV_COLOR_DEPTH_1 for using monochromatic screen
#endif

        /* Monochrome can be used only in RGB565 color format */
        ESP_RETURN_ON_FALSE(display_color_format == LV_COLOR_FORMAT_RGB565 || display_color_format == LV_COLOR_FORMAT_I1, NULL, TAG, "Monochrome can be used only in display color format RGB565 or I1!");

        /* When using monochromatic display, there must be used full bufer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == buffer_size), ESP_ERR_INVALID_ARG, err, TAG, "Monochromatic display must using full buffer!");

        disp_ctx->flags.monochrome = 1;
        lv_display_set_buffers(disp, buf1, buf2, buffer_size * color_bytes, LV_DISPLAY_RENDER_MODE_FULL);

        if (display_color_format == LV_COLOR_FORMAT_I1) {
            /* OLED monochrome buffer */
            // To use LV_COLOR_FORMAT_I1, we need an extra buffer to hold the converted data
            disp_ctx->oled_buffer = heap_caps_malloc(buffer_size, buff_caps);
            ESP_GOTO_ON_FALSE(disp_ctx->oled_buffer, ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (OLED buffer) allocation!");
        }

    } else if (disp_cfg->flags.direct_mode) {
        /* When using direct_mode, there must be used full bufer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == buffer_size), ESP_ERR_INVALID_ARG, err, TAG, "Direct mode must using full buffer!");

        disp_ctx->flags.direct_mode = 1;
        lv_display_set_buffers(disp, buf1, buf2, buffer_size * color_bytes, LV_DISPLAY_RENDER_MODE_DIRECT);
    } else if (disp_cfg->flags.full_refresh) {
        /* When using full_refresh, there must be used full bufer! */
        ESP_GOTO_ON_FALSE((disp_cfg->hres * disp_cfg->vres == buffer_size), ESP_ERR_INVALID_ARG, err, TAG, "Full refresh must using full buffer!");

        disp_ctx->flags.full_refresh = 1;
        lv_display_set_buffers(disp, buf1, buf2, buffer_size * color_bytes, LV_DISPLAY_RENDER_MODE_FULL);
    } else {
        lv_display_set_buffers(disp, buf1, buf2, buffer_size * color_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    }

    lv_display_set_flush_cb(disp, lvgl_port_flush_callback);
    lv_display_add_event_cb(disp, lvgl_port_disp_size_update_callback, LV_EVENT_RESOLUTION_CHANGED, disp_ctx);
    lv_display_add_event_cb(disp, lvgl_port_display_invalidate_callback, LV_EVENT_INVALIDATE_AREA, disp_ctx);
    lv_display_add_event_cb(disp, lvgl_port_display_invalidate_callback, LV_EVENT_REFR_REQUEST, disp_ctx);

    lv_display_set_driver_data(disp, disp_ctx);
    disp_ctx->disp_drv = disp;

    /* Use SW rotation */
    if (disp_cfg->flags.sw_rotate) {
#if LVGL_PORT_PPA
        ESP_LOGI(TAG, "Setting PPA context for SW rotation");
        uint32_t pixel_format = COLOR_PIXEL_RGB565;
        if (disp_cfg->color_format == LV_COLOR_FORMAT_RGB888) {
            pixel_format = COLOR_PIXEL_RGB888;
        }

        /* Create LCD PPA for rotation */
        lvgl_port_ppa_cfg_t ppa_cfg = {
            .buffer_size = disp_cfg->buffer_size * color_bytes,
            .color_space = COLOR_SPACE_RGB,
            .pixel_format = pixel_format,
            .flags = {
                .buff_dma = disp_cfg->flags.buff_dma,
                .buff_spiram = disp_cfg->flags.buff_spiram,
            }
        };
        disp_ctx->ppa_handle = lvgl_port_ppa_create(&ppa_cfg);
        assert(disp_ctx->ppa_handle != NULL);
#else
        disp_ctx->draw_buffs[2] = heap_caps_malloc(buffer_size * color_bytes, buff_caps);
        ESP_GOTO_ON_FALSE(disp_ctx->draw_buffs[2], ESP_ERR_NO_MEM, err, TAG, "Not enough memory for LVGL buffer (rotation buffer) allocation!");

#endif //LVGL_PORT_PPA
    }


err:
    if (ret != ESP_OK) {
        if (disp_ctx->draw_buffs[0]) {
            free(disp_ctx->draw_buffs[0]);
        }
        if (disp_ctx->draw_buffs[1]) {
            free(disp_ctx->draw_buffs[1]);
        }
        if (disp_ctx->draw_buffs[2]) {
            free(disp_ctx->draw_buffs[2]);
        }
        if (disp_ctx->oled_buffer) {
            free(disp_ctx->oled_buffer);
        }
        if (disp_ctx) {
            free(disp_ctx);
        }
        if (trans_sem) {
            vSemaphoreDelete(trans_sem);
        }
    }

    return disp;
}

#if LVGL_PORT_HANDLE_FLUSH_READY
static bool lvgl_port_flush_io_ready_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp_drv = (lv_display_t *)user_ctx;
    assert(disp_drv != NULL);
    lv_disp_flush_ready(disp_drv);
    lvgl_port_disp_give_trans_sem(disp_drv, true);
    return false;
}

#if (CONFIG_IDF_TARGET_ESP32P4 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 3, 0))
static bool lvgl_port_flush_dpi_panel_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp_drv = (lv_display_t *)user_ctx;
    assert(disp_drv != NULL);
    lv_disp_flush_ready(disp_drv);
    return false;
}

static bool lvgl_port_flush_dpi_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    lv_display_t *disp_drv = (lv_display_t *)user_ctx;
    assert(disp_drv != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp_drv);
    assert(disp_ctx != NULL);

    if (disp_ctx->trans_sem) {
        xSemaphoreGiveFromISR(disp_ctx->trans_sem, &need_yield);
    }

    return (need_yield == pdTRUE);
}
#endif

#if CONFIG_IDF_TARGET_ESP32S3 && ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
static bool lvgl_port_flush_rgb_vsync_ready_callback(esp_lcd_panel_handle_t panel_io, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx)
{
    BaseType_t need_yield = pdFALSE;

    lv_display_t *disp_drv = (lv_display_t *)user_ctx;
    assert(disp_drv != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp_drv);
    assert(disp_ctx != NULL);

    if (disp_ctx->trans_sem) {
        xSemaphoreGiveFromISR(disp_ctx->trans_sem, &need_yield);
    }

    return (need_yield == pdTRUE);
}
#endif
#endif

static void _lvgl_port_transform_monochrome(lv_display_t *display, const lv_area_t *area, uint8_t **color_map)
{
    assert(color_map);
    assert(*color_map);
    uint8_t *src = *color_map;
    lv_color16_t *color = (lv_color16_t *)*color_map;
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(display);
    uint16_t hor_res = lv_display_get_physical_horizontal_resolution(display);
    uint16_t ver_res = lv_display_get_physical_vertical_resolution(display);
    uint16_t res = hor_res;
    bool swap_xy = (lv_display_get_rotation(display) == LV_DISPLAY_ROTATION_90 || lv_display_get_rotation(display) == LV_DISPLAY_ROTATION_270);

    int x1 = area->x1;
    int x2 = area->x2;
    int y1 = area->y1;
    int y2 = area->y2;

    lv_color_format_t color_format = lv_display_get_color_format(display);
    if (color_format == LV_COLOR_FORMAT_I1) {
        // This is necessary because LVGL reserves 2 x 4 bytes in the buffer, as these are assumed to be used as a palette. Skip the palette here
        // More information about the monochrome, please refer to https://docs.lvgl.io/9.2/porting/display.html#monochrome-displays
        src += 8;
        /*Use oled_buffer as output */
        *color_map = disp_ctx->oled_buffer;
    }

    int out_x, out_y;
    for (int y = y1; y <= y2; y++) {
        for (int x = x1; x <= x2; x++) {
            bool chroma_color = 0;
            if (color_format == LV_COLOR_FORMAT_I1) {
                chroma_color = (src[(hor_res >> 3) * y  + (x >> 3)] & 1 << (7 - x % 8));
            } else {
                chroma_color = (color[hor_res * y + x].blue > 16);
            }

            if (swap_xy) {
                out_x = y;
                out_y = x;
                res = ver_res;
            } else {
                out_x = x;
                out_y = y;
                res = hor_res;
            }

            /* Write to the buffer as required for the display.
            * It writes only 1-bit for monochrome displays mapped vertically.*/
            uint8_t *outbuf = NULL;
            outbuf = *color_map + res * (out_y >> 3) + (out_x);
            if (chroma_color) {
                (*outbuf) &= ~(1 << (out_y % 8));
            } else {
                (*outbuf) |= (1 << (out_y % 8));
            }
        }
    }

}

void lvgl_port_rotate_area(lv_display_t *disp, lv_area_t *area)
{
    lv_display_rotation_t rotation = lv_display_get_rotation(disp);

    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    int32_t hres = lv_display_get_horizontal_resolution(disp);
    int32_t vres = lv_display_get_vertical_resolution(disp);
    if (rotation == LV_DISPLAY_ROTATION_90 || rotation == LV_DISPLAY_ROTATION_270) {
        vres = lv_display_get_horizontal_resolution(disp);
        hres = lv_display_get_vertical_resolution(disp);
    }

    switch (rotation) {
    case LV_DISPLAY_ROTATION_0:
        return;
    case LV_DISPLAY_ROTATION_90:
        area->y2 = vres - area->x1 - 1;
        area->x1 = area->y1;
        area->x2 = area->x1 + h - 1;
        area->y1 = area->y2 - w + 1;
        break;
    case LV_DISPLAY_ROTATION_180:
        area->y2 = vres - area->y1 - 1;
        area->y1 = area->y2 - h + 1;
        area->x2 = hres - area->x1 - 1;
        area->x1 = area->x2 - w + 1;
        break;
    case LV_DISPLAY_ROTATION_270:
        area->x1 = hres - area->y2 - 1;
        area->y2 = area->x2;
        area->x2 = area->x1 + h - 1;
        area->y1 = area->y2 - w + 1;
        break;
    }
}

static void lvgl_port_flush_callback(lv_display_t *drv, const lv_area_t *area, uint8_t *color_map)
{
    assert(drv != NULL);
    assert(area != NULL);
    assert(color_map != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(drv);
    assert(disp_ctx != NULL);

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;

    /* SW rotation enabled */
    if (disp_ctx->flags.sw_rotate && (disp_ctx->current_rotation > LV_DISPLAY_ROTATION_0)) {
#if LVGL_PORT_PPA
        if (disp_ctx->ppa_handle) {
            /* Screen vertical size */
            int32_t hres = lv_display_get_horizontal_resolution(drv);
            int32_t vres = lv_display_get_vertical_resolution(drv);
            lvgl_port_ppa_disp_rotate_t rotate_cfg = {
                .in_buff = color_map,
                .area = {
                    .x1 = area->x1,
                    .x2 = area->x2,
                    .y1 = area->y1,
                    .y2 = area->y2,
                },
                .disp_size = {
                    .hres = hres,
                    .vres = vres,
                },
                .rotation = disp_ctx->current_rotation,
                .ppa_mode = PPA_TRANS_MODE_BLOCKING,
                .swap_bytes = (disp_ctx->flags.swap_bytes ? true : false),
                .user_data = disp_ctx
            };
            /* Do operation */
            esp_err_t err = lvgl_port_ppa_rotate(disp_ctx->ppa_handle, &rotate_cfg);
            if (err == ESP_OK) {
                color_map = lvgl_port_ppa_get_output_buffer(disp_ctx->ppa_handle);
                offsetx1 = rotate_cfg.area.x1;
                offsetx2 = rotate_cfg.area.x2;
                offsety1 = rotate_cfg.area.y1;
                offsety2 = rotate_cfg.area.y2;
            }
        }
#else
        /* SW rotation */
        if (disp_ctx->draw_buffs[2]) {
            int32_t ww = lv_area_get_width(area);
            int32_t hh = lv_area_get_height(area);
            lv_color_format_t cf = lv_display_get_color_format(drv);
            uint32_t w_stride = lv_draw_buf_width_to_stride(ww, cf);
            uint32_t h_stride = lv_draw_buf_width_to_stride(hh, cf);
            if (disp_ctx->current_rotation == LV_DISPLAY_ROTATION_180) {
                lv_draw_sw_rotate(color_map, disp_ctx->draw_buffs[2], hh, ww, h_stride, h_stride, LV_DISPLAY_ROTATION_180, cf);
            } else if (disp_ctx->current_rotation == LV_DISPLAY_ROTATION_90) {
                lv_draw_sw_rotate(color_map, disp_ctx->draw_buffs[2], ww, hh, w_stride, h_stride, LV_DISPLAY_ROTATION_90, cf);
            } else if (disp_ctx->current_rotation == LV_DISPLAY_ROTATION_270) {
                lv_draw_sw_rotate(color_map, disp_ctx->draw_buffs[2], ww, hh, w_stride, h_stride, LV_DISPLAY_ROTATION_270, cf);
            }
            color_map = (uint8_t *)disp_ctx->draw_buffs[2];
            lvgl_port_rotate_area(drv, (lv_area_t *)area);
            offsetx1 = area->x1;
            offsetx2 = area->x2;
            offsety1 = area->y1;
            offsety2 = area->y2;
        }
#endif //LVGL_PORT_PPA
    }

    if (disp_ctx->flags.swap_bytes) {
        size_t len = lv_area_get_size(area);
        lv_draw_sw_rgb565_swap(color_map, len);
    }
    /* Transfer data in buffer for monochromatic screen */
    if (disp_ctx->flags.monochrome) {
        _lvgl_port_transform_monochrome(drv, area, &color_map);
    }

    if ((disp_ctx->disp_type == LVGL_PORT_DISP_TYPE_RGB || disp_ctx->disp_type == LVGL_PORT_DISP_TYPE_DSI) && (disp_ctx->flags.direct_mode || disp_ctx->flags.full_refresh)) {
        if (lv_disp_flush_is_last(drv)) {
            /* If the interface is I80 or SPI, this step cannot be used for drawing. */
            esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, 0, 0, lv_disp_get_hor_res(drv), lv_disp_get_ver_res(drv), color_map);
            /* Waiting for the last frame buffer to complete transmission */
            xSemaphoreTake(disp_ctx->trans_sem, 0);
            xSemaphoreTake(disp_ctx->trans_sem, portMAX_DELAY);
        }
    } else if (!disp_ctx->flags.dummy_draw) {
        lvgl_port_disp_take_trans_sem(drv, portMAX_DELAY);
        if (!disp_ctx->flags.dummy_draw) {
            esp_lcd_panel_draw_bitmap(disp_ctx->panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
        }
    }

    if (disp_ctx->disp_type == LVGL_PORT_DISP_TYPE_RGB || (disp_ctx->disp_type == LVGL_PORT_DISP_TYPE_DSI && (disp_ctx->flags.direct_mode || disp_ctx->flags.full_refresh)) || disp_ctx->flags.dummy_draw) {
        lv_disp_flush_ready(drv);
    }
}

static void lvgl_port_disp_rotation_update(lvgl_port_display_ctx_t *disp_ctx)
{
    assert(disp_ctx != NULL);

    disp_ctx->current_rotation = lv_display_get_rotation(disp_ctx->disp_drv);
    if (disp_ctx->flags.sw_rotate) {
        return;
    }

    esp_lcd_panel_handle_t control_handle = (disp_ctx->control_handle ? disp_ctx->control_handle : disp_ctx->panel_handle);
    /* Solve rotation screen and touch */
    switch (lv_display_get_rotation(disp_ctx->disp_drv)) {
    case LV_DISPLAY_ROTATION_0:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, disp_ctx->rotation.swap_xy);
        esp_lcd_panel_mirror(control_handle, disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        break;
    case LV_DISPLAY_ROTATION_90:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, !disp_ctx->rotation.swap_xy);
        if (disp_ctx->rotation.swap_xy) {
            esp_lcd_panel_mirror(control_handle, !disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        } else {
            esp_lcd_panel_mirror(control_handle, disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        }
        break;
    case LV_DISPLAY_ROTATION_180:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, disp_ctx->rotation.swap_xy);
        esp_lcd_panel_mirror(control_handle, !disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        break;
    case LV_DISPLAY_ROTATION_270:
        /* Rotate LCD display */
        esp_lcd_panel_swap_xy(control_handle, !disp_ctx->rotation.swap_xy);
        if (disp_ctx->rotation.swap_xy) {
            esp_lcd_panel_mirror(control_handle, disp_ctx->rotation.mirror_x, !disp_ctx->rotation.mirror_y);
        } else {
            esp_lcd_panel_mirror(control_handle, !disp_ctx->rotation.mirror_x, disp_ctx->rotation.mirror_y);
        }
        break;
    }

    /* Wake LVGL task, if needed */
    lvgl_port_task_wake(LVGL_PORT_EVENT_DISPLAY, disp_ctx->disp_drv);
}

static void lvgl_port_disp_size_update_callback(lv_event_t *e)
{
    assert(e);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_event_get_user_data(e);
    lvgl_port_disp_rotation_update(disp_ctx);
}

static void lvgl_port_display_invalidate_callback(lv_event_t *e)
{
    /* Wake LVGL task, if needed */
    lvgl_port_task_wake(LVGL_PORT_EVENT_DISPLAY, NULL);
}

void lvgl_port_disp_set_dummy_draw(lv_display_t *disp, bool enable)
{
    assert(disp != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);
    assert(disp_ctx != NULL);
    disp_ctx->flags.dummy_draw = enable;
}

esp_err_t lvgl_port_disp_take_trans_sem(lv_display_t *disp, uint32_t timeout_ms)
{
    assert(disp != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);
    assert(disp_ctx != NULL);

    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return (xSemaphoreTake(disp_ctx->trans_sem, timeout_ticks) == pdTRUE ? ESP_OK : ESP_ERR_TIMEOUT);
}

void lvgl_port_disp_give_trans_sem(lv_display_t *disp, bool from_isr)
{
    assert(disp != NULL);
    lvgl_port_display_ctx_t *disp_ctx = (lvgl_port_display_ctx_t *)lv_display_get_driver_data(disp);
    assert(disp_ctx != NULL);
    assert(disp_ctx->trans_sem != NULL);

    BaseType_t need_yield = pdFALSE;
    if (from_isr) {
        xSemaphoreGiveFromISR(disp_ctx->trans_sem, &need_yield);
    } else {
        xSemaphoreGive(disp_ctx->trans_sem);
    }

    if (need_yield == pdTRUE) {
        portYIELD_FROM_ISR();
    }

}
