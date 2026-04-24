/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_PANEL_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "lcd_panel_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
#include "esp_board_manager_includes.h"
#include "esp_lcd_panel_ops.h"

namespace esp_brookesia::hal {

namespace {
esp_lcd_panel_handle_t get_panel_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->panel_handle;
}

esp_lcd_panel_io_handle_t get_io_handle(void *handles)
{
    return reinterpret_cast<dev_display_lcd_handles_t *>(handles)->io_handle;
}

DisplayPanelIface::Info generate_info()
{
    dev_display_lcd_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, reinterpret_cast<void **>(&config));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, {}, "Failed to get LCD config");
    BROOKESIA_CHECK_NULL_RETURN(config, {}, "Failed to get LCD config");

    DisplayPanelIface::Info info{};
    info.h_res = config->lcd_width;
    info.v_res = config->lcd_height;
    auto pixel_bits = config->bits_per_pixel;
    switch (pixel_bits) {
    case 16:
        info.pixel_format = DisplayPanelIface::PixelFormat::RGB565;
        break;
    case 24:
        info.pixel_format = DisplayPanelIface::PixelFormat::RGB888;
        break;
    default:
        BROOKESIA_CHECK_FALSE_RETURN(false, {}, "Unsupported pixel bits(%1%)", pixel_bits);
    }

    return info;
}
} // namespace

LcdDisplayPanelImpl::LcdDisplayPanelImpl()
    : DisplayPanelIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Info: %1%", get_info());

    BROOKESIA_CHECK_FALSE_EXIT(get_info().is_valid(), "Invalid panel information");

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init LCD panel");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");
}

LcdDisplayPanelImpl::~LcdDisplayPanelImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD);
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit LCD"); });
}

bool LcdDisplayPanelImpl::draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const uint8_t *data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: x1(%1%), y1(%2%), x2(%3%), y2(%4%), data(%5%)", x1, y1, x2, y2, data);

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD is not initialized");

    auto ret = esp_lcd_panel_draw_bitmap(get_panel_handle(handles_), x1, y1, x2, y2, data);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to draw bitmap");

    return true;
}

bool LcdDisplayPanelImpl::get_driver_specific(DriverSpecific &specific)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    dev_display_lcd_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_DISPLAY_LCD, reinterpret_cast<void **>(&config));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to get LCD config");
    BROOKESIA_CHECK_NULL_RETURN(config, false, "Failed to get LCD config");

    specific.io_handle = get_io_handle(handles_);
    specific.panel_handle = get_panel_handle(handles_);
    if (strcmp(config->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_SPI) == 0) {
        specific.bus_type = DisplayPanelIface::BusType::Generic;
    } else if (strcmp(config->sub_type, ESP_BOARD_DEVICE_LCD_SUB_TYPE_DSI) == 0) {
        specific.bus_type = DisplayPanelIface::BusType::MIPI;
    } else {
        BROOKESIA_LOGW("Unsupported sub type: %1%", config->sub_type);
    }

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
