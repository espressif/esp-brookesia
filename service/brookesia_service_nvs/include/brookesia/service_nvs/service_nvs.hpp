/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_helper/nvs.hpp"

namespace esp_brookesia::service {

class NVS : public ServiceBase {
public:
    using Helper = helper::NVS;

    static NVS &get_instance()
    {
        static NVS instance;
        return instance;
    }

private:
    inline static const FunctionSchema *FUNCTION_DEFINITIONS = Helper::get_function_definitions();

    NVS()
        : ServiceBase({
        .name = Helper::SERVICE_NAME,
#if BROOKESIA_SERVICE_NVS_ENABLE_TASK_SCHEDULER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_NVS_TASK_SCHEDULER_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_NVS_TASK_SCHEDULER_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_NVS_TASK_SCHEDULER_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_NVS_TASK_SCHEDULER_WORKER_STACK_SIZE,
                    .stack_in_ext = false,
                },
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_NVS_TASK_SCHEDULER_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_NVS_ENABLE_TASK_SCHEDULER
    })
    {}
    ~NVS() = default;

    bool on_init() override;
    void on_deinit() override;

    std::expected<boost::json::array, std::string> function_list(const std::string &nspace);
    std::expected<void, std::string> function_set(const std::string &nspace, boost::json::array &&key_value_pairs);
    std::expected<boost::json::object, std::string> function_get(
        const std::string &nspace, boost::json::array &&keys
    );
    std::expected<void, std::string> function_erase(const std::string &nspace, boost::json::array &&keys);

    std::vector<FunctionSchema> get_function_definitions() override
    {
        return std::vector<FunctionSchema>(
                   FUNCTION_DEFINITIONS, FUNCTION_DEFINITIONS + Helper::FunctionIndexMax
               );
    }
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_FUNC_HANDLER_1(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexList].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexList].parameters[0].name, std::string,
                function_list(PARAM)
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSet].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSet].parameters[0].name, std::string,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexSet].parameters[1].name, boost::json::array,
                function_set(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexGet].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexGet].parameters[0].name, std::string,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexGet].parameters[1].name, boost::json::array,
                function_get(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_FUNC_HANDLER_2(
                FUNCTION_DEFINITIONS[Helper::FunctionIndexErase].name,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexErase].parameters[0].name, std::string,
                FUNCTION_DEFINITIONS[Helper::FunctionIndexErase].parameters[1].name, boost::json::array,
                function_erase(PARAM1, std::move(PARAM2))
            )
        };
    }
};

} // namespace esp_brookesia::service
