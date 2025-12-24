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
#include "brookesia/service_helper/nvs.hpp"

namespace esp_brookesia::service {

class NVS : public ServiceBase {
public:
    using Helper = helper::NVS;
    using KeyValueMap = Helper::KeyValueMap;

    static NVS &get_instance()
    {
        static NVS instance;
        return instance;
    }

private:
    NVS()
        : ServiceBase({
        .name = Helper::get_name().data(),
        // NVS operations must be performed in a thread with an SRAM stack.
        // If the Service Manager's task scheduler uses an external stack,
        // we need to use a custom task scheduler to ensure NVS operations run in a thread with an SRAM stack.
#if BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_NVS_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_NVS_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_NVS_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_NVS_WORKER_STACK_SIZE,
                    .stack_in_ext = false,
                },
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_NVS_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_MANAGER_WORKER_STACK_IN_EXT
    })
    {}
    ~NVS() = default;

    bool on_init() override;
    void on_deinit() override;

    std::expected<boost::json::array, std::string> function_list(const std::string &nspace);
    std::expected<void, std::string> function_set(const std::string &nspace, boost::json::object &&key_value_map);
    std::expected<boost::json::object, std::string> function_get(const std::string &nspace, boost::json::array &&keys);
    std::expected<void, std::string> function_erase(const std::string &nspace, boost::json::array &&keys);

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::List, std::string,
                function_list(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::Set, std::string, boost::json::object,
                function_set(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::Get, std::string, boost::json::array,
                function_get(PARAM1, std::move(PARAM2))
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::Erase, std::string, boost::json::array,
                function_erase(PARAM1, std::move(PARAM2))
            )
        };
    }
};

} // namespace esp_brookesia::service
