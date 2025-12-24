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

static void simple_task()
{
    g_counter++;
    BROOKESIA_LOGI("Simple task executed, counter = %1%", g_counter.load());
}

// ============================================================================
// Basic functionality tests
// ============================================================================

TEST_CASE("Test basic start and stop", "[utils][task_scheduler][basic][start_stop]")
{
    BROOKESIA_LOGI("=== TaskScheduler Basic Start/Stop Test ===");

    TaskScheduler scheduler;

    // Start scheduler
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);
    TEST_ASSERT_TRUE(scheduler.is_running());

    vTaskDelay(pdMS_TO_TICKS(100));

    // Stop scheduler
    scheduler.stop();
    TEST_ASSERT_FALSE(scheduler.is_running());
}

TEST_CASE("Test post immediate task", "[utils][task_scheduler][basic][post]")
{
    BROOKESIA_LOGI("=== TaskScheduler Post Immediate Task Test ===");

    reset_counters();
    {
        TaskScheduler scheduler;
        scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

        TaskScheduler::TaskId task_id = 0;
        bool result = scheduler.post(simple_task, &task_id);
        TEST_ASSERT_TRUE(result);
        BROOKESIA_LOGI("Posted task with id: %1%", task_id);
        TEST_ASSERT_TRUE(task_id > 0);

        // Use wait instead of delay
        bool completed = scheduler.wait(task_id, 1000);
        TEST_ASSERT_TRUE(completed);
        TEST_ASSERT_EQUAL(1, g_counter.load());

        scheduler.stop();
    }

    // Wait for resources to be released
    vTaskDelay(pdMS_TO_TICKS(50));
}

TEST_CASE("Test post multiple tasks", "[utils][task_scheduler][basic][multiple]")
{
    BROOKESIA_LOGI("=== TaskScheduler Post Multiple Tasks Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    const int task_count = 10;
    for (int i = 0; i < task_count; i++) {
        scheduler.post(simple_task);
    }

    // Use wait_all instead of delay
    bool completed = scheduler.wait_all(1000);
    TEST_ASSERT_TRUE(completed);

    BROOKESIA_LOGI("Counter value: %1%", g_counter.load());
    TEST_ASSERT_EQUAL(task_count, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// Delayed task tests
// ============================================================================

TEST_CASE("Test post delayed task", "[utils][task_scheduler][delayed][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Post Delayed Task Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    auto start = esp_timer_get_time();
    BROOKESIA_TIME_PROFILER_START_EVENT("total_delay");

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed(simple_task, 200, &task_id);

    BROOKESIA_TIME_PROFILER_START_EVENT("wait_before_execution");
    vTaskDelay(pdMS_TO_TICKS(100));
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_before_execution");
    TEST_ASSERT_EQUAL(0, g_counter.load()); // Not executed yet

    // Use wait instead of delay
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_for_execution");
    bool completed = scheduler.wait(task_id, 1000);
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_for_execution");
    BROOKESIA_TIME_PROFILER_END_EVENT("total_delay");

    TEST_ASSERT_TRUE(completed);
    auto elapsed = (esp_timer_get_time() - start) / 1000;
    BROOKESIA_LOGI("Elapsed time: %1% ms", elapsed);
    TEST_ASSERT_EQUAL(1, g_counter.load()); // Executed

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test post multiple delayed tasks", "[utils][task_scheduler][delayed][multiple]")
{
    BROOKESIA_LOGI("=== TaskScheduler Post Multiple Delayed Tasks Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler.post_delayed(simple_task, 100);
    scheduler.post_delayed(simple_task, 200);
    scheduler.post_delayed(simple_task, 300);

    vTaskDelay(pdMS_TO_TICKS(150));
    TEST_ASSERT_EQUAL(1, g_counter.load());

    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(2, g_counter.load());

    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(3, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// Batch task tests
// ============================================================================

TEST_CASE("Test post batch", "[utils][task_scheduler][batch][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Post Batch Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    std::vector<TaskScheduler::OnceTask> tasks;
    for (int i = 0; i < 5; i++) {
        tasks.push_back(simple_task);
    }

    std::vector<TaskScheduler::TaskId> ids;
    bool result = scheduler.post_batch(std::move(tasks), &ids);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL(5, ids.size());

    // Use wait_all instead of delay
    bool completed = scheduler.wait_all(1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(5, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// Task cancellation tests
// ============================================================================

TEST_CASE("Test cancel task", "[utils][task_scheduler][cancel][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Cancel Task Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed(simple_task, 300, &task_id);

    vTaskDelay(pdMS_TO_TICKS(100));

    scheduler.cancel(task_id);

    vTaskDelay(pdMS_TO_TICKS(300));

    TEST_ASSERT_EQUAL(0, g_counter.load()); // Task was cancelled, not executed

    scheduler.stop();
}

TEST_CASE("Test cancel all tasks", "[utils][task_scheduler][cancel][all]")
{
    BROOKESIA_LOGI("=== TaskScheduler Cancel All Tasks Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    for (int i = 0; i < 5; i++) {
        scheduler.post_delayed(simple_task, 300);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    scheduler.cancel_all();

    vTaskDelay(pdMS_TO_TICKS(300));

    TEST_ASSERT_EQUAL(0, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// Task state tests
// ============================================================================

TEST_CASE("Test get task state", "[utils][task_scheduler][state][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Get Task State Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed(simple_task, 200, &task_id);

    auto state1 = scheduler.get_state(task_id);
    BROOKESIA_LOGI("Initial state: %1%", BROOKESIA_DESCRIBE_TO_STR(state1));
    TEST_ASSERT_EQUAL(static_cast<int>(TaskScheduler::TaskState::Running), static_cast<int>(state1));

    vTaskDelay(pdMS_TO_TICKS(300));

    auto state2 = scheduler.get_state(task_id);
    BROOKESIA_LOGI("Final state: %1%", BROOKESIA_DESCRIBE_TO_STR(state2));
    TEST_ASSERT_EQUAL(static_cast<int>(TaskScheduler::TaskState::Finished), static_cast<int>(state2));

    scheduler.stop();
}

// ============================================================================
// Statistics tests
// ============================================================================

TEST_CASE("Test statistics", "[utils][task_scheduler][statistics][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Statistics Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler.post(simple_task);
    scheduler.post(simple_task);
    scheduler.post(simple_task);

    // Use wait_all instead of delay
    bool completed = scheduler.wait_all(1000);
    TEST_ASSERT_TRUE(completed);

    auto stats = scheduler.get_statistics();
    BROOKESIA_LOGI("Statistics: %1%", BROOKESIA_DESCRIBE_TO_STR(stats));
    TEST_ASSERT_EQUAL(3, stats.total_tasks);
    TEST_ASSERT_EQUAL(3, stats.completed_tasks);
    TEST_ASSERT_EQUAL(0, stats.failed_tasks);
    TEST_ASSERT_EQUAL(0, stats.canceled_tasks);

    scheduler.stop();
}

TEST_CASE("Test reset statistics", "[utils][task_scheduler][statistics][reset]")
{
    BROOKESIA_LOGI("=== TaskScheduler Reset Statistics Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post(simple_task, &task_id);

    // Use wait instead of delay
    bool completed = scheduler.wait(task_id, 1000);
    TEST_ASSERT_TRUE(completed);

    auto stats1 = scheduler.get_statistics();
    TEST_ASSERT_EQUAL(1, stats1.total_tasks);

    scheduler.reset_statistics();

    auto stats2 = scheduler.get_statistics();
    TEST_ASSERT_EQUAL(0, stats2.total_tasks);

    scheduler.stop();
}

// ============================================================================
// Thread config tests
// ============================================================================

TEST_CASE("Test with thread config", "[utils][task_scheduler][thread_config][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler with ThreadConfig Test ===");

    reset_counters();
    TaskScheduler scheduler;

    scheduler.start({
        .worker_configs = {{
                .name = "TaskWorker",
                .priority = 10,
                .stack_size = 8192
            }
        },
        .worker_poll_interval_ms = 1
    });
    TEST_ASSERT_TRUE(scheduler.is_running());

    TaskScheduler::TaskId task_id = 0;
    scheduler.post(simple_task, &task_id);

    // Use wait instead of delay
    bool completed = scheduler.wait(task_id, 1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// wait_all tests
// ============================================================================

TEST_CASE("Test wait all", "[utils][task_scheduler][wait][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Wait All Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    for (int i = 0; i < 3; i++) {
        scheduler.post([i]() {
            vTaskDelay(pdMS_TO_TICKS(100));
            g_counter++;
        });
    }

    bool completed = scheduler.wait_all(1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(3, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test wait all timeout", "[utils][task_scheduler][wait][timeout]")
{
    BROOKESIA_LOGI("=== TaskScheduler Wait All Timeout Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler.post_delayed(simple_task, 500);

    bool completed = scheduler.wait_all(200);
    TEST_ASSERT_FALSE(completed); // Timeout

    scheduler.stop();
}

// ============================================================================
// Exception handling tests
// ============================================================================

TEST_CASE("Test task exception handling", "[utils][task_scheduler][exception][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Task Exception Handling Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler.post(
    []() {
        throw std::runtime_error("Test exception");
    }
    );

    vTaskDelay(pdMS_TO_TICKS(200));

    auto stats = scheduler.get_statistics();
    TEST_ASSERT_EQUAL(1, stats.failed_tasks);

    scheduler.stop();
}

// ============================================================================
// Multithreaded concurrent tests
// ============================================================================

TEST_CASE("Test concurrent tasks", "[utils][task_scheduler][concurrent][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Concurrent Tasks Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    const int task_count = 4;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < task_count; i++) {
        futures.push_back(std::async(std::launch::async, [&scheduler]() {
            scheduler.post([]() {
                vTaskDelay(pdMS_TO_TICKS(10));
                g_counter++;
            });
        }));
    }

    // Wait for all async tasks to complete
    for (auto &f : futures) {
        f.get();
    }

    bool completed = scheduler.wait_all(1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(task_count, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// Stress tests
// ============================================================================

TEST_CASE("Test stress - many tasks", "[utils][task_scheduler][stress][many]")
{
    BROOKESIA_LOGI("=== TaskScheduler Stress Test - Many Tasks ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    const int task_count = 50;
    for (int i = 0; i < task_count; i++) {
        scheduler.post(simple_task);
    }

    // Use wait_all instead of delay
    bool completed = scheduler.wait_all(2000);
    TEST_ASSERT_TRUE(completed);

    BROOKESIA_LOGI("Final counter: %1%", g_counter.load());
    TEST_ASSERT_EQUAL(task_count, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test stress - rapid start stop", "[utils][task_scheduler][stress][start_stop]")
{
    BROOKESIA_LOGI("=== TaskScheduler Stress Test - Rapid Start/Stop ===");

    TaskScheduler scheduler;

    for (int i = 0; i < 5; i++) {
        scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);
        vTaskDelay(pdMS_TO_TICKS(50));
        scheduler.stop();
    }

    TEST_ASSERT_FALSE(scheduler.is_running());
}

// ============================================================================
// Performance tests
// ============================================================================

TEST_CASE("Test performance", "[utils][task_scheduler][performance][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Performance Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    const int task_count = 100;
    {
        auto post_function = [&scheduler]() {
            simple_task();
            BROOKESIA_TIME_PROFILER_END_EVENT("single_task");
        };
        BROOKESIA_TIME_PROFILER_SCOPE("post_all");
        for (int i = 0; i < task_count; i++) {
            BROOKESIA_TIME_PROFILER_START_EVENT("single_task");
            scheduler.post(post_function);
        }
    }

    bool completed = false;
    {
        BROOKESIA_TIME_PROFILER_SCOPE("wait_all");
        completed = scheduler.wait_all(2000);
    }
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(task_count, g_counter.load());

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();

    scheduler.stop();
}

// ============================================================================
// Multiple schedulers coexistence tests
// ============================================================================

TEST_CASE("Test multiple schedulers - basic", "[utils][task_scheduler][multiple][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Basic Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};
    std::atomic<int> counter3{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;
    TaskScheduler scheduler3;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler3.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TEST_ASSERT_TRUE(scheduler1.is_running());
    TEST_ASSERT_TRUE(scheduler2.is_running());
    TEST_ASSERT_TRUE(scheduler3.is_running());

    scheduler1.post([&counter1]() {
        counter1++;
    });
    scheduler2.post([&counter2]() {
        counter2++;
    });
    scheduler3.post([&counter3]() {
        counter3++;
    });

    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(1, counter1.load());
    TEST_ASSERT_EQUAL(1, counter2.load());
    TEST_ASSERT_EQUAL(1, counter3.load());

    scheduler1.stop();
    scheduler2.stop();
    scheduler3.stop();

    TEST_ASSERT_FALSE(scheduler1.is_running());
    TEST_ASSERT_FALSE(scheduler2.is_running());
    TEST_ASSERT_FALSE(scheduler3.is_running());
}

TEST_CASE("Test multiple schedulers - concurrent tasks", "[utils][task_scheduler][multiple][concurrent]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Concurrent Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

    const int tasks_per_scheduler = 4;
    std::vector<std::future<void>> futures;

    for (int i = 0; i < tasks_per_scheduler; i++) {
        futures.push_back(std::async(std::launch::async, [&counter1, &scheduler1]() {
            scheduler1.post([&counter1]() {
                vTaskDelay(pdMS_TO_TICKS(10));
                counter1++;
            });
        }));
        futures.push_back(std::async(std::launch::async, [&counter2, &scheduler2]() {
            scheduler2.post([&counter2]() {
                vTaskDelay(pdMS_TO_TICKS(10));
                counter2++;
            });
        }));
    }

    // Wait for all async tasks to complete
    for (auto &f : futures) {
        f.get();
    }

    bool completed1 = scheduler1.wait_all(1000);
    TEST_ASSERT_TRUE(completed1);
    TEST_ASSERT_EQUAL(tasks_per_scheduler, counter1.load());
    bool completed2 = scheduler2.wait_all(1000);
    TEST_ASSERT_TRUE(completed2);
    TEST_ASSERT_EQUAL(tasks_per_scheduler, counter2.load());

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test multiple schedulers - different thread counts", "[utils][task_scheduler][multiple][threads]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Different Thread Counts Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};
    std::atomic<int> counter3{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;
    TaskScheduler scheduler3;

    scheduler1.start(); // 1 thread
    scheduler2.start(TEST_SCHEDULER_CONFIG_TWO_THREADS); // 2 threads
    scheduler3.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS); // 4 threads

    const int task_count = 20;

    for (int i = 0; i < task_count; i++) {
        scheduler1.post([&counter1]() {
            counter1++;
        });
        scheduler2.post([&counter2]() {
            counter2++;
        });
        scheduler3.post([&counter3]() {
            counter3++;
        });
    }

    vTaskDelay(pdMS_TO_TICKS(500));

    BROOKESIA_LOGI("Counter1: %1%, Counter2: %2%, Counter3: %3%",
                   counter1.load(), counter2.load(), counter3.load());
    TEST_ASSERT_EQUAL(task_count, counter1.load());
    TEST_ASSERT_EQUAL(task_count, counter2.load());
    TEST_ASSERT_EQUAL(task_count, counter3.load());

    scheduler1.stop();
    scheduler2.stop();
    scheduler3.stop();
}

TEST_CASE("Test multiple schedulers - delayed tasks", "[utils][task_scheduler][multiple][delayed]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Delayed Tasks Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler1.post_delayed([&counter1]() {
        counter1++;
    }, 100);
    scheduler2.post_delayed([&counter2]() {
        counter2++;
    }, 200);

    vTaskDelay(pdMS_TO_TICKS(150));
    TEST_ASSERT_EQUAL(1, counter1.load());
    TEST_ASSERT_EQUAL(0, counter2.load());

    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(1, counter1.load());
    TEST_ASSERT_EQUAL(1, counter2.load());

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test multiple schedulers - independent cancellation", "[utils][task_scheduler][multiple][cancel]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Independent Cancellation Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task1 = 0;
    scheduler1.post_delayed([&counter1]() {
        counter1++;
    }, 300, &task1);
    scheduler2.post_delayed([&counter2]() {
        counter2++;
    }, 300);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Only cancel scheduler1's task
    scheduler1.cancel(task1);

    vTaskDelay(pdMS_TO_TICKS(300));

    TEST_ASSERT_EQUAL(0, counter1.load()); // scheduler1's task is cancelled
    TEST_ASSERT_EQUAL(1, counter2.load()); // scheduler2's task is executed normally

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test multiple schedulers - independent groups", "[utils][task_scheduler][multiple][groups]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Independent Groups Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Two schedulers use the same group name, but should be independent
    scheduler1.post_delayed([&counter1]() {
        counter1++;
    }, 300, nullptr, "group1");
    scheduler1.post_delayed([&counter1]() {
        counter1++;
    }, 300, nullptr, "group1");
    scheduler2.post_delayed([&counter2]() {
        counter2++;
    }, 300, nullptr, "group1");
    scheduler2.post_delayed([&counter2]() {
        counter2++;
    }, 300, nullptr, "group1");

    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(2, scheduler1.get_group_task_count("group1"));
    TEST_ASSERT_EQUAL(2, scheduler2.get_group_task_count("group1"));

    // Only cancel scheduler1's group1
    scheduler1.cancel_group("group1");

    vTaskDelay(pdMS_TO_TICKS(300));

    TEST_ASSERT_EQUAL(0, counter1.load()); // scheduler1's group is cancelled
    TEST_ASSERT_EQUAL(2, counter2.load()); // scheduler2's group is executed normally

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test multiple schedulers - independent statistics", "[utils][task_scheduler][multiple][statistics]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Independent Statistics Test ===");

    reset_counters();

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // scheduler1 executes 3 tasks
    scheduler1.post(simple_task);
    scheduler1.post(simple_task);
    scheduler1.post(simple_task);

    // scheduler2 executes 5 tasks
    for (int i = 0; i < 5; i++) {
        scheduler2.post(simple_task);
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    auto stats1 = scheduler1.get_statistics();
    auto stats2 = scheduler2.get_statistics();

    BROOKESIA_LOGI("Scheduler1: %1%", BROOKESIA_DESCRIBE_TO_STR(stats1));
    BROOKESIA_LOGI("Scheduler2: %1%", BROOKESIA_DESCRIBE_TO_STR(stats2));

    TEST_ASSERT_EQUAL(3, stats1.total_tasks);
    TEST_ASSERT_EQUAL(3, stats1.completed_tasks);
    TEST_ASSERT_EQUAL(5, stats2.total_tasks);
    TEST_ASSERT_EQUAL(5, stats2.completed_tasks);

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test multiple schedulers - sequential start stop", "[utils][task_scheduler][multiple][sequential]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Sequential Start/Stop Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    // Start scheduler1 first
    scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler1.post([&counter1]() {
        counter1++;
    });
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(1, counter1.load());
    scheduler1.stop();

    // Start scheduler2 second
    scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);
    scheduler2.post([&counter2]() {
        counter2++;
    });
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(1, counter2.load());
    scheduler2.stop();

    TEST_ASSERT_FALSE(scheduler1.is_running());
    TEST_ASSERT_FALSE(scheduler2.is_running());
}

TEST_CASE("Test multiple schedulers - different thread configs", "[utils][task_scheduler][multiple][thread_config]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Different ThreadConfigs Test ===");

    reset_counters();

    std::atomic<int> counter1{0};
    std::atomic<int> counter2{0};

    TaskScheduler scheduler1;
    TaskScheduler scheduler2;

    scheduler1.start({
        .worker_configs = {{
                .name = "Worker1",
                .priority = 5,
                .stack_size = 4096
            }
        },
        .worker_poll_interval_ms = 1
    });
    scheduler2.start({
        .worker_configs = {{
                .name = "Worker2",
                .priority = 10,
                .stack_size = 8192
            }
        },
        .worker_poll_interval_ms = 1
    });

    scheduler1.post([&counter1]() {
        counter1++;
    });
    scheduler2.post([&counter2]() {
        counter2++;
    });

    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(1, counter1.load());
    TEST_ASSERT_EQUAL(1, counter2.load());

    scheduler1.stop();
    scheduler2.stop();
}

TEST_CASE("Test multiple schedulers - stress test", "[utils][task_scheduler][multiple][stress]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Stress Test ===");

    reset_counters();

    const int scheduler_count = 3;
    const int tasks_per_scheduler = 20;

    std::vector<std::unique_ptr<TaskScheduler>> schedulers;
    std::vector<std::atomic<int>> counters(scheduler_count);

    for (int i = 0; i < scheduler_count; i++) {
        schedulers.push_back(std::make_unique<TaskScheduler>());
        schedulers[i]->start(TEST_SCHEDULER_CONFIG_GENERIC);
        counters[i] = 0;
    }

    for (int i = 0; i < scheduler_count; i++) {
        for (int j = 0; j < tasks_per_scheduler; j++) {
            schedulers[i]->post([&counters, i]() {
                vTaskDelay(pdMS_TO_TICKS(5));
                counters[i]++;
            });
        }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int i = 0; i < scheduler_count; i++) {
        BROOKESIA_LOGI("Scheduler %1% counter: %2%", i, counters[i].load());
        TEST_ASSERT_EQUAL(tasks_per_scheduler, counters[i].load());
    }

    for (int i = 0; i < scheduler_count; i++) {
        schedulers[i]->stop();
    }
}

TEST_CASE("Test multiple schedulers - memory isolation", "[utils][task_scheduler][multiple][memory]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Schedulers Memory Isolation Test ===");

    reset_counters();

    {
        TaskScheduler scheduler1;
        scheduler1.start(TEST_SCHEDULER_CONFIG_GENERIC);

        for (int i = 0; i < 10; i++) {
            scheduler1.post(simple_task);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
        scheduler1.stop();
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    int counter_after_first = g_counter.load();
    TEST_ASSERT_EQUAL(10, counter_after_first);

    {
        TaskScheduler scheduler2;
        scheduler2.start(TEST_SCHEDULER_CONFIG_GENERIC);

        for (int i = 0; i < 5; i++) {
            scheduler2.post(simple_task);
        }

        vTaskDelay(pdMS_TO_TICKS(200));
        scheduler2.stop();
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL(15, g_counter.load());
}

// ============================================================================
// New interface tests - wait series
// ============================================================================

TEST_CASE("Test wait single task", "[utils][task_scheduler][wait][single]")
{
    BROOKESIA_LOGI("=== TaskScheduler Wait Single Task Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed([]() {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_counter++;
    }, 50, &task_id);

    // Wait for task to complete
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_for_task");
    bool completed = scheduler.wait(task_id, 500);
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_for_task");

    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test wait single task timeout", "[utils][task_scheduler][wait][single_timeout]")
{
    BROOKESIA_LOGI("=== TaskScheduler Wait Single Task Timeout Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed([]() {
        vTaskDelay(pdMS_TO_TICKS(300));
        g_counter++;
    }, 50, &task_id);

    // Wait for timeout
    bool completed = scheduler.wait(task_id, 100);
    TEST_ASSERT_FALSE(completed);

    scheduler.stop();
}

TEST_CASE("Test wait group", "[utils][task_scheduler][wait][group]")
{
    BROOKESIA_LOGI("=== TaskScheduler Wait Group Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Submit multiple tasks to the same group
    for (int i = 0; i < 3; i++) {
        scheduler.post([i]() {
            vTaskDelay(pdMS_TO_TICKS(50));
            g_counter++;
        }, nullptr, "test_group");
    }

    // Wait for all tasks in the group to complete
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_for_group");
    bool completed = scheduler.wait_group("test_group", 500);
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_for_group");

    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(3, g_counter.load());

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test wait group timeout", "[utils][task_scheduler][wait][group_timeout]")
{
    BROOKESIA_LOGI("=== TaskScheduler Wait Group Timeout Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    for (int i = 0; i < 3; i++) {
        scheduler.post([i]() {
            vTaskDelay(pdMS_TO_TICKS(200));
            g_counter++;
        }, nullptr, "test_group");
    }

    // Wait for timeout
    bool completed = scheduler.wait_group("test_group", 100);
    TEST_ASSERT_FALSE(completed);

    scheduler.stop();
}

// ============================================================================
// New interface tests - suspend/resume series
// ============================================================================

TEST_CASE("Test suspend and resume delayed task", "[utils][task_scheduler][suspend][delayed]")
{
    BROOKESIA_LOGI("=== TaskScheduler Suspend/Resume Delayed Task Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    auto start = esp_timer_get_time();
    BROOKESIA_TIME_PROFILER_START_EVENT("total_suspend_resume");

    scheduler.post_delayed([]() {
        g_counter++;
        BROOKESIA_LOGI("Task executed");
    }, 500, &task_id);

    // Wait for 100ms to suspend (remaining about 400ms)
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_before_suspend");
    vTaskDelay(pdMS_TO_TICKS(100));
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_before_suspend");

    bool suspended = scheduler.suspend(task_id);
    TEST_ASSERT_TRUE(suspended);
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Suspend for 500ms
    BROOKESIA_TIME_PROFILER_START_EVENT("suspended_duration");
    vTaskDelay(pdMS_TO_TICKS(500));
    BROOKESIA_TIME_PROFILER_END_EVENT("suspended_duration");
    TEST_ASSERT_EQUAL(0, g_counter.load()); // Still not executed

    // Resume task (should execute in about 400ms)
    bool resumed = scheduler.resume(task_id);
    TEST_ASSERT_TRUE(resumed);

    // Wait for task to complete
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_after_resume");
    vTaskDelay(pdMS_TO_TICKS(500));
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_after_resume");
    BROOKESIA_TIME_PROFILER_END_EVENT("total_suspend_resume");

    TEST_ASSERT_EQUAL(1, g_counter.load());

    auto elapsed = (esp_timer_get_time() - start) / 1000;
    BROOKESIA_LOGI("Total elapsed time: %1% ms (expected ~800ms)", elapsed);
    // Total time should be approximately 100 + 300 + 400 = 800ms

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test suspend immediate task fails", "[utils][task_scheduler][suspend][immediate_fail]")
{
    BROOKESIA_LOGI("=== TaskScheduler Suspend Immediate Task Fails Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_counter++;
    }, &task_id);

    // Try to suspend immediate task (should fail)
    vTaskDelay(pdMS_TO_TICKS(10));
    bool suspended = scheduler.suspend(task_id);
    TEST_ASSERT_FALSE(suspended); // Not supported to suspend immediate task

    vTaskDelay(pdMS_TO_TICKS(150));
    scheduler.stop();
}

TEST_CASE("Test suspend group", "[utils][task_scheduler][suspend][group]")
{
    BROOKESIA_LOGI("=== TaskScheduler Suspend Group Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    BROOKESIA_TIME_PROFILER_START_EVENT("suspend_group_test");

    // Submit multiple delayed tasks to the same group
    for (int i = 0; i < 3; i++) {
        scheduler.post_delayed([]() {
            g_counter++;
        }, 300, nullptr, "suspend_group");
    }

    BROOKESIA_TIME_PROFILER_START_EVENT("wait_before_group_suspend");
    vTaskDelay(pdMS_TO_TICKS(100));
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_before_group_suspend");

    // Suspend the entire group
    size_t suspended = scheduler.suspend_group("suspend_group");
    TEST_ASSERT_EQUAL(3, suspended);

    BROOKESIA_TIME_PROFILER_START_EVENT("group_suspended");
    vTaskDelay(pdMS_TO_TICKS(300));
    BROOKESIA_TIME_PROFILER_END_EVENT("group_suspended");
    TEST_ASSERT_EQUAL(0, g_counter.load()); // All tasks are suspended

    // Resume the entire group
    size_t resumed = scheduler.resume_group("suspend_group");
    TEST_ASSERT_EQUAL(3, resumed);

    BROOKESIA_TIME_PROFILER_START_EVENT("wait_after_group_resume");
    vTaskDelay(pdMS_TO_TICKS(300));
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_after_group_resume");
    BROOKESIA_TIME_PROFILER_END_EVENT("suspend_group_test");

    TEST_ASSERT_EQUAL(3, g_counter.load()); // All tasks resume execution

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test suspend and resume all", "[utils][task_scheduler][suspend][all]")
{
    BROOKESIA_LOGI("=== TaskScheduler Suspend/Resume All Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Submit multiple delayed tasks
    for (int i = 0; i < 5; i++) {
        scheduler.post_delayed([]() {
            g_counter++;
        }, 300);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Suspend all tasks
    size_t suspended = scheduler.suspend_all();
    TEST_ASSERT_EQUAL(5, suspended);

    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Resume all tasks
    size_t resumed = scheduler.resume_all();
    TEST_ASSERT_EQUAL(5, resumed);

    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_EQUAL(5, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test strand under stress", "[utils][task_scheduler][strand][stress]")
{
    BROOKESIA_LOGI("=== TaskScheduler Strand Under Stress Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    // Configure strand group
    TaskScheduler::GroupConfig strand_config;
    strand_config.enable_serial_execution = true;
    scheduler.configure_group("strand_group", strand_config);

    std::vector<int> strand_order_record;
    boost::mutex strand_mutex;

    int task_count = 100;
    int delay_ms = 100;

    // Submit tasks to strand group
    for (int i = 0; i < task_count; i++) {
        scheduler.post([i, delay_ms, &strand_order_record, &strand_mutex]() {
            int random_delay = rand() % delay_ms; // 0-delay_ms ms
            boost::this_thread::sleep_for(boost::chrono::milliseconds(random_delay));

            {
                boost::lock_guard<boost::mutex> lock(strand_mutex);
                strand_order_record.push_back(i);
            }
        }, nullptr, "strand_group");
    }

    TEST_ASSERT_TRUE(scheduler.wait_all(delay_ms * task_count));

    scheduler.stop();

    // Verify strand group: execution order should be strictly increasing (0, 1, 2, 3, ...)
    {
        boost::lock_guard<boost::mutex> lock(strand_mutex);
        TEST_ASSERT_EQUAL(task_count, strand_order_record.size());
        for (int i = 0; i < task_count; i++) {
            BROOKESIA_LOGI("Strand order[%1%] = %2%", i, strand_order_record[i]);
            TEST_ASSERT_EQUAL(i, strand_order_record[i]);
        }
    }
}

// ============================================================================
// New interface tests - comprehensive test
// ============================================================================

TEST_CASE("Test comprehensive suspend resume with wait", "[utils][task_scheduler][comprehensive][suspend_wait]")
{
    BROOKESIA_LOGI("=== TaskScheduler Comprehensive Suspend/Resume with Wait Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    BROOKESIA_TIME_PROFILER_START_EVENT("comprehensive_test");

    scheduler.post_delayed([]() {
        g_counter++;
        BROOKESIA_LOGI("Task completed");
    }, 500, &task_id);

    // Suspend task
    BROOKESIA_TIME_PROFILER_START_EVENT("before_suspend");
    vTaskDelay(pdMS_TO_TICKS(100));
    BROOKESIA_TIME_PROFILER_END_EVENT("before_suspend");

    scheduler.suspend(task_id);

    // Try to wait for suspended task (should timeout)
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_on_suspended_task");
    bool wait_result = scheduler.wait(task_id, 200);
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_on_suspended_task");

    TEST_ASSERT_FALSE(wait_result); // Timeout
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Resume task
    scheduler.resume(task_id);

    // Wait again (should succeed)
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_after_resume");
    wait_result = scheduler.wait(task_id, 1000);
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_after_resume");
    BROOKESIA_TIME_PROFILER_END_EVENT("comprehensive_test");

    TEST_ASSERT_TRUE(wait_result);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

// ============================================================================
// Callback functionality tests
// ============================================================================

TEST_CASE("Test pre_execute_callback", "[utils][task_scheduler][callback][pre_execute]")
{
    BROOKESIA_LOGI("=== TaskScheduler PreExecuteCallback Test ===");

    reset_counters();

    struct CallbackData {
        std::atomic<int> pre_call_count{0};
        std::atomic<TaskScheduler::TaskId> last_task_id{0};
        std::atomic<int> last_task_type{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};
    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_call_count++;
        cb_data.last_task_id = id;
        cb_data.last_task_type = static_cast<int>(type);
        BROOKESIA_LOGI("Pre-execute: task_id=%1%, type=%2%", id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Test 1: Immediate task
    BROOKESIA_LOGI("Test immediate task");
    TaskScheduler::TaskId task_id = 0;
    scheduler.post([]() {
        BROOKESIA_LOGI("Immediate task executing");
        vTaskDelay(pdMS_TO_TICKS(10));
    }, &task_id);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, cb_data.pre_call_count.load());
    TEST_ASSERT_EQUAL(task_id, cb_data.last_task_id.load());
    TEST_ASSERT_EQUAL(static_cast<int>(TaskScheduler::TaskType::Immediate), cb_data.last_task_type.load());

    // Test 2: Delayed task
    BROOKESIA_LOGI("Test delayed task");
    scheduler.post_delayed([]() {
        BROOKESIA_LOGI("Delayed task executing");
    }, 50, &task_id);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(2, cb_data.pre_call_count.load());
    TEST_ASSERT_EQUAL(task_id, cb_data.last_task_id.load());
    TEST_ASSERT_EQUAL(static_cast<int>(TaskScheduler::TaskType::Delayed), cb_data.last_task_type.load());

    // Test 3: Periodic task (should be called multiple times)
    BROOKESIA_LOGI("Test periodic task");
    std::atomic<int> period_count{0};
    scheduler.post_periodic([&period_count]() -> bool {
        period_count++;
        BROOKESIA_LOGI("Periodic task executing, count=%1%", period_count.load());
        return period_count < 3;
    }, 50, &task_id);
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(3, period_count.load());
    TEST_ASSERT_GREATER_OR_EQUAL(5, cb_data.pre_call_count.load()); // 1 + 1 + 3 = 5
    TEST_ASSERT_EQUAL(static_cast<int>(TaskScheduler::TaskType::Periodic), cb_data.last_task_type.load());

    scheduler.stop();
    BROOKESIA_LOGI("Total pre-execute callbacks: %1%", cb_data.pre_call_count.load());
}

TEST_CASE("Test post_execute_callback", "[utils][task_scheduler][callback][post_execute]")
{
    BROOKESIA_LOGI("=== TaskScheduler PostExecuteCallback Test ===");

    reset_counters();

    struct CallbackData {
        std::atomic<int> post_call_count{0};
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        std::atomic<TaskScheduler::TaskId> last_task_id{0};
        std::atomic<int> last_task_type{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};
    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_call_count++;
        if (success) {
            cb_data.success_count++;
        } else {
            cb_data.failure_count++;
        }
        cb_data.last_task_id = id;
        cb_data.last_task_type = static_cast<int>(type);
        BROOKESIA_LOGI("Post-execute: task_id=%1%, type=%2%, success=%3%",
                       id, BROOKESIA_DESCRIBE_TO_STR(type), success);
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Test 1: Successful immediate task
    BROOKESIA_LOGI("Test successful immediate task");
    scheduler.post([]() {
        BROOKESIA_LOGI("Successful task");
        vTaskDelay(pdMS_TO_TICKS(10));
    });
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, cb_data.post_call_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.success_count.load());
    TEST_ASSERT_EQUAL(0, cb_data.failure_count.load());

    // Test 2: Failed task (throws exception)
    BROOKESIA_LOGI("Test failed task");
    scheduler.post([]() {
        BROOKESIA_LOGI("Task about to fail");
        throw std::runtime_error("Intentional failure for testing");
    });
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(2, cb_data.post_call_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.success_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.failure_count.load());

    // Test 3: Successful delayed task
    BROOKESIA_LOGI("Test successful delayed task");
    scheduler.post_delayed([]() {
        BROOKESIA_LOGI("Delayed task");
    }, 50);
    vTaskDelay(pdMS_TO_TICKS(100));
    TEST_ASSERT_EQUAL(3, cb_data.post_call_count.load());
    TEST_ASSERT_EQUAL(2, cb_data.success_count.load());

    // Test 4: Periodic task (callback called for each execution)
    BROOKESIA_LOGI("Test periodic task");
    std::atomic<int> period_count{0};
    scheduler.post_periodic([&period_count]() -> bool {
        period_count++;
        BROOKESIA_LOGI("Periodic task, count=%1%", period_count.load());
        return period_count < 3; // Stop after 3 iterations
    }, 50);
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(3, period_count.load());
    // Post-execute callback should be called for each execution (3 times for periodic task)
    TEST_ASSERT_EQUAL(6, cb_data.post_call_count.load()); // 1 (immediate) + 1 (failed) + 1 (delayed) + 3 (periodic)
    TEST_ASSERT_EQUAL(5, cb_data.success_count.load()); // 1 + 0 + 1 + 3

    scheduler.stop();
    BROOKESIA_LOGI("Total post-execute callbacks: %1% (success: %2%, failure: %3%)",
                   cb_data.post_call_count.load(),
                   cb_data.success_count.load(),
                   cb_data.failure_count.load());
}

TEST_CASE("Test pre and post execute callbacks together", "[utils][task_scheduler][callback][combined]")
{
    BROOKESIA_LOGI("=== TaskScheduler Pre+Post Execute Callbacks Test ===");

    reset_counters();

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::vector<std::pair<std::string, TaskScheduler::TaskId>> execution_order;
        boost::mutex mutex;
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        {
            boost::lock_guard<boost::mutex> lock(cb_data.mutex);
            cb_data.execution_order.push_back({"PRE", id});
        }
        BROOKESIA_LOGI("PRE: task_id=%1%, type=%2%", id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        {
            boost::lock_guard<boost::mutex> lock(cb_data.mutex);
            cb_data.execution_order.push_back({"POST", id});
        }
        BROOKESIA_LOGI("POST: task_id=%1%, type=%2%, success=%3%",
                       id, BROOKESIA_DESCRIBE_TO_STR(type), success);
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Execute multiple tasks
    std::atomic<int> task_count{0};
    for (int i = 0; i < 5; i++) {
        scheduler.post([&task_count, i]() {
            task_count++;
            BROOKESIA_LOGI("Task %1% executing", i);
            vTaskDelay(pdMS_TO_TICKS(10));
        });
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(5, task_count.load());
    TEST_ASSERT_EQUAL(5, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(5, cb_data.post_count.load());

    // Verify execution order: each task should have PRE before POST
    BROOKESIA_LOGI("Execution order:");
    for (const auto &entry : cb_data.execution_order) {
        BROOKESIA_LOGI("  %1%: task_id=%2%", entry.first, entry.second);
    }

    scheduler.stop();
}

TEST_CASE("Test callbacks with task cancellation", "[utils][task_scheduler][callback][cancel]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callbacks with Cancellation Test ===");

    reset_counters();

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        BROOKESIA_LOGI("PRE: task_id=%1%", id);
    };

    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        BROOKESIA_LOGI("POST: task_id=%1%, success=%2%", id, success);
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Submit a delayed task and cancel it before execution
    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed([]() {
        BROOKESIA_LOGI("This task should NOT execute");
    }, 200, &task_id);

    vTaskDelay(pdMS_TO_TICKS(50));
    scheduler.cancel(task_id);

    vTaskDelay(pdMS_TO_TICKS(200));

    // Canceled task should NOT trigger callbacks
    TEST_ASSERT_EQUAL(0, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(0, cb_data.post_count.load());

    scheduler.stop();
}

TEST_CASE("Test callbacks with suspend and resume", "[utils][task_scheduler][callback][suspend_resume]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callbacks with Suspend/Resume Test ===");

    reset_counters();

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        BROOKESIA_LOGI("PRE: task_id=%1%, type=%2%", id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        BROOKESIA_LOGI("POST: task_id=%1%, success=%2%", id, success);
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Submit a periodic task and suspend/resume it
    std::atomic<int> exec_count{0};
    TaskScheduler::TaskId task_id = 0;
    scheduler.post_periodic([&exec_count]() -> bool {
        exec_count++;
        BROOKESIA_LOGI("Periodic task execution %1%", exec_count.load());
        return exec_count < 5;
    }, 50, &task_id);

    // Let it run twice
    vTaskDelay(pdMS_TO_TICKS(150));
    TEST_ASSERT_GREATER_OR_EQUAL(2, exec_count.load());
    TEST_ASSERT_GREATER_OR_EQUAL(2, cb_data.pre_count.load());
    TEST_ASSERT_GREATER_OR_EQUAL(2, cb_data.post_count.load()); // Called for each execution

    // Suspend
    scheduler.suspend(task_id);
    vTaskDelay(pdMS_TO_TICKS(100));
    int exec_count_suspended = exec_count.load();
    int pre_count_suspended = cb_data.pre_count.load();
    int post_count_suspended = cb_data.post_count.load();

    // Should not execute while suspended
    TEST_ASSERT_EQUAL(exec_count_suspended, exec_count.load());
    TEST_ASSERT_EQUAL(pre_count_suspended, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(post_count_suspended, cb_data.post_count.load());

    // Resume
    scheduler.resume(task_id);
    vTaskDelay(pdMS_TO_TICKS(200));

    // Should continue executing until completion
    TEST_ASSERT_EQUAL(5, exec_count.load());
    TEST_ASSERT_EQUAL(5, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(5, cb_data.post_count.load()); // Called for each execution (5 times total)

    scheduler.stop();
}

TEST_CASE("Test callback exception handling", "[utils][task_scheduler][callback][exception]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callback Exception Handling Test ===");

    reset_counters();

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> task_count{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    // Pre-callback that throws exception
    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        BROOKESIA_LOGI("PRE callback (will throw): task_id=%1%", id);
        if (cb_data.pre_count == 2) {
            throw std::runtime_error("Pre-callback exception");
        }
    };

    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        BROOKESIA_LOGI("POST callback: task_id=%1%, success=%2%", id, success);
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Task 1: Normal execution
    scheduler.post([&cb_data]() {
        cb_data.task_count++;
        BROOKESIA_LOGI("Task 1 executing");
    });
    vTaskDelay(pdMS_TO_TICKS(50));

    // Task 2: Pre-callback throws, but task should still execute
    scheduler.post([&cb_data]() {
        cb_data.task_count++;
        BROOKESIA_LOGI("Task 2 executing (after pre-callback exception)");
    });
    vTaskDelay(pdMS_TO_TICKS(50));

    // Task 3: Normal execution
    scheduler.post([&cb_data]() {
        cb_data.task_count++;
        BROOKESIA_LOGI("Task 3 executing");
    });
    vTaskDelay(pdMS_TO_TICKS(50));

    // All tasks should execute despite callback exception
    TEST_ASSERT_EQUAL(3, cb_data.task_count.load());
    TEST_ASSERT_EQUAL(3, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(3, cb_data.post_count.load());

    scheduler.stop();
}

TEST_CASE("Test callbacks with mutex lock", "[utils][task_scheduler][callback][mutex]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callbacks with Mutex Lock Test ===");

    reset_counters();

    struct CallbackData {
        boost::mutex task_mutex;
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> task_execution_count{0};
        std::atomic<bool> lock_acquired_in_task{false};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        cb_data.task_mutex.lock(); // Lock in pre-execute
        BROOKESIA_LOGI("PRE: task_id=%1%, mutex locked", id);
    };

    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        cb_data.task_mutex.unlock(); // Unlock in post-execute
        BROOKESIA_LOGI("POST: task_id=%1%, mutex unlocked, success=%2%", id, success);
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Test 1: Single immediate task - verify mutex is locked during execution
    BROOKESIA_LOGI("Test 1: Single immediate task with mutex verification");
    scheduler.post([&cb_data]() {
        cb_data.task_execution_count++;
        BROOKESIA_LOGI("Task executing");

        // Try to lock mutex - should fail because pre_execute already locked it
        bool acquired = cb_data.task_mutex.try_lock();
        cb_data.lock_acquired_in_task = acquired;

        if (acquired) {
            // If we acquired it, unlock it immediately (shouldn't happen)
            cb_data.task_mutex.unlock();
            BROOKESIA_LOGE("ERROR: Task acquired mutex, but it should have been locked by pre-execute!");
        } else {
            BROOKESIA_LOGI("Good: Mutex is locked during task execution (as expected)");
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    });

    vTaskDelay(pdMS_TO_TICKS(100));

    // Verify: task should not have acquired the mutex (it was locked by pre-execute)
    TEST_ASSERT_FALSE(cb_data.lock_acquired_in_task.load());
    TEST_ASSERT_EQUAL(1, cb_data.task_execution_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.post_count.load());

    // Test 2: Verify mutex is unlocked after post-execute
    BROOKESIA_LOGI("Test 2: Verify mutex is unlocked after task completion");
    bool can_lock_after_task = cb_data.task_mutex.try_lock();
    TEST_ASSERT_TRUE(can_lock_after_task);
    if (can_lock_after_task) {
        BROOKESIA_LOGI("Good: Mutex is unlocked after post-execute callback");
        cb_data.task_mutex.unlock();
    }

    // Test 3: Multiple tasks - each should be protected by its own lock/unlock cycle
    BROOKESIA_LOGI("Test 3: Multiple tasks with independent lock cycles");
    std::atomic<int> multi_task_count{0};
    std::atomic<int> failed_lock_attempts{0};

    for (int i = 0; i < 3; i++) {
        scheduler.post([&cb_data, &multi_task_count, &failed_lock_attempts, i]() {
            multi_task_count++;
            BROOKESIA_LOGI("Multi-task %1% executing", i);

            // Verify mutex is locked
            if (!cb_data.task_mutex.try_lock()) {
                failed_lock_attempts++;
                BROOKESIA_LOGI("Task %1%: Mutex correctly locked", i);
            } else {
                cb_data.task_mutex.unlock();
                BROOKESIA_LOGE("Task %1%: ERROR - Mutex not locked!", i);
            }

            vTaskDelay(pdMS_TO_TICKS(10));
        });
    }

    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_EQUAL(3, multi_task_count.load());
    TEST_ASSERT_EQUAL(3, failed_lock_attempts.load()); // All 3 tasks should fail to acquire mutex
    TEST_ASSERT_EQUAL(4, cb_data.pre_count.load()); // 1 + 3
    TEST_ASSERT_EQUAL(4, cb_data.post_count.load()); // 1 + 3

    // Test 4: Verify mutex is unlocked after all tasks
    BROOKESIA_LOGI("Test 4: Final mutex state verification");
    bool final_can_lock = cb_data.task_mutex.try_lock();
    TEST_ASSERT_TRUE(final_can_lock);
    if (final_can_lock) {
        BROOKESIA_LOGI("Good: Mutex is unlocked after all tasks completed");
        cb_data.task_mutex.unlock();
    }

    scheduler.stop();
    BROOKESIA_LOGI("✓ Mutex lock/unlock test passed - callbacks executed in correct order");
}

TEST_CASE("Test callbacks mutex with delayed and periodic tasks", "[utils][task_scheduler][callback][mutex_complex]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callbacks Mutex with Delayed/Periodic Tasks Test ===");

    reset_counters();

    struct CallbackData {
        boost::mutex task_mutex;
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> locked_execution_count{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        cb_data.task_mutex.lock();
        BROOKESIA_LOGI("PRE: task_id=%1%, type=%2%, locked", id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    config.post_execute_callback = [&cb_data](TaskScheduler::TaskId id, TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        cb_data.task_mutex.unlock();
        BROOKESIA_LOGI("POST: task_id=%1%, type=%2%, unlocked", id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    TaskScheduler scheduler;
    scheduler.start(config);

    // Test delayed task
    BROOKESIA_LOGI("Test delayed task with mutex");
    scheduler.post_delayed([&cb_data]() {
        BROOKESIA_LOGI("Delayed task executing");
        if (!cb_data.task_mutex.try_lock()) {
            cb_data.locked_execution_count++;
            BROOKESIA_LOGI("Delayed task: Mutex correctly locked");
        } else {
            cb_data.task_mutex.unlock();
            BROOKESIA_LOGE("Delayed task: ERROR - Mutex not locked!");
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }, 50);

    vTaskDelay(pdMS_TO_TICKS(150));

    TEST_ASSERT_EQUAL(1, cb_data.locked_execution_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.pre_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.post_count.load());

    // Test periodic task - each execution should have lock/unlock cycle
    BROOKESIA_LOGI("Test periodic task with mutex");
    std::atomic<int> period_count{0};
    scheduler.post_periodic([&cb_data, &period_count]() -> bool {
        period_count++;
        BROOKESIA_LOGI("Periodic task execution %1%", period_count.load());

        if (!cb_data.task_mutex.try_lock())
        {
            cb_data.locked_execution_count++;
            BROOKESIA_LOGI("Periodic task %1%: Mutex correctly locked", period_count.load());
        } else
        {
            cb_data.task_mutex.unlock();
            BROOKESIA_LOGE("Periodic task %1%: ERROR - Mutex not locked!", period_count.load());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
        return period_count < 3;
    }, 50);

    vTaskDelay(pdMS_TO_TICKS(300));

    TEST_ASSERT_EQUAL(3, period_count.load());
    TEST_ASSERT_EQUAL(4, cb_data.locked_execution_count.load()); // 1 (delayed) + 3 (periodic)
    TEST_ASSERT_EQUAL(4, cb_data.pre_count.load()); // 1 (delayed) + 3 (periodic)
    TEST_ASSERT_EQUAL(4, cb_data.post_count.load()); // 1 (delayed) + 3 (periodic)

    // Verify mutex is unlocked at the end
    bool final_can_lock = cb_data.task_mutex.try_lock();
    TEST_ASSERT_TRUE(final_can_lock);
    if (final_can_lock) {
        cb_data.task_mutex.unlock();
        BROOKESIA_LOGI("✓ Mutex is unlocked after all tasks");
    }

    scheduler.stop();
    BROOKESIA_LOGI("✓ Complex mutex test passed - all task types protected correctly");
}

// ============================================================================
// Nested task tests - post inside task executes immediately
// ============================================================================

TEST_CASE("Test dispatch inside task executes immediately", "[utils][task_scheduler][dispatch][immediate]")
{
    BROOKESIA_LOGI("=== TaskScheduler Dispatch Inside Task Immediate Execution Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_TWO_THREADS); // Use 2 threads to allow concurrent execution

    std::atomic<bool> outer_task_started{false};
    std::atomic<bool> inner_task_started{false};
    std::atomic<bool> outer_task_finished{false};
    std::atomic<int64_t> outer_start_time{0};
    std::atomic<int64_t> inner_start_time{0};
    std::atomic<int64_t> outer_end_time{0};

    // Outer task that posts another task
    BROOKESIA_TIME_PROFILER_START_EVENT("total_test");
    TaskScheduler::TaskId outer_task_id = 0;
    scheduler.post([&]() {
        outer_task_started = true;
        outer_start_time = esp_timer_get_time();
        BROOKESIA_LOGI("Outer task started");

        // Post inner task while outer task is still running
        TaskScheduler::TaskId inner_task_id = 0;
        scheduler.dispatch([&]() {
            inner_task_started = true;
            inner_start_time = esp_timer_get_time();
            BROOKESIA_LOGI("Inner task started (posted from outer task)");
            vTaskDelay(pdMS_TO_TICKS(50));
            g_counter++;
            BROOKESIA_LOGI("Inner task finished");
        }, &inner_task_id);

        BROOKESIA_LOGI("Inner task posted with id: %1%", inner_task_id);

        // Outer task continues running
        vTaskDelay(pdMS_TO_TICKS(100));
        outer_task_finished = true;
        outer_end_time = esp_timer_get_time();
        g_counter++;
        BROOKESIA_LOGI("Outer task finished");
    }, &outer_task_id);

    // Wait for both tasks to complete
    BROOKESIA_TIME_PROFILER_START_EVENT("wait_for_completion");
    bool completed = scheduler.wait_all(2000);
    BROOKESIA_TIME_PROFILER_END_EVENT("wait_for_completion");
    BROOKESIA_TIME_PROFILER_END_EVENT("total_test");

    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_TRUE(outer_task_started.load());
    TEST_ASSERT_TRUE(inner_task_started.load());
    TEST_ASSERT_TRUE(outer_task_finished.load());
    TEST_ASSERT_EQUAL(2, g_counter.load());

    // Verify that inner task started before outer task finished (immediate execution)
    int64_t outer_duration = outer_end_time.load() - outer_start_time.load();
    int64_t inner_start_offset = inner_start_time.load() - outer_start_time.load();

    BROOKESIA_LOGI("Outer task duration: %1% us", outer_duration / 1000);
    BROOKESIA_LOGI("Inner task started after: %1% us from outer task start", inner_start_offset / 1000);

    // Inner task should start while outer task is still running
    TEST_ASSERT_TRUE(inner_start_time.load() < outer_end_time.load());
    // Inner task should start very quickly after being posted (within the outer task execution)
    TEST_ASSERT_LESS_THAN(50000, inner_start_offset); // Should start within 50ms

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test multiple dispatch inside task execute immediately", "[utils][task_scheduler][dispatch][multiple]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multiple Dispatch Inside Task Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS); // Use 4 threads for better concurrency

    std::atomic<int> nested_task_count{0};
    std::vector<int64_t> start_times(5);
    boost::mutex times_mutex;

    TaskScheduler::TaskId outer_task_id = 0;
    auto outer_start = esp_timer_get_time();

    scheduler.post([&]() {
        BROOKESIA_LOGI("Outer task: posting 5 nested tasks");

        // Post 5 tasks from within this task
        for (int i = 0; i < 5; i++) {
            scheduler.dispatch([ &, i]() {
                {
                    boost::lock_guard<boost::mutex> lock(times_mutex);
                    start_times[i] = esp_timer_get_time();
                }
                BROOKESIA_LOGI("Nested task %1% executing", i);
                vTaskDelay(pdMS_TO_TICKS(20));
                nested_task_count++;
            });
        }

        BROOKESIA_LOGI("Outer task: all nested tasks posted, now waiting");
        vTaskDelay(pdMS_TO_TICKS(200)); // Keep outer task alive while nested tasks run
        g_counter++;
        BROOKESIA_LOGI("Outer task finished");
    }, &outer_task_id);

    // Wait for all tasks to complete
    bool completed = scheduler.wait_all(2000);
    TEST_ASSERT_TRUE(completed);

    TEST_ASSERT_EQUAL(1, g_counter.load());
    TEST_ASSERT_EQUAL(5, nested_task_count.load());

    // Verify all nested tasks started quickly (immediate execution)
    BROOKESIA_LOGI("Nested task start times:");
    for (int i = 0; i < 5; i++) {
        int64_t offset = (start_times[i] - outer_start) / 1000;
        BROOKESIA_LOGI("  Task %1%: started at +%2% ms", i, offset);
        TEST_ASSERT_LESS_THAN(150000, start_times[i] - outer_start); // All should start within 150ms
    }

    scheduler.stop();
}

TEST_CASE("Test dispatch inside delayed task executes immediately", "[utils][task_scheduler][dispatch][delayed]")
{
    BROOKESIA_LOGI("=== TaskScheduler Dispatch Inside Delayed Task Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_TWO_THREADS);

    std::atomic<int64_t> delayed_task_time{0};
    std::atomic<int64_t> nested_task_time{0};

    auto test_start = esp_timer_get_time();

    // Post a delayed task that will post another immediate task
    scheduler.post_delayed([&]() {
        delayed_task_time = esp_timer_get_time();
        BROOKESIA_LOGI("Delayed task executing, now posting immediate task");

        scheduler.dispatch([&]() {
            nested_task_time = esp_timer_get_time();
            BROOKESIA_LOGI("Nested immediate task executing");
            g_counter++;
        });

        vTaskDelay(pdMS_TO_TICKS(50));
        g_counter++;
        BROOKESIA_LOGI("Delayed task finished");
    }, 200);

    bool completed = scheduler.wait_all(1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(2, g_counter.load());

    // Verify timing
    int64_t delayed_offset = (delayed_task_time.load() - test_start) / 1000;
    int64_t nested_offset = (nested_task_time.load() - test_start) / 1000;

    BROOKESIA_LOGI("Delayed task started at: %1% ms", delayed_offset);
    BROOKESIA_LOGI("Nested task started at: %1% ms", nested_offset);

    // Nested task should start very close to when it was posted
    int64_t nested_delay = (nested_task_time.load() - delayed_task_time.load()) / 1000;
    BROOKESIA_LOGI("Nested task delay from post: %1% ms", nested_delay);
    TEST_ASSERT_LESS_THAN(30, nested_delay); // Should start within 30ms of being posted

    scheduler.stop();
}
