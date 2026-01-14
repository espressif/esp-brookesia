/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include "esp_board_manager.h"
#include "esp_board_periph.h"
#include "esp_board_manager_includes.h"
#include "private/utils.hpp"
#include "board.hpp"

static void *display_lcd_handles = nullptr;
static void *display_lcd_cfg = nullptr;
static DisplayCallbacks display_callbacks{};

bool board_manager_init()
{
    esp_board_manager_init();
    esp_board_manager_print();
    esp_board_device_init_all();

    return true;
}

// Helper function to check if board name matches
static inline bool is_board_name(const char *board_name, const char *target_name)
{
    if (board_name == nullptr || target_name == nullptr) {
        return false;
    }
    return strcmp(board_name, target_name) == 0;
}

bool board_audio_peripheral_init(esp_brookesia::service::Audio::PeripheralConfig &config)
{
    esp_err_t ret = ESP_OK;
    dev_audio_codec_handles_t *play_dev_handles = NULL;
    dev_audio_codec_handles_t *rec_dev_handles = NULL;
    auto &manager_config = config.manager_config;

    ret = esp_board_device_get_handle("audio_dac", (void **)&play_dev_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio_dac handle");
    BROOKESIA_CHECK_NULL_RETURN(play_dev_handles, false, "Failed to get audio_dac handle");
    manager_config.play_dev = play_dev_handles->codec_dev;

    ret = esp_board_device_get_handle("audio_adc", (void **)&rec_dev_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get audio_adc handle");
    BROOKESIA_CHECK_NULL_RETURN(rec_dev_handles, false, "Failed to get audio_adc handle");
    manager_config.rec_dev = rec_dev_handles->codec_dev;

    config.player_volume_default = 80;
    config.player_volume_min = 0;
    config.player_volume_max = 100;
    config.recorder_gain = 32.0;
    config.recorder_channel_gains[2] = 20.0;

    if (is_board_name(g_esp_board_info.name, "esp32s3_korvo2_v3") ||
            is_board_name(g_esp_board_info.name, "echoear_core_board_v1_2") ||
            is_board_name(g_esp_board_info.name, "esp_box_3")) {
        strncpy(manager_config.mic_layout, "RMNN", sizeof(manager_config.mic_layout));
        manager_config.board_sample_rate = 16000;
        manager_config.board_bits = 32;
        manager_config.board_channels = 2;
    } else if (is_board_name(g_esp_board_info.name, "esp32_s3_korvo2l_v1") ||
               is_board_name(g_esp_board_info.name, "esp32_p4_function_ev")) {
        strncpy(manager_config.mic_layout, "MR", sizeof(manager_config.mic_layout));
        manager_config.board_sample_rate = 16000;
        manager_config.board_bits = 16;
        manager_config.board_channels = 2;
    } else {
        BROOKESIA_LOGE("Unsupported board: %s", g_esp_board_info.name);
        return false;
    }

    return true;
}

bool board_display_backlight_set(int percent)
{
#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
    static periph_ledc_handle_t *ledc_handle = NULL;

    if (percent > 100) {
        percent = 100;
    }
    if (percent < 0) {
        percent = 0;
    }

    if (ledc_handle == NULL) {
        auto result = esp_board_manager_get_device_handle("lcd_brightness", (void **)&ledc_handle);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Get LEDC control device handle failed");
    }

    dev_ledc_ctrl_config_t *dev_ledc_cfg = NULL;
    {
        auto result = esp_board_manager_get_device_config("lcd_brightness", (void **)&dev_ledc_cfg);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to get LEDC peripheral config");
    }
    periph_ledc_config_t *ledc_config = NULL;
    esp_board_manager_get_periph_config(dev_ledc_cfg->ledc_name, (void **)&ledc_config);
    uint32_t duty = (percent * ((1 << (uint32_t)ledc_config->duty_resolution) - 1)) / 100;

    {
        auto result = ledc_set_duty(ledc_handle->speed_mode, ledc_handle->channel, duty);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "LEDC set duty failed");
    }

    {
        auto result = ledc_update_duty(ledc_handle->speed_mode, ledc_handle->channel);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "LEDC update duty failed");
    }

    BROOKESIA_LOGI("Setting LCD backlight: %d%%,", percent);
#endif  /* CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT */

    return true;
}

static inline esp_lcd_panel_handle_t display_get_panel_handle()
{
    return static_cast<dev_display_lcd_handles_t *>(display_lcd_handles)->panel_handle;
}

static inline esp_lcd_panel_io_handle_t display_get_io_handle()
{
    return static_cast<dev_display_lcd_handles_t *>(display_lcd_handles)->io_handle;
}

bool board_display_peripheral_init(DisplayPeripheralConfig &config)
{
    esp_err_t ret = ESP_OK;

    ret = esp_board_manager_get_device_handle("display_lcd", &display_lcd_handles);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD device handle");
    BROOKESIA_CHECK_NULL_RETURN(display_lcd_handles, false, "Failed to get LCD device config");

    ret = esp_board_manager_get_device_config("display_lcd", &display_lcd_cfg);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD device config");
    BROOKESIA_CHECK_NULL_RETURN(display_lcd_cfg, false, "Failed to get LCD device config");

    dev_display_lcd_config_t *lcd_cfg = static_cast<dev_display_lcd_config_t *>(display_lcd_cfg);
    config.h_res = lcd_cfg->lcd_width;
    config.v_res = lcd_cfg->lcd_height;
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_SPI_SUPPORT
    config.flag_swap_color_bytes = true;
#elif CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    config.flag_swap_color_bytes = false;
#endif

    esp_lcd_panel_mirror(display_get_panel_handle(), lcd_cfg->mirror_x, lcd_cfg->mirror_y);
    esp_lcd_panel_swap_xy(display_get_panel_handle(), lcd_cfg->swap_xy);

    return true;
}

bool board_display_draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data)
{
    auto panel_handle = display_get_panel_handle();
    BROOKESIA_CHECK_NULL_RETURN(panel_handle, false, "Failed to get panel handle");

    auto ret = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, data);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to draw bitmap");

    return true;
}

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_SPI_SUPPORT
static bool display_spi_panel_color_trans_done(
    esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx
)
{
    if (display_callbacks.bitmap_flush_done) {
        return display_callbacks.bitmap_flush_done();
    }
    return false;
}
#elif CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
static bool display_dpi_panel_color_trans_done(
    esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx
)
{
    if (display_callbacks.bitmap_flush_done) {
        return display_callbacks.bitmap_flush_done();
    }
    return false;
}
#endif

bool board_display_register_callbacks(DisplayCallbacks &callbacks)
{
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_SPI_SUPPORT
    auto io_handle = display_get_io_handle();
    BROOKESIA_CHECK_NULL_RETURN(io_handle, false, "Failed to get IO handle");

    esp_lcd_panel_io_callbacks_t io_callbacks = {
        .on_color_trans_done = display_spi_panel_color_trans_done,
    };
    auto ret = esp_lcd_panel_io_register_event_callbacks(io_handle, &io_callbacks, NULL);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to register event callbacks");
#elif CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    auto panel_handle = display_get_panel_handle();
    BROOKESIA_CHECK_NULL_RETURN(panel_handle, false, "Failed to get panel handle");

    esp_lcd_dpi_panel_event_callbacks_t dpi_callbacks = {
        .on_color_trans_done = display_dpi_panel_color_trans_done,
    };
    auto ret = esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &dpi_callbacks, NULL);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to register event callbacks");
#else
    BROOKESIA_CHECK_FALSE_RETURN(false, "Unsupported display LCD type");
#endif

    display_callbacks = callbacks;

    return true;
}
