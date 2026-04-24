/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <expected>
#include <string>
#include "boost/json.hpp"
#include "brookesia/service_helper/battery.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::service {

/**
 * @brief Service facade for battery status retrieval.
 */
class Battery : public ServiceBase {
public:
    /**
     * @brief Helper type used to expose battery schemas and conversion helpers.
     */
    using Helper = helper::Battery;
    /**
     * @brief Battery status payload returned by helper APIs.
     */
    using Status = Helper::Status;

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton battery service.
     */
    static Battery &get_instance()
    {
        static Battery instance;
        return instance;
    }

private:
    Battery()
        : ServiceBase({
        .name = Helper::get_name().data(),
    })
    {}
    ~Battery() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<boost::json::object, std::string> function_get_status();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }
    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetStatus,
                function_get_status()
            ),
        };
    }
};

} // namespace esp_brookesia::service
