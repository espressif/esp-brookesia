/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "unity.h"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/time_profiler.hpp"

using namespace esp_brookesia::lib_utils;

#define TEST_SCHEDULER_CONFIG_GENERIC TaskScheduler::StartConfig{ \
    .worker_configs = {                                           \
        {.name = "TS_Worker", .stack_size = 8192},                \
    },                                                            \
    .worker_poll_interval_ms = 1                                  \
}

// Global variables used for testing
static std::atomic<int> g_counter{0};
static std::atomic<int64_t> g_execution_time{0};

// Helper functions
static void reset_counters()
{
    g_counter = 0;
    g_execution_time = 0;
}

// ============================================================================
// restart_timer tests for delayed tasks
// ============================================================================

TEST_CASE("Test restart_timer basic delayed task", "[utils][task_scheduler][restart_timer][delayed][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Basic Delayed Task Test ===");
    BROOKESIA_TIME_PROFILER_CLEAR();

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    auto start = esp_timer_get_time();
    BROOKESIA_TIME_PROFILER_START_EVENT("total_test");

    TaskScheduler::TaskId task_id = 0;
    // Post a delayed task with 300ms delay
    scheduler.post_delayed([&]() {
        g_execution_time = esp_timer_get_time();
        g_counter++;
        BROOKESIA_LOGI("Delayed task executed");
    }, 300, &task_id);

    // Wait 100ms, then restart timer
    vTaskDelay(pdMS_TO_TICKS(100));
    BROOKESIA_LOGI("Restarting timer after 100ms");
    bool restarted = scheduler.restart_timer(task_id);
    TEST_ASSERT_TRUE(restarted);

    // Task should NOT have executed yet (counter should still be 0)
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Wait for task to complete (300ms from restart)
    bool completed = scheduler.wait(task_id, 500);
    BROOKESIA_TIME_PROFILER_END_EVENT("total_test");

    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    // Verify total elapsed time is approximately 100 + 300 = 400ms
    auto elapsed = (g_execution_time.load() - start) / 1000;
    BROOKESIA_LOGI("Task executed after %1% ms (expected ~400ms)", elapsed);
    TEST_ASSERT_GREATER_OR_EQUAL(380, elapsed);
    TEST_ASSERT_LESS_OR_EQUAL(500, elapsed);

    BROOKESIA_TIME_PROFILER_REPORT();
    scheduler.stop();
}

TEST_CASE("Test restart_timer multiple restarts delayed task", "[utils][task_scheduler][restart_timer][delayed][multiple]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Multiple Restarts Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    auto start = esp_timer_get_time();

    TaskScheduler::TaskId task_id = 0;
    // Post a delayed task with 200ms delay
    scheduler.post_delayed([&]() {
        g_execution_time = esp_timer_get_time();
        g_counter++;
        BROOKESIA_LOGI("Delayed task executed");
    }, 200, &task_id);

    // Restart timer 3 times, every 100ms
    for (int i = 0; i < 3; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
        BROOKESIA_LOGI("Restart #%1%", i + 1);
        bool restarted = scheduler.restart_timer(task_id);
        TEST_ASSERT_TRUE(restarted);
        TEST_ASSERT_EQUAL(0, g_counter.load()); // Task should not have executed yet
    }

    // Wait for task to complete
    bool completed = scheduler.wait(task_id, 500);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    // Verify total elapsed time is approximately 3*100 + 200 = 500ms
    auto elapsed = (g_execution_time.load() - start) / 1000;
    BROOKESIA_LOGI("Task executed after %1% ms (expected ~500ms)", elapsed);
    TEST_ASSERT_GREATER_OR_EQUAL(480, elapsed);
    TEST_ASSERT_LESS_OR_EQUAL(600, elapsed);

    scheduler.stop();
}

TEST_CASE("Test restart_timer debounce pattern", "[utils][task_scheduler][restart_timer][delayed][debounce]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Debounce Pattern Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    const int DEBOUNCE_DELAY_MS = 150;

    // Post initial delayed task
    scheduler.post_delayed([&]() {
        g_counter++;
        BROOKESIA_LOGI("Debounced action executed, counter = %1%", g_counter.load());
    }, DEBOUNCE_DELAY_MS, &task_id);

    // Simulate rapid events (like button presses or input changes)
    // Each event restarts the timer
    for (int i = 0; i < 5; i++) {
        vTaskDelay(pdMS_TO_TICKS(50)); // Event every 50ms
        scheduler.restart_timer(task_id);
        BROOKESIA_LOGI("Event %1%: timer restarted", i + 1);
    }

    // Task should not have executed during rapid events
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Wait for debounce period after last event
    vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_DELAY_MS + 50));

    // Now task should have executed exactly once
    TEST_ASSERT_EQUAL(1, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// restart_timer tests for periodic tasks
// ============================================================================

TEST_CASE("Test restart_timer periodic task", "[utils][task_scheduler][restart_timer][periodic][basic]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Periodic Task Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    auto start = esp_timer_get_time();

    TaskScheduler::TaskId task_id = 0;
    std::atomic<int> exec_count{0};

    // Post a periodic task with 200ms interval
    scheduler.post_periodic([&]() -> bool {
        exec_count++;
        BROOKESIA_LOGI("Periodic task executed, count = %1%", exec_count.load());
        return exec_count < 3; // Stop after 3 executions
    }, 200, &task_id);

    // Wait 100ms, then restart timer (should delay first execution)
    vTaskDelay(pdMS_TO_TICKS(100));
    BROOKESIA_LOGI("Restarting periodic task timer");
    bool restarted = scheduler.restart_timer(task_id);
    TEST_ASSERT_TRUE(restarted);

    // Task should NOT have executed yet
    TEST_ASSERT_EQUAL(0, exec_count.load());

    // Wait for all periodic executions
    bool completed = scheduler.wait(task_id, 1000);
    TEST_ASSERT_TRUE(completed);
    TEST_ASSERT_EQUAL(3, exec_count.load());

    auto elapsed = (esp_timer_get_time() - start) / 1000;
    // Expected: 100 (wait) + 200 (first after restart) + 200 (second) + 200 (third) ≈ 700ms
    BROOKESIA_LOGI("All periodic executions completed after %1% ms", elapsed);
    TEST_ASSERT_GREATER_OR_EQUAL(650, elapsed);

    scheduler.stop();
}

TEST_CASE("Test restart_timer watchdog pattern", "[utils][task_scheduler][restart_timer][periodic][watchdog]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Watchdog Pattern Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId watchdog_id = 0;
    std::atomic<bool> watchdog_triggered{false};
    const int WATCHDOG_TIMEOUT_MS = 200;

    // Create watchdog timer
    scheduler.post_delayed([&]() {
        watchdog_triggered = true;
        BROOKESIA_LOGE("WATCHDOG TIMEOUT! System not responding");
    }, WATCHDOG_TIMEOUT_MS, &watchdog_id);

    // Simulate system heartbeats (feed the watchdog)
    for (int i = 0; i < 5; i++) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Heartbeat every 100ms (< 200ms timeout)
        if (scheduler.get_state(watchdog_id) == TaskScheduler::TaskState::Running) {
            scheduler.restart_timer(watchdog_id);
            BROOKESIA_LOGI("Heartbeat %1%: watchdog fed", i + 1);
        }
    }

    // Watchdog should NOT have triggered (we kept feeding it)
    TEST_ASSERT_FALSE(watchdog_triggered.load());

    // Now stop feeding and let watchdog trigger
    BROOKESIA_LOGI("Stopping heartbeats, waiting for watchdog timeout");
    vTaskDelay(pdMS_TO_TICKS(WATCHDOG_TIMEOUT_MS + 50));

    // Watchdog should have triggered
    TEST_ASSERT_TRUE(watchdog_triggered.load());

    scheduler.stop();
}

// ============================================================================
// restart_timer error cases
// ============================================================================

TEST_CASE("Test restart_timer non-existent task", "[utils][task_scheduler][restart_timer][error][not_found]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Non-existent Task Test ===");

    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    // Try to restart a non-existent task
    bool result = scheduler.restart_timer(9999);
    TEST_ASSERT_FALSE(result);

    scheduler.stop();
}

TEST_CASE("Test restart_timer immediate task fails", "[utils][task_scheduler][restart_timer][error][immediate]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Immediate Task Fails Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post([&]() {
        vTaskDelay(pdMS_TO_TICKS(200)); // Long running task
        g_counter++;
    }, &task_id);

    // Give task time to start
    vTaskDelay(pdMS_TO_TICKS(50));

    // Try to restart timer for immediate task (should fail)
    bool result = scheduler.restart_timer(task_id);
    TEST_ASSERT_FALSE(result);

    // Wait for task to complete
    scheduler.wait(task_id, 500);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test restart_timer suspended task fails", "[utils][task_scheduler][restart_timer][error][suspended]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Suspended Task Fails Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed([&]() {
        g_counter++;
    }, 300, &task_id);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Suspend the task
    bool suspended = scheduler.suspend(task_id);
    TEST_ASSERT_TRUE(suspended);

    // Try to restart timer for suspended task (should fail)
    bool result = scheduler.restart_timer(task_id);
    TEST_ASSERT_FALSE(result);

    // Resume and wait for completion
    scheduler.resume(task_id);
    scheduler.wait(task_id, 500);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    scheduler.stop();
}

TEST_CASE("Test restart_timer canceled task fails", "[utils][task_scheduler][restart_timer][error][canceled]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Canceled Task Fails Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed([&]() {
        g_counter++;
    }, 300, &task_id);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Cancel the task
    scheduler.cancel(task_id);

    // Try to restart timer for canceled task (should fail - task no longer exists)
    bool result = scheduler.restart_timer(task_id);
    TEST_ASSERT_FALSE(result);

    // Task should not execute
    vTaskDelay(pdMS_TO_TICKS(300));
    TEST_ASSERT_EQUAL(0, g_counter.load());

    scheduler.stop();
}

// ============================================================================
// restart_timer combined with other operations
// ============================================================================

TEST_CASE("Test restart_timer with suspend and resume", "[utils][task_scheduler][restart_timer][combined][suspend]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer with Suspend/Resume Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    auto start = esp_timer_get_time();

    TaskScheduler::TaskId task_id = 0;
    scheduler.post_delayed([&]() {
        g_execution_time = esp_timer_get_time();
        g_counter++;
        BROOKESIA_LOGI("Task executed");
    }, 300, &task_id);

    // Wait 100ms, restart timer
    vTaskDelay(pdMS_TO_TICKS(100));
    scheduler.restart_timer(task_id);
    BROOKESIA_LOGI("Timer restarted at 100ms");

    // Wait another 100ms, suspend
    vTaskDelay(pdMS_TO_TICKS(100));
    scheduler.suspend(task_id);
    BROOKESIA_LOGI("Task suspended at 200ms");

    // Wait 200ms while suspended
    vTaskDelay(pdMS_TO_TICKS(200));
    TEST_ASSERT_EQUAL(0, g_counter.load()); // Should not execute while suspended

    // Resume
    scheduler.resume(task_id);
    BROOKESIA_LOGI("Task resumed at 400ms");

    // Wait for completion
    scheduler.wait(task_id, 500);
    TEST_ASSERT_EQUAL(1, g_counter.load());

    auto elapsed = (g_execution_time.load() - start) / 1000;
    // Expected: 100 (initial) + 100 (to suspend) + 200 (suspended) + ~200 (remaining after resume) ≈ 600ms
    BROOKESIA_LOGI("Task executed after %1% ms", elapsed);

    scheduler.stop();
}

TEST_CASE("Test restart_timer stress test", "[utils][task_scheduler][restart_timer][stress]")
{
    BROOKESIA_LOGI("=== TaskScheduler restart_timer Stress Test ===");

    reset_counters();
    TaskScheduler scheduler;
    scheduler.start(TEST_SCHEDULER_CONFIG_GENERIC);

    TaskScheduler::TaskId task_id = 0;
    const int RESTART_COUNT = 20;
    const int RESTART_INTERVAL_MS = 30;
    const int TASK_DELAY_MS = 100;

    scheduler.post_delayed([&]() {
        g_counter++;
        BROOKESIA_LOGI("Task finally executed after %1% restarts", RESTART_COUNT);
    }, TASK_DELAY_MS, &task_id);

    // Rapidly restart timer many times
    for (int i = 0; i < RESTART_COUNT; i++) {
        vTaskDelay(pdMS_TO_TICKS(RESTART_INTERVAL_MS));
        if (scheduler.get_state(task_id) == TaskScheduler::TaskState::Running) {
            bool restarted = scheduler.restart_timer(task_id);
            if (restarted) {
                BROOKESIA_LOGI("Restart %1%/%2%", i + 1, RESTART_COUNT);
            }
        }
    }

    // Task should not have executed during restarts
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Wait for final execution
    vTaskDelay(pdMS_TO_TICKS(TASK_DELAY_MS + 50));
    TEST_ASSERT_EQUAL(1, g_counter.load());

    scheduler.stop();
}
