/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include <future>
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_manager.hpp"
#include "common_def.hpp"
#include "service_test.hpp"
#include "service_test_with_scheduler.hpp"

namespace esp_brookesia::service {

namespace {

static auto &service_manager = ServiceManager::get_instance();

FunctionParameterMap make_binary_number_parameters(double a, double b)
{
    return {
        {"a", FunctionValue(a)},
        {"b", FunctionValue(b)},
    };
}

FunctionCall make_binary_number_call(const std::string &name, double a, double b)
{
    return {
        .name = name,
        .parameters = make_binary_number_parameters(a, b),
    };
}

FunctionCall make_add_call(double a, double b)
{
    return make_binary_number_call(ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexAdd].name, a, b);
}

FunctionCall make_divide_call(double a, double b)
{
    return make_binary_number_call(ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexDivide].name, a, b);
}

const std::string &get_add_name()
{
    return ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexAdd].name;
}

const std::string &get_divide_name()
{
    return ServiceTest::FUNCTION_SCHEMAS[ServiceTest::FunctionIndexDivide].name;
}

const char *get_first_error_or_empty(const FunctionBatchResult &result)
{
    return result.results.empty() ? "" : result.results.front().error_message.c_str();
}

void assert_number_call_result(const FunctionCallResult &result, const std::string &name, double expected)
{
    TEST_ASSERT_EQUAL_STRING(name.c_str(), result.name.c_str());
    TEST_ASSERT_TRUE_MESSAGE(result.success, result.error_message.c_str());
    TEST_ASSERT_TRUE(result.data.has_value());
    TEST_ASSERT_EQUAL_DOUBLE(expected, std::get<double>(result.data.value()));
}

FunctionBatchResult wait_batch_result(std::future<FunctionBatchResult> &future)
{
    auto status = future.wait_for(std::chrono::seconds(2));
    TEST_ASSERT_EQUAL(std::future_status::ready, status);

    return future.get();
}

void cleanup_service_manager()
{
    service_manager.stop();
    service_manager.deinit();
}

} // namespace

BROOKESIA_TEST_CASE(test_batch_call_sync_batch_success, "Test Batch Call: sync batch success", "[brookesia][service][batch_call][sync][success]")
{
    BROOKESIA_LOGI("=== Test sync batch success ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    auto result = service->call_functions_sync({
        make_add_call(10.0, 20.0),
        make_divide_call(20.0, 4.0),
    }, 500);

    TEST_ASSERT_TRUE_MESSAGE(result.success, get_first_error_or_empty(result));
    TEST_ASSERT_EQUAL_size_t(2, result.results.size());
    assert_number_call_result(result.results[0], get_add_name(), 30.0);
    assert_number_call_result(result.results[1], get_divide_name(), 5.0);

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_sync_batch_fail_fast, "Test Batch Call: sync batch fail-fast", "[brookesia][service][batch_call][sync][fail_fast]")
{
    BROOKESIA_LOGI("=== Test sync batch fail-fast ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    auto result = service->call_functions_sync({
        make_add_call(1.0, 2.0),
        make_divide_call(1.0, 0.0),
        make_add_call(100.0, 200.0),
    }, 500);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_size_t(2, result.results.size());
    assert_number_call_result(result.results[0], get_add_name(), 3.0);
    TEST_ASSERT_EQUAL_STRING(get_divide_name().c_str(), result.results[1].name.c_str());
    TEST_ASSERT_FALSE(result.results[1].success);
    TEST_ASSERT_FALSE(result.results[1].error_message.empty());

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_sync_batch_empty, "Test Batch Call: sync batch empty", "[brookesia][service][batch_call][sync][empty]")
{
    BROOKESIA_LOGI("=== Test sync batch empty ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    auto result = service->call_functions_sync({}, 500);
    TEST_ASSERT_TRUE(result.success);
    TEST_ASSERT_TRUE(result.results.empty());

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_sync_batch_non_existent_function, "Test Batch Call: sync batch non-existent function", "[brookesia][service][batch_call][sync][not_exist]")
{
    BROOKESIA_LOGI("=== Test sync batch non-existent function ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    auto result = service->call_functions_sync({
        make_add_call(1.0, 2.0),
        {
            .name = "non_existent",
            .parameters = {},
        },
    }, 500);

    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_size_t(1, result.results.size());
    TEST_ASSERT_FALSE(result.results[0].success);
    TEST_ASSERT_FALSE(result.results[0].error_message.empty());

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_async_batch_success, "Test Batch Call: async batch success", "[brookesia][service][batch_call][async][success]")
{
    BROOKESIA_LOGI("=== Test async batch success ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    std::promise<FunctionBatchResult> promise;
    auto future = promise.get_future();
    auto handler = [&promise](FunctionBatchResult && result) {
        promise.set_value(std::move(result));
    };

    bool called = service->call_functions_async({
        make_add_call(10.0, 20.0),
        make_divide_call(20.0, 4.0),
    }, handler);
    TEST_ASSERT_TRUE(called);

    auto result = wait_batch_result(future);
    TEST_ASSERT_TRUE_MESSAGE(result.success, get_first_error_or_empty(result));
    TEST_ASSERT_EQUAL_size_t(2, result.results.size());
    assert_number_call_result(result.results[0], get_add_name(), 30.0);
    assert_number_call_result(result.results[1], get_divide_name(), 5.0);

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_async_batch_fail_fast, "Test Batch Call: async batch fail-fast", "[brookesia][service][batch_call][async][fail_fast]")
{
    BROOKESIA_LOGI("=== Test async batch fail-fast ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    std::promise<FunctionBatchResult> promise;
    auto future = promise.get_future();
    auto handler = [&promise](FunctionBatchResult && result) {
        promise.set_value(std::move(result));
    };

    bool called = service->call_functions_async({
        make_add_call(1.0, 2.0),
        make_divide_call(1.0, 0.0),
        make_add_call(100.0, 200.0),
    }, handler);
    TEST_ASSERT_TRUE(called);

    auto result = wait_batch_result(future);
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_size_t(2, result.results.size());
    assert_number_call_result(result.results[0], get_add_name(), 3.0);
    TEST_ASSERT_EQUAL_STRING(get_divide_name().c_str(), result.results[1].name.c_str());
    TEST_ASSERT_FALSE(result.results[1].success);
    TEST_ASSERT_FALSE(result.results[1].error_message.empty());

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_async_batch_non_existent_function, "Test Batch Call: async batch non-existent function", "[brookesia][service][batch_call][async][not_exist]")
{
    BROOKESIA_LOGI("=== Test async batch non-existent function ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    std::atomic<bool> handler_called{false};
    auto handler = [&handler_called](FunctionBatchResult &&) {
        handler_called = true;
    };
    bool called = service->call_functions_async({
        make_add_call(1.0, 2.0),
        {
            .name = "non_existent",
            .parameters = {},
        },
    }, handler);

    TEST_ASSERT_FALSE(called);
    TEST_ASSERT_FALSE(handler_called.load());

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_when_service_not_running, "Test Batch Call: when service not running", "[brookesia][service][batch_call][not_running]")
{
    BROOKESIA_LOGI("=== Test batch call when service not running ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTest::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    binding.release();
    TEST_ASSERT_FALSE(service->is_running());

    auto sync_result = service->call_functions_sync({
        make_add_call(1.0, 2.0),
    }, 100);
    TEST_ASSERT_FALSE(sync_result.success);
    TEST_ASSERT_EQUAL_size_t(1, sync_result.results.size());
    TEST_ASSERT_FALSE(sync_result.results[0].error_message.empty());

    std::atomic<bool> handler_called{false};
    auto handler = [&handler_called](FunctionBatchResult &&) {
        handler_called = true;
    };
    bool async_called = service->call_functions_async({
        make_add_call(1.0, 2.0),
    }, handler);
    TEST_ASSERT_FALSE(async_called);
    TEST_ASSERT_FALSE(handler_called.load());

    cleanup_service_manager();
}

BROOKESIA_TEST_CASE(test_batch_call_async_batch_with_dedicated_scheduler, "Test Batch Call: async batch with dedicated scheduler", "[brookesia][service][batch_call][async][dedicated_scheduler]")
{
    BROOKESIA_LOGI("=== Test async batch with dedicated scheduler ===");

    TEST_ASSERT_TRUE(service_manager.init());
    TEST_ASSERT_TRUE(service_manager.start());

    auto binding = service_manager.bind(ServiceTestWithScheduler::SERVICE_NAME);
    TEST_ASSERT_TRUE(binding.is_valid());

    auto service = binding.get_service();
    TEST_ASSERT_NOT_NULL(service);

    std::promise<FunctionBatchResult> promise;
    auto future = promise.get_future();
    auto handler = [&promise](FunctionBatchResult && result) {
        promise.set_value(std::move(result));
    };

    bool called = service->call_functions_async({
        make_add_call(30.0, 12.0),
        make_divide_call(42.0, 6.0),
    }, handler);
    TEST_ASSERT_TRUE(called);

    auto result = wait_batch_result(future);
    TEST_ASSERT_TRUE_MESSAGE(result.success, get_first_error_or_empty(result));
    TEST_ASSERT_EQUAL_size_t(2, result.results.size());
    assert_number_call_result(result.results[0], get_add_name(), 42.0);
    assert_number_call_result(result.results[1], get_divide_name(), 7.0);

    cleanup_service_manager();
}

} // namespace esp_brookesia::service
