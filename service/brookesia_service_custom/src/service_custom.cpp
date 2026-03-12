/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <vector>
#include <string>
#include <memory>
#include "brookesia/service_custom/macro_configs.h"
#if !BROOKESIA_SERVICE_CUSTOM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_custom/service_custom.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/function/registry.hpp"

namespace esp_brookesia::service {

bool CustomService::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_CUSTOM_VER_MAJOR, BROOKESIA_SERVICE_CUSTOM_VER_MINOR,
        BROOKESIA_SERVICE_CUSTOM_VER_PATCH
    );

    /* Initialize custom service */

    return true;
}

void CustomService::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    /* Deinitialize custom service */
    BROOKESIA_LOGI("Deinitialized");
}

bool CustomService::register_function(FunctionSchema schema, FunctionHandler handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: schema(%1%), handler(%2%)", schema, handler);

    const std::string function_name = schema.name;
    if (function_name.empty() || function_name.size() > 64) {
        BROOKESIA_LOGE("Function name is invalid: %1%", function_name);
        return false;
    }

    if (handler == nullptr) {
        BROOKESIA_LOGE("Function handler is null: %1%", function_name);
        return false;
    }

    std::vector<FunctionSchema> schemas = {std::move(schema)};
    FunctionHandlerMap handlers = {
        {function_name, std::move(handler)},
    };

    bool success = register_functions(std::move(schemas), std::move(handlers));
    BROOKESIA_CHECK_FALSE_RETURN(success, false, "Failed to register function: %1%", function_name);
    return true;
}

bool CustomService::register_event(EventSchema event_schema)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_schema(%1%)", event_schema);

    const std::string event_name = event_schema.name;
    if (event_name.empty() || event_name.size() > 64) {
        BROOKESIA_LOGE("Event name is invalid: %1%", event_name);
        return false;
    }

    std::vector<EventSchema> schemas = {std::move(event_schema)};

    bool success = register_events(std::move(schemas));
    BROOKESIA_CHECK_FALSE_RETURN(success, false, "Failed to register event: %1%", event_name);
    return true;
}

bool CustomService::publish_event(const std::string &event_name, EventItemMap event_items)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(
        this->ServiceBase::publish_event(event_name, std::move(event_items), false), false,
        "Failed to publish event: %1%", event_name
    );
    return true;
}

std::vector<FunctionSchema> CustomService::get_function_schemas()
{
    auto function_registry = get_function_registry();
    if (!function_registry) {
        return {};
    }
    return function_registry->get_schemas();
}

std::vector<EventSchema> CustomService::get_event_schemas()
{
    auto event_registry = get_event_registry();
    if (!event_registry) {
        return {};
    }
    return event_registry->get_schemas();
}

bool CustomService::unregister_function(const std::string &function_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!function_name.empty(), false, "Function name is empty");
    return unregister_functions({function_name});
}

bool CustomService::unregister_event(const std::string &event_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!event_name.empty(), false, "Event name is empty");
    return unregister_events({event_name});
}

#if BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, CustomService, CustomService::get_instance().get_attributes().name, CustomService::get_instance(),
    BROOKESIA_SERVICE_CUSTOM_PLUGIN_SYMBOL
);
#endif // BROOKESIA_SERVICE_CUSTOM_ENABLE_AUTO_REGISTER

} // namespace esp_brookesia::service
