/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include <future>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "unity.h"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/time_profiler.hpp"

using namespace esp_brookesia::lib_utils;

#define TEST_SCHEDULER_CONFIG_GENERIC
#define TEST_SCHEDULER_CONFIG_TWO_THREADS esp_brookesia::lib_utils::TaskScheduler::StartConfig{ \
    .worker_configs = {                                                                    \
        {.name = "TS_Worker1", .core_id = 0, .stack_size = 8192},                            \
        {.name = "TS_Worker2", .core_id = 1, .stack_size = 8192},                            \
    },                                                                                        \
    .worker_poll_interval_ms = 1                                                                  \
}
#define TEST_SCHEDULER_CONFIG_FOUR_THREADS esp_brookesia::lib_utils::TaskScheduler::StartConfig{ \
    .worker_configs = {                                                                    \
        {.name = "TS_Worker1", .core_id = 0, .stack_size = 8192},                            \
        {.name = "TS_Worker2", .core_id = 1, .stack_size = 8192},                            \
        {.name = "TS_Worker3", .core_id = 0, .stack_size = 8192},                            \
        {.name = "TS_Worker4", .core_id = 1, .stack_size = 8192},                            \
    },                                                                                        \
    .worker_poll_interval_ms = 1                                                                  \
}

// Global variables used for testing
static std::atomic<int> g_counter{0};
static std::atomic<int> g_callback_counter{0};
static std::atomic<bool> g_task_executed{false};

// Helper functions
static void reset_counters()
{
    g_counter = 0;
    g_callback_counter = 0;
    g_task_executed = false;
}

// ============================================================================
// Periodic task tests
// ============================================================================

TEST_CASE("Test post periodic task", "[utils][task_scheduler][periodic][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Post Periodic Task Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    std::atomic<int> periodic_counter{0};
    TaskScheduler::TaskId task_id = 0;
    scheduler.post_periodic([&periodic_counter]() -> bool {
        periodic_counter++;
        BROOKESIA_LOGI("Periodic task executed, count = %1%", periodic_counter.load());
        return periodic_counter < 5; // Stop after executing 5 times
    }, 100, &task_id);

    // Use wait instead of delay
    bool completed = scheduler.wait(task_id, 1000);
    TEST_ASSERT_TRUE(completed);

    BROOKESIA_LOGI("Final periodic counter: %1%", periodic_counter.load());
    TEST_ASSERT_EQUAL(5, periodic_counter.load());

    scheduler.stop();
}

TEST_CASE("Test periodic task early stop", "[utils][task_scheduler][periodic][early_stop]")
{
    BROOKESIA_LOGI("=== TaskScheduler Periodic Task Early Stop Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    std::atomic<int> periodic_counter{0};
    TaskScheduler::TaskId task_id = 0;
    scheduler.post_periodic([&periodic_counter]() -> bool {
        periodic_counter++;
        BROOKESIA_LOGI("Periodic task executed, count = %1%", periodic_counter.load());
        return periodic_counter < 3; // Stop after executing 3 times
    }, 100, &task_id);

    // Use wait instead of delay
    bool completed = scheduler.wait(task_id, 1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(3, periodic_counter.load());

    scheduler.stop();
}

TEST_CASE("Test multiple schedulers - periodic tasks", "[utils][task_scheduler][multiple][periodic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Periodic Tasks Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler1.post_periodic([&counter1]() -> bool {
        counter1++;
        return counter1 < 3;
    }, 100);

    scheduler2.post_periodic([&counter2]() -> bool {
        counter2++;
        return counter2 < 5;
    }, 100);

    vTaskDelay(pdMS_TO_TICKS(600));

    BROOKESIA_LOGI("Counter1: %1%, Counter2: %2%", counter1.load(), counter2.load());
    TEST_ASSERT_EQUAL(3, counter1.load());
    TEST_ASSERT_EQUAL(5, counter2.load());

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test suspend and resume periodic task", "[utils][task_scheduler][suspend][periodic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Suspend/Resume Periodic Task Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    std::atomic<int> periodic_counter{0};
    TaskScheduler::TaskId task_id = 0;
    BROOKESIA_TIME_PROFILER_START_EVENT("total_periodic_test");

    scheduler.post_periodic([&periodic_counter]() -> bool {
        periodic_counter++;
        BROOKESIA_LOGI("Periodic task executed, count = %1%", periodic_counter.load());
        return periodic_counter < 10;
    }, 100, &task_id);

    // Wait for a few executions
    BROOKESIA_TIME_PROFILER_START_EVENT("periodic_before_suspend");
    vTaskDelay(pdMS_TO_TICKS(250));
    BROOKESIA_TIME_PROFILER_END_EVENT("periodic_before_suspend");

    int count_before_suspend = periodic_counter.load();
    BROOKESIA_LOGI("Count before suspend: %1%", count_before_suspend);
    TEST_ASSERT_GREATER_THAN(0, count_before_suspend);

    // Suspend
    bool suspended = scheduler.suspend(task_id);
    TEST_ASSERT_TRUE(suspended);

    // During suspension, should not increase
    BROOKESIA_TIME_PROFILER_START_EVENT("periodic_suspended");
    vTaskDelay(pdMS_TO_TICKS(300));
    BROOKESIA_TIME_PROFILER_END_EVENT("periodic_suspended");

    int count_during_suspend = periodic_counter.load();
    BROOKESIA_LOGI("Count during suspend: %1%", count_during_suspend);
    TEST_ASSERT_EQUAL(count_before_suspend, count_during_suspend);

    // Resume
    bool resumed = scheduler.resume(task_id);
    TEST_ASSERT_TRUE(resumed);

    // After resume, continue execution
    BROOKESIA_TIME_PROFILER_START_EVENT("periodic_after_resume");
    vTaskDelay(pdMS_TO_TICKS(300));
    BROOKESIA_TIME_PROFILER_END_EVENT("periodic_after_resume");
    BROOKESIA_TIME_PROFILER_END_EVENT("total_periodic_test");

    int count_after_resume = periodic_counter.load();
    BROOKESIA_LOGI("Count after resume: %1%", count_after_resume);
    TEST_ASSERT_GREATER_THAN(count_during_suspend, count_after_resume);

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test strand with periodic tasks", "[utils][task_scheduler][strand][periodic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Strand with Periodic Tasks Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    // Configure strand group
    TaskScheduler::GroupConfig config;
    config.enable_serial_execution = true;
    scheduler.configure_group("periodic_strand", config);

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    // Submit two periodic tasks to strand group
    scheduler.post_periodic([&counter1]() -> bool {
        counter1++;
        vTaskDelay(pdMS_TO_TICKS(20));
        return counter1 < 3;
    }, 50, nullptr, "periodic_strand");

    scheduler.post_periodic([&counter2]() -> bool {
        counter2++;
        vTaskDelay(pdMS_TO_TICKS(20));
        return counter2 < 3;
    }, 50, nullptr, "periodic_strand");

    vTaskDelay(pdMS_TO_TICKS(500));

    BROOKESIA_LOGI("Counter1: %1%, Counter2: %2%", counter1.load(), counter2.load());
    TEST_ASSERT_EQUAL(3, counter1.load());
    TEST_ASSERT_EQUAL(3, counter2.load());

    scheduler.stop();
}

TEST_CASE("Test periodic task is_executing prevents parallel execution without strand", "[utils][task_scheduler][periodic][is_executing][parallel]")
{
    BROOKESIA_LOGI("=== TaskScheduler Periodic Task is_executing Test (No Strand) ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    // Do NOT configure strand group - test is_executing mechanism alone
    // This ensures that even without strand, periodic tasks won't run in parallel

    std::atomic<int> periodic_counter{0};
    std::atomic<int> concurrent_count{0};
    std::atomic<int> max_concurrent{0};

    // Submit a periodic task with short interval (50ms) but long execution time (100ms)
    // This creates a scenario where the timer might fire again before the previous execution finishes
    // Without is_executing protection, we would see concurrent executions
    TaskScheduler::TaskId task_id = 0;
    scheduler.post_periodic([&periodic_counter, &concurrent_count, &max_concurrent]() -> bool {
        // Track concurrent execution
        int current = concurrent_count.fetch_add(1) + 1;
        int max = max_concurrent.load();
        while (current > max && !max_concurrent.compare_exchange_weak(max, current))
        {
            max = max_concurrent.load();
        }

        periodic_counter++;
        BROOKESIA_LOGI("Periodic task executed, count = %1%, concurrent = %2%",
                       periodic_counter.load(), current);

        // Simulate long execution time (100ms) - longer than interval (50ms)
        // This ensures timer fires again before this execution finishes
        vTaskDelay(pdMS_TO_TICKS(100));

        concurrent_count.fetch_sub(1);
        return periodic_counter < 10; // Stop after executing 10 times
    }, 50, &task_id);

    // Wait for task to complete
    bool completed = scheduler.wait(task_id, 5000);
    TEST_ASSERT_TRUE(completed);

    BROOKESIA_LOGI("Final periodic counter: %1%", periodic_counter.load());
    BROOKESIA_LOGI("Max concurrent executions: %1%", max_concurrent.load());

    // Verify: max_concurrent should be 1 (only one instance executing at a time)
    // This proves that is_executing flag prevents parallel execution
    TEST_ASSERT_EQUAL(1, max_concurrent.load());
    TEST_ASSERT_EQUAL(10, periodic_counter.load());

    scheduler.stop();
}
