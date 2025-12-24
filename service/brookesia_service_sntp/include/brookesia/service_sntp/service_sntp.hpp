/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include "brookesia/service_sntp/macro_configs.h"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_helper/sntp.hpp"

namespace esp_brookesia::service {

class SNTP : public ServiceBase {
public:
    static constexpr std::string_view DEFAULT_NTP_SERVER = BROOKESIA_SERVICE_SNTP_DEFAULT_NTP_SERVER;
    static constexpr std::string_view DEFAULT_TIMEZONE = BROOKESIA_SERVICE_SNTP_DEFAULT_TIMEZONE;

    enum class DataType {
        Timezone,
        Servers,
        Max,
    };

    static SNTP &get_instance()
    {
        static SNTP instance;
        return instance;
    }

private:
    using Helper = helper::SNTP;

    SNTP()
        : ServiceBase({
        .name = Helper::get_name().data(),
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
    std::expected<bool, std::string> function_is_time_synced();
    std::expected<void, std::string> function_reset_data();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
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
                Helper, Helper::FunctionId::IsTimeSynced,
                function_is_time_synced()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            )
        };
    }

    bool is_sntp_initialized() const
    {
        return is_sntp_initialized_;
    }

    bool is_sntp_running() const
    {
        return is_sntp_running_;
    }

    bool is_time_synced() const
    {
        return is_time_synced_;
    }

    bool sntp_start();
    void sntp_stop();
    void reset_data();

    bool do_sntp_init();
    void do_sntp_deinit();
    bool do_sntp_start();

    bool is_network_connected();

    void print_sntp_servers();
    void update_timezone();
    void update_local_time();

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
    lib_utils::TaskScheduler::TaskId wait_for_network_task_ = 0;
    lib_utils::TaskScheduler::TaskId sync_time_task_ = 0;

    bool is_sntp_initialized_ = false;
    bool is_sntp_running_ = false;
    bool is_time_synced_ = false;

    bool is_data_loaded_ = false;
    std::string data_timezone_ = DEFAULT_TIMEZONE.data();
    std::vector<std::string> data_servers_ = {DEFAULT_NTP_SERVER.data()};
};

BROOKESIA_DESCRIBE_ENUM(SNTP::DataType, Timezone, Servers, Max)

} // namespace esp_brookesia::service
