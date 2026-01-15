/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <thread>
#include <chrono>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "brookesia/lib_utils/time_profiler.hpp"
#include "brookesia/lib_utils/log.hpp"

using namespace esp_brookesia::lib_utils;

// ==================== Helper Functions ====================

void simulate_work(int milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

void fast_function()
{
    BROOKESIA_TIME_PROFILER_SCOPE("fast_function");
    simulate_work(10);
}

void slow_function()
{
    BROOKESIA_TIME_PROFILER_SCOPE("slow_function");
    simulate_work(50);
}

void nested_function_level1()
{
    BROOKESIA_TIME_PROFILER_SCOPE("nested_level1");
    simulate_work(20);

    {
        BROOKESIA_TIME_PROFILER_SCOPE("nested_level2");
        simulate_work(30);

        {
            BROOKESIA_TIME_PROFILER_SCOPE("nested_level3");
            simulate_work(15);
        }
    }
}

void function_with_loop()
{
    BROOKESIA_TIME_PROFILER_SCOPE("function_with_loop");
    for (int i = 0; i < 5; i++) {
        BROOKESIA_TIME_PROFILER_SCOPE("loop_iteration");
        simulate_work(5);
    }
}

// ==================== TimeProfilerScope Basic Usage Test ====================

TEST_CASE("Test TimeProfilerScope basic usage", "[utils][time_profiler][scope][basic]")
{
    BROOKESIA_LOGI("=== TimeProfilerScope Basic Usage Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("test_scope");
        simulate_work(50);
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfilerScope multiple calls", "[utils][time_profiler][scope][multiple]")
{
    BROOKESIA_LOGI("=== TimeProfilerScope Multiple Calls Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    for (int i = 0; i < 3; i++) {
        BROOKESIA_TIME_PROFILER_SCOPE("repeated_scope");
        simulate_work(20);
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfilerScope nested scopes", "[utils][time_profiler][scope][nested]")
{
    BROOKESIA_LOGI("=== TimeProfilerScope Nested Scopes Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("outer_scope");
        simulate_work(10);

        {
            BROOKESIA_TIME_PROFILER_SCOPE("inner_scope_1");
            simulate_work(20);
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("inner_scope_2");
            simulate_work(30);
        }
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfilerScope deep nesting", "[utils][time_profiler][scope][deep_nested]")
{
    BROOKESIA_LOGI("=== TimeProfilerScope Deep Nesting Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    nested_function_level1();

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Function Calls Test ====================

TEST_CASE("Test TimeProfiler with function calls", "[utils][time_profiler][function]")
{
    BROOKESIA_LOGI("=== TimeProfiler Function Calls Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    fast_function();
    slow_function();
    fast_function();

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler with loop", "[utils][time_profiler][loop]")
{
    BROOKESIA_LOGI("=== TimeProfiler Loop Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    function_with_loop();

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler start/end event", "[utils][time_profiler][event][basic]")
{
    BROOKESIA_LOGI("=== TimeProfiler Start/End Event Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    BROOKESIA_TIME_PROFILER_START_EVENT("manual_event");
    simulate_work(30);
    BROOKESIA_TIME_PROFILER_END_EVENT("manual_event");

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler multiple events", "[utils][time_profiler][event][multiple]")
{
    BROOKESIA_LOGI("=== TimeProfiler Multiple Events Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    for (int i = 0; i < 3; i++) {
        BROOKESIA_TIME_PROFILER_START_EVENT("repeated_event");
        simulate_work(15);
        BROOKESIA_TIME_PROFILER_END_EVENT("repeated_event");
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler nested events", "[utils][time_profiler][event][nested]")
{
    BROOKESIA_LOGI("=== TimeProfiler Nested Events Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    BROOKESIA_TIME_PROFILER_START_EVENT("outer_event");
    simulate_work(10);

    BROOKESIA_TIME_PROFILER_START_EVENT("inner_event");
    simulate_work(20);
    BROOKESIA_TIME_PROFILER_END_EVENT("inner_event");

    simulate_work(10);
    BROOKESIA_TIME_PROFILER_END_EVENT("outer_event");

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Mixed Scope and Event Test ====================

TEST_CASE("Test TimeProfiler mixed scope and event", "[utils][time_profiler][mixed]")
{
    BROOKESIA_LOGI("=== TimeProfiler Mixed Scope and Event Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("scope_function");
        simulate_work(10);

        BROOKESIA_TIME_PROFILER_START_EVENT("event_inside_scope");
        simulate_work(20);
        BROOKESIA_TIME_PROFILER_END_EVENT("event_inside_scope");

        simulate_work(10);
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Format Options Default Test ====================

TEST_CASE("Test TimeProfiler format options default", "[utils][time_profiler][format][default]")
{
    BROOKESIA_LOGI("=== TimeProfiler Format Options Default Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    fast_function();
    slow_function();

    BROOKESIA_LOGI("Using default format:");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler format options custom", "[utils][time_profiler][format][custom]")
{
    BROOKESIA_LOGI("=== TimeProfiler Format Options Custom Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.name_width = 30;
    options.calls_width = 8;
    options.num_width = 12;
    options.precision = 3;
    options.use_unicode = true;
    options.show_percentages = true;

    TimeProfiler::get_instance().set_format_options(options);

    fast_function();
    slow_function();

    BROOKESIA_LOGI("Using custom format:");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler format without percentages", "[utils][time_profiler][format][no_percent]")
{
    BROOKESIA_LOGI("=== TimeProfiler Format Without Percentages Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.show_percentages = false;

    TimeProfiler::get_instance().set_format_options(options);

    fast_function();
    slow_function();

    BROOKESIA_LOGI("Without percentages:");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler format without unicode", "[utils][time_profiler][format][no_unicode]")
{
    BROOKESIA_LOGI("=== TimeProfiler Format Without Unicode Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.use_unicode = false;

    TimeProfiler::get_instance().set_format_options(options);

    nested_function_level1();

    BROOKESIA_LOGI("Without unicode:");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler time unit microseconds", "[utils][time_profiler][format][unit_us]")
{
    BROOKESIA_LOGI("=== TimeProfiler Time Unit Microseconds Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.time_unit = TimeProfiler::FormatOptions::TimeUnit::Microseconds;

    TimeProfiler::get_instance().set_format_options(options);

    fast_function();

    BROOKESIA_LOGI("Unit: Microseconds");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler time unit seconds", "[utils][time_profiler][format][unit_s]")
{
    BROOKESIA_LOGI("=== TimeProfiler Time Unit Seconds Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.time_unit = TimeProfiler::FormatOptions::TimeUnit::Seconds;

    TimeProfiler::get_instance().set_format_options(options);

    slow_function();

    BROOKESIA_LOGI("Unit: Seconds");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler sort by total desc", "[utils][time_profiler][format][sort_total]")
{
    BROOKESIA_LOGI("=== TimeProfiler Sort By Total Desc Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.sort_by = TimeProfiler::FormatOptions::SortBy::TotalDesc;

    TimeProfiler::get_instance().set_format_options(options);

    fast_function();
    slow_function();
    fast_function();

    BROOKESIA_LOGI("Sorted by total (descending):");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler sort by name asc", "[utils][time_profiler][format][sort_name]")
{
    BROOKESIA_LOGI("=== TimeProfiler Sort By Name Asc Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.sort_by = TimeProfiler::FormatOptions::SortBy::NameAsc;

    TimeProfiler::get_instance().set_format_options(options);

    slow_function();
    fast_function();

    BROOKESIA_LOGI("Sorted by name (ascending):");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler sort none", "[utils][time_profiler][format][sort_none]")
{
    BROOKESIA_LOGI("=== TimeProfiler Sort None Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    TimeProfiler::FormatOptions options;
    options.sort_by = TimeProfiler::FormatOptions::SortBy::None;

    TimeProfiler::get_instance().set_format_options(options);

    slow_function();
    fast_function();

    BROOKESIA_LOGI("No sorting:");
    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Multiple Threads Test ====================

TEST_CASE("Test TimeProfiler with multiple threads", "[utils][time_profiler][thread][multiple]")
{
    BROOKESIA_LOGI("=== TimeProfiler Multiple Threads Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    std::vector<std::thread> threads;

    for (int i = 0; i < 3; i++) {
        threads.emplace_back([i]() {
            BROOKESIA_TIME_PROFILER_SCOPE("thread_work");
            simulate_work(20 + i * 10);
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler with nested threads", "[utils][time_profiler][thread][nested]")
{
    BROOKESIA_LOGI("=== TimeProfiler Nested Threads Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("main_thread");

        std::thread t1([]() {
            BROOKESIA_TIME_PROFILER_SCOPE("worker_thread_1");
            simulate_work(30);
        });

        std::thread t2([]() {
            BROOKESIA_TIME_PROFILER_SCOPE("worker_thread_2");
            simulate_work(40);
        });

        simulate_work(20);

        t1.join();
        t2.join();
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Real World - Data Processing Test ====================

TEST_CASE("Test TimeProfiler real world data processing", "[utils][time_profiler][real_world][data]")
{
    BROOKESIA_LOGI("=== TimeProfiler Real World - Data Processing Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("data_processing");

        {
            BROOKESIA_TIME_PROFILER_SCOPE("load_data");
            simulate_work(20);
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("process_data");
            for (int i = 0; i < 3; i++) {
                BROOKESIA_TIME_PROFILER_SCOPE("process_batch");
                simulate_work(15);
            }
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("save_results");
            simulate_work(10);
        }
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Real World - API Request Test ====================

TEST_CASE("Test TimeProfiler real world api request", "[utils][time_profiler][real_world][api]")
{
    BROOKESIA_LOGI("=== TimeProfiler Real World - API Request Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("api_request");

        {
            BROOKESIA_TIME_PROFILER_SCOPE("validate_input");
            simulate_work(5);
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("query_database");
            simulate_work(30);
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("format_response");
            simulate_work(10);
        }
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Real World - Rendering Pipeline Test ====================

TEST_CASE("Test TimeProfiler real world rendering pipeline", "[utils][time_profiler][real_world][render]")
{
    BROOKESIA_LOGI("=== TimeProfiler Real World - Rendering Pipeline Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    for (int frame = 0; frame < 3; frame++) {
        BROOKESIA_TIME_PROFILER_SCOPE("render_frame");

        {
            BROOKESIA_TIME_PROFILER_SCOPE("update_scene");
            simulate_work(5);
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("render_objects");
            simulate_work(20);
        }

        {
            BROOKESIA_TIME_PROFILER_SCOPE("post_processing");
            simulate_work(10);
        }
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Boundary Test ====================

TEST_CASE("Test TimeProfiler empty scope", "[utils][time_profiler][boundary][empty]")
{
    BROOKESIA_LOGI("=== TimeProfiler Empty Scope Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("empty_scope");
        // Do nothing
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler very fast operation", "[utils][time_profiler][boundary][fast]")
{
    BROOKESIA_LOGI("=== TimeProfiler Very Fast Operation Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    for (int i = 0; i < 100; i++) {
        BROOKESIA_TIME_PROFILER_SCOPE("fast_op");
        volatile int x = i * 2;
        (void)x;
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler long scope name", "[utils][time_profiler][boundary][long_name]")
{
    BROOKESIA_LOGI("=== TimeProfiler Long Scope Name Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("this_is_a_very_long_scope_name_that_might_cause_formatting_issues");
        simulate_work(10);
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Clear Test ====================

TEST_CASE("Test TimeProfiler clear", "[utils][time_profiler][clear]")
{
    BROOKESIA_LOGI("=== TimeProfiler Clear Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    fast_function();

    BROOKESIA_LOGI("Before clear:");
    BROOKESIA_TIME_PROFILER_REPORT();

    BROOKESIA_TIME_PROFILER_CLEAR();

    BROOKESIA_LOGI("After clear:");
    BROOKESIA_TIME_PROFILER_REPORT();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Stress Test ====================

TEST_CASE("Test TimeProfiler stress many scopes", "[utils][time_profiler][stress][many]")
{
    BROOKESIA_LOGI("=== TimeProfiler Stress - Many Scopes Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    for (int i = 0; i < 20; i++) {
        BROOKESIA_TIME_PROFILER_SCOPE("stress_scope");
        simulate_work(5);
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test TimeProfiler stress deep nesting", "[utils][time_profiler][stress][deep]")
{
    BROOKESIA_LOGI("=== TimeProfiler Stress - Deep Nesting Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    {
        BROOKESIA_TIME_PROFILER_SCOPE("level_1");
        {
            BROOKESIA_TIME_PROFILER_SCOPE("level_2");
            {
                BROOKESIA_TIME_PROFILER_SCOPE("level_3");
                {
                    BROOKESIA_TIME_PROFILER_SCOPE("level_4");
                    {
                        BROOKESIA_TIME_PROFILER_SCOPE("level_5");
                        simulate_work(10);
                    }
                }
            }
        }
    }

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    TEST_ASSERT_TRUE(true);
}

// ==================== TimeProfiler Accuracy and Stability Test ====================

/**
 * @brief Helper function to find a node in statistics by name
 */
const TimeProfiler::NodeStatistics *find_statistics_node(const std::vector<TimeProfiler::NodeStatistics> &nodes, const std::string &name)
{
    for (const auto &node : nodes) {
        if (node.name == name) {
            return &node;
        }
        // Recursively search in children
        const auto *found = find_statistics_node(node.children, name);
        if (found) {
            return found;
        }
    }
    return nullptr;
}

TEST_CASE("Test TimeProfiler scope timing accuracy", "[utils][time_profiler][accuracy][scope]")
{
    BROOKESIA_LOGI("=== TimeProfiler Scope Timing Accuracy Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Test multiple different time durations
    const std::vector<int> test_durations_ms = {10, 25, 50, 100, 200};
    const double tolerance_ms = 10.0;  // Allow 10ms tolerance

    for (size_t i = 0; i < test_durations_ms.size(); i++) {
        int expected_ms = test_durations_ms[i];
        std::string scope_name = "accuracy_test_scope_" + std::to_string(expected_ms) + "ms";

        {
            BROOKESIA_TIME_PROFILER_SCOPE(scope_name.c_str());
            simulate_work(expected_ms);
        }

        const auto stats = TimeProfiler::get_instance().get_statistics();
        const auto *node = find_statistics_node(stats.root_children, scope_name);

        TEST_ASSERT_NOT_NULL(node);
        if (node) {
            // Convert to milliseconds if needed
            double measured_ms = node->total;
            if (stats.unit_name == "s") {
                measured_ms = node->total * 1000.0;
            } else if (stats.unit_name == "us") {
                measured_ms = node->total / 1000.0;
            }

            BROOKESIA_LOGI("Duration %zu: Expected: %d ms, Measured: %.2f ms, Tolerance: %.2f ms",
                           i + 1, expected_ms, measured_ms, tolerance_ms);
            TEST_ASSERT_TRUE(measured_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(measured_ms <= (expected_ms + tolerance_ms));
            TEST_ASSERT_EQUAL(1, node->count);
        }
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler scope timing stability", "[utils][time_profiler][stability][scope]")
{
    BROOKESIA_LOGI("=== TimeProfiler Scope Timing Stability Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Test multiple different time durations
    const std::vector<int> test_durations_ms = {15, 30, 60, 120};
    const double tolerance_ms = 10.0;  // Allow 10ms tolerance
    const int iterations = 10;

    for (size_t duration_idx = 0; duration_idx < test_durations_ms.size(); duration_idx++) {
        int expected_ms = test_durations_ms[duration_idx];
        std::string scope_name = "stability_test_scope_" + std::to_string(expected_ms) + "ms";

        for (int i = 0; i < iterations; i++) {
            {
                BROOKESIA_TIME_PROFILER_SCOPE(scope_name.c_str());
                simulate_work(expected_ms);
            }
        }

        const auto stats = TimeProfiler::get_instance().get_statistics();
        const auto *node = find_statistics_node(stats.root_children, scope_name);

        TEST_ASSERT_NOT_NULL(node);
        if (node) {
            // Convert to milliseconds if needed
            double total_ms = node->total;
            double avg_ms = node->avg;
            double min_ms = node->min;
            double max_ms = node->max;

            if (stats.unit_name == "s") {
                total_ms = node->total * 1000.0;
                avg_ms = node->avg * 1000.0;
                min_ms = node->min * 1000.0;
                max_ms = node->max * 1000.0;
            } else if (stats.unit_name == "us") {
                total_ms = node->total / 1000.0;
                avg_ms = node->avg / 1000.0;
                min_ms = node->min / 1000.0;
                max_ms = node->max / 1000.0;
            }

            BROOKESIA_LOGI("Duration %zu: Iterations: %d, Expected avg: %d ms", duration_idx + 1, iterations, expected_ms);
            BROOKESIA_LOGI("Total: %.2f ms, Avg: %.2f ms, Min: %.2f ms, Max: %.2f ms", total_ms, avg_ms, min_ms, max_ms);

            TEST_ASSERT_EQUAL(iterations, node->count);
            TEST_ASSERT_TRUE(avg_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(avg_ms <= (expected_ms + tolerance_ms));
            TEST_ASSERT_TRUE(min_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(max_ms <= (expected_ms + tolerance_ms));
        }
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler event timing accuracy", "[utils][time_profiler][accuracy][event]")
{
    BROOKESIA_LOGI("=== TimeProfiler Event Timing Accuracy Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Test multiple different time durations
    const std::vector<int> test_durations_ms = {20, 40, 80, 150};
    const double tolerance_ms = 10.0;  // Allow 10ms tolerance

    for (size_t i = 0; i < test_durations_ms.size(); i++) {
        int expected_ms = test_durations_ms[i];
        std::string event_name = "accuracy_test_event_" + std::to_string(expected_ms) + "ms";

        BROOKESIA_TIME_PROFILER_START_EVENT(event_name.c_str());
        simulate_work(expected_ms);
        BROOKESIA_TIME_PROFILER_END_EVENT(event_name.c_str());

        const auto stats = TimeProfiler::get_instance().get_statistics();
        const auto *node = find_statistics_node(stats.root_children, event_name);

        TEST_ASSERT_NOT_NULL(node);
        if (node) {
            // Convert to milliseconds if needed
            double measured_ms = node->total;
            if (stats.unit_name == "s") {
                measured_ms = node->total * 1000.0;
            } else if (stats.unit_name == "us") {
                measured_ms = node->total / 1000.0;
            }

            BROOKESIA_LOGI("Duration %zu: Expected: %d ms, Measured: %.2f ms, Tolerance: %.2f ms",
                           i + 1, expected_ms, measured_ms, tolerance_ms);
            TEST_ASSERT_TRUE(measured_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(measured_ms <= (expected_ms + tolerance_ms));
            TEST_ASSERT_EQUAL(1, node->count);
        }
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler event timing stability", "[utils][time_profiler][stability][event]")
{
    BROOKESIA_LOGI("=== TimeProfiler Event Timing Stability Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Test multiple different time durations
    const std::vector<int> test_durations_ms = {20, 35, 70, 140};
    const double tolerance_ms = 10.0;  // Allow 10ms tolerance
    const int iterations = 10;

    for (size_t duration_idx = 0; duration_idx < test_durations_ms.size(); duration_idx++) {
        int expected_ms = test_durations_ms[duration_idx];
        std::string event_name = "stability_test_event_" + std::to_string(expected_ms) + "ms";

        for (int i = 0; i < iterations; i++) {
            BROOKESIA_TIME_PROFILER_START_EVENT(event_name.c_str());
            simulate_work(expected_ms);
            BROOKESIA_TIME_PROFILER_END_EVENT(event_name.c_str());
        }

        const auto stats = TimeProfiler::get_instance().get_statistics();
        const auto *node = find_statistics_node(stats.root_children, event_name);

        TEST_ASSERT_NOT_NULL(node);
        if (node) {
            // Convert to milliseconds if needed
            double total_ms = node->total;
            double avg_ms = node->avg;
            double min_ms = node->min;
            double max_ms = node->max;

            if (stats.unit_name == "s") {
                total_ms = node->total * 1000.0;
                avg_ms = node->avg * 1000.0;
                min_ms = node->min * 1000.0;
                max_ms = node->max * 1000.0;
            } else if (stats.unit_name == "us") {
                total_ms = node->total / 1000.0;
                avg_ms = node->avg / 1000.0;
                min_ms = node->min / 1000.0;
                max_ms = node->max / 1000.0;
            }

            BROOKESIA_LOGI("Duration %zu: Iterations: %d, Expected avg: %d ms", duration_idx + 1, iterations, expected_ms);
            BROOKESIA_LOGI("Total: %.2f ms, Avg: %.2f ms, Min: %.2f ms, Max: %.2f ms", total_ms, avg_ms, min_ms, max_ms);

            TEST_ASSERT_EQUAL(iterations, node->count);
            TEST_ASSERT_TRUE(avg_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(avg_ms <= (expected_ms + tolerance_ms));
            TEST_ASSERT_TRUE(min_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(max_ms <= (expected_ms + tolerance_ms));
        }
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler scope timing consistency multiple scopes", "[utils][time_profiler][consistency][scope]")
{
    BROOKESIA_LOGI("=== TimeProfiler Scope Timing Consistency Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Different scopes use different time durations
    const std::vector<int> scope_durations_ms = {15, 30, 60};
    const double tolerance_ms = 8.0;  // Allow 8ms tolerance
    const int iterations = 5;

    for (int i = 0; i < iterations; i++) {
        {
            BROOKESIA_TIME_PROFILER_SCOPE("consistency_scope_1");
            simulate_work(scope_durations_ms[0]);
        }
        {
            BROOKESIA_TIME_PROFILER_SCOPE("consistency_scope_2");
            simulate_work(scope_durations_ms[1]);
        }
        {
            BROOKESIA_TIME_PROFILER_SCOPE("consistency_scope_3");
            simulate_work(scope_durations_ms[2]);
        }
    }

    const auto stats = TimeProfiler::get_instance().get_statistics();

    // Check all three scopes with their respective expected durations
    for (int scope_num = 1; scope_num <= 3; scope_num++) {
        std::string scope_name = "consistency_scope_" + std::to_string(scope_num);
        int expected_ms = scope_durations_ms[scope_num - 1];
        const auto *node = find_statistics_node(stats.root_children, scope_name);

        TEST_ASSERT_NOT_NULL(node);
        if (node) {
            // Convert to milliseconds if needed
            double avg_ms = node->avg;
            if (stats.unit_name == "s") {
                avg_ms = node->avg * 1000.0;
            } else if (stats.unit_name == "us") {
                avg_ms = node->avg / 1000.0;
            }

            BROOKESIA_LOGI("Scope: %s, Expected avg: %d ms, Measured avg: %.2f ms", scope_name.c_str(), expected_ms, avg_ms);

            TEST_ASSERT_EQUAL(iterations, node->count);
            TEST_ASSERT_TRUE(avg_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(avg_ms <= (expected_ms + tolerance_ms));
        }
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler event timing consistency multiple events", "[utils][time_profiler][consistency][event]")
{
    BROOKESIA_LOGI("=== TimeProfiler Event Timing Consistency Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Different events use different time durations
    const std::vector<int> event_durations_ms = {12, 25, 50};
    const double tolerance_ms = 8.0;  // Allow 8ms tolerance
    const int iterations = 5;

    for (int i = 0; i < iterations; i++) {
        BROOKESIA_TIME_PROFILER_START_EVENT("consistency_event_1");
        simulate_work(event_durations_ms[0]);
        BROOKESIA_TIME_PROFILER_END_EVENT("consistency_event_1");

        BROOKESIA_TIME_PROFILER_START_EVENT("consistency_event_2");
        simulate_work(event_durations_ms[1]);
        BROOKESIA_TIME_PROFILER_END_EVENT("consistency_event_2");

        BROOKESIA_TIME_PROFILER_START_EVENT("consistency_event_3");
        simulate_work(event_durations_ms[2]);
        BROOKESIA_TIME_PROFILER_END_EVENT("consistency_event_3");
    }

    const auto stats = TimeProfiler::get_instance().get_statistics();

    // Check all three events with their respective expected durations
    for (int event_num = 1; event_num <= 3; event_num++) {
        std::string event_name = "consistency_event_" + std::to_string(event_num);
        int expected_ms = event_durations_ms[event_num - 1];
        const auto *node = find_statistics_node(stats.root_children, event_name);

        TEST_ASSERT_NOT_NULL(node);
        if (node) {
            // Convert to milliseconds if needed
            double avg_ms = node->avg;
            if (stats.unit_name == "s") {
                avg_ms = node->avg * 1000.0;
            } else if (stats.unit_name == "us") {
                avg_ms = node->avg / 1000.0;
            }

            BROOKESIA_LOGI("Event: %s, Expected avg: %d ms, Measured avg: %.2f ms", event_name.c_str(), expected_ms, avg_ms);

            TEST_ASSERT_EQUAL(iterations, node->count);
            TEST_ASSERT_TRUE(avg_ms >= (expected_ms - tolerance_ms));
            TEST_ASSERT_TRUE(avg_ms <= (expected_ms + tolerance_ms));
        }
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler same scope name statistics accumulation", "[utils][time_profiler][statistics][scope]")
{
    BROOKESIA_LOGI("=== TimeProfiler Same Scope Name Statistics Accumulation Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Use the same scope name multiple times with different durations
    const std::vector<int> durations_ms = {20, 30, 40, 25, 35};
    const std::string scope_name = "same_scope_name";
    const double tolerance_ms = 10.0;

    // Calculate expected values
    int expected_total_ms = 0;
    int expected_min_ms = durations_ms[0];
    int expected_max_ms = durations_ms[0];
    for (int duration : durations_ms) {
        expected_total_ms += duration;
        if (duration < expected_min_ms) {
            expected_min_ms = duration;
        }
        if (duration > expected_max_ms) {
            expected_max_ms = duration;
        }
    }
    double expected_avg_ms = static_cast<double>(expected_total_ms) / durations_ms.size();

    // Execute scopes with same name but different durations
    for (int duration : durations_ms) {
        {
            BROOKESIA_TIME_PROFILER_SCOPE(scope_name.c_str());
            simulate_work(duration);
        }
    }

    const auto stats = TimeProfiler::get_instance().get_statistics();
    const auto *node = find_statistics_node(stats.root_children, scope_name);

    TEST_ASSERT_NOT_NULL(node);
    if (node) {
        // Convert to milliseconds if needed
        double total_ms = node->total;
        double avg_ms = node->avg;
        double min_ms = node->min;
        double max_ms = node->max;

        if (stats.unit_name == "s") {
            total_ms = node->total * 1000.0;
            avg_ms = node->avg * 1000.0;
            min_ms = node->min * 1000.0;
            max_ms = node->max * 1000.0;
        } else if (stats.unit_name == "us") {
            total_ms = node->total / 1000.0;
            avg_ms = node->avg / 1000.0;
            min_ms = node->min / 1000.0;
            max_ms = node->max / 1000.0;
        }

        BROOKESIA_LOGI("Scope: %s", scope_name.c_str());
        BROOKESIA_LOGI("Expected: count=%zu, total=%d ms, avg=%.2f ms, min=%d ms, max=%d ms",
                       durations_ms.size(), expected_total_ms, expected_avg_ms, expected_min_ms, expected_max_ms);
        BROOKESIA_LOGI("Measured: count=%zu, total=%.2f ms, avg=%.2f ms, min=%.2f ms, max=%.2f ms",
                       node->count, total_ms, avg_ms, min_ms, max_ms);

        // Verify count
        TEST_ASSERT_EQUAL(durations_ms.size(), node->count);

        // Verify total (with tolerance)
        TEST_ASSERT_TRUE(total_ms >= (expected_total_ms - tolerance_ms * durations_ms.size()));
        TEST_ASSERT_TRUE(total_ms <= (expected_total_ms + tolerance_ms * durations_ms.size()));

        // Verify average (with tolerance)
        TEST_ASSERT_TRUE(avg_ms >= (expected_avg_ms - tolerance_ms));
        TEST_ASSERT_TRUE(avg_ms <= (expected_avg_ms + tolerance_ms));

        // Verify min (should be close to expected_min, but may be slightly less due to timing variations)
        TEST_ASSERT_TRUE(min_ms >= (expected_min_ms - tolerance_ms));
        TEST_ASSERT_TRUE(min_ms <= (expected_min_ms + tolerance_ms));

        // Verify max (should be close to expected_max, but may be slightly more due to timing variations)
        TEST_ASSERT_TRUE(max_ms >= (expected_max_ms - tolerance_ms));
        TEST_ASSERT_TRUE(max_ms <= (expected_max_ms + tolerance_ms));
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler same event name statistics accumulation", "[utils][time_profiler][statistics][event]")
{
    BROOKESIA_LOGI("=== TimeProfiler Same Event Name Statistics Accumulation Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Use the same event name multiple times with different durations
    const std::vector<int> durations_ms = {15, 25, 35, 20, 30};
    const std::string event_name = "same_event_name";
    const double tolerance_ms = 10.0;

    // Calculate expected values
    int expected_total_ms = 0;
    int expected_min_ms = durations_ms[0];
    int expected_max_ms = durations_ms[0];
    for (int duration : durations_ms) {
        expected_total_ms += duration;
        if (duration < expected_min_ms) {
            expected_min_ms = duration;
        }
        if (duration > expected_max_ms) {
            expected_max_ms = duration;
        }
    }
    double expected_avg_ms = static_cast<double>(expected_total_ms) / durations_ms.size();

    // Execute events with same name but different durations
    for (int duration : durations_ms) {
        BROOKESIA_TIME_PROFILER_START_EVENT(event_name.c_str());
        simulate_work(duration);
        BROOKESIA_TIME_PROFILER_END_EVENT(event_name.c_str());
    }

    const auto stats = TimeProfiler::get_instance().get_statistics();
    const auto *node = find_statistics_node(stats.root_children, event_name);

    TEST_ASSERT_NOT_NULL(node);
    if (node) {
        // Convert to milliseconds if needed
        double total_ms = node->total;
        double avg_ms = node->avg;
        double min_ms = node->min;
        double max_ms = node->max;

        if (stats.unit_name == "s") {
            total_ms = node->total * 1000.0;
            avg_ms = node->avg * 1000.0;
            min_ms = node->min * 1000.0;
            max_ms = node->max * 1000.0;
        } else if (stats.unit_name == "us") {
            total_ms = node->total / 1000.0;
            avg_ms = node->avg / 1000.0;
            min_ms = node->min / 1000.0;
            max_ms = node->max / 1000.0;
        }

        BROOKESIA_LOGI("Event: %s", event_name.c_str());
        BROOKESIA_LOGI("Expected: count=%zu, total=%d ms, avg=%.2f ms, min=%d ms, max=%d ms",
                       durations_ms.size(), expected_total_ms, expected_avg_ms, expected_min_ms, expected_max_ms);
        BROOKESIA_LOGI("Measured: count=%zu, total=%.2f ms, avg=%.2f ms, min=%.2f ms, max=%.2f ms",
                       node->count, total_ms, avg_ms, min_ms, max_ms);

        // Verify count
        TEST_ASSERT_EQUAL(durations_ms.size(), node->count);

        // Verify total (with tolerance)
        TEST_ASSERT_TRUE(total_ms >= (expected_total_ms - tolerance_ms * durations_ms.size()));
        TEST_ASSERT_TRUE(total_ms <= (expected_total_ms + tolerance_ms * durations_ms.size()));

        // Verify average (with tolerance)
        TEST_ASSERT_TRUE(avg_ms >= (expected_avg_ms - tolerance_ms));
        TEST_ASSERT_TRUE(avg_ms <= (expected_avg_ms + tolerance_ms));

        // Verify min (should be close to expected_min, but may be slightly less due to timing variations)
        TEST_ASSERT_TRUE(min_ms >= (expected_min_ms - tolerance_ms));
        TEST_ASSERT_TRUE(min_ms <= (expected_min_ms + tolerance_ms));

        // Verify max (should be close to expected_max, but may be slightly more due to timing variations)
        TEST_ASSERT_TRUE(max_ms >= (expected_max_ms - tolerance_ms));
        TEST_ASSERT_TRUE(max_ms <= (expected_max_ms + tolerance_ms));
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test TimeProfiler same scope name with same duration statistics", "[utils][time_profiler][statistics][scope]")
{
    BROOKESIA_LOGI("=== TimeProfiler Same Scope Name With Same Duration Statistics Test ===");

    BROOKESIA_TIME_PROFILER_CLEAR();

    // Use the same scope name multiple times with the same duration
    const int duration_ms = 30;
    const int iterations = 8;
    const std::string scope_name = "same_duration_scope";
    const double tolerance_ms = 10.0;

    // Calculate expected values
    int expected_total_ms = duration_ms * iterations;
    double expected_avg_ms = static_cast<double>(duration_ms);
    int expected_min_ms = duration_ms;
    int expected_max_ms = duration_ms;

    // Execute scopes with same name and same duration
    for (int i = 0; i < iterations; i++) {
        {
            BROOKESIA_TIME_PROFILER_SCOPE(scope_name.c_str());
            simulate_work(duration_ms);
        }
    }

    const auto stats = TimeProfiler::get_instance().get_statistics();
    const auto *node = find_statistics_node(stats.root_children, scope_name);

    TEST_ASSERT_NOT_NULL(node);
    if (node) {
        // Convert to milliseconds if needed
        double total_ms = node->total;
        double avg_ms = node->avg;
        double min_ms = node->min;
        double max_ms = node->max;

        if (stats.unit_name == "s") {
            total_ms = node->total * 1000.0;
            avg_ms = node->avg * 1000.0;
            min_ms = node->min * 1000.0;
            max_ms = node->max * 1000.0;
        } else if (stats.unit_name == "us") {
            total_ms = node->total / 1000.0;
            avg_ms = node->avg / 1000.0;
            min_ms = node->min / 1000.0;
            max_ms = node->max / 1000.0;
        }

        BROOKESIA_LOGI("Scope: %s", scope_name.c_str());
        BROOKESIA_LOGI("Expected: count=%d, total=%d ms, avg=%.2f ms, min=%d ms, max=%d ms",
                       iterations, expected_total_ms, expected_avg_ms, expected_min_ms, expected_max_ms);
        BROOKESIA_LOGI("Measured: count=%zu, total=%.2f ms, avg=%.2f ms, min=%.2f ms, max=%.2f ms",
                       node->count, total_ms, avg_ms, min_ms, max_ms);

        // Verify count
        TEST_ASSERT_EQUAL(iterations, node->count);

        // Verify total (with tolerance)
        TEST_ASSERT_TRUE(total_ms >= (expected_total_ms - tolerance_ms * iterations));
        TEST_ASSERT_TRUE(total_ms <= (expected_total_ms + tolerance_ms * iterations));

        // Verify average (with tolerance)
        TEST_ASSERT_TRUE(avg_ms >= (expected_avg_ms - tolerance_ms));
        TEST_ASSERT_TRUE(avg_ms <= (expected_avg_ms + tolerance_ms));

        // Verify min and max should be close to duration_ms
        TEST_ASSERT_TRUE(min_ms >= (expected_min_ms - tolerance_ms));
        TEST_ASSERT_TRUE(min_ms <= (expected_min_ms + tolerance_ms));
        TEST_ASSERT_TRUE(max_ms >= (expected_max_ms - tolerance_ms));
        TEST_ASSERT_TRUE(max_ms <= (expected_max_ms + tolerance_ms));
    }

    BROOKESIA_TIME_PROFILER_CLEAR();
}
