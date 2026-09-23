/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_gsp_deployable.h"

struct FakeGsp;
using esp_gsp_handle_t = FakeGsp *;
using esp_err_t = int;

constexpr int ESP_OK = 0;

struct esp_gsp_event_t {
    int marker = 0;
};

using esp_gsp_event_cb_t = void (*)(esp_gsp_handle_t, const esp_gsp_event_t *, void *);

enum esp_display_present_panel_t {
    ESP_DISPLAY_PRESENT_PANEL_AUTO,
    ESP_DISPLAY_PRESENT_PANEL_MIPI_DSI,
    ESP_DISPLAY_PRESENT_PANEL_RGB,
    ESP_DISPLAY_PRESENT_PANEL_IO,
};

enum esp_display_present_pixel_format_t {
    ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565,
    ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB888,
};

enum esp_display_present_rotate_t {
    ESP_DISPLAY_PRESENT_ROTATE_0,
};

enum esp_display_present_mode_t {
    ESP_DISPLAY_PRESENT_MODE_AUTO,
};

enum esp_gsp_touch_input_mode_t {
    ESP_GSP_TOUCH_INPUT_POLLING,
    ESP_GSP_TOUCH_INPUT_INTERRUPT,
};

using esp_lcd_panel_handle_t = void *;
using esp_lcd_panel_io_handle_t = void *;
using esp_lcd_touch_handle_t = void *;

struct esp_gsp_esp_lcd_config_t {
    struct {
        struct {
            esp_lcd_panel_handle_t panel = nullptr;
            esp_lcd_panel_io_handle_t io = nullptr;
            esp_display_present_panel_t panel_type = ESP_DISPLAY_PRESENT_PANEL_AUTO;
            esp_display_present_pixel_format_t input_pixel_format = ESP_DISPLAY_PRESENT_PIXEL_FORMAT_RGB565;
            esp_display_present_rotate_t rotation = ESP_DISPLAY_PRESENT_ROTATE_0;
            bool swap_bytes = false;
            bool te_enabled = false;
            struct {
                int gpio_num = 0;
            } te_sync;
        } hw;
        struct {
            esp_display_present_mode_t mode = ESP_DISPLAY_PRESENT_MODE_AUTO;
            int frame_buffer_count = 0;
        } fb;
    } display;
    esp_lcd_touch_handle_t touch = nullptr;
    esp_gsp_touch_input_mode_t touch_input_mode = ESP_GSP_TOUCH_INPUT_POLLING;
    uint32_t task_stack_size = 0;
    bool task_stack_psram = false;
};

#define ESP_GSP_ESP_LCD_CONFIG_INIT() esp_gsp_esp_lcd_config_t{}

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_gsp_esp_lcd_start(
    const esp_gsp_config_t *config, const esp_gsp_esp_lcd_config_t *lcd, esp_gsp_handle_t *out_gsp
);
esp_gsp_err_t esp_gsp_on_event(esp_gsp_handle_t gsp, esp_gsp_event_cb_t callback, void *user_ctx);
esp_gsp_err_t esp_gsp_stop(esp_gsp_handle_t gsp);

#ifdef __cplusplus
}
#endif

namespace esp_brookesia::hal::display {

class PanelIface {
public:
    enum class BusType { MIPI, RGB, Generic, Max };
    enum class PixelFormat { RGB565, RGB888, Max };
    struct DriverSpecific {
        void *panel_handle = nullptr;
        void *io_handle = nullptr;
        BusType bus_type = BusType::Max;
    };
    struct Info {
        int h_res = 320;
        int v_res = 240;
        PixelFormat pixel_format = PixelFormat::RGB565;
    };

    bool get_driver_specific(DriverSpecific &specific)
    {
        specific = {reinterpret_cast<void *>(1), nullptr, BusType::Generic};
        return true;
    }

    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

class TouchIface {
public:
    enum class OperationMode { Polling, Interrupt };
    struct DriverSpecific {
        void *touch_handle = nullptr;
    };
    struct Info {
        OperationMode operation_mode = OperationMode::Polling;
    };

    bool get_driver_specific(DriverSpecific &specific)
    {
        specific = {reinterpret_cast<void *>(2)};
        return true;
    }

    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

} // namespace esp_brookesia::hal::display
