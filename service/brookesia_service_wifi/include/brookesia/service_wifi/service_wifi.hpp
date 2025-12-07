/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/service_wifi/hal.hpp"
#include "brookesia/service_wifi/state_machine.hpp"

namespace esp_brookesia::service::wifi {

class Wifi : public ServiceBase {
public:
    using Helper = helper::Wifi;

    static Wifi &get_instance()
    {
        static Wifi instance;
        return instance;
    }

private:
    inline static const FunctionSchema *FUNCTION_DEFINITIONS = Helper::get_function_definitions();
    inline static const EventSchema *EVENT_DEFINITIONS = Helper::get_event_definitions();

    Wifi()
        : ServiceBase({
        .name = Helper::SERVICE_NAME,
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER1_NAME,
                    .core_id = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER1_CORE_ID,
                    .priority = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER1_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER1_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER1_STACK_IN_EXT,
                },
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER2_NAME,
                    .core_id = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER2_CORE_ID,
                    .priority = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER2_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER2_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER2_STACK_IN_EXT,
                }
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_WIFI_TASK_SCHEDULER_WORKER_POLL_INTERVAL_MS,
        }
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

    std::vector<FunctionSchema> get_function_definitions() override
    {
        return std::vector<FunctionSchema>(
                   FUNCTION_DEFINITIONS, FUNCTION_DEFINITIONS + Helper::FunctionIndexMax
               );
    }

    std::vector<EventSchema> get_event_definitions() override
    {
        return std::vector<EventSchema>(
                   EVENT_DEFINITIONS, EVENT_DEFINITIONS + Helper::EventIndexMax
               );
    }
    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_FUNC_HANDLER_1(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexTriggerGeneralAction].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexTriggerGeneralAction].parameters[0].name, std::string,
                function_trigger_general_action(PARAM)
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_0(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexTriggerScanStart].name,
                function_trigger_scan_start()
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_0(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexTriggerScanStop].name,
                function_trigger_scan_stop()
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_3(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetScanParams].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetScanParams].parameters[0].name, double,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetScanParams].parameters[1].name, double,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetScanParams].parameters[2].name, double,
                function_set_scan_params(PARAM1, PARAM2, PARAM3)
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetConnectAp].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetConnectAp].parameters[0].name, std::string,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSetConnectAp].parameters[1].name, std::string,
                function_set_connect_ap(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_0(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexGetConnectAp].name,
                function_get_connect_ap()
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_0(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexGetConnectedAps].name,
                function_get_connected_aps()
            ),
        };
    }

    std::string get_target_event_state(GeneralEvent event);

    bool is_nvs_valid();
    void try_load_from_nvs();
    void try_save_last_connected_ap_info_to_nvs();
    void try_save_connected_ap_info_list_to_nvs();
    void try_save_scan_params_to_nvs();

    std::vector<FunctionSchema> function_definitions_;
    std::vector<EventSchema> event_definitions_;
    FunctionHandlerMap function_handlers_;

    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    std::shared_ptr<Hal> hal_;
    std::unique_ptr<StateMachine> state_machine_;
};

} // namespace esp_brookesia::service::wifi
