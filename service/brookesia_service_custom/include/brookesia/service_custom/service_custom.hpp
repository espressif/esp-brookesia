/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include "brookesia/service_custom/macro_configs.h"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/function/definition.hpp"


namespace esp_brookesia::service {

static const char *CustomServiceName = "CustomService";

class CustomService : public ServiceBase {
public:

    static CustomService &get_instance()
    {
        static CustomService instance;
        return instance;
    }

    /**
     * @brief Register a function for custom service
     *
     * @param[in] schema Function schema
     * @param[in] handler Function handler
     * @return true if registered successfully, false otherwise
     */
    bool register_function(FunctionSchema schema, FunctionHandler handler);

    /**
     * @brief Unregister a function
     *
     * @param[in] function_name Function name
     * @return true if unregistered successfully, false otherwise
     */
    bool unregister_function(const std::string &function_name);

    /**
     * @brief Get currently registered function schemas for custom service
     *
     * @return std::vector<FunctionSchema> Function schemas
     */
    std::vector<FunctionSchema> get_function_schemas() override;

    /**
     * @brief Register an event for custom service
     *
     * @param[in] schema Event schema
     * @return true if registered successfully, false otherwise
     */
    bool register_event(EventSchema event_schema);

    /**
     * @brief Unregister an event
     *
     * @param[in] event_name Event name
     * @return true if unregistered successfully, false otherwise
     */
    bool unregister_event(const std::string &event_name);

    /**
     * @brief Publish an event for custom service
     *
     * @param[in] event_name Event name
     * @param[in] event_items Event items
     * @return true if published successfully, false otherwise
     */
    bool publish_event(const std::string &event_name, EventItemMap event_items);

    /**
     * @brief Get currently registered event schemas for custom service
     *
     * @return std::vector<EventSchema> Event schemas
     */
    std::vector<EventSchema> get_event_schemas() override;

private:
    CustomService()
        : ServiceBase({
        .name = CustomServiceName,
#if BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER
        .task_scheduler_config = lib_utils::TaskScheduler::StartConfig{
            .worker_configs = {
                lib_utils::ThreadConfig{
                    .name = BROOKESIA_SERVICE_CUSTOM_WORKER_NAME,
                    .core_id = BROOKESIA_SERVICE_CUSTOM_WORKER_CORE_ID,
                    .priority = BROOKESIA_SERVICE_CUSTOM_WORKER_PRIORITY,
                    .stack_size = BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_SIZE,
                    .stack_in_ext = BROOKESIA_SERVICE_CUSTOM_WORKER_STACK_IN_EXT,
                },
            },
            .worker_poll_interval_ms = BROOKESIA_SERVICE_CUSTOM_WORKER_POLL_INTERVAL_MS,
        }
#endif // BROOKESIA_SERVICE_CUSTOM_ENABLE_WORKER
    })
    {}
    ~CustomService() = default;

    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia::service
