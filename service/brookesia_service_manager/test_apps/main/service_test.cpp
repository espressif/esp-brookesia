/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <cmath>
#include <algorithm>
#include "brookesia/lib_utils.hpp"
#include "service_test.hpp"

namespace esp_brookesia::service {

bool ServiceTest::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Initialized");

    return true;
}

void ServiceTest::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Deinitialized");
}

bool ServiceTest::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Started");

    return true;
}

void ServiceTest::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Stopped");
}

std::expected<double, std::string> ServiceTest::function_add(double a, double b)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: a(%1%), b(%2%)", a, b);

    return a + b;
}

std::expected<double, std::string> ServiceTest::function_divide(double a, double b)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: a(%1%), b(%2%)", a, b);

    if (b == 0) {
        return std::unexpected("Division by zero");
    }

    return a / b;
}

std::expected<boost::json::object, std::string> ServiceTest::function_test_all_types(
    const FunctionParameterMap &args
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: args.size(%1%)", args.size());

    // Get parameters of all types
    bool boolean_param = std::get<bool>(args.at(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[0].name));
    double number_param = std::get<double>(args.at(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[1].name));
    std::string string_param = std::get<std::string>(args.at(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[2].name));

    // Check if optional parameters are present
    bool has_object = args.find(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[3].name) != args.end();
    bool has_array = args.find(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[4].name) != args.end();

    // Build return object
    boost::json::object result_obj;
    result_obj["received_types"] = boost::json::object();

    auto &types = result_obj["received_types"].as_object();
    types["boolean"] = boolean_param;
    types["number"] = number_param;
    types["string"] = string_param;

    if (has_object) {
        boost::json::object obj_param = std::get<boost::json::object>(args.at(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[3].name));
        types["object"] = obj_param;
    }

    if (has_array) {
        boost::json::array array_param = std::get<boost::json::array>(args.at(FUNCTION_SCHEMAS[FunctionIndexTestAllTypes].parameters[4].name));
        types["array"] = array_param;
    }

    result_obj["message"] = "Successfully processed all parameter types!";
    result_obj["total_params"] = static_cast<int>(args.size());

    return result_obj;
}

std::expected<void, std::string> ServiceTest::function_suspend()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI("Suspending for 100ms");

    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));

    return {};
}

bool ServiceTest::trigger_event()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    event_value_ = random();

    // Publish event using automatic EventItemMap assembly
    BROOKESIA_CHECK_FALSE_RETURN(
        publish_event("value_change", std::vector<EventItem>({event_value_})),
        false, "Failed to publish event"
    );

    return true;
}

BROOKESIA_PLUGIN_REGISTER(ServiceBase, ServiceTest, ServiceTest::SERVICE_NAME);

} // namespace esp_brookesia::service
