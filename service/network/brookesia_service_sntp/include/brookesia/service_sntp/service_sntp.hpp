/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <vector>
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/network/connectivity.hpp"
#include "brookesia/hal_interface/interfaces/network/sntp_client.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_sntp/macro_configs.h"

namespace esp_brookesia::service {

/**
 * @brief Service facade for SNTP configuration and time synchronization control.
 */
class SNTP : public ServiceBase {
public:
    /**
     * @brief Built-in default NTP server used when no persisted configuration exists.
     */
    static constexpr std::string_view DEFAULT_NTP_SERVER = BROOKESIA_SERVICE_SNTP_DEFAULT_NTP_SERVER;
    /**
     * @brief Built-in default timezone used when no persisted configuration exists.
     */
    static constexpr std::string_view DEFAULT_TIMEZONE = BROOKESIA_SERVICE_SNTP_DEFAULT_TIMEZONE;

    /**
     * @brief Persisted data items managed by the SNTP service.
     */
    enum class DataType {
        Timezone,
        Servers,
        Max,
    };

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton SNTP service.
     */
    static SNTP &get_instance()
    {
        static SNTP instance;
        return instance;
    }

private:
    static std::string get_component_version();

    using Helper = helper::SNTP;
    using StorageHelper = helper::Storage;
    using State = Helper::State;

    SNTP()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .description = "Synchronize system time through SNTP.",
        .version = get_component_version(),
        .dependencies = {
            StorageHelper::get_name().data(),
        },
    })
    {}
    ~SNTP() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_set_servers(const boost::json::array &servers);
    std::expected<void, std::string> function_set_timezone(const std::string &timezone);
    std::expected<void, std::string> function_start();
    std::expected<void, std::string> function_stop();
    std::expected<boost::json::array, std::string> function_get_servers();
    std::expected<std::string, std::string> function_get_timezone();
    std::expected<std::string, std::string> function_get_state();
    std::expected<bool, std::string> function_is_time_synced();
    std::expected<void, std::string> function_load_data();
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
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetServers, boost::json::array,
                function_set_servers(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetTimezone, std::string,
                function_set_timezone(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Start,
                function_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Stop,
                function_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetServers,
                function_get_servers()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetTimezone,
                function_get_timezone()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetState,
                function_get_state()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::IsTimeSynced,
                function_is_time_synced()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::LoadData,
                function_load_data()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            )
        };
    }

    bool ensure_sntp_iface();
    bool ensure_connectivity_iface();
    bool start_state_machine();
    bool start_checking_network();
    bool start_syncing();
    void stop_state_machine(State next_state);
    void stop_tasks();
    void stop_backend();
    void reset_data();

    void set_state(State state, bool publish = true);
    bool publish_state_changed();
    bool publish_timezone_changed();

    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::Timezone) {
            return data_timezone_;
        } else if constexpr (type == DataType::Servers) {
            return data_servers_;
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    void set_data(auto &&data)
    {
        if constexpr (type == DataType::Timezone) {
            data_timezone_ = std::move(data);
        } else if constexpr (type == DataType::Servers) {
            data_servers_ = std::move(data);
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    std::shared_ptr<lib_utils::TaskScheduler> scheduler_;
    lib_utils::TaskScheduler::TaskId wait_for_network_task_id_ = 0;
    lib_utils::TaskScheduler::TaskId sync_time_task_id_ = 0;
    lib_utils::TaskScheduler::TaskId retry_task_id_ = 0;

    hal::InterfaceHandle<hal::network::SntpClientIface> sntp_iface_;
    hal::InterfaceHandle<hal::network::ConnectivityIface> connectivity_iface_;
    State state_ = State::Stopped;
    bool is_time_synced_ = false;

    bool is_data_loaded_ = false;
    std::string data_timezone_ = DEFAULT_TIMEZONE.data();
    std::vector<std::string> data_servers_ = {DEFAULT_NTP_SERVER.data()};
};

BROOKESIA_DESCRIBE_ENUM(SNTP::DataType, Timezone, Servers, Max)

} // namespace esp_brookesia::service
