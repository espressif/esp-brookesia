/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"

using namespace esp_brookesia::lib_utils;

// ============================================================================
// Tests for operations without starting the scheduler
// All operations should return false when scheduler is not started
// ============================================================================

static std::atomic<int> g_counter{0};

static void simple_task()
{
    g_counter++;
}

static bool periodic_task()
{
    g_counter++;
    return true;
}

TEST_CASE("Test post without start returns false", "[utils][task_scheduler][without_start][post]")
{
    BROOKESIA_LOGI("=== Test post without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    g_counter = 0;
    TaskScheduler::TaskId task_id = 0;

    // post should return false
    bool result = scheduler.post(simple_task, &task_id);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, task_id);
    TEST_ASSERT_EQUAL(0, g_counter.load());

    BROOKESIA_LOGI("post correctly returned false when scheduler not started");
}

TEST_CASE("Test dispatch without start returns false", "[utils][task_scheduler][without_start][dispatch]")
{
    BROOKESIA_LOGI("=== Test dispatch without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    g_counter = 0;
    TaskScheduler::TaskId task_id = 0;

    // dispatch should return false
    bool result = scheduler.dispatch(simple_task, &task_id);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, task_id);
    TEST_ASSERT_EQUAL(0, g_counter.load());

    BROOKESIA_LOGI("dispatch correctly returned false when scheduler not started");
}

TEST_CASE("Test post_delayed without start returns false", "[utils][task_scheduler][without_start][post_delayed]")
{
    BROOKESIA_LOGI("=== Test post_delayed without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    g_counter = 0;
    TaskScheduler::TaskId task_id = 0;

    // post_delayed should return false
    bool result = scheduler.post_delayed(simple_task, 100, &task_id);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, task_id);

    BROOKESIA_LOGI("post_delayed correctly returned false when scheduler not started");
}

TEST_CASE("Test post_periodic without start returns false", "[utils][task_scheduler][without_start][post_periodic]")
{
    BROOKESIA_LOGI("=== Test post_periodic without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    g_counter = 0;
    TaskScheduler::TaskId task_id = 0;

    // post_periodic should return false
    bool result = scheduler.post_periodic(periodic_task, 100, &task_id);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL(0, task_id);

    BROOKESIA_LOGI("post_periodic correctly returned false when scheduler not started");
}

TEST_CASE("Test post_batch without start returns false", "[utils][task_scheduler][without_start][post_batch]")
{
    BROOKESIA_LOGI("=== Test post_batch without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    g_counter = 0;
    std::vector<TaskScheduler::OnceTask> tasks = {simple_task, simple_task, simple_task};
    std::vector<TaskScheduler::TaskId> task_ids;

    // post_batch should return false
    bool result = scheduler.post_batch(tasks, &task_ids);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_TRUE(task_ids.empty());
    TEST_ASSERT_EQUAL(0, g_counter.load());

    BROOKESIA_LOGI("post_batch correctly returned false when scheduler not started");
}

TEST_CASE("Test suspend without start returns false", "[utils][task_scheduler][without_start][suspend]")
{
    BROOKESIA_LOGI("=== Test suspend without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    // suspend should return false (even with invalid id)
    bool result = scheduler.suspend(1);
    TEST_ASSERT_FALSE(result);

    // suspend_group should return 0
    size_t count = scheduler.suspend_group("test_group");
    TEST_ASSERT_EQUAL(0, count);

    // suspend_all should return 0
    count = scheduler.suspend_all();
    TEST_ASSERT_EQUAL(0, count);

    BROOKESIA_LOGI("suspend operations correctly returned false/0 when scheduler not started");
}

TEST_CASE("Test resume without start returns false", "[utils][task_scheduler][without_start][resume]")
{
    BROOKESIA_LOGI("=== Test resume without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    // resume should return false (even with invalid id)
    bool result = scheduler.resume(1);
    TEST_ASSERT_FALSE(result);

    // resume_group should return 0
    size_t count = scheduler.resume_group("test_group");
    TEST_ASSERT_EQUAL(0, count);

    // resume_all should return 0
    count = scheduler.resume_all();
    TEST_ASSERT_EQUAL(0, count);

    BROOKESIA_LOGI("resume operations correctly returned false/0 when scheduler not started");
}

TEST_CASE("Test wait without start returns false", "[utils][task_scheduler][without_start][wait]")
{
    BROOKESIA_LOGI("=== Test wait without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    // wait should return false (even with invalid id)
    bool result = scheduler.wait(1, 100);
    TEST_ASSERT_FALSE(result);

    // wait_group should return false
    result = scheduler.wait_group("test_group", 100);
    TEST_ASSERT_FALSE(result);

    // wait_all should return false
    result = scheduler.wait_all(100);
    TEST_ASSERT_FALSE(result);

    BROOKESIA_LOGI("wait operations correctly returned false when scheduler not started");
}

TEST_CASE("Test restart_timer without start returns false", "[utils][task_scheduler][without_start][restart_timer]")
{
    BROOKESIA_LOGI("=== Test restart_timer without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    // restart_timer should return false (even with invalid id)
    bool result = scheduler.restart_timer(1);
    TEST_ASSERT_FALSE(result);

    BROOKESIA_LOGI("restart_timer correctly returned false when scheduler not started");
}

TEST_CASE("Test configure_group without start returns false", "[utils][task_scheduler][without_start][configure_group]")
{
    BROOKESIA_LOGI("=== Test configure_group without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    TaskScheduler::GroupConfig config{.enable_serial_execution = true};

    // configure_group should return false
    bool result = scheduler.configure_group("test_group", config);
    TEST_ASSERT_FALSE(result);

    BROOKESIA_LOGI("configure_group correctly returned false when scheduler not started");
}

TEST_CASE("Test comprehensive operations without start", "[utils][task_scheduler][without_start][comprehensive]")
{
    BROOKESIA_LOGI("=== Test comprehensive operations without start ===");

    TaskScheduler scheduler;
    TEST_ASSERT_FALSE(scheduler.is_running());

    g_counter = 0;
    TaskScheduler::TaskId task_id = 0;

    // All post variants should fail
    TEST_ASSERT_FALSE(scheduler.post(simple_task, &task_id));
    TEST_ASSERT_FALSE(scheduler.dispatch(simple_task, &task_id));
    TEST_ASSERT_FALSE(scheduler.post_delayed(simple_task, 100, &task_id));
    TEST_ASSERT_FALSE(scheduler.post_periodic(periodic_task, 100, &task_id));

    std::vector<TaskScheduler::OnceTask> tasks = {simple_task};
    std::vector<TaskScheduler::TaskId> ids;
    TEST_ASSERT_FALSE(scheduler.post_batch(tasks, &ids));

    // All suspend/resume variants should fail
    TEST_ASSERT_FALSE(scheduler.suspend(1));
    TEST_ASSERT_EQUAL(0, scheduler.suspend_group("group"));
    TEST_ASSERT_EQUAL(0, scheduler.suspend_all());

    TEST_ASSERT_FALSE(scheduler.resume(1));
    TEST_ASSERT_EQUAL(0, scheduler.resume_group("group"));
    TEST_ASSERT_EQUAL(0, scheduler.resume_all());

    // All wait variants should fail
    TEST_ASSERT_FALSE(scheduler.wait(1, 100));
    TEST_ASSERT_FALSE(scheduler.wait_group("group", 100));
    TEST_ASSERT_FALSE(scheduler.wait_all(100));

    // Other operations should fail
    TEST_ASSERT_FALSE(scheduler.restart_timer(1));
    TEST_ASSERT_FALSE(scheduler.configure_group("group", {}));

    // No tasks should have been executed
    TEST_ASSERT_EQUAL(0, g_counter.load());

    BROOKESIA_LOGI("All operations correctly failed when scheduler not started");
}

TEST_CASE("Test operations work after start", "[utils][task_scheduler][without_start][after_start]")
{
    BROOKESIA_LOGI("=== Test operations work after start ===");

    TaskScheduler scheduler;

    g_counter = 0;
    TaskScheduler::TaskId task_id = 0;

    // Operations should fail before start
    TEST_ASSERT_FALSE(scheduler.post(simple_task, &task_id));
    TEST_ASSERT_EQUAL(0, g_counter.load());

    // Start the scheduler
    scheduler.start();
    TEST_ASSERT_TRUE(scheduler.is_running());

    // Operations should succeed after start
    task_id = 0;
    TEST_ASSERT_TRUE(scheduler.post(simple_task, &task_id));
    TEST_ASSERT_TRUE(task_id > 0);

    // Wait for task completion
    TEST_ASSERT_TRUE(scheduler.wait(task_id, 1000));
    TEST_ASSERT_EQUAL(1, g_counter.load());

    // Stop and verify operations fail again
    scheduler.stop();
    TEST_ASSERT_FALSE(scheduler.is_running());

    task_id = 0;
    TEST_ASSERT_FALSE(scheduler.post(simple_task, &task_id));
    TEST_ASSERT_EQUAL(0, task_id);

    BROOKESIA_LOGI("Operations correctly work after start and fail after stop");
}
