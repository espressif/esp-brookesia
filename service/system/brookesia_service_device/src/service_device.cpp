/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <expected>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_device/macro_configs.h"
#if !BROOKESIA_SERVICE_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/service_device/service_device.hpp"
#include "private/utils.hpp"

namespace esp_brookesia::service {

std::string Device::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_DEVICE_VER_MAJOR, BROOKESIA_SERVICE_DEVICE_VER_MINOR, BROOKESIA_SERVICE_DEVICE_VER_PATCH
           );
}

bool Device::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_DEVICE_VER_MAJOR, BROOKESIA_SERVICE_DEVICE_VER_MINOR,
        BROOKESIA_SERVICE_DEVICE_VER_PATCH
    );

    BROOKESIA_LOGI("Initialized");

    return true;
}

void Device::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_power_battery_polling();
    power_.cached_state.reset();
    power_.cached_charge_config.reset();
    power_.polling_requested = false;
    reset_interfaces();

    BROOKESIA_LOGI("Deinitialized");
}

bool Device::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    power_.cached_state.reset();
    power_.cached_charge_config.reset();
    if (power_.polling_requested) {
        BROOKESIA_CHECK_FALSE_RETURN(start_power_battery_polling(), false, "Failed to start power battery polling");
    }

    return true;
}

void Device::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    stop_power_battery_polling();
    power_.cached_state.reset();
    power_.cached_charge_config.reset();
    reset_interfaces();
}

std::vector<FunctionSchema> Device::get_function_schemas()
{
    auto function_schemas = Helper::get_function_schemas();
    return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
}

std::vector<EventSchema> Device::get_event_schemas()
{
    auto event_schemas = Helper::get_event_schemas();
    return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
}

ServiceBase::FunctionHandlerMap Device::get_function_handlers()
{
    return {
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetCapabilities,
            function_get_capabilities()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetBoardInfo,
            function_get_board_info()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetCameraDeviceInfos,
            function_get_camera_device_infos()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetNetworkConnectivityInfo,
            function_get_network_connectivity_info()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetPowerBatteryInfo,
            function_get_power_battery_info()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetPowerBatteryState,
            function_get_power_battery_state()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetPowerBatteryChargeConfig,
            function_get_power_battery_charge_config()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            Helper, Helper::FunctionId::SetPowerBatteryChargeConfig, boost::json::object,
            function_set_power_battery_charge_config(PARAM)
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            Helper, Helper::FunctionId::SetPowerBatteryChargingEnabled, bool,
            function_set_power_battery_charging_enabled(PARAM)
        ),
    };
}

bool Device::ensure_power_battery_iface()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (power_.battery_iface) {
        return true;
    }
    if (!hal::has_interface(hal::power::BatteryIface::NAME)) {
        BROOKESIA_LOGD("Power battery interface is not available");
        return false;
    }

    power_.battery_iface = hal::acquire_first_interface<hal::power::BatteryIface>();
    return static_cast<bool>(power_.battery_iface);
}

void Device::reset_interfaces()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    power_.battery_iface.reset();
}

#if BROOKESIA_SERVICE_DEVICE_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Device, Device::get_instance().get_attributes().name, Device::get_instance(),
    BROOKESIA_SERVICE_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service
