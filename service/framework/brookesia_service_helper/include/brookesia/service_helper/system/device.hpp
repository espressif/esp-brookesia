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
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/system/board_info.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
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
        Max,
    };

    enum class EventId {
        PowerBatteryStateChanged,
        PowerBatteryChargeConfigChanged,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetPowerBatteryChargeConfigParam {
        Config,
    };

    enum class FunctionSetPowerBatteryChargingEnabledParam {
        Enabled,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventPowerBatteryStateChangedParam {
        State,
    };

    enum class EventPowerBatteryChargeConfigChangedParam {
        Config,
    };

private:
    static FunctionSchema function_schema_get_capabilities()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetCapabilities),
            .description = "Get available HAL devices and interfaces.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(Capabilities({
                    hal::DeviceInfo{
                        .name = "General",
                        .interfaces = {
                            hal::InterfaceInfo{hal::system::BoardInfoIface::NAME, "System:BoardInfo"},
                        },
                    },
                    hal::DeviceInfo{
                        .name = "Display",
                        .interfaces = {
                            hal::InterfaceInfo{hal::display::PanelIface::NAME, "Display:PanelA"},
                            hal::InterfaceInfo{hal::display::PanelIface::NAME, "Display:PanelB"},
                        },
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_board_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetBoardInfo),
            .description = "Get static board information.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((BoardInfo{
                    .name = "esp32_s3_touch_amoled_1_8",
                    .chip = "ESP32-S3",
                    .version = "v1.0",
                    .description = "Example board information",
                    .manufacturer = "Espressif",
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_camera_device_infos()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetCameraDeviceInfos),
            .description = "Get available camera devices.",
            .default_timeout_ms = 2000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description =
                (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(CameraDeviceInfos({
                    hal::video::CameraIface::DeviceInfo{
                        .id = 0,
                        .name = "camera",
                        .device_path = "/dev/video0",
                        .supported_formats = {
                            hal::video::EncoderSinkFormat::YUV422,
                        },
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_network_connectivity_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetNetworkConnectivityInfo),
            .description = "Get current network connectivity status.",
            .default_timeout_ms = 2000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description =
                (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(NetworkConnectivityInfos({
                    NetworkConnectivityInfo{
                        .instance_name = "Network:Connectivity:0",
                        .status = hal::network::NetworkStatus{
                            .interface_type = hal::network::InterfaceType::WifiStation,
                            .link_state = hal::network::LinkState::Up,
                            .ip_state = hal::network::IpState::Ready,
                            .reachability = hal::network::Reachability::LocalOnly,
                        },
                        .state = hal::network::ConnectivityState::LocalNetworkReady,
                        .network_ready = true,
                        .internet_ready = false,
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_power_battery_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPowerBatteryInfo),
            .description = "Get static power battery information.",
            .default_timeout_ms = 2000,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((PowerBatteryInfo{
                    .name = "MainBattery",
                    .chemistry = "Li-ion",
                    .abilities = {
                        hal::power::BatteryIface::Ability::Voltage,
                        hal::power::BatteryIface::Ability::Percentage,
                        hal::power::BatteryIface::Ability::ChargeState,
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_power_battery_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPowerBatteryState),
            .description = "Get current power battery state.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((PowerBatteryState{
                    .is_present = true,
                    .power_source = hal::power::BatteryIface::PowerSource::Battery,
                    .charge_state = hal::power::BatteryIface::ChargeState::NotCharging,
                    .level_source = hal::power::BatteryIface::LevelSource::VoltageCurve,
                    .voltage_mv = 3920,
                    .percentage = 67,
                    .vbus_voltage_mv = std::nullopt,
                    .system_voltage_mv = std::nullopt,
                    .is_low = false,
                    .is_critical = false,
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_power_battery_charge_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPowerBatteryChargeConfig),
            .description = "Get current power battery charge configuration.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((PowerBatteryChargeConfig{
                    .enabled = true,
                    .target_voltage_mv = 4200,
                    .charge_current_ma = 500,
                    .precharge_current_ma = 100,
                    .termination_current_ma = 100,
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_set_power_battery_charge_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetPowerBatteryChargeConfig),
            .description = "Set power battery charge configuration.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetPowerBatteryChargeConfigParam::Config),
                    .description = "Battery charge configuration object.",
                    .type = FunctionValueType::Object,
                }
            },
        };
    }

    static FunctionSchema function_schema_set_power_battery_charging_enabled()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetPowerBatteryChargingEnabled),
            .description = "Enable or disable battery charging.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetPowerBatteryChargingEnabledParam::Enabled),
                    .description = "True to enable charging, false to disable charging.",
                    .type = FunctionValueType::Boolean,
                }
            },
        };
    }

    static EventSchema event_schema_power_battery_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::PowerBatteryStateChanged),
            .description = "Emitted when the power battery state snapshot changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventPowerBatteryStateChangedParam::State),
                    .description = "Current power battery state snapshot.",
                    .type = EventItemType::Object,
                }
            },
        };
    }

    static EventSchema event_schema_power_battery_charge_config_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::PowerBatteryChargeConfigChanged),
            .description = "Emitted when the power battery charge configuration changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventPowerBatteryChargeConfigChangedParam::Config),
                    .description = "Current power battery charge configuration.",
                    .type = EventItemType::Object,
                }
            },
        };
    }

public:
    static constexpr std::string_view get_name()
    {
        return "Device";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_get_capabilities(),
                function_schema_get_board_info(),
                function_schema_get_camera_device_infos(),
                function_schema_get_network_connectivity_info(),
                function_schema_get_power_battery_info(),
                function_schema_get_power_battery_state(),
                function_schema_get_power_battery_charge_config(),
                function_schema_set_power_battery_charge_config(),
                function_schema_set_power_battery_charging_enabled(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_power_battery_state_changed(),
                event_schema_power_battery_charge_config_changed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

BROOKESIA_DESCRIBE_ENUM(
    Device::FunctionId, GetCapabilities, GetBoardInfo, GetCameraDeviceInfos, GetNetworkConnectivityInfo,
    GetPowerBatteryInfo, GetPowerBatteryState, GetPowerBatteryChargeConfig, SetPowerBatteryChargeConfig,
    SetPowerBatteryChargingEnabled, Max
);
BROOKESIA_DESCRIBE_STRUCT(
    Device::NetworkConnectivityInfo, (), (instance_name, status, state, network_ready, internet_ready)
);
BROOKESIA_DESCRIBE_ENUM(
    Device::EventId, PowerBatteryStateChanged, PowerBatteryChargeConfigChanged, Max
);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetPowerBatteryChargeConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetPowerBatteryChargingEnabledParam, Enabled);
BROOKESIA_DESCRIBE_ENUM(Device::EventPowerBatteryStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(Device::EventPowerBatteryChargeConfigChangedParam, Config);

} // namespace esp_brookesia::service::helper
