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
#include <vector>
#include "boost/json.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/system/board_info.hpp"
#include "brookesia/hal_interface/interfaces/power/battery.hpp"
#include "brookesia/hal_interface/interfaces/video/camera.hpp"
#include "brookesia/service_device/macro_configs.h"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

/**
 * @brief Service facade for basic device control and persisted device state.
 */
class Device: public ServiceBase {
public:
    static Device &get_instance()
    {
        static Device instance;
        return instance;
    }

private:
    using Helper = helper::Device;
    using BoardInfo = Helper::BoardInfo;
    using CameraDeviceInfos = Helper::CameraDeviceInfos;
    using Capabilities = Helper::Capabilities;
    using NetworkConnectivityInfo = Helper::NetworkConnectivityInfo;
    using NetworkConnectivityInfos = Helper::NetworkConnectivityInfos;
    using PowerBatteryChargeConfig = Helper::PowerBatteryChargeConfig;
    using PowerBatteryInfo = Helper::PowerBatteryInfo;
    using PowerBatteryState = Helper::PowerBatteryState;

    struct PowerState {
        lib_utils::TaskScheduler::TaskId poll_task_id = 0;
        bool polling_requested = false;
        std::optional<PowerBatteryState> cached_state;
        std::optional<PowerBatteryChargeConfig> cached_charge_config;
        hal::InterfaceHandle<hal::power::BatteryIface> battery_iface;
    };

    Device()
        : ServiceBase({
        .name = Helper::get_name().data(),
#if BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_DEVICE_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_DEVICE_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_DEVICE_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_DEVICE_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_DEVICE_WORKER_STACK_IN_EXT,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_DEVICE_WORKER_POLL_INTERVAL_MS,
        },
#endif // BROOKESIA_SERVICE_DEVICE_ENABLE_WORKER
    })
    {}
    ~Device() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;
    void on_event_subscribed(const std::string &event_name) override;

    std::expected<boost::json::array, std::string> function_get_capabilities();
    std::expected<boost::json::object, std::string> function_get_board_info();
    std::expected<boost::json::array, std::string> function_get_camera_device_infos();
    std::expected<boost::json::array, std::string> function_get_network_connectivity_info();
    std::expected<boost::json::object, std::string> function_get_power_battery_info();
    std::expected<boost::json::object, std::string> function_get_power_battery_state();
    std::expected<boost::json::object, std::string> function_get_power_battery_charge_config();
    std::expected<void, std::string> function_set_power_battery_charge_config(const boost::json::object &config);
    std::expected<void, std::string> function_set_power_battery_charging_enabled(bool enabled);

    std::vector<FunctionSchema> get_function_schemas() override;
    std::vector<EventSchema> get_event_schemas() override;
    ServiceBase::FunctionHandlerMap get_function_handlers() override;

    Capabilities get_capabilities() const;

    bool ensure_power_battery_iface();
    void reset_interfaces();
    void request_power_battery_polling();
    bool start_power_battery_polling();
    void stop_power_battery_polling();
    bool poll_power_battery_once();
    bool publish_power_battery_state_changed(const PowerBatteryState &state);
    bool publish_power_battery_charge_config_changed(const PowerBatteryChargeConfig &config);
    static bool is_power_battery_state_equal(const PowerBatteryState &lhs, const PowerBatteryState &rhs);
    static bool is_power_battery_charge_config_equal(
        const PowerBatteryChargeConfig &lhs, const PowerBatteryChargeConfig &rhs
    );

    PowerState power_;
};

} // namespace esp_brookesia::service
