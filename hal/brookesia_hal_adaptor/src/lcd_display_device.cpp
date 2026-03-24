/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/hal_adaptor/macro_configs.h"
#undef BROOKESIA_LOG_TAG
#define BROOKESIA_LOG_TAG BROOKESIA_HAL_ADAPTOR_LOG_TAG
#include "lcd_display_device.hpp"
#include "esp_lcd_panel_ops.h"
#include "esp_board_device.h"
#include "esp_board_manager_includes.h"

using namespace esp_brookesia::hal;

bool LCDDisplayDevice::check_initialized_intern() const
{
    return display_lcd_handles_ != nullptr && display_touch_handles_ != nullptr &&
           config_.h_res > 0 && config_.v_res > 0;
}

bool LCDDisplayDevice::check_initialized() const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);
    return check_initialized_intern();
}

bool LCDDisplayDevice::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    if (check_initialized_intern()) {
        BROOKESIA_LOGW("LCD display device is already initialized");
        return true;
    }

    esp_err_t ret = ESP_OK;

    BROOKESIA_LOGI("Initializing LCD display device...");
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_board_manager_init_device_by_name(
            ESP_BOARD_DEVICE_NAME_DISPLAY_LCD), false, "Failed to initialize LCD display device");
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_board_manager_init_device_by_name(
            ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS), false, "Failed to initialize LCD brightness device");
    BROOKESIA_CHECK_ESP_ERR_RETURN(
        esp_board_manager_init_device_by_name(
            ESP_BOARD_DEVICE_NAME_LCD_TOUCH), false, "Failed to initialize LCD touch device");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &display_lcd_handles_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD device handle");
    BROOKESIA_CHECK_NULL_RETURN(display_lcd_handles_, false, "Failed to get LCD device config");

    ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &display_lcd_cfg_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD device config");
    BROOKESIA_CHECK_NULL_RETURN(display_lcd_cfg_, false, "Failed to get LCD device config");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, &display_touch_handles_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD touch device handle");
    BROOKESIA_CHECK_NULL_RETURN(display_touch_handles_, false, "Failed to get LCD touch device config");

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT
    dev_display_lcd_config_t *lcd_cfg = static_cast<dev_display_lcd_config_t *>(display_lcd_cfg_);
    config_.h_res = lcd_cfg->lcd_width;
    config_.v_res = lcd_cfg->lcd_height;
#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_SPI_SUPPORT
    config_.flag_swap_color_bytes = true;
#elif CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    config_.flag_swap_color_bytes = false;
#endif
#endif

    return true;
}

bool LCDDisplayDevice::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    esp_err_t ret = ESP_OK;
    if (display_lcd_handles_ != nullptr) {
        ret = esp_board_device_deinit(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to deinit LCD device");
    }
    if (display_touch_handles_ != nullptr) {
        ret = esp_board_device_deinit(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to deinit LCD touch device");
    }
    if (ledc_handle != nullptr) {
        ret = esp_board_device_deinit(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to deinit LCD brightness device");
    }

    display_lcd_handles_ = nullptr;
    display_touch_handles_ = nullptr;
    ledc_handle = nullptr;
    display_lcd_cfg_ = nullptr;
    config_ = {};
    display_callbacks_ = {};
    return true;
}

bool LCDDisplayDevice::set_backlight(int percent)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: percent(%1%)", percent);

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(
        check_initialized_intern(),
        false, "LCD display device is not initialized"
    );

#if CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT
    if (percent > 100) {
        percent = 100;
    }
    if (percent < 0) {
        percent = 0;
    }

    periph_ledc_handle_t *ledc = static_cast<periph_ledc_handle_t *>(ledc_handle);
    if (ledc == nullptr) {
        auto result = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, (void **)&ledc);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Get LEDC control device handle failed");
        ledc_handle = ledc;
    }

    dev_ledc_ctrl_config_t *dev_ledc_cfg = nullptr;
    {
        auto result = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_BRIGHTNESS, (void **)&dev_ledc_cfg);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to get LEDC peripheral config");
    }
    periph_ledc_config_t *ledc_config = nullptr;
    esp_board_manager_get_periph_config(dev_ledc_cfg->ledc_name, (void **)&ledc_config);
    uint32_t duty = (percent * ((1 << (uint32_t)ledc_config->duty_resolution) - 1)) / 100;

    {
        auto result = ledc_set_duty(ledc->speed_mode, ledc->channel, duty);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "LEDC set duty failed");
    }

    {
        auto result = ledc_update_duty(ledc->speed_mode, ledc->channel);
        BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "LEDC update duty failed");
    }

    BROOKESIA_LOGI("Setting LCD backlight: %d%%,", percent);
#endif  /* CONFIG_ESP_BOARD_DEV_LEDC_CTRL_SUPPORT */

    return true;
}


bool LCDDisplayDevice::draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: x1(%1%), y1(%2%), x2(%3%), y2(%4%), data(%5)", x1, y1, x2, y2, data);

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(
        check_initialized_intern(),
        false, "LCD display device is not initialized"
    );

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUPPORT

    esp_lcd_panel_handle_t panel_handle = static_cast<dev_display_lcd_handles_t *>(display_lcd_handles_)->panel_handle;
    BROOKESIA_CHECK_NULL_RETURN(panel_handle, false, "Failed to get panel handle");

    auto ret = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, data);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to draw bitmap");
#endif

    return true;
}

bool LCDDisplayDevice::register_callbacks(const DisplayCallbacks &callbacks)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(
        check_initialized_intern(),
        false, "LCD display device is not initialized"
    );
    display_callbacks_ = callbacks;

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_SPI_SUPPORT
    esp_lcd_panel_io_handle_t io_handle = static_cast<dev_display_lcd_handles_t *>(display_lcd_handles_)->io_handle;
    BROOKESIA_CHECK_NULL_RETURN(io_handle, false, "Failed to get IO handle");

    esp_lcd_panel_io_callbacks_t io_callbacks = {
        .on_color_trans_done = [](esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
        {
            (void)panel_io;
            (void)edata;
            LCDDisplayDevice *self = static_cast<LCDDisplayDevice *>(user_ctx);
            DisplayPanelIface::DisplayBitmapFlushDoneCallback callback;
            {
                callback = self->display_callbacks_.bitmap_flush_done;
            }
            if (callback) {
                return callback();
            }
            return false;
        },
    };
    auto ret = esp_lcd_panel_io_register_event_callbacks(io_handle, &io_callbacks, static_cast<void *>(this));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to register event callbacks");

#elif CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    esp_lcd_panel_handle_t panel_handle = static_cast<dev_display_lcd_handles_t *>(display_lcd_handles_)->panel_handle;
    BROOKESIA_CHECK_NULL_RETURN(panel_handle, false, "Failed to get panel handle");

    esp_lcd_dpi_panel_event_callbacks_t dpi_callbacks = {};
    dpi_callbacks.on_color_trans_done = [](esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata,
    void *user_ctx) {
        (void)panel;
        (void)edata;
        LCDDisplayDevice *self = static_cast<LCDDisplayDevice *>(user_ctx);
        DisplayPanelIface::DisplayBitmapFlushDoneCallback callback;
        {
            callback = self->display_callbacks_.bitmap_flush_done;
        }
        if (callback) {
            return callback();
        }
        return false;
    };
    dpi_callbacks.on_refresh_done = nullptr;
    auto ret = esp_lcd_dpi_panel_register_event_callbacks(panel_handle, &dpi_callbacks, static_cast<void *>(this));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to register event callbacks");

#else
    BROOKESIA_CHECK_FALSE_RETURN(false, false, "Unsupported display LCD type");
#endif

    return true;
}

#if CONFIG_BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_WITH_SYMBOL(Device, LCDDisplayDevice, DISPLAY_DEVICE,
                                      BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_PLUGIN_SYMBOL);
#endif
