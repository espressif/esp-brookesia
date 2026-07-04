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
constexpr const char *DISPLAY_GROUP_ID = "display_lcd";

esp_lcd_touch_handle_t get_touch_handle(void *handles)
{
    return reinterpret_cast<dev_lcd_touch_handles_t *>(handles)->touch_handle;
}

esp_lcd_panel_io_handle_t get_io_handle(void *handles)
{
    return reinterpret_cast<dev_lcd_touch_handles_t *>(handles)->io_handle;
}

display::TouchIface::Info generate_info()
{
    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH)) {
        BROOKESIA_LOGW("LCD touch device not found");
        return {};
    }

    dev_lcd_touch_config_t *config = nullptr;
    auto ret = esp_board_manager_get_device_config(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, reinterpret_cast<void **>(&config));
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, {}, "Failed to get LCD touch config");
    BROOKESIA_CHECK_NULL_RETURN(config, {}, "Failed to get LCD touch config");

    return display::TouchIface::Info {
        .x_max = config->touch_config.x_max,
        .y_max = config->touch_config.y_max,
        .max_points = static_cast<uint8_t>(CONFIG_ESP_LCD_TOUCH_MAX_POINTS),
        .operation_mode = GPIO_IS_VALID_GPIO(config->touch_config.int_gpio_num) ?
        display::TouchIface::OperationMode::Interrupt : display::TouchIface::OperationMode::Polling,
        .group_id = DISPLAY_GROUP_ID,
    };
}
} // namespace

I2cDisplayTouchImpl::I2cDisplayTouchImpl()
    : display::TouchIface(generate_info())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", get_info());

    BROOKESIA_CHECK_FALSE_EXIT(get_info().is_valid(), "Invalid touch information");

    if (!esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH)) {
        BROOKESIA_LOGW("LCD touch device not found, skip");
        return;
    }

    auto ret = esp_board_manager_init_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to init LCD touch");

    ret = esp_board_manager_get_device_handle(ESP_BOARD_DEVICE_NAME_LCD_TOUCH, &handles_);
    BROOKESIA_CHECK_ESP_ERR_EXIT(ret, "Failed to get handles");
}

I2cDisplayTouchImpl::~I2cDisplayTouchImpl()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    register_interrupt_handler(nullptr, nullptr);

    if (esp_board_manager_check_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH)) {
        auto ret = esp_board_manager_deinit_device_by_name(ESP_BOARD_DEVICE_NAME_LCD_TOUCH);
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, { BROOKESIA_LOGE("Failed to deinit LCD touch"); });
    }
}

bool I2cDisplayTouchImpl::read_points(std::vector<display::TouchIface::Point> &points)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::unique_lock<boost::mutex> lock(mutex_);

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
        points[i] = display::TouchIface::Point {
            .x = static_cast<int16_t>(data[i].x),
            .y = static_cast<int16_t>(data[i].y),
            .pressure = static_cast<uint16_t>(data[i].strength),
            .track_id = data[i].track_id,
        };
    }

    return true;
}

bool I2cDisplayTouchImpl::register_interrupt_handler(InterruptHandler handler, void *ctx)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: handler_registered(%1%), ctx(%2%)", handler != nullptr, ctx);

    boost::lock_guard<boost::mutex> lock(mutex_);

    if ((interrupt_handler_ == nullptr) && (handler == nullptr)) {
        BROOKESIA_LOGD("Interrupt handler is already unregistered");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(is_valid_internal(), false, "LCD touch is not initialized");
    BROOKESIA_CHECK_FALSE_RETURN(
        get_info().operation_mode == OperationMode::Interrupt, false, "Only support in interrupt mode"
    );

    if (handler) {
        auto callback = [](esp_lcd_touch_handle_t tp) {
            auto iface = reinterpret_cast<I2cDisplayTouchImpl *>(tp->config.user_data);
            if (iface == nullptr) {
                return;
            }

            InterruptHandler interrupt_handler = nullptr;
            void *interrupt_handler_ctx = nullptr;
            portENTER_CRITICAL_ISR(&iface->interrupt_lock_);
            interrupt_handler = iface->interrupt_handler_;
            interrupt_handler_ctx = iface->interrupt_handler_ctx_;
            portEXIT_CRITICAL_ISR(&iface->interrupt_lock_);

            if ((interrupt_handler != nullptr) && interrupt_handler(interrupt_handler_ctx)) {
                portYIELD_FROM_ISR();
            }
        };

        portENTER_CRITICAL(&interrupt_lock_);
        interrupt_handler_ = handler;
        interrupt_handler_ctx_ = ctx;
        portEXIT_CRITICAL(&interrupt_lock_);

        auto ret = esp_lcd_touch_register_interrupt_callback_with_data(get_touch_handle(handles_), callback, this);
        if (ret != ESP_OK) {
            portENTER_CRITICAL(&interrupt_lock_);
            interrupt_handler_ = nullptr;
            interrupt_handler_ctx_ = nullptr;
            portEXIT_CRITICAL(&interrupt_lock_);
            BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to register interrupt callback");
        }
        return true;
    }

    portENTER_CRITICAL(&interrupt_lock_);
    interrupt_handler_ = nullptr;
    interrupt_handler_ctx_ = nullptr;
    portEXIT_CRITICAL(&interrupt_lock_);

    auto ret = esp_lcd_touch_register_interrupt_callback_with_data(get_touch_handle(handles_), nullptr, nullptr);
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
