/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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
