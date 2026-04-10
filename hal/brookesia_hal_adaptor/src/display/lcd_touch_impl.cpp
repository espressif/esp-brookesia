/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_DISPLAY_LCD_TOUCH_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "lcd_touch_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL
#include "esp_board_manager_includes.h"
#include "esp_lcd_touch.h"

namespace esp_brookesia::hal {

namespace {
esp_lcd_touch_handle_t get_touch_handle(void *handles)
{
    return reinterpret_cast<dev_lcd_touch_i2c_handles_t *>(handles)->touch_handle;
}

esp_lcd_panel_io_handle_t get_io_handle(void *handles)
{
    return reinterpret_cast<dev_lcd_touch_i2c_handles_t *>(handles)->io_handle;
}

DisplayTouchIface::Info generate_info()
{
    dev_lcd_touch_i2c_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, reinterpret_cast<void **>(&config));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, {}, "Failed to get LCD touch config");
    BROOKESIA_CHECK_NULL_RETURN(config, {}, "Failed to get LCD touch config");

    return DisplayTouchIface::Info {
        .x_max = config->touch_config.x_max,
        .y_max = config->touch_config.y_max,
        .operation_mode = config->touch_config.int_gpio_num != 0 ?  DisplayTouchIface::OperationMode::Interrupt :
        DisplayTouchIface::OperationMode::Polling,
    };
}
} // namespace

I2cDisplayTouchImpl::I2cDisplayTouchImpl()
    : DisplayTouchIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", get_info());

    BROOKESIA_CHECK_FALSE_EXIT(get_info().is_valid(), "Invalid touch information");

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init LCD touch");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");
}

I2cDisplayTouchImpl::~I2cDisplayTouchImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    register_interrupt_handler(nullptr);

    auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit LCD touch"); });
}

bool I2cDisplayTouchImpl::read_points(std::vector<DisplayTouchIface::Point> &points)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard<boost::mutex> lock(mutex_);

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD touch is not initialized");

    auto touch_handle = get_touch_handle(handles_);
    auto read_ret = esp_lcd_touch_read_data(touch_handle);
    BROOKESIA_CHECK_ESP_ERR_RETURN(read_ret, false, "Failed to read touch data");

    uint8_t count = 0;
    std::array<esp_lcd_touch_point_data_t, CONFIG_ESP_LCD_TOUCH_MAX_POINTS> data;
    bool get_ret = esp_lcd_touch_get_data(touch_handle, data.data(), &count, CONFIG_ESP_LCD_TOUCH_MAX_POINTS);
    BROOKESIA_CHECK_ESP_ERR_RETURN(get_ret, false, "Failed to get touch data");

    points.resize(count);
    if (count == 0) {
        BROOKESIA_LOGD("No touch points detected");
        return true;
    }

    for (size_t i = 0; i < count; ++i) {
        points[i] = DisplayTouchIface::Point {
            .x = static_cast<int16_t>(data[i].x),
            .y = static_cast<int16_t>(data[i].y),
            .pressure = static_cast<uint16_t>(data[i].strength),
        };
    }

    return true;
}

bool I2cDisplayTouchImpl::register_interrupt_handler(InterruptHandler handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handler(%1%)", handler);

    boost::lock_guard<boost::mutex> lock(mutex_);

    if ((interrupt_handler_ == nullptr) && (handler == nullptr)) {
        BROOKESIA_LOGD("Interrupt handler is already unregistered");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD touch is not initialized");
    BROOKESIA_CHECK_FALSE_RETURN(
        get_info().operation_mode == OperationMode::Interrupt, false, "Only support in interrupt mode"
    );

    interrupt_handler_ = std::move(handler);

    esp_lcd_touch_interrupt_callback_t callback = nullptr;
    if (interrupt_handler_) {
        callback = [](esp_lcd_touch_handle_t tp) {
            auto iface = reinterpret_cast<I2cDisplayTouchImpl *>(tp->config.user_data);
            if (iface && iface->interrupt_handler_) {
                iface->interrupt_handler_();
            }
        };
    }
    auto ret = esp_lcd_touch_register_interrupt_callback_with_data(get_touch_handle(handles_), callback, this);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to register interrupt callback");

    return true;
}

bool I2cDisplayTouchImpl::get_driver_specific(DriverSpecific &specific)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    specific.io_handle = get_io_handle(handles_);
    specific.touch_handle = get_touch_handle(handles_);

    return true;
}

} // namespace esp_brookesia::hal
#endif // BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL
