/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_wifi/hal/hal.hpp"
#include "brookesia/service_wifi/state_machine.hpp"
#include "brookesia/service_wifi/macro_configs.h"

namespace esp_brookesia::service::wifi {

/**
 * @brief Wi-Fi service that coordinates HAL operations, persistence, and the Wi-Fi state machine.
 */
class Wifi : public ServiceBase {
    friend class StateMachine;
    friend class Hal;
    friend class SoftAp;

public:
    /**
     * @brief Persisted data items managed by the Wi-Fi service.
     */
    enum class DataType : uint8_t {
        LastAp,
        ConnectedAps,
        ScanParams,
        SoftApParams,
        Max,
    };

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton Wi-Fi service.
     */
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
    std::expected<std::string, std::string> function_get_general_state();
    std::expected<void, std::string> function_set_connect_ap(const std::string &ssid, const std::string &password);
    std::expected<boost::json::object, std::string> function_get_connect_ap();
    std::expected<boost::json::array, std::string> function_get_connected_aps();
    std::expected<void, std::string> function_set_scan_params(const boost::json::object &params);
    std::expected<void, std::string> function_trigger_scan_start();
    std::expected<void, std::string> function_trigger_scan_stop();
    std::expected<void, std::string> function_set_softap_params(const boost::json::object &params);
    std::expected<boost::json::object, std::string> function_get_softap_params();
    std::expected<void, std::string> function_trigger_softap_start();
    std::expected<void, std::string> function_trigger_softap_stop();
    std::expected<void, std::string> function_trigger_softap_provision_start();
    std::expected<void, std::string> function_trigger_softap_provision_stop();
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
                Helper, Helper::FunctionId::GetGeneralState,
                function_get_general_state()
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
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetScanParams, boost::json::object,
                function_set_scan_params(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetSoftApParams, boost::json::object,
                function_set_softap_params(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetSoftApParams,
                function_get_softap_params()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerScanStart,
                function_trigger_scan_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerScanStop,
                function_trigger_scan_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApStart,
                function_trigger_softap_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApStop,
                function_trigger_softap_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApProvisionStart,
                function_trigger_softap_provision_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::TriggerSoftApProvisionStop,
                function_trigger_softap_provision_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetData,
                function_reset_data()
            ),
        };
    }

    lib_utils::TaskScheduler::Group get_state_task_group() const
    {
        return get_call_task_group();
    }

    bool publish_general_event(GeneralEvent event, bool is_unexpected);
    bool publish_general_action(GeneralAction action);
    bool publish_scan_state_changed(bool is_running);
    bool publish_scan_ap_infos(std::span<const ScanApInfo> &ap_infos);
    bool publish_softap_event(SoftApEvent event);

    bool on_hal_general_event(GeneralEvent event, bool is_unexpected);
    bool on_hal_error_state();

    void try_load_data();
    void try_save_data(DataType type);
    void try_erase_data();

    template<DataType type>
    constexpr auto &get_data()
    {
        if constexpr (type == DataType::LastAp) {
            return hal_->get_last_connected_ap_info();
        } else if constexpr (type == DataType::ConnectedAps) {
            return hal_->get_connected_ap_infos();
        } else if constexpr (type == DataType::ScanParams) {
            return hal_->get_scan_params();
        } else if constexpr (type == DataType::SoftApParams) {
            return hal_->get_softap_params();
        } else {
            static_assert(false, "Invalid data type");
        }
    }
    template<DataType type>
    bool set_data(const auto &data, bool is_force = false, bool is_skip_save = false)
    {
        // If the data is the same, skip
        if (!is_force && (get_data<type>() == data)) {
            return true;
        }
        // Otherwise, set the data and save to NVS
        bool result = false;
        if constexpr (type == DataType::LastAp) {
            result = hal_->set_last_connected_ap_info(data);
        } else if constexpr (type == DataType::ConnectedAps) {
            result = hal_->set_connected_ap_infos(data);
        } else if constexpr (type == DataType::ScanParams) {
            result = hal_->set_scan_params(data);
        } else if constexpr (type == DataType::SoftApParams) {
            result = hal_->set_softap_params(data);
        } else {
            static_assert(false, "Invalid data type");
        }
        if (result && !is_skip_save) {
            try_save_data(type);
        }

        return result;
    }
    void reset_data();

    bool is_general_action_queue_task_running();
    GeneralAction pop_general_action_pending_queue_front();
    GeneralAction pop_general_action_ready_queue_front();
    bool start_general_action_queue_task();
    void stop_general_action_queue_task();
    void make_general_action_ready_queue(GeneralAction action);
    bool trigger_general_action(const GeneralAction &action);

    bool is_data_loaded_ = false;

    std::shared_ptr<Hal> hal_;
    std::unique_ptr<StateMachine> state_machine_;

    std::queue<GeneralAction> general_action_pending_queue_;
    std::queue<GeneralAction> general_action_ready_queue_;
    lib_utils::TaskScheduler::TaskId general_action_queue_processing_task_ = 0;
};

BROOKESIA_DESCRIBE_ENUM(Wifi::DataType, LastAp, ConnectedAps, ScanParams, SoftApParams, Max);

} // namespace esp_brookesia::service::wifi
