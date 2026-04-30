/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_POWER_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_adaptor/power/device.hpp"
#include "battery_axp2101_impl.hpp"
#include "battery_adc_impl.hpp"

namespace esp_brookesia::hal {

bool PowerDevice::probe()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return true;
}

bool PowerDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY
    BROOKESIA_CHECK_FALSE_RETURN(init_battery(), false, "Failed to init battery");
#endif
    BROOKESIA_CHECK_FALSE_RETURN(!interfaces_.empty(), false, "No valid power interfaces initialized");

    return true;
}

void PowerDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

#if BROOKESIA_HAL_ADAPTOR_POWER_ENABLE_BATTERY
    deinit_battery();
#endif
}

bool PowerDevice::init_battery()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(BATTERY_IMPL_NAME)) {
        BROOKESIA_LOGD("Battery is already initialized, skip");
        return true;
    }

    bool has_valid_iface = false;

#if BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_AXP2101
    {
        std::shared_ptr<BatteryAxp2101Impl> iface = nullptr;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            iface = std::make_shared<BatteryAxp2101Impl>(), false, "Failed to create AXP2101 battery interface"
        );
        if (iface->is_valid()) {
            interfaces_.emplace(BATTERY_IMPL_NAME, iface);
            has_valid_iface = true;
        }
    }
#endif

#if BROOKESIA_HAL_ADAPTOR_POWER_BATTERY_IMPL_ADC && defined(CONFIG_ESP_BOARD_PERIPH_ADC_SUPPORT)
    {
        std::shared_ptr<BatteryAdcImpl> iface = nullptr;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            iface = std::make_shared<BatteryAdcImpl>(), false, "Failed to create ADC battery interface"
        );
        if (iface->is_valid()) {
            interfaces_.emplace(BATTERY_IMPL_NAME, iface);
            has_valid_iface = true;
        }
    }
#endif

    if (!has_valid_iface) {
        BROOKESIA_LOGW("No valid battery interface found, skip");
    }

    return true;
}

void PowerDevice::deinit_battery()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_iface_initialized(BATTERY_IMPL_NAME)) {
        interfaces_.erase(BATTERY_IMPL_NAME);
    }
}

#if BROOKESIA_HAL_ADAPTOR_ENABLE_POWER_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, PowerDevice, PowerDevice::DEVICE_NAME, PowerDevice::get_instance(),
    BROOKESIA_HAL_ADAPTOR_POWER_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
