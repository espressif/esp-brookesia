/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/hal_interface/interfaces/expansion/module_manager.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/system/board_info.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/detail/static_schema.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the device control service.
 */
class Device: public Base<Device> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Runtime capability snapshot for the device control service.
     */
    using Capabilities = hal::DeviceInfoList;

    /**
     * @brief Static board information.
     */
    using BoardInfo = hal::system::BoardInfoIface::Info;

    /**
     * @brief Static battery information.
     */
    using PowerBatteryInfo = hal::power::BatteryIface::Info;

    /**
     * @brief Runtime battery state snapshot.
     */
    using PowerBatteryState = hal::power::BatteryIface::State;

    /**
     * @brief Battery charge configuration.
     */
    using PowerBatteryChargeConfig = hal::power::BatteryIface::ChargeConfig;

    /**
     * @brief Camera device information list.
     */
    using CameraDeviceInfos = std::vector<hal::video::CameraIface::DeviceInfo>;

    /**
     * @brief Expansion module information list.
     */
    using ExpansionModuleInfos = std::vector<hal::expansion::ModuleInfo>;

    /**
     * @brief Current network connectivity snapshot.
     */
    struct NetworkConnectivityInfo {
        std::string instance_name;
        hal::network::NetworkStatus status;
        hal::network::ConnectivityState state = hal::network::ConnectivityState::Unknown;
        bool network_ready = false;
        bool internet_ready = false;
    };
    using NetworkConnectivityInfos = std::vector<NetworkConnectivityInfo>;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        GetCapabilities,
        GetBoardInfo,
        GetCameraDeviceInfos,
        GetNetworkConnectivityInfo,
        GetPowerBatteryInfo,
        GetPowerBatteryState,
        GetPowerBatteryChargeConfig,
        SetPowerBatteryChargeConfig,
        SetPowerBatteryChargingEnabled,
        GetExpansionModuleInfos,
        Max,
    };

    enum class EventId {
        PowerBatteryStateChanged,
        PowerBatteryChargeConfigChanged,
        ExpansionModuleChanged,
        Max,
    };
private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the static schema specifications ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using EventItemSpec = detail::static_schema::EventItemSpec;
    using EventSpec = detail::static_schema::EventSpec;
    using FunctionParameterSpec = detail::static_schema::FunctionParameterSpec;
    using FunctionReturnSpec = detail::static_schema::FunctionReturnSpec;
    using FunctionSpec = detail::static_schema::FunctionSpec;

    inline static constexpr std::span<const FunctionParameterSpec> EMPTY_PARAMETERS = {};

    inline static constexpr std::array<FunctionParameterSpec, 1> SET_POWER_BATTERY_CHARGE_CONFIG_PARAMETERS = {{
            {
                .name = "Config",
                .description = "Battery charge configuration object.",
                .type = FunctionValueType::Object,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> SET_POWER_BATTERY_CHARGING_ENABLED_PARAMETERS = {{
            {
                .name = "Enabled",
                .description = "True to enable charging, false to disable charging.",
                .type = FunctionValueType::Boolean,
            },
        }
    };

    inline static constexpr std::array<FunctionSpec, static_cast<size_t>(FunctionId::Max)> FUNCTION_SPECS = {{
            {
                .name = "GetCapabilities",
                .description = "Get available HAL devices and interfaces.",
                .parameters = EMPTY_PARAMETERS,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description =
                    R"(Example: [{"name":"General","interfaces":[{"type_name":"SystemBoardInfo",)"
                    R"("instance_name":"System:BoardInfo"}]},{"name":"Display","interfaces":[)"
                    R"({"type_name":"DisplayPanel","instance_name":"Display:PanelA"},{"type_name":)"
                    R"("DisplayPanel","instance_name":"Display:PanelB"}]}])",
                },
            },
            {
                .name = "GetBoardInfo",
                .description = "Get static board information.",
                .parameters = EMPTY_PARAMETERS,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Object,
                    .description =
                    R"(Example: {"name":"esp32_s3_touch_amoled_1_8","chip":"ESP32-S3","version":"v1.0",)"
                    R"("description":"Example board information","manufacturer":"Espressif"})",
                },
            },
            {
                .name = "GetCameraDeviceInfos",
                .description = "Get available camera devices.",
                .parameters = EMPTY_PARAMETERS,
                .default_timeout_ms = 2000,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description = R"(Example: [{"id":0,"name":"camera","device_path":"/dev/video0",)"
                    R"("supported_formats":["YUV422"]}])",
                },
            },
            {
                .name = "GetNetworkConnectivityInfo",
                .description = "Get current network connectivity status.",
                .parameters = EMPTY_PARAMETERS,
                .default_timeout_ms = 2000,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description =
                    R"(Example: [{"instance_name":"Network:Connectivity:0","status":)"
                    R"({"interface_type":"WifiStation",)"
                    R"("link_state":"Up","ip_state":"Ready","reachability":"LocalOnly","ip_info":null,)"
                    R"("signal_dbm":null,"connected_duration_ms":null},"state":"LocalNetworkReady",)"
                    R"("network_ready":true,"internet_ready":false}])",
                },
            },
            {
                .name = "GetPowerBatteryInfo",
                .description = "Get static power battery information.",
                .parameters = EMPTY_PARAMETERS,
                .default_timeout_ms = 2000,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Object,
                    .description =
                    R"(Example: {"name":"MainBattery","chemistry":"Li-ion","abilities":["Voltage",)"
                    R"("Percentage","ChargeState"]})",
                },
            },
            {
                .name = "GetPowerBatteryState",
                .description = "Get current power battery state.",
                .parameters = EMPTY_PARAMETERS,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Object,
                    .description =
                    R"(Example: {"is_present":true,"power_source":"Battery","charge_state":"NotCharging",)"
                    R"("level_source":"VoltageCurve","voltage_mv":3920,"percentage":67,"vbus_voltage_mv":null,)"
                    R"("system_voltage_mv":null,"is_low":false,"is_critical":false})",
                },
            },
            {
                .name = "GetPowerBatteryChargeConfig",
                .description = "Get current power battery charge configuration.",
                .parameters = EMPTY_PARAMETERS,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Object,
                    .description =
                    R"(Example: {"enabled":true,"target_voltage_mv":4200,"charge_current_ma":500,)"
                    R"("precharge_current_ma":100,"termination_current_ma":100})",
                },
            },
            {
                .name = "SetPowerBatteryChargeConfig",
                .description = "Set power battery charge configuration.",
                .parameters = SET_POWER_BATTERY_CHARGE_CONFIG_PARAMETERS,
            },
            {
                .name = "SetPowerBatteryChargingEnabled",
                .description = "Enable or disable battery charging.",
                .parameters = SET_POWER_BATTERY_CHARGING_ENABLED_PARAMETERS,
            },
            {
                .name = "GetExpansionModuleInfos",
                .description = "Get stable expansion module states for all slots.",
                .parameters = EMPTY_PARAMETERS,
                .default_timeout_ms = 2000,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description =
                    R"(Example: [{"provider":"mosaico","slot":"left","type":"camera",)"
                    R"("board_id":7,"board_name":"OV3640 Camera","generation":1,"state":"Ready"}])",
                },
            },
        }
    };
    static_assert(FUNCTION_SPECS.size() == static_cast<size_t>(FunctionId::Max));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schema specifications //////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    inline static constexpr std::array<EventItemSpec, 1> EXPANSION_MODULE_CHANGED_ITEMS = {{
            {
                .name = "Module",
                .description = "Current stable expansion module snapshot.",
                .type = EventItemType::Object,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> POWER_BATTERY_STATE_CHANGED_ITEMS = {{
            {
                .name = "State",
                .description = "Current power battery state snapshot.",
                .type = EventItemType::Object,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> POWER_BATTERY_CHARGE_CONFIG_CHANGED_ITEMS = {{
            {
                .name = "Config",
                .description = "Current power battery charge configuration.",
                .type = EventItemType::Object,
            },
        }
    };

    inline static constexpr std::array<EventSpec, static_cast<size_t>(EventId::Max)> EVENT_SPECS = {{
            {
                .name = "PowerBatteryStateChanged",
                .description = "Emitted when the power battery state snapshot changes.",
                .items = POWER_BATTERY_STATE_CHANGED_ITEMS,
            },
            {
                .name = "PowerBatteryChargeConfigChanged",
                .description = "Emitted when the power battery charge configuration changes.",
                .items = POWER_BATTERY_CHARGE_CONFIG_CHANGED_ITEMS,
            },
            {
                .name = "ExpansionModuleChanged",
                .description = "Emitted when a stable expansion module state changes.",
                .items = EXPANSION_MODULE_CHANGED_ITEMS,
            },
        }
    };
    static_assert(EVENT_SPECS.size() == static_cast<size_t>(EventId::Max));

public:
    static constexpr std::string_view get_name()
    {
        return "Device";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static std::array<FunctionSchema, FUNCTION_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_function_schemas(FUNCTION_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static std::array<EventSchema, EVENT_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_event_schemas(EVENT_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }
};

BROOKESIA_DESCRIBE_ENUM(
    Device::FunctionId, GetCapabilities, GetBoardInfo, GetCameraDeviceInfos, GetNetworkConnectivityInfo,
    GetPowerBatteryInfo, GetPowerBatteryState, GetPowerBatteryChargeConfig, SetPowerBatteryChargeConfig,
    SetPowerBatteryChargingEnabled, GetExpansionModuleInfos, Max
);
BROOKESIA_DESCRIBE_STRUCT(
    Device::NetworkConnectivityInfo, (), (instance_name, status, state, network_ready, internet_ready)
);
BROOKESIA_DESCRIBE_ENUM(
    Device::EventId, PowerBatteryStateChanged, PowerBatteryChargeConfigChanged, ExpansionModuleChanged, Max
);

} // namespace esp_brookesia::service::helper
