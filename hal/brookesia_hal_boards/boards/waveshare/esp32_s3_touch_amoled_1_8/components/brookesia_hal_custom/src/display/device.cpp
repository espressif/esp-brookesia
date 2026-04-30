/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_custom/macro_configs.h"
#if !BROOKESIA_HAL_CUSTOM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_adaptor.hpp"
#include "brookesia/hal_custom/display/device.hpp"
#include "display/backlight_impl.hpp"

using namespace esp_brookesia;

namespace esp_brookesia::hal {

bool CustomDisplayDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool CustomDisplayDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_EXECUTE(init_backlight(), {}, { BROOKESIA_LOGE("Failed to init display backlight"); });

    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid display interfaces found");

    return true;
}

void CustomDisplayDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    deinit_backlight();
}

bool CustomDisplayDevice::init_backlight()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(DISPLAY_BACKLIGHT_IMPL_NAME)) {
        BROOKESIA_LOGD("Backlight is already initialized, skip");
        return true;
    }

    std::shared_ptr<CustomDisplayBacklightImpl> iface = nullptr;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        iface = std::make_shared<CustomDisplayBacklightImpl>(), false,
        "Failed to create backlight interface"
    );
    lib_utils::FunctionGuard iface_delete_guard([this, &iface]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        iface.reset();
    });

    BROOKESIA_CHECK_FALSE_RETURN(iface->is_valid(), false, "Failed to create backlight interface");

    interfaces_.emplace(DISPLAY_BACKLIGHT_IMPL_NAME, iface);

    iface_delete_guard.release();

    return true;
}

void CustomDisplayDevice::deinit_backlight()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_iface_initialized(DISPLAY_BACKLIGHT_IMPL_NAME)) {
        BROOKESIA_LOGD("Backlight is not initialized, skip");
        return;
    }

    interfaces_.erase(DISPLAY_BACKLIGHT_IMPL_NAME);
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_DISPLAY_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, CustomDisplayDevice, CustomDisplayDevice::DEVICE_NAME, CustomDisplayDevice::get_instance(),
    BROOKESIA_HAL_CUSTOM_DISPLAY_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
