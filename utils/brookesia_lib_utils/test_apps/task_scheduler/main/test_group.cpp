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
// Task group tests
// ============================================================================

TEST_CASE("Test task groups", "[utils][task_scheduler][group][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler Task Groups Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Use post_delayed to ensure tasks are pending when we check the count
    scheduler.post_delayed(simple_task, 200, nullptr, "group1");
    scheduler.post_delayed(simple_task, 200, nullptr, "group1");
    scheduler.post_delayed(simple_task, 200, nullptr, "group2");

    vTaskDelay(pdMS_TO_TICKS(50)); // Small delay to ensure tasks are registered

    TEST_ASSERT_EQUAL(2, scheduler.get_group_task_count("group1"));
    TEST_ASSERT_EQUAL(1, scheduler.get_group_task_count("group2"));

    auto groups = scheduler.get_active_groups();
    BROOKESIA_LOGI("Active groups count: %1%", groups.size());
    TEST_ASSERT_EQUAL(2, groups.size());

    // Wait for tasks to complete
    vTaskDelay(pdMS_TO_TICKS(250));
    TEST_ASSERT_EQUAL(3, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test cancel group", "[utils][task_scheduler][group][cancel]")
{
    BROOKESIA_LOGI("=== TaskScheduler Cancel Group Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    scheduler.post_delayed(simple_task, 500, nullptr, "group1");
    scheduler.post_delayed(simple_task, 500, nullptr, "group1");
    scheduler.post_delayed(simple_task, 500, nullptr, "group2");

    vTaskDelay(pdMS_TO_TICKS(100));

    scheduler.cancel_group("group1");

    vTaskDelay(pdMS_TO_TICKS(500));

    TEST_ASSERT_EQUAL(1, g_counter.load()); // Only tasks in group2 are executed

    scheduler.stop();
}

TEST_CASE("Test get group", "[utils][task_scheduler][group][get_group]")
{
    BROOKESIA_LOGI("=== TaskScheduler Get Group Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Test 1: Get group for immediate task (query before execution completes)
    TaskScheduler::TaskId task1_id = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(100)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &task1_id, "group_immediate");

    // Query immediately after post, before task completes
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to ensure task is registered
    const auto &group1 = scheduler.get_group(task1_id);
    BROOKESIA_LOGI("Task1 group: '%1%'", group1);
    TEST_ASSERT_EQUAL_STRING("group_immediate", group1.c_str());

    // Test 2: Get group for delayed task
    TaskScheduler::TaskId task2_id = 0;
    scheduler.post_delayed([]() {
        g_counter++;
    }, 200, &task2_id, "group_delayed");

    vTaskDelay(pdMS_TO_TICKS(50));
    const auto &group2 = scheduler.get_group(task2_id);
    BROOKESIA_LOGI("Task2 group: '%1%'", group2);
    TEST_ASSERT_EQUAL_STRING("group_delayed", group2.c_str());

    // Test 3: Get group for periodic task
    TaskScheduler::TaskId task3_id = 0;
    std::atomic<int> periodic_count{0};
    scheduler.post_periodic([&periodic_count]() -> bool {
        periodic_count++;
        return periodic_count < 2;
    }, 100, &task3_id, "group_periodic");

    vTaskDelay(pdMS_TO_TICKS(50));
    const auto &group3 = scheduler.get_group(task3_id);
    BROOKESIA_LOGI("Task3 group: '%1%'", group3);
    TEST_ASSERT_EQUAL_STRING("group_periodic", group3.c_str());

    // Test 4: Get group for task with default (empty) group
    TaskScheduler::TaskId task4_id = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(100)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &task4_id); // No group specified

    // Query immediately after post, before task completes
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to ensure task is registered
    const auto &group4 = scheduler.get_group(task4_id);
    BROOKESIA_LOGI("Task4 group: '%1%'", group4);
    TEST_ASSERT_EQUAL_STRING("", group4.c_str());

    // Test 5: Get group for non-existent task ID
    TaskScheduler::TaskId non_existent_id = 99999;
    const auto &group5 = scheduler.get_group(non_existent_id);
    BROOKESIA_LOGI("Non-existent task group: '%1%'", group5);
    TEST_ASSERT_EQUAL_STRING("", group5.c_str());

    // Test 6: Get group for multiple tasks in same group
    TaskScheduler::TaskId task6a_id = 0;
    TaskScheduler::TaskId task6b_id = 0;
    TaskScheduler::TaskId task6c_id = 0;
    scheduler.post_delayed([]() {
        g_counter++;
    }, 200, &task6a_id, "group_multi");
    scheduler.post_delayed([]() {
        g_counter++;
    }, 200, &task6b_id, "group_multi");
    scheduler.post_delayed([]() {
        g_counter++;
    }, 200, &task6c_id, "group_multi");

    vTaskDelay(pdMS_TO_TICKS(50));
    const auto &group6a = scheduler.get_group(task6a_id);
    const auto &group6b = scheduler.get_group(task6b_id);
    const auto &group6c = scheduler.get_group(task6c_id);
    BROOKESIA_LOGI("Task6a group: '%1%', Task6b group: '%2%', Task6c group: '%3%'", group6a, group6b, group6c);
    TEST_ASSERT_EQUAL_STRING("group_multi", group6a.c_str());
    TEST_ASSERT_EQUAL_STRING("group_multi", group6b.c_str());
    TEST_ASSERT_EQUAL_STRING("group_multi", group6c.c_str());

    // Test 7: Get group after task completion (should return empty string as task is removed)
    bool completed = scheduler.wait(task1_id, 1000);
    TEST_ASSERT_TRUE(completed);
    const auto &group1_after = scheduler.get_group(task1_id);
    BROOKESIA_LOGI("Task1 group after completion: '%1%'", group1_after);
    // Note: After completion, task is removed from tasks_ map, so get_group returns empty string
    TEST_ASSERT_EQUAL_STRING("", group1_after.c_str());

    // Wait for all tasks to complete
    vTaskDelay(pdMS_TO_TICKS(300));
    scheduler.stop();
}

TEST_CASE("Test get group with different group names", "[utils][task_scheduler][group][get_group_names]")
{
    BROOKESIA_LOGI("=== TaskScheduler Get Group with Different Names Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Test with various group name formats
    std::vector<std::pair<TaskScheduler::TaskId, std::string>> tasks;

    // Empty group name
    TaskScheduler::TaskId id1 = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(50)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &id1, "");
    tasks.push_back({id1, ""});

    // Simple group name
    TaskScheduler::TaskId id2 = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(50)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &id2, "simple");
    tasks.push_back({id2, "simple"});

    // Group name with underscore
    TaskScheduler::TaskId id3 = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(50)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &id3, "group_name");
    tasks.push_back({id3, "group_name"});

    // Group name with numbers
    TaskScheduler::TaskId id4 = 0;
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(50)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &id4, "group123");
    tasks.push_back({id4, "group123"});

    // Long group name
    TaskScheduler::TaskId id5 = 0;
    std::string long_group_name = "very_long_group_name_for_testing_purposes";
    scheduler.post([]() {
        vTaskDelay(pdMS_TO_TICKS(50)); // Add delay to ensure task is still running when queried
        g_counter++;
    }, &id5, long_group_name.c_str());
    tasks.push_back({id5, long_group_name});

    // Query immediately after all posts, before tasks complete
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay to ensure tasks are registered

    // Verify all groups are correct
    for (const auto &[task_id, expected_group] : tasks) {
        const auto &actual_group = scheduler.get_group(task_id);
        BROOKESIA_LOGI("Task %1%: expected='%2%', actual='%3%'", task_id, expected_group, actual_group);
        TEST_ASSERT_EQUAL_STRING(expected_group.c_str(), actual_group.c_str());
    }

    // Wait for tasks to complete
    vTaskDelay(pdMS_TO_TICKS(3000));
    TEST_ASSERT_EQUAL(5, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test group strand vs non-strand execution order", "[utils][task_scheduler][group][strand][execution_order]")
{
    BROOKESIA_LOGI("=== TaskScheduler Group Strand vs Non-Strand Execution Order Test ===");

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

    // Verify strand group: execution order should be strictly increasing (0, 1, 2, 3, ...)
    {
        boost::lock_guard<boost::mutex> lock(strand_mutex);
        TEST_ASSERT_EQUAL(task_count, strand_order_record.size());
        for (int i = 0; i < task_count; i++) {
            BROOKESIA_LOGI("Strand order[%1%] = %2%", i, strand_order_record[i]);
            TEST_ASSERT_EQUAL(i, strand_order_record[i]);
        }
    }

    std::vector<int> normal_order_record;
    boost::mutex normal_mutex;

    // Submit tasks to normal group (execute in parallel)
    for (int i = 0; i < task_count; i++) {
        scheduler.post([i, delay_ms, &normal_order_record, &normal_mutex]() {
            int random_delay = rand() % delay_ms; // 0-delay_ms ms
            boost::this_thread::sleep_for(boost::chrono::milliseconds(random_delay));

            {
                boost::lock_guard<boost::mutex> lock(normal_mutex);
                normal_order_record.push_back(i);
            }
        }, nullptr, "");
    }

    TEST_ASSERT_TRUE(scheduler.wait_all(delay_ms * task_count));

    scheduler.stop();

    // Verify normal group: all tasks are executed, but the order may not be continuous
    {
        boost::lock_guard<boost::mutex> lock(normal_mutex);
        TEST_ASSERT_EQUAL(task_count, normal_order_record.size());
        BROOKESIA_LOGI("Normal group execution order:");
        for (int i = 0; i < task_count; i++) {
            BROOKESIA_LOGI("Normal order[%1%] = %2%", i, normal_order_record[i]);
        }
        // Not strictly increasing, as long as all tasks are executed
    }
}

TEST_CASE("Test group strand with mixed task types", "[utils][task_scheduler][group][strand][mixed]")
{
    BROOKESIA_LOGI("=== TaskScheduler Group Strand Mixed Task Types Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_FOUR_THREADS);

    // Configure group with enable_serial_execution
    TaskScheduler::GroupConfig strand_config;
    strand_config.enable_serial_execution = true;
    TEST_ASSERT_TRUE(scheduler.configure_group("mixed_group", strand_config));

    // Execution order tracker
    std::vector<std::string> execution_order;
    boost::mutex order_mutex;
    std::atomic<int> concurrent_count{0};
    std::atomic<int> max_concurrent{0};

    const int TASK_DELAY_MS = 10;
    const int ONCE_TASK_COUNT = 25;
    const int DELAYED_TASK_COUNT = 25;
    const int PERIODIC_TASK_COUNT = 25;
    const int PERIODIC_RUNS = 2; // Each periodic task runs 2 times
    const int TOTAL_EXPECTED = ONCE_TASK_COUNT + DELAYED_TASK_COUNT + PERIODIC_TASK_COUNT * PERIODIC_RUNS; // 100 tasks

    // Submit Once tasks
    for (int i = 0; i < ONCE_TASK_COUNT; i++) {
        scheduler.post([i, &execution_order, &order_mutex, &concurrent_count, &max_concurrent]() {
            int current = concurrent_count.fetch_add(1) + 1;
            int max = max_concurrent.load();
            while (current > max && !max_concurrent.compare_exchange_weak(max, current)) {
                max = max_concurrent.load();
            }

            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));

            {
                boost::lock_guard<boost::mutex> lock(order_mutex);
                execution_order.push_back("once_" + std::to_string(i));
            }

            concurrent_count.fetch_sub(1);
        }, nullptr, "mixed_group");
    }

    // Submit Delayed tasks
    for (int i = 0; i < DELAYED_TASK_COUNT; i++) {
        scheduler.post_delayed([i, &execution_order, &order_mutex, &concurrent_count, &max_concurrent]() {
            int current = concurrent_count.fetch_add(1) + 1;
            int max = max_concurrent.load();
            while (current > max && !max_concurrent.compare_exchange_weak(max, current)) {
                max = max_concurrent.load();
            }

            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));

            {
                boost::lock_guard<boost::mutex> lock(order_mutex);
                execution_order.push_back("delayed_" + std::to_string(i));
            }

            concurrent_count.fetch_sub(1);
        }, 50 + i * 5, nullptr, "mixed_group");
    }

    // Submit Periodic tasks (each runs PERIODIC_RUNS times)
    std::vector<std::atomic<int>> periodic_counters(PERIODIC_TASK_COUNT);
    for (int i = 0; i < PERIODIC_TASK_COUNT; i++) {
        periodic_counters[i].store(0);
        scheduler.post_periodic([i, PERIODIC_RUNS, &execution_order, &order_mutex, &concurrent_count, &max_concurrent, &periodic_counters]() -> bool {
            int current = concurrent_count.fetch_add(1) + 1;
            int max = max_concurrent.load();
            while (current > max && !max_concurrent.compare_exchange_weak(max, current))
            {
                max = max_concurrent.load();
            }

            vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS));

            int count = periodic_counters[i].fetch_add(1) + 1;
            {
                boost::lock_guard<boost::mutex> lock(order_mutex);
                execution_order.push_back("periodic_" + std::to_string(i) + "_" + std::to_string(count));
            }

            concurrent_count.fetch_sub(1);
            return count < PERIODIC_RUNS;
        }, 100, nullptr, "mixed_group");
    }

    // Wait for all tasks to complete
    TEST_ASSERT_TRUE(scheduler.wait_all(10000));

    // Verify: max_concurrent should be 1 (only one task executing at a time)
    BROOKESIA_LOGI("Max concurrent executions: %1%", max_concurrent.load());
    TEST_ASSERT_EQUAL(1, max_concurrent.load());

    // Verify: all tasks executed
    {
        boost::lock_guard<boost::mutex> lock(order_mutex);
        BROOKESIA_LOGI("Total tasks executed: %1%", execution_order.size());
        BROOKESIA_LOGI("Expected tasks: %1% (Once: %2%, Delayed: %3%, Periodic: %4% * %5%)",
                       TOTAL_EXPECTED, ONCE_TASK_COUNT, DELAYED_TASK_COUNT, PERIODIC_TASK_COUNT, PERIODIC_RUNS);
        TEST_ASSERT_EQUAL(TOTAL_EXPECTED, execution_order.size());
    }

    scheduler.stop();
}
