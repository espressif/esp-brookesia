/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "brookesia/service_wifi/hal.hpp"
#include "brookesia/service_wifi/state_machine.hpp"

namespace esp_brookesia::service::wifi {

class Wifi : public ServiceBase {
public:
    enum class DataType {
        LastAp,
        ConnectedAps,
        ScanParams,
        Max,
    };

    static Wifi &get_instance()
    {
        static Wifi instance;
        return instance;
    }

private:
    using Helper = helper::Wifi;

    Wifi()
        : ServiceBase({
        .name = Helper::get_name().data(),
#if BROOKESIA_SERVICE_WIFI_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_WIFI_WORKER_NAME "0",
                    .core_id = BROOKESIA_SERVICE_WIFI_WORKER_0_CORE_ID,
                    .priority = BROOKESIA_SERVICE_WIFI_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_WIFI_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_WIFI_WORKER_STACK_IN_EXT,
                },
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_WIFI_WORKER_NAME "1",
                    .core_id = BROOKESIA_SERVICE_WIFI_WORKER_1_CORE_ID,
                    .priority = BROOKESIA_SERVICE_WIFI_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_WIFI_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_WIFI_WORKER_STACK_IN_EXT,
                },
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_WIFI_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_WIFI_ENABLE_WORKER
    })
    {}
    ~Wifi() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_trigger_general_action(const std::string &action);
    std::expected<void, std::string> function_trigger_scan_start();
    std::expected<void, std::string> function_trigger_scan_stop();
    std::expected<void, std::string> function_set_scan_params(double ap_count, double interval_ms, double timeout_ms);
    std::expected<void, std::string> function_set_connect_ap(const std::string &ssid, const std::string &password);
    std::expected<std::string, std::string> function_get_connect_ap();
    std::expected<boost::json::array, std::string> function_get_connected_aps();
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
                Helper, Helper::FunctionId::TriggerGeneralAction, std::string,
                function_trigger_general_action(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerScanStart,
                function_trigger_scan_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerScanStop,
                function_trigger_scan_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_3(
                Helper, Helper::FunctionId::SetScanParams, double, double, double,
                function_set_scan_params(PARAM1, PARAM2, PARAM3)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetConnectAp, std::string, std::string,
                function_set_connect_ap(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetConnectAp,
                function_get_connect_ap()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetConnectedAps,
                function_get_connected_aps()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            ),
        };
    }

    std::string get_target_event_state(GeneralEvent event);

    template<DataType type>
    constexpr auto get_data()
    {
        if constexpr (type == DataType::LastAp) {
            return hal_->get_last_connected_ap_info();
        } else if constexpr (type == DataType::ConnectedAps) {
            std::vector<ConnectApInfo> ap_infos;
            hal_->get_connected_ap_infos(ap_infos);
            return ap_infos;
        } else if constexpr (type == DataType::ScanParams) {
            return hal_->get_scan_params();
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    constexpr auto set_data(const auto &data)
    {
        if constexpr (type == DataType::LastAp) {
            hal_->set_last_connected_ap_info(data);
        } else if constexpr (type == DataType::ConnectedAps) {
            hal_->clear_connected_ap_infos();
            for (const auto &ap_info : data) {
                hal_->add_connected_ap_info(ap_info);
            }
        } else if constexpr (type == DataType::ScanParams) {
            hal_->set_scan_params(data);
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    bool is_data_loaded_ = false;

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    std::shared_ptr<Hal> hal_;
    std::unique_ptr<StateMachine> state_machine_;
};

BROOKESIA_DESCRIBE_ENUM(Wifi::DataType, LastAp, ConnectedAps, ScanParams, Max);

} // namespace esp_brookesia::service::wifi
