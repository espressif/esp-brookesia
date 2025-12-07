/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "boost/json.hpp"
#include "brookesia/lib_utils.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_manager/service/local_runner.hpp"
#include "common_def.hpp"
#include "service_test.hpp"
#include "service_test_with_scheduler.hpp"

using namespace esp_brookesia::lib_utils;
using namespace esp_brookesia::service;

static auto &service_manager = ServiceManager::get_instance();
static auto &time_profiler = TimeProfiler::get_instance();

static bool startup();
static void shutdown();

// ============================================================================
// Test cases
// ============================================================================

TEST_CASE("Test LocalTestRunner - basic functionality", "[service][test_runner][basic]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_basic");
    BROOKESIA_LOGI("=== Test LocalTestRunner - basic functionality ===");

    // Initialize service manager
    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Define test sequence
    std::vector<LocalTestItem> test_items = {
        {
            .name = "Test add function",
            .method = "add",
            .params = boost::json::object{{"a", 10.0}, {"b", 20.0}},
            .validator = [](const FunctionValue & result) -> bool {
                auto double_result = std::get_if<double>(&result);
                BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
                BROOKESIA_CHECK_FALSE_RETURN(*double_result == 30.0, false, "Add result incorrect");
                return true;
            },
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
        {
            .name = "Test divide function",
            .method = "divide",
            .params = boost::json::object{{"a", 100.0}, {"b", 5.0}},
            .validator = [](const FunctionValue & result) -> bool {
                auto double_result = std::get_if<double>(&result);
                BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
                BROOKESIA_CHECK_FALSE_RETURN(*double_result == 20.0, false, "Divide result incorrect");
                return true;
            },
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
    };

    // Execute tests
    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    // Verify results
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Not all tests passed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(test_items.size(), results.size());
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE_MESSAGE(results[i], (std::string("Test failed: ") + test_items[i].name).c_str());
    }
}

TEST_CASE("Test LocalTestRunner - with delays", "[service][test_runner][delays]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_delays");
    BROOKESIA_LOGI("=== Test LocalTestRunner - with delays ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<LocalTestItem> test_items = {
        {
            .name = "First add",
            .method = "add",
            .params = boost::json::object{{"a", 1.0}, {"b", 2.0}},
            .validator = nullptr,
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 200,
        },
        {
            .name = "Second add with delay",
            .method = "add",
            .params = boost::json::object{{"a", 3.0}, {"b", 4.0}},
            .validator = nullptr,
            .start_delay_ms = 300,
            .call_timeout_ms = 100,
            .run_duration_ms = 200,
        },
        {
            .name = "Third add with delay",
            .method = "add",
            .params = boost::json::object{{"a", 5.0}, {"b", 6.0}},
            .validator = nullptr,
            .start_delay_ms = 300,
            .call_timeout_ms = 100,
            .run_duration_ms = 200,
        },
    };

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Tests with delays failed");
}

TEST_CASE("Test LocalTestRunner - validation failures", "[service][test_runner][validation]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_validation");
    BROOKESIA_LOGI("=== Test LocalTestRunner - validation failures ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<LocalTestItem> test_items = {
        {
            .name = "Test with incorrect validation",
            .method = "add",
            .params = boost::json::object{{"a", 10.0}, {"b", 20.0}},
            .validator = [](const FunctionValue & result) -> bool {
                auto double_result = std::get_if<double>(&result);
                BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
                // Use incorrect expected value intentionally
                BROOKESIA_CHECK_FALSE_RETURN(*double_result == 999.0, false, "Validation should fail");
                return true;
            },
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
    };

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    // This test should fail (validator expects incorrect value)
    TEST_ASSERT_FALSE_MESSAGE(all_passed, "Test should have failed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(1, results.size());
    TEST_ASSERT_FALSE(results[0]);
}

TEST_CASE("Test LocalTestRunner - error handling", "[service][test_runner][error]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_error");
    BROOKESIA_LOGI("=== Test LocalTestRunner - error handling ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<LocalTestItem> test_items = {
        {
            .name = "Test divide by zero",
            .method = "divide",
            .params = boost::json::object{{"a", 100.0}, {"b", 0.0}},
            .validator = nullptr,
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
        {
            .name = "Test non-existent function",
            .method = "non_existent_function",
            .params = boost::json::object{},
            .validator = nullptr,
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
    };

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    // At least one test should fail
    TEST_ASSERT_FALSE_MESSAGE(all_passed, "Error tests should have failures");
}

TEST_CASE("Test LocalTestRunner - all parameter types", "[service][test_runner][all_types]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_all_types");
    BROOKESIA_LOGI("=== Test LocalTestRunner - all parameter types ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<LocalTestItem> test_items = {
        {
            .name = "Test all parameter types",
            .method = "test_all_types",
            .params = boost::json::object{
                {"boolean_param", true},
                {"number_param", 42.0},
                {"string_param", "test string"},
                {"object_param", boost::json::object{{"key", "value"}}},
                {"array_param", boost::json::array{1, 2, 3}}
            },
            .validator = [](const FunctionValue & result) -> bool {
                auto obj_result = std::get_if<boost::json::object>(&result);
                BROOKESIA_CHECK_NULL_RETURN(obj_result, false, "Result is not an object");

                // Verify returned object structure
                BROOKESIA_CHECK_FALSE_RETURN(
                    obj_result->contains("message"), false, "Missing message field"
                );
                BROOKESIA_CHECK_FALSE_RETURN(
                    obj_result->contains("total_params"), false, "Missing total_params field"
                );

                return true;
            },
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 200,
        },
    };

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "All types test failed");
}

TEST_CASE("Test LocalTestRunner - custom config", "[service][test_runner][config]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_config");
    BROOKESIA_LOGI("=== Test LocalTestRunner - custom config ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<LocalTestItem> test_items = {
        {
            .name = "Test 1",
            .method = "add",
            .params = boost::json::object{{"a", 1.0}, {"b", 1.0}},
            .validator = nullptr,
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
        {
            .name = "Test 2",
            .method = "add",
            .params = boost::json::object{{"a", 2.0}, {"b", 2.0}},
            .validator = nullptr,
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
        {
            .name = "Test 3",
            .method = "add",
            .params = boost::json::object{{"a", 3.0}, {"b", 3.0}},
            .validator = nullptr,
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        },
    };

    // Use custom configuration
    LocalTestRunner::RunTestsConfig config(ServiceTest::SERVICE_NAME);
    config.scheduler_config = {
        .worker_configs = {
            {
                .name = "custom_scheduler",
                .stack_size = 8 * 1024,
            }
        },
    };
    config.extra_timeout_ms = 2000;

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(config, test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Custom config test failed");
}

TEST_CASE("Test LocalTestRunner - sequential execution", "[service][test_runner][sequential]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_sequential");
    BROOKESIA_LOGI("=== Test LocalTestRunner - sequential execution ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Test sequential execution (through cumulative delay verification)
    std::vector<LocalTestItem> test_items = {
        {
            .name = "Step 1",
            .method = "add",
            .params = boost::json::object{{"a", 1.0}, {"b", 1.0}},
            .validator = nullptr,
            .start_delay_ms = 100,
            .call_timeout_ms = 100,
            .run_duration_ms = 500,
        },
        {
            .name = "Step 2",
            .method = "add",
            .params = boost::json::object{{"a", 2.0}, {"b", 2.0}},
            .validator = nullptr,
            .start_delay_ms = 100,
            .call_timeout_ms = 100,
            .run_duration_ms = 500,
        },
        {
            .name = "Step 3",
            .method = "add",
            .params = boost::json::object{{"a", 3.0}, {"b", 3.0}},
            .validator = nullptr,
            .start_delay_ms = 100,
            .call_timeout_ms = 100,
            .run_duration_ms = 500,
        },
    };

    auto start_time = std::chrono::steady_clock::now();

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start_time
                   ).count();

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Sequential test failed");

    // Verify execution time (should be cumulative)
    // Total time = run_duration(500+500+500) + start_delay(0+100+100) + call_timeout(100*3) ≈ 1800ms
    BROOKESIA_LOGI("Total execution time: %d ms", elapsed);
    TEST_ASSERT_TRUE_MESSAGE(elapsed >= 1700, "Execution time too short");
}

TEST_CASE("Test LocalTestRunner - empty test list", "[service][test_runner][empty]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_empty");
    BROOKESIA_LOGI("=== Test LocalTestRunner - empty test list ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    std::vector<LocalTestItem> test_items;  // Empty test list

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    // Empty test list should return true
    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Empty test list should pass");
    TEST_ASSERT_EQUAL(0, runner.get_results().size());
}

TEST_CASE("Test LocalTestRunner - stress test", "[service][test_runner][stress]")
{
    BROOKESIA_TIME_PROFILER_SCOPE("test_runner_stress");
    BROOKESIA_LOGI("=== Test LocalTestRunner - stress test ===");

    BROOKESIA_CHECK_FALSE_RETURN(startup(),, "Failed to startup");
    FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    // Create a large number of test items
    std::vector<LocalTestItem> test_items;
    for (int i = 0; i < 20; i++) {
        test_items.push_back({
            .name = (boost::format("Test iteration %1%") % i).str(),
            .method = "add",
            .params = boost::json::object{{"a", static_cast<double>(i)}, {"b", static_cast<double>(i + 1)}},
            .validator = [i](const FunctionValue & result) -> bool {
                auto double_result = std::get_if<double>(&result);
                BROOKESIA_CHECK_NULL_RETURN(double_result, false, "Result is not a double");
                double expected = static_cast<double>(i) + static_cast<double>(i + 1);
                BROOKESIA_CHECK_FALSE_RETURN(
                    *double_result == expected, false, "Result mismatch"
                );
                return true;
            },
            .start_delay_ms = 0,
            .call_timeout_ms = 100,
            .run_duration_ms = 100,
        });
    }

    LocalTestRunner runner;
    bool all_passed = runner.run_tests(ServiceTest::SERVICE_NAME, test_items);

    TEST_ASSERT_TRUE_MESSAGE(all_passed, "Stress test failed");

    const auto &results = runner.get_results();
    TEST_ASSERT_EQUAL(20, results.size());

    // Verify all tests passed
    for (size_t i = 0; i < results.size(); i++) {
        TEST_ASSERT_TRUE(results[i]);
    }
}

static bool startup()
{
    // Configure TimeProfiler
    TimeProfiler::FormatOptions opt;
    opt.use_unicode = true;
    opt.use_color = true;
    opt.sort_by = TimeProfiler::FormatOptions::SortBy::TotalDesc;
    opt.show_percentages = true;
    opt.name_width = 40;
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
