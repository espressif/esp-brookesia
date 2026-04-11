/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_board_manager_includes.h"
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_adaptor/display/device.hpp"
#include "ledc_backlight_impl.hpp"
#include "lcd_panel_impl.hpp"
#include "lcd_touch_impl.hpp"

namespace esp_brookesia::hal {

bool DisplayDevice::set_ledc_backlight_info(DisplayBacklightIface::Info info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", BROOKESIA_DESCRIBE_TO_STR(info));

    BROOKESIA_CHECK_FALSE_RETURN(
        !is_iface_initialized(LEDC_BACKLIGHT_IMPL_NAME), false, "Should called before initializing LEDC backlight"
    );

    ledc_backlight_info_ = info;

    return true;
}

bool DisplayDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool DisplayDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
    BROOKESIA_CHECK_FALSE_EXECUTE(init_lcd_panel(), {}, { BROOKESIA_LOGE("Failed to init LCD panel"); });
#endif
#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL
    BROOKESIA_CHECK_FALSE_EXECUTE(init_lcd_touch(), {}, { BROOKESIA_LOGE("Failed to init LCD touch"); });
#endif
#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL
    BROOKESIA_CHECK_FALSE_EXECUTE(init_ledc_backlight(), {}, { BROOKESIA_LOGE("Failed to init LEDC backlight"); });
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid display interfaces found");

    return true;
}

void DisplayDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LEDC_BACKLIGHT_IMPL
    deinit_ledc_backlight();
#endif
#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_TOUCH_IMPL
    deinit_lcd_touch();
#endif
#if BROOKESIA_HAL_ADAPTOR_DISPLAY_ENABLE_LCD_PANEL_IMPL
    deinit_lcd_panel();
#endif
}

bool DisplayDevice::init_lcd_panel()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(LCD_PANEL_IMPL_NAME)) {
        BROOKESIA_LOGD("LCD panel is already initialized, skip");
        return true;
    }

    std::shared_ptr<LcdDisplayPanelImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<LcdDisplayPanelImpl>(), false, "Failed to create LCD panel interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Failed to create panel interface");

    interfaces_.emplace(LCD_PANEL_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void DisplayDevice::deinit_lcd_panel()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(LCD_PANEL_IMPL_NAME)) {
        BROOKESIA_LOGD("LCD panel is not initialized, skip");
        return;
    }

    interfaces_.erase(LCD_PANEL_IMPL_NAME);
}

bool DisplayDevice::init_ledc_backlight()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(LEDC_BACKLIGHT_IMPL_NAME)) {
        BROOKESIA_LOGD("LEDC backlight is already initialized, skip");
        return true;
    }

    std::shared_ptr<LedcDisplayBacklightImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<LedcDisplayBacklightImpl>(ledc_backlight_info_), false,
        "Failed to create LEDC backlight interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Failed to create LEDC backlight interface");

    interfaces_.emplace(LEDC_BACKLIGHT_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void DisplayDevice::deinit_ledc_backlight()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(LEDC_BACKLIGHT_IMPL_NAME)) {
        BROOKESIA_LOGD("LEDC backlight is not initialized, skip");
        return;
    }

    interfaces_.erase(LEDC_BACKLIGHT_IMPL_NAME);
}

bool DisplayDevice::init_lcd_touch()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(LCD_TOUCH_IMPL_NAME)) {
        BROOKESIA_LOGD("LCD touch is already initialized, skip");
        return true;
    }

    std::shared_ptr<I2cDisplayTouchImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<I2cDisplayTouchImpl>(), false, "Failed to create LCD touch interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Failed to create LCD touch interface");

    interfaces_.emplace(LCD_TOUCH_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void DisplayDevice::deinit_lcd_touch()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(LCD_TOUCH_IMPL_NAME)) {
        BROOKESIA_LOGD("LCD touch is not initialized, skip");
        return;
    }

    interfaces_.erase(LCD_TOUCH_IMPL_NAME);
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, DisplayDevice, DisplayDevice::DEVICE_NAME, DisplayDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_DISPLAY_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
