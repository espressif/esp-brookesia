/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include "boost/json.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/display/backlight.hpp"
#include "brookesia/hal_interface/interfaces/general/board_info.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/interfaces/storage/fs.hpp"
#include "brookesia/service_device/macro_configs.h"
#include "brookesia/service_helper/device.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

/**
 * @brief Service facade for basic device control and persisted device state.
 */
class Device: public ServiceBase {
public:
    enum class DataType : uint8_t {
        Volume,
        Mute,
        Brightness,
        Max,
    };

    static Device &get_instance()
    {
        static Device instance;
        return instance;
    }

private:
    using Helper = helper::Device;
    using BoardInfo = Helper::BoardInfo;
    using Capabilities = Helper::Capabilities;
    using PowerBatteryChargeConfig = Helper::PowerBatteryChargeConfig;
    using PowerBatteryInfo = Helper::PowerBatteryInfo;
    using PowerBatteryState = Helper::PowerBatteryState;
    using StorageFsInfo = Helper::StorageFsInfo;

    Device()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_DEVICE_WORKER_NAME,
                    .priority = BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS,
        },
    })
    {}
    ~Device() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<boost::json::object, std::string> function_get_capabilities();
    std::expected<boost::json::object, std::string> function_get_board_info();
    std::expected<void, std::string> function_set_display_backlight_brightness(double brightness);
    std::expected<double, std::string> function_get_display_backlight_brightness();
    std::expected<void, std::string> function_set_display_backlight_on_off(bool on);
    std::expected<bool, std::string> function_get_display_backlight_on_off();
    std::expected<void, std::string> function_set_audio_player_volume(double volume);
    std::expected<double, std::string> function_get_audio_player_volume();
    std::expected<void, std::string> function_set_audio_player_mute(bool enable);
    std::expected<bool, std::string> function_get_audio_player_mute();
    std::expected<boost::json::array, std::string> function_get_storage_file_systems();
    std::expected<boost::json::object, std::string> function_get_power_battery_info();
    std::expected<boost::json::object, std::string> function_get_power_battery_state();
    std::expected<boost::json::object, std::string> function_get_power_battery_charge_config();
    std::expected<void, std::string> function_set_power_battery_charge_config(const boost::json::object &config);
    std::expected<void, std::string> function_set_power_battery_charging_enabled(bool enabled);
    std::expected<void, std::string> function_reset_data();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    ServiceBase::FunctionHandlerMap get_function_handlers() override
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
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetDisplayBacklightBrightness, double,
                function_set_display_backlight_brightness(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetDisplayBacklightBrightness,
                function_get_display_backlight_brightness()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetDisplayBacklightOnOff, bool,
                function_set_display_backlight_on_off(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetDisplayBacklightOnOff,
                function_get_display_backlight_on_off()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetAudioPlayerVolume, double,
                function_set_audio_player_volume(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetAudioPlayerVolume,
                function_get_audio_player_volume()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetAudioPlayerMute, bool,
                function_set_audio_player_mute(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetAudioPlayerMute,
                function_get_audio_player_mute()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetStorageFileSystems,
                function_get_storage_file_systems()
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
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            ),
        };
    }

    Capabilities get_capabilities() const;

    void discover_interfaces();
    void reset_interfaces();
    void initialize_default_state();
    bool apply_audio_state_to_hal();
    bool apply_display_brightness_to_hal();
    bool start_power_battery_polling();
    void stop_power_battery_polling();
    bool poll_power_battery_once();
    bool publish_power_battery_state_changed(const PowerBatteryState &state);
    bool publish_power_battery_charge_config_changed(const PowerBatteryChargeConfig &config);
    static bool is_power_battery_state_equal(const PowerBatteryState &lhs, const PowerBatteryState &rhs);
    static bool is_power_battery_charge_config_equal(
        const PowerBatteryChargeConfig &lhs, const PowerBatteryChargeConfig &rhs
    );

    bool publish_volume_changed();
    bool publish_mute_changed();
    bool publish_brightness_changed();
    bool publish_backlight_on_off_changed();

    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();
    void reset_data();

    bool is_data_loaded_ = false;
    uint8_t desired_volume_ = 0;
    bool desired_mute_ = false;
    uint8_t current_brightness_ = 0;
    bool desired_backlight_on_ = false;
    lib_utils::TaskScheduler::TaskId power_battery_poll_task_id_ = 0;
    std::optional<PowerBatteryState> cached_power_battery_state_;
    std::optional<PowerBatteryChargeConfig> cached_power_battery_charge_config_;

    std::shared_ptr<hal::BoardInfoIface> board_info_iface_;
    std::shared_ptr<hal::DisplayBacklightIface> display_backlight_iface_;
    std::shared_ptr<hal::AudioCodecPlayerIface> audio_player_iface_;
    std::shared_ptr<hal::StorageFsIface> storage_fs_iface_;
    std::shared_ptr<hal::PowerBatteryIface> power_battery_iface_;
};

BROOKESIA_DESCRIBE_ENUM(Device::DataType, Volume, Mute, Brightness, Max);

} // namespace esp_brookesia::service
