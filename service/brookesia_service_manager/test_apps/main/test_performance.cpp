/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <future>
#include <mutex>
#include <thread>
#include <chrono>
#include <variant>
#include "unity.h"
#include "boost/format.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "service_test.hpp"
#include "service_test_with_scheduler.hpp"
#include "common_def.hpp"

using namespace esp_brookesia::lib_utils;
using namespace esp_brookesia::service;

struct TestItem {
    std::string method;
    boost::json::object params;
    std::function<bool(const FunctionValue &)> validator;
};
using TestMap = std::map<std::string /** service name */, std::vector<TestItem>>;

static boost::json::object service_test_build_add_params();
static bool service_test_validate_add_result(double result);
static boost::json::object service_test_build_test_all_types_params();
static bool service_test_validate_test_all_types_result(const boost::json::object &result_obj);

// For multiple tests
constexpr size_t TEST_REPEATITIVE_NUM = 100;
// For call function
constexpr size_t TEST_CALL_FUNCTION_TIMEOUT_MS = 100;
// For concurrent call function
constexpr const char *TEST_CONCURRENT_SERVICE_NAME = ServiceTest::SERVICE_NAME;
constexpr size_t TEST_CONCURRENT_NUM = 10;
constexpr size_t TEST_CONCURRENT_THREAD_STACK_SIZE = 10 * 1024;
constexpr size_t TEST_CONCURRENT_TOTAL_TIMEOUT_MS = TEST_CONCURRENT_NUM * TEST_CALL_FUNCTION_TIMEOUT_MS + 1000; // Extra time for concurrent overhead
constexpr size_t TEST_CONCURRENT_SUCCESS_RATE = 90;
static const TestItem TEST_CONCURRENT_TEST_ITEM = {
    ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexAdd].name, service_test_build_add_params(),
    [](const FunctionValue & result) -> bool {
        auto double_result = std::get_if<double>(&result);
        BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
        BROOKESIA_CHECK_FALSE_RETURN(
            service_test_validate_add_result(*double_result), false, "Validation failed"
        );
        return true;
    }
};
static const TestItem TEST_CONCURRENT_TEST_ITEM_WITH_SCHEDULER = {
    ServiceTestWithScheduler::FUNCTION_SCHEMAS[ServiceTestWithScheduler::FunctionIndexAdd].name, service_test_build_add_params(),
    [](const FunctionValue & result) -> bool {
        auto double_result = std::get_if<double>(&result);
        BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
        BROOKESIA_CHECK_FALSE_RETURN(
            service_test_validate_add_result(*double_result), false, "Validation failed"
        );
        return true;
    }
};

struct CallConfig {
    size_t timeout_ms = TEST_CALL_FUNCTION_TIMEOUT_MS;
};
struct ConcurrentCallConfig {
    size_t num = TEST_CONCURRENT_NUM;
    size_t stack_size = TEST_CONCURRENT_THREAD_STACK_SIZE;
    size_t total_timeout_ms = TEST_CONCURRENT_TOTAL_TIMEOUT_MS;
    size_t success_rate = TEST_CONCURRENT_SUCCESS_RATE;
};

static const TestMap TEST_SERVICE_FUNCTION_MAP = {
    {
        ServiceTest::SERVICE_NAME, {
            {
                ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexAdd].name, service_test_build_add_params(),
                [](const FunctionValue & result) -> bool {
                    auto double_result = std::get_if<double>(&result);
                    BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
                    BROOKESIA_CHECK_FALSE_RETURN(
                        service_test_validate_add_result(*double_result), false, "Validation failed"
                    );
                    return true;
                }
            },
            {
                ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexTestAllTypes].name, service_test_build_test_all_types_params(),
                [](const FunctionValue & result) -> bool {
                    auto result_obj = std::get_if<boost::json::object>(&result);
                    BROOKESIA_CHECK_NULL_RETURN(result_obj, false, "Result is not a object");
                    BROOKESIA_CHECK_FALSE_RETURN(
                        service_test_validate_test_all_types_result(*result_obj), false, "Validation failed"
                    );
                    return true;
                }
            },
        }
    },
    {
        ServiceTestWithScheduler::SERVICE_NAME, {
            {
                ServiceTestWithScheduler::FUNCTION_SCHEMAS[ServiceTestWithScheduler::FunctionIndexAdd].name, service_test_build_add_params(),
                [](const FunctionValue & result) -> bool {
                    auto double_result = std::get_if<double>(&result);
                    BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
                    BROOKESIA_CHECK_FALSE_RETURN(
                        service_test_validate_add_result(*double_result), false, "Validation failed"
                    );
                    return true;
                }
            },
            {
                ServiceTestWithScheduler::FUNCTION_SCHEMAS[ServiceTestWithScheduler::FunctionIndexTestAllTypes].name, service_test_build_test_all_types_params(),
                [](const FunctionValue & result) -> bool {
                    auto result_obj = std::get_if<boost::json::object>(&result);
                    BROOKESIA_CHECK_NULL_RETURN(result_obj, false, "Result is not a object");
                    BROOKESIA_CHECK_FALSE_RETURN(
                        service_test_validate_test_all_types_result(*result_obj), false, "Validation failed"
                    );
                    return true;
                }
            },
        }
    }
};

static auto &service_manager = ServiceManager::get_instance();
static auto &time_profiler = TimeProfiler::get_instance();

static bool do_validate_call_function(
    const TestMap &test_map, size_t test_repeatitive_num, const CallConfig &call_config
);
static bool do_concurrent_call_function(
    const std::string &service_name, const TestItem &test_item,
    const ConcurrentCallConfig &concurrent_config,
    size_t timeout_ms
);

TEST_CASE("Test Performance: call function", "[brookesia][service][call_function_sync]")
{
    CallConfig call_config{};
    bool result = false;
    {
        BROOKESIA_TIME_PROFILER_SCOPE("test_call_function");
        result = do_validate_call_function(TEST_SERVICE_FUNCTION_MAP, TEST_REPEATITIVE_NUM, call_config);
    }
    TEST_ASSERT_TRUE_MESSAGE(result, "Call function test failed");
}

TEST_CASE("Test Performance: concurrent call function", "[brookesia][service][call_function_sync][concurrent]")
{
    ConcurrentCallConfig concurrent_config{};
    bool result = false;
    {
        BROOKESIA_TIME_PROFILER_SCOPE("test_concurrent_call_function");
        result = do_concurrent_call_function(
                     TEST_CONCURRENT_SERVICE_NAME, TEST_CONCURRENT_TEST_ITEM, concurrent_config, TEST_CALL_FUNCTION_TIMEOUT_MS
                 );
    }
    TEST_ASSERT_TRUE_MESSAGE(result, "Concurrent call function test failed");
}

TEST_CASE("Test Performance with scheduler: concurrent call function", "[brookesia][service][call_function_sync][concurrent][with_scheduler]")
{
    ConcurrentCallConfig concurrent_config{};
    bool result = false;
    {
        BROOKESIA_TIME_PROFILER_SCOPE("test_concurrent_call_function_with_scheduler");
        result = do_concurrent_call_function(
                     ServiceTestWithScheduler::SERVICE_NAME, TEST_CONCURRENT_TEST_ITEM_WITH_SCHEDULER, concurrent_config, TEST_CALL_FUNCTION_TIMEOUT_MS
                 );
    }
    TEST_ASSERT_TRUE_MESSAGE(result, "Concurrent call function with scheduler test failed");
}

static boost::json::object service_test_build_add_params()
{
    boost::json::object params;
    params["a"] = 15.5;
    params["b"] = 4.5;
    return params;
}

static bool service_test_validate_add_result(double result)
{
    BROOKESIA_CHECK_FALSE_RETURN(std::abs(result - 20.0) < 0.001, false, "Result is not 20.0, actual: %f", result);
    return true;
}

static boost::json::object service_test_build_test_all_types_params()
{
    boost::json::object params;

    // Optional parameters
    // params["string_param"] = "Hello World";

    params["number_param"] = 42.5;
    params["boolean_param"] = true;

    // Build complex object parameters
    boost::json::object test_object;
    test_object["key1"] = "value1";
    test_object["key2"] = 123;
    boost::json::object nested_object;
    nested_object["inner"] = "data";
    test_object["nested"] = nested_object;
    params["object_param"] = test_object;

    // Build array parameters
    boost::json::array test_array;
    test_array.push_back(1);
    test_array.push_back(2);
    test_array.push_back("three");
    test_array.push_back(true);
    test_array.push_back(nullptr);
    boost::json::object array_object;
    array_object["item"] = "value";
    test_array.push_back(array_object);
    params["array_param"] = test_array;

    return params;
}

// Check if result_obj contains and correctly handles all supported type parameters
static bool service_test_validate_test_all_types_result(const boost::json::object &result_obj)
{
    std::stringstream ss;

    // Check if it contains the expected fields
    if (!result_obj.contains("received_types") || !result_obj.contains("message") || !result_obj.contains("total_params")) {
        BROOKESIA_LOGE(" (Missing necessary fields in the result)");
        return false;
    }

    // Verify if received_types is an object
    if (!result_obj.at("received_types").is_object()) {
        BROOKESIA_LOGE(" (received_types is not an object)");
        return false;
    }

    auto received_types = result_obj.at("received_types").as_object();

    // Verify if all types are correctly received
    bool has_string = received_types.contains("string") && received_types["string"].is_string();
    bool has_number = received_types.contains("number") && received_types["number"].is_double();
    bool has_boolean = received_types.contains("boolean") && received_types["boolean"].is_bool();
    bool has_object = received_types.contains("object") && received_types["object"].is_object();
    bool has_array = received_types.contains("array") && received_types["array"].is_array();

    if (!has_string || !has_number || !has_boolean || !has_object || !has_array) {
        ss << " (Some types are not correctly processed: string=" << has_string
           << ", number=" << has_number << ", boolean=" << has_boolean
           << ", object=" << has_object
           << ", array=" << has_array << ")";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    // Verify parameter count
    if (!result_obj.at("total_params").is_int64() || result_obj.at("total_params").as_int64() != 5) {
        ss << " (parameter count mismatch, expected: 5, actual: " << result_obj.at("total_params").as_int64() << ")";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    // Verify message field
    if (!result_obj.at("message").is_string()) {
        ss << " (message field type error)";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    // Verify each type value
    if (boost::json::value_to<std::string>(received_types["string"]) != "Hello World") {
        ss << " (string value mismatch)";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    if (std::abs(received_types["number"].as_double() - 42.5) > 0.001) {
        ss << " (number value mismatch)";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    if (!received_types["boolean"].as_bool()) {
        ss << " (boolean value mismatch)";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    // Verify object content
    auto received_object = received_types["object"].as_object();
    if (!received_object.contains("key1") ||
            boost::json::value_to<std::string>(received_object["key1"]) != "value1") {
        ss << " (object content validation failed)";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    // Verify array content
    auto received_array = received_types["array"].as_array();
    if (received_array.size() != 6) {
        ss << " (array length mismatch, expected: 6, actual: " << received_array.size() << ")";
        BROOKESIA_LOGE("%s", ss.str().c_str());
        return false;
    }

    // ss << " (Successfully processed " << result_obj.at("total_params") << " parameters, including all "
    //    << static_cast<size_t>(FunctionValueType::Array) + 1 << " types)";
    // BROOKESIA_LOGI("%s", ss.str().c_str());
    // BROOKESIA_LOGI("✓");

    return true;
}

static bool startup()
{
    TimeProfiler::FormatOptions opt;
    opt.use_unicode = true;
    opt.use_color = true;          // Enable color highlighting (>50% red, >20% yellow, >5% cyan)
    opt.sort_by = TimeProfiler::FormatOptions::SortBy::TotalDesc;
    opt.show_percentages = true;
    opt.name_width = 30;
    opt.calls_width = 6;
    opt.num_width = 10;
    opt.percent_width = 7;
    opt.precision = 2;
    opt.time_unit = TimeProfiler::FormatOptions::TimeUnit::Milliseconds;
    time_profiler.set_format_options(opt);

    BROOKESIA_CHECK_FALSE_RETURN(service_manager.init(), false, "Failed to initialize service manager");
    BROOKESIA_CHECK_FALSE_RETURN(service_manager.start(), false, "Failed to start service manager");

    return true;
}

static void shutdown()
{
    service_manager.stop();
    service_manager.deinit();
    time_profiler.report();
    time_profiler.clear();
}

static ServiceBinding bind_service(const std::string &name)
{
    auto test_service_binding = service_manager.bind(name);
    BROOKESIA_CHECK_FALSE_RETURN(
        test_service_binding.is_valid(), ServiceBinding(), "Failed to bind service: %s", name.c_str()
    );
    return test_service_binding;
}

static FunctionResult do_call_function(
    std::shared_ptr<ServiceBase> service, const std::string &method,
    const boost::json::object &params, size_t timeout_ms
)
{
    BROOKESIA_TIME_PROFILER_START_EVENT("call_function_sync");

    // Convert boost::json::object to FunctionParameterMap
    FunctionParameterMap parameters;
    for (const auto &[key, value] : params) {
        if (value.is_bool()) {
            parameters[key] = FunctionValue(value.as_bool());
        } else if (value.is_double()) {
            parameters[key] = FunctionValue(value.as_double());
        } else if (value.is_string()) {
            parameters[key] = FunctionValue(std::string(value.as_string()));
        } else if (value.is_object()) {
            parameters[key] = FunctionValue(value.as_object());
        } else if (value.is_array()) {
            parameters[key] = FunctionValue(value.as_array());
        }
    }

    FunctionResult result;
    {
        BROOKESIA_TIME_PROFILER_SCOPE("call_function_sync");
        result = service->call_function_sync(method, std::move(parameters), timeout_ms);
    }

    BROOKESIA_TIME_PROFILER_END_EVENT("call_function_sync");

    return result;
}

static bool do_validate_call_function(
    const TestMap &test_map, size_t test_repeatitive_num, const CallConfig &call_config
)
{
    BROOKESIA_CHECK_FALSE_RETURN(startup(), false, "Failed to startup");

    bool success = true;
    for (const auto& [service_name, test_items] : test_map) {
        BROOKESIA_TIME_PROFILER_SCOPE(service_name);

        BROOKESIA_LOGI("\nTesting service: %s", service_name.c_str());

        auto test_service_binding = bind_service(service_name);
        auto service = test_service_binding.get_service();
        BROOKESIA_CHECK_NULL_RETURN(service, false, "Failed to get service");

        for (const auto &test : test_items) {
            BROOKESIA_LOGI("\n\tExecuting %s...", test.method.c_str());

            for (size_t i = 0; i < test_repeatitive_num; i++) {
                FunctionResult result;
                {
                    BROOKESIA_TIME_PROFILER_SCOPE(test.method);
                    result = do_call_function(
                                 service, test.method, boost::json::object(test.params), call_config.timeout_ms
                             );
                }

                if (!result.success || !test.validator(result.data.value())) {
                    success = false;
                    std::stringstream ss;
                    ss << "✗ " << i << "/" << test_repeatitive_num << "(";
                    if (!result.success) {
                        ss << "Call failed with error: " << result.error_message;
                    } else {
                        ss << "Validation failed";
                    }
                    ss << ")";
                    BROOKESIA_LOGE("%s", ss.str().c_str());
                    break;
                }
            }
        }
    }

    shutdown();

    return success;
}

static bool do_concurrent_call_function(
    const std::string &service_name, const TestItem &test_item,
    const ConcurrentCallConfig &concurrent_config,
    size_t timeout_ms
)
{
    BROOKESIA_CHECK_FALSE_RETURN(startup(), false, "Failed to startup");

    BROOKESIA_LOGI(
        "\nTesting concurrent call function: %s::%s (%zu concurrent requests in %zu ms)", service_name.c_str(),
        test_item.method.c_str(), concurrent_config.num, concurrent_config.total_timeout_ms
    );

    auto test_service_binding = bind_service(service_name);
    auto service = test_service_binding.get_service();
    BROOKESIA_CHECK_NULL_RETURN(service, false, "Failed to get service");

    // use std::async to call function concurrently
    std::map<int, std::future<FunctionResult>> request_futures;

    auto &method = test_item.method;
    auto &params = test_item.params;
    auto &validator = test_item.validator;

    for (size_t i = 0; i < concurrent_config.num; ++i) {
        ThreadConfigGuard config_guard({
            .name = (boost::format("ConcT%1%") % i).str().c_str(),
        });
        auto request_future = std::async(std::launch::async, [service, &method, &params, timeout_ms, i]() {
            BROOKESIA_TIME_PROFILER_SCOPE("request_" + std::to_string(i));

            return do_call_function(
                       service, method, boost::json::object(params), timeout_ms
                   );
        });
        request_futures[i] = std::move(request_future);
    }

    // Poll to wait for all requests to complete
    size_t successful_requests = 0;
    auto start_wait = std::chrono::steady_clock::now();

    // Mark which futures have been processed
    std::vector<bool> processed(request_futures.size(), false);
    size_t remaining_futures = request_futures.size();

    // Continuously poll until timeout or all requests are completed
    while (remaining_futures > 0) {
        // Check if timeout has occurred
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start_wait).count();
        if (elapsed > concurrent_config.total_timeout_ms) {
            BROOKESIA_LOGE(
                "Concurrent test timeout (elapsed: %zu ms, timeout: %zu ms)",
                elapsed, concurrent_config.total_timeout_ms
            );
            break;
        }

        // Iterate through all futures, try to get ready ones
        bool any_ready = false;
        for (size_t i = 0; i < request_futures.size(); ++i) {
            // Skip already processed futures
            if (processed[i]) {
                continue;
            }

            // Try to get immediately (non-blocking)
            if (request_futures[i].wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
                any_ready = true;
                remaining_futures--;

                try {
                    auto result = request_futures[i].get();
                    if (result.success && validator(result.data.value())) {
                        successful_requests++;
                    } else {
                        std::stringstream ss;
                        ss << "Request " << i << " failed";
                        if (!result.success) {
                            ss << ": " << result.error_message;
                        } else {
                            ss << ": Validation failed";
                        }
                        BROOKESIA_LOGE("%s", ss.str().c_str());
                    }
                } catch (const std::exception &e) {
                    BROOKESIA_LOGE("Request %zu failed with exception: %s", i, e.what());
                }

                // mark as processed
                processed[i] = true;
            }
        }

        // if no future is ready, sleep for a short time to avoid spinning
        if (!any_ready) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    size_t success_rate = successful_requests * 100 / concurrent_config.num;
    bool test_passed = success_rate >= concurrent_config.success_rate;
    if (!test_passed) {
        BROOKESIA_LOGE(
            "\n\t✗ Concurrent test failed: %zu/%zu (success rate: %zu%% < %zu%%)", successful_requests,
            concurrent_config.num, success_rate, concurrent_config.success_rate
        );
    } else {
        BROOKESIA_LOGI(
            "\n\t✓ Concurrent test passed: %zu/%zu (success rate: %zu%% >= %zu%%)", successful_requests,
            concurrent_config.num, success_rate, concurrent_config.success_rate
        );
    }

    shutdown();

    return test_passed;
}
