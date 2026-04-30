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
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/hal_interface/interfaces/general/board_info.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/interfaces/storage/fs.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

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
    using Capabilities = hal::Capabilities;

    /**
     * @brief Static board information.
     */
    using BoardInfo = hal::BoardInfoIface::Info;

    /**
     * @brief Storage file system information.
     */
    using StorageFsInfo = hal::StorageFsIface::Info;

    /**
     * @brief Static battery information.
     */
    using PowerBatteryInfo = hal::PowerBatteryIface::Info;

    /**
     * @brief Runtime battery state snapshot.
     */
    using PowerBatteryState = hal::PowerBatteryIface::State;

    /**
     * @brief Battery charge configuration.
     */
    using PowerBatteryChargeConfig = hal::PowerBatteryIface::ChargeConfig;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        GetCapabilities,
        GetBoardInfo,
        SetDisplayBacklightBrightness,
        GetDisplayBacklightBrightness,
        SetDisplayBacklightOnOff,
        GetDisplayBacklightOnOff,
        SetAudioPlayerVolume,
        GetAudioPlayerVolume,
        SetAudioPlayerMute,
        GetAudioPlayerMute,
        GetStorageFileSystems,
        GetPowerBatteryInfo,
        GetPowerBatteryState,
        GetPowerBatteryChargeConfig,
        SetPowerBatteryChargeConfig,
        SetPowerBatteryChargingEnabled,
        ResetData,
        Max,
    };

    enum class EventId {
        DisplayBacklightBrightnessChanged,
        DisplayBacklightOnOffChanged,
        AudioPlayerVolumeChanged,
        AudioPlayerMuteChanged,
        PowerBatteryStateChanged,
        PowerBatteryChargeConfigChanged,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetDisplayBacklightBrightnessParam {
        Brightness,
    };

    enum class FunctionSetDisplayBacklightOnOffParam {
        On,
    };

    enum class FunctionSetAudioPlayerVolumeParam {
        Volume,
    };

    enum class FunctionSetAudioPlayerMuteParam {
        Enable,
    };

    enum class FunctionSetPowerBatteryChargeConfigParam {
        Config,
    };

    enum class FunctionSetPowerBatteryChargingEnabledParam {
        Enabled,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventDisplayBacklightBrightnessChangedParam {
        Brightness,
    };

    enum class EventDisplayBacklightOnOffChangedParam {
        IsOn,
    };

    enum class EventAudioPlayerVolumeChangedParam {
        Volume,
    };

    enum class EventAudioPlayerMuteChangedParam {
        IsMuted,
    };

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
            .description = (boost::format("Get device control service capabilities. Return type: JSON object. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(Capabilities({
                {std::string(hal::BoardInfoIface::NAME), {"General:BoardInfo"}},
                {std::string(hal::DisplayPanelIface::NAME), {"Display:PanelA", "Display:PanelB"}},
            }))).str(),
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_get_board_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetBoardInfo),
            .description = (boost::format("Get static board information. Return type: JSON object. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((BoardInfo{
                .name = "esp32_s3_touch_amoled_1_8",
                .chip = "ESP32-S3",
                .version = "v1.0",
                .description = "Example board information",
                .manufacturer = "Espressif",
            }))).str(),
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_set_display_backlight_brightness()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetDisplayBacklightBrightness),
            .description = "Set display backlight brightness percentage.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetDisplayBacklightBrightnessParam::Brightness),
                    .description =
                    "Backlight brightness percentage in range [0, 100]. The actual brightness will be mapped to the hardware range"
                    " [BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MIN, "
                    "BROOKESIA_SERVICE_DEVICE_DISPLAY_BACKLIGHT_BRIGHTNESS_MAX].",
                    .type = FunctionValueType::Number,
                }
            },
        };
    }

    static FunctionSchema function_schema_get_display_backlight_brightness()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetDisplayBacklightBrightness),
            .description = "Get current display backlight brightness percentage [0, 100]. Return type: number.",
        };
    }

    static FunctionSchema function_schema_set_display_backlight_on_off()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetDisplayBacklightOnOff),
            .description = "Turn display backlight on or off.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetDisplayBacklightOnOffParam::On),
                    .description = "True to turn on the backlight, false to turn it off.",
                    .type = FunctionValueType::Boolean,
                }
            },
        };
    }

    static FunctionSchema function_schema_get_display_backlight_on_off()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetDisplayBacklightOnOff),
            .description = "Get whether display backlight is enabled. Return type: boolean.",
        };
    }

    static FunctionSchema function_schema_set_audio_player_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAudioPlayerVolume),
            .description = "Set target audio player volume percentage.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAudioPlayerVolumeParam::Volume),
                    .description = "Player volume percentage in range [0, 100]. The actual volume will be mapped to the hardware range"
                    " [BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MIN, BROOKESIA_SERVICE_DEVICE_AUDIO_PLAYER_VOLUME_MAX].",
                    .type = FunctionValueType::Number,
                }
            },
        };
    }

    static FunctionSchema function_schema_get_audio_player_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAudioPlayerVolume),
            .description = "Get target audio player volume percentage. Return type: number.",
        };
    }

    static FunctionSchema function_schema_set_audio_player_mute()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAudioPlayerMute),
            .description = "Set whether audio player is muted.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAudioPlayerMuteParam::Enable),
                    .description = "True to mute audio player, false to unmute it.",
                    .type = FunctionValueType::Boolean,
                }
            },
        };
    }

    static FunctionSchema function_schema_get_audio_player_mute()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAudioPlayerMute),
            .description = "Get whether audio player is muted. Return type: boolean.",
        };
    }

    static FunctionSchema function_schema_get_storage_file_systems()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetStorageFileSystems),
            .description = (boost::format("Get mounted storage file systems. Return type: JSON array<object>. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<StorageFsInfo>({
                {
                    .fs_type = hal::StorageFsIface::FileSystemType::SPIFFS,
                    .medium_type = hal::StorageFsIface::MediumType::Flash,
                    .mount_point = "/spiffs",
                },
                {
                    .fs_type = hal::StorageFsIface::FileSystemType::FATFS,
                    .medium_type = hal::StorageFsIface::MediumType::SDCard,
                    .mount_point = "/sdcard",
                },
            }))).str(),
        };
    }

    static FunctionSchema function_schema_get_power_battery_info()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPowerBatteryInfo),
            .description = (boost::format("Get static power battery information. Return type: JSON object. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((PowerBatteryInfo{
                .name = "MainBattery",
                .chemistry = "Li-ion",
                .abilities = {
                    hal::PowerBatteryIface::Ability::Voltage,
                    hal::PowerBatteryIface::Ability::Percentage,
                    hal::PowerBatteryIface::Ability::ChargeState,
                },
            }))).str(),
        };
    }

    static FunctionSchema function_schema_get_power_battery_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPowerBatteryState),
            .description = (boost::format("Get current power battery state. Return type: JSON object. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((PowerBatteryState{
                .is_present = true,
                .power_source = hal::PowerBatteryIface::PowerSource::Battery,
                .charge_state = hal::PowerBatteryIface::ChargeState::NotCharging,
                .level_source = hal::PowerBatteryIface::LevelSource::VoltageCurve,
                .voltage_mv = 3920,
                .percentage = 67,
                .vbus_voltage_mv = std::nullopt,
                .system_voltage_mv = std::nullopt,
                .is_low = false,
                .is_critical = false,
            }))).str(),
        };
    }

    static FunctionSchema function_schema_get_power_battery_charge_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetPowerBatteryChargeConfig),
            .description = (boost::format("Get current power battery charge configuration. Return type: JSON object. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((PowerBatteryChargeConfig{
                .enabled = true,
                .target_voltage_mv = 4200,
                .charge_current_ma = 500,
                .precharge_current_ma = 100,
                .termination_current_ma = 100,
            }))).str(),
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

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset persisted device control state, including backlight brightness, player volume, and player mute.",
        };
    }

    static EventSchema event_schema_audio_player_volume_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AudioPlayerVolumeChanged),
            .description = "Emitted when the target audio player volume changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAudioPlayerVolumeChangedParam::Volume),
                    .description = "Current target player volume percentage [0, 100].",
                    .type = EventItemType::Number,
                }
            },
        };
    }

    static EventSchema event_schema_audio_player_mute_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AudioPlayerMuteChanged),
            .description = "Emitted when the target audio player mute state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAudioPlayerMuteChangedParam::IsMuted),
                    .description = "Whether audio player is currently muted. True if muted, false if unmuted.",
                    .type = EventItemType::Boolean,
                }
            },
        };
    }

    static EventSchema event_schema_display_backlight_brightness_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::DisplayBacklightBrightnessChanged),
            .description = "Emitted when the target display backlight brightness changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventDisplayBacklightBrightnessChangedParam::Brightness),
                    .description = "Current target backlight brightness percentage [0, 100].",
                    .type = EventItemType::Number,
                }
            },
        };
    }

    static EventSchema event_schema_display_backlight_on_off_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::DisplayBacklightOnOffChanged),
            .description = "Emitted when the target display backlight on/off state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventDisplayBacklightOnOffChangedParam::IsOn),
                    .description = "Whether display backlight is currently on. True if on, false if off.",
                    .type = EventItemType::Boolean,
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
                function_schema_set_display_backlight_brightness(),
                function_schema_get_display_backlight_brightness(),
                function_schema_set_display_backlight_on_off(),
                function_schema_get_display_backlight_on_off(),
                function_schema_set_audio_player_volume(),
                function_schema_get_audio_player_volume(),
                function_schema_set_audio_player_mute(),
                function_schema_get_audio_player_mute(),
                function_schema_get_storage_file_systems(),
                function_schema_get_power_battery_info(),
                function_schema_get_power_battery_state(),
                function_schema_get_power_battery_charge_config(),
                function_schema_set_power_battery_charge_config(),
                function_schema_set_power_battery_charging_enabled(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_display_backlight_brightness_changed(),
                event_schema_display_backlight_on_off_changed(),
                event_schema_audio_player_volume_changed(),
                event_schema_audio_player_mute_changed(),
                event_schema_power_battery_state_changed(),
                event_schema_power_battery_charge_config_changed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

BROOKESIA_DESCRIBE_ENUM(
    Device::FunctionId, GetCapabilities, GetBoardInfo, SetDisplayBacklightBrightness, GetDisplayBacklightBrightness,
    SetDisplayBacklightOnOff, GetDisplayBacklightOnOff, SetAudioPlayerVolume, GetAudioPlayerVolume, SetAudioPlayerMute,
    GetAudioPlayerMute, GetStorageFileSystems, GetPowerBatteryInfo, GetPowerBatteryState, GetPowerBatteryChargeConfig,
    SetPowerBatteryChargeConfig, SetPowerBatteryChargingEnabled, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(
    Device::EventId, DisplayBacklightBrightnessChanged, DisplayBacklightOnOffChanged, AudioPlayerVolumeChanged,
    AudioPlayerMuteChanged, PowerBatteryStateChanged, PowerBatteryChargeConfigChanged, Max
);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetDisplayBacklightBrightnessParam, Brightness);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetDisplayBacklightOnOffParam, On);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetAudioPlayerVolumeParam, Volume);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetAudioPlayerMuteParam, Enable);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetPowerBatteryChargeConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Device::FunctionSetPowerBatteryChargingEnabledParam, Enabled);
BROOKESIA_DESCRIBE_ENUM(Device::EventAudioPlayerVolumeChangedParam, Volume);
BROOKESIA_DESCRIBE_ENUM(Device::EventAudioPlayerMuteChangedParam, IsMuted);
BROOKESIA_DESCRIBE_ENUM(Device::EventDisplayBacklightBrightnessChangedParam, Brightness);
BROOKESIA_DESCRIBE_ENUM(Device::EventDisplayBacklightOnOffChangedParam, IsOn);
BROOKESIA_DESCRIBE_ENUM(Device::EventPowerBatteryStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(Device::EventPowerBatteryChargeConfigChangedParam, Config);

} // namespace esp_brookesia::service::helper
