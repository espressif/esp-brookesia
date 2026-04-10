/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include <future>
#include <map>
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

// ============================================================================
// Callback functionality tests
// ============================================================================

TEST_CASE("Test pre_execute_callback", "[utils][task_scheduler][callback][pre_execute]")
{
    BROOKESIA_LOGI("=== TaskScheduler PreExecuteCallback Test ===");

    struct CallbackData {
        std::atomic<int> pre_call_count{0};
        std::atomic<TaskScheduler::TaskId> last_task_id{0};
        std::atomic<int> last_task_type{0};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
    } cb_data;

    // All tasks in this test are posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};
    config.pre_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
    TaskScheduler::TaskId id, TaskScheduler::TaskType type) {
        cb_data.pre_call_count++;
        cb_data.last_task_id = id;
        cb_data.last_task_type = static_cast<int>(type);
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("Pre-execute: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        BROOKESIA_LOGI("Pre-execute: group='%1%', task_id=%2%, type=%3%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type));
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

    scheduler.stop();
    BROOKESIA_LOGI("Total pre-execute callbacks: %1%", cb_data.pre_call_count.load());
}

TEST_CASE("Test post_execute_callback", "[utils][task_scheduler][callback][post_execute]")
{
    BROOKESIA_LOGI("=== TaskScheduler PostExecuteCallback Test ===");

    struct CallbackData {
        std::atomic<int> post_call_count{0};
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        std::atomic<TaskScheduler::TaskId> last_task_id{0};
        std::atomic<int> last_task_type{0};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
    } cb_data;

    // All tasks in this test are posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};
    config.post_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                   TaskScheduler::TaskId id,
    TaskScheduler::TaskType type, bool success) {
        cb_data.post_call_count++;
        if (success) {
            cb_data.success_count++;
        } else {
            cb_data.failure_count++;
        }
        cb_data.last_task_id = id;
        cb_data.last_task_type = static_cast<int>(type);
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("Post-execute: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        BROOKESIA_LOGI("Post-execute: group='%1%', task_id=%2%, type=%3%, success=%4%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type), success);
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

    scheduler.stop();
    BROOKESIA_LOGI("Total post-execute callbacks: %1% (success: %2%, failure: %3%)",
                   cb_data.post_call_count.load(),
                   cb_data.success_count.load(),
                   cb_data.failure_count.load());
}

TEST_CASE("Test pre and post execute callbacks together", "[utils][task_scheduler][callback][combined]")
{
    BROOKESIA_LOGI("=== TaskScheduler Pre+Post Execute Callbacks Test ===");

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
        std::vector<std::pair<std::string, TaskScheduler::TaskId>> execution_order;
        boost::mutex mutex;
    } cb_data;

    // All tasks in this test are posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                  TaskScheduler::TaskId id,
    TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("PRE: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        {
            boost::lock_guard<boost::mutex> lock(cb_data.mutex);
            cb_data.execution_order.push_back({"PRE", id});
        }
        BROOKESIA_LOGI("PRE: group='%1%', task_id=%2%, type=%3%", group, id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    config.post_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                   TaskScheduler::TaskId id,
    TaskScheduler::TaskType type, bool success) {
        cb_data.post_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("POST: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        {
            boost::lock_guard<boost::mutex> lock(cb_data.mutex);
            cb_data.execution_order.push_back({"POST", id});
        }
        BROOKESIA_LOGI("POST: group='%1%', task_id=%2%, type=%3%, success=%4%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type), success);
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

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

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
    } cb_data;

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data](const TaskScheduler::Group & group, TaskScheduler::TaskId id,
    TaskScheduler::TaskType) {
        cb_data.pre_count++;
        BROOKESIA_LOGI("PRE: group='%1%', task_id=%2%", group, id);
    };

    config.post_execute_callback = [&cb_data](const TaskScheduler::Group & group, TaskScheduler::TaskId id,
    TaskScheduler::TaskType, bool success) {
        cb_data.post_count++;
        BROOKESIA_LOGI("POST: group='%1%', task_id=%2%, success=%3%", group, id, success);
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

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
    } cb_data;

    // The periodic task is posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                  TaskScheduler::TaskId id,
    TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("PRE: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        BROOKESIA_LOGI("PRE: group='%1%', task_id=%2%, type=%3%", group, id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    config.post_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                   TaskScheduler::TaskId id,
    TaskScheduler::TaskType, bool success) {
        cb_data.post_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("POST: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        BROOKESIA_LOGI("POST: group='%1%', task_id=%2%, success=%3%", group, id, success);
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

    scheduler.stop();
}

TEST_CASE("Test callback exception handling", "[utils][task_scheduler][callback][exception]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callback Exception Handling Test ===");

    struct CallbackData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> task_count{0};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
    } cb_data;

    // All tasks in this test are posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    // Pre-callback that throws exception
    config.pre_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                  TaskScheduler::TaskId id,
    TaskScheduler::TaskType) {
        cb_data.pre_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("PRE: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        BROOKESIA_LOGI("PRE callback (will throw): group='%1%', task_id=%2%", group, id);
        if (cb_data.pre_count == 2) {
            throw std::runtime_error("Pre-callback exception");
        }
    };

    config.post_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                   TaskScheduler::TaskId id,
    TaskScheduler::TaskType, bool success) {
        cb_data.post_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("POST: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        BROOKESIA_LOGI("POST callback: group='%1%', task_id=%2%, success=%3%", group, id, success);
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

    scheduler.stop();
}

TEST_CASE("Test callbacks with mutex lock", "[utils][task_scheduler][callback][mutex]")
{
    BROOKESIA_LOGI("=== TaskScheduler Callbacks with Mutex Lock Test ===");

    struct CallbackData {
        boost::mutex task_mutex;
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> task_execution_count{0};
        std::atomic<bool> lock_acquired_in_task{false};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
    } cb_data;

    // All tasks in this test are posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                  TaskScheduler::TaskId id,
    TaskScheduler::TaskType) {
        cb_data.pre_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("PRE: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        cb_data.task_mutex.lock(); // Lock in pre-execute
        BROOKESIA_LOGI("PRE: group='%1%', task_id=%2%, mutex locked", group, id);
    };

    config.post_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                   TaskScheduler::TaskId id,
    TaskScheduler::TaskType, bool success) {
        cb_data.post_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("POST: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        cb_data.task_mutex.unlock(); // Unlock in post-execute
        BROOKESIA_LOGI("POST: group='%1%', task_id=%2%, mutex unlocked, success=%3%", group, id, success);
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

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

    struct CallbackData {
        boost::mutex task_mutex;
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> locked_execution_count{0};
        std::atomic<int> group_check_fail{0}; // incremented when group != expected
    } cb_data;

    // All tasks in this test are posted without a group, so group must always be ""
    const TaskScheduler::Group kExpectedGroup = "";

    TaskScheduler::StartConfig config;
    config.worker_configs = {{.name = "worker", .stack_size = 8192}};

    config.pre_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                  TaskScheduler::TaskId id,
    TaskScheduler::TaskType type) {
        cb_data.pre_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("PRE: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        cb_data.task_mutex.lock();
        BROOKESIA_LOGI("PRE: group='%1%', task_id=%2%, type=%3%, locked",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type));
    };

    config.post_execute_callback = [&cb_data, &kExpectedGroup](const TaskScheduler::Group & group,
                                   TaskScheduler::TaskId id,
    TaskScheduler::TaskType type, bool) {
        cb_data.post_count++;
        if (group != kExpectedGroup) {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("POST: unexpected group='%1%' (expected='%2%') for Task[%3%]",
                           group, kExpectedGroup, id);
        }
        cb_data.task_mutex.unlock();
        BROOKESIA_LOGI("POST: group='%1%', task_id=%2%, type=%3%, unlocked",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type));
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
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load()); // All tasks should have group==""

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
// Group callback tests (via GroupConfig)
// ============================================================================

TEST_CASE("Test group pre_execute_callback via configure_group", "[utils][task_scheduler][callback][group_pre]")
{
    BROOKESIA_LOGI("=== TaskScheduler Group Pre-Execute Callback via configure_group Test ===");

    struct CallbackData {
        std::atomic<int> group_pre_count{0};
        std::atomic<int> ungrouped_task_count{0};
        std::atomic<int> grouped_task_count{0};
        std::atomic<int> group_check_ok{0};
        std::atomic<int> group_check_fail{0};
    } cb_data;

    TaskScheduler scheduler;
    scheduler.start();

    // Configure group with a pre-execute callback
    const TaskScheduler::Group kGroup = "myGroup";
    TaskScheduler::GroupConfig group_config;
    group_config.pre_execute_callback = [&cb_data, &kGroup](const TaskScheduler::Group & group,
                                        TaskScheduler::TaskId id,
    TaskScheduler::TaskType type) {
        cb_data.group_pre_count++;
        // Verify the group parameter matches the expected group
        if (group == kGroup) {
            cb_data.group_check_ok++;
        } else {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("Group PRE: expected '%1%', got '%2%' for Task[%3%]", kGroup, group, id);
        }
        BROOKESIA_LOGI("Group PRE: group='%1%', task_id=%2%, type=%3%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type));
    };
    TEST_ASSERT_TRUE(scheduler.configure_group(kGroup, group_config));

    // Task with group — should trigger group pre callback
    scheduler.post([&cb_data]() {
        cb_data.grouped_task_count++;
        BROOKESIA_LOGI("Grouped task executing");
    }, nullptr, kGroup);

    // Task without group — should NOT trigger group pre callback
    scheduler.post([&cb_data]() {
        cb_data.ungrouped_task_count++;
        BROOKESIA_LOGI("Ungrouped task executing");
    });

    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, cb_data.grouped_task_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.ungrouped_task_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.group_pre_count.load());  // Only the grouped task triggers it
    TEST_ASSERT_EQUAL(1, cb_data.group_check_ok.load());   // group parameter matches kGroup
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load());

    scheduler.stop();
    BROOKESIA_LOGI("✓ Group pre-execute callback via configure_group passed");
}

TEST_CASE("Test group post_execute_callback via configure_group", "[utils][task_scheduler][callback][group_post]")
{
    BROOKESIA_LOGI("=== TaskScheduler Group Post-Execute Callback via configure_group Test ===");

    struct CallbackData {
        std::atomic<int> group_post_count{0};
        std::atomic<int> success_count{0};
        std::atomic<int> failure_count{0};
        std::atomic<int> group_check_ok{0};
        std::atomic<int> group_check_fail{0};
    } cb_data;

    TaskScheduler scheduler;
    scheduler.start();

    const TaskScheduler::Group kGroup = "testGroup";
    TaskScheduler::GroupConfig group_config;
    group_config.post_execute_callback = [&cb_data, &kGroup](const TaskScheduler::Group & group,
                                         TaskScheduler::TaskId id,
    TaskScheduler::TaskType type, bool success) {
        cb_data.group_post_count++;
        if (success) {
            cb_data.success_count++;
        } else {
            cb_data.failure_count++;
        }
        // Verify the group parameter matches the expected group
        if (group == kGroup) {
            cb_data.group_check_ok++;
        } else {
            cb_data.group_check_fail++;
            BROOKESIA_LOGE("Group POST: expected '%1%', got '%2%' for Task[%3%]", kGroup, group, id);
        }
        BROOKESIA_LOGI("Group POST: group='%1%', task_id=%2%, type=%3%, success=%4%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type), success);
    };
    TEST_ASSERT_TRUE(scheduler.configure_group(kGroup, group_config));

    // Successful grouped task
    scheduler.post([]() {
        BROOKESIA_LOGI("Successful grouped task");
    }, nullptr, kGroup);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Failed grouped task
    scheduler.post([]() {
        BROOKESIA_LOGI("Failing grouped task");
        throw std::runtime_error("Intentional failure");
    }, nullptr, kGroup);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Ungrouped task — should NOT trigger group post callback
    scheduler.post([]() {
        BROOKESIA_LOGI("Ungrouped task");
    });
    vTaskDelay(pdMS_TO_TICKS(50));

    TEST_ASSERT_EQUAL(2, cb_data.group_post_count.load()); // Only the 2 grouped tasks
    TEST_ASSERT_EQUAL(1, cb_data.success_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.failure_count.load());
    TEST_ASSERT_EQUAL(2, cb_data.group_check_ok.load());   // Both grouped tasks passed group verification
    TEST_ASSERT_EQUAL(0, cb_data.group_check_fail.load());

    scheduler.stop();
    BROOKESIA_LOGI("✓ Group post-execute callback via configure_group passed");
}

TEST_CASE("Test global and group callbacks fire together", "[utils][task_scheduler][callback][global_and_group]")
{
    BROOKESIA_LOGI("=== TaskScheduler Global + Group Callbacks Together Test ===");

    struct CallbackData {
        std::atomic<int> global_pre_count{0};
        std::atomic<int> global_post_count{0};
        std::atomic<int> group_pre_count{0};
        std::atomic<int> group_post_count{0};
    } cb_data;

    // Register global callbacks via StartConfig
    TaskScheduler::StartConfig start_config;
    start_config.worker_configs = {{.name = "worker", .stack_size = 8192}};
    start_config.pre_execute_callback = [&cb_data](const TaskScheduler::Group & group,
    TaskScheduler::TaskId id, TaskScheduler::TaskType) {
        cb_data.global_pre_count++;
        BROOKESIA_LOGI("Global PRE: group='%1%', task_id=%2%", group, id);
    };
    start_config.post_execute_callback = [&cb_data](const TaskScheduler::Group & group,
                                         TaskScheduler::TaskId id, TaskScheduler::TaskType,
    bool success) {
        cb_data.global_post_count++;
        BROOKESIA_LOGI("Global POST: group='%1%', task_id=%2%, success=%3%", group, id, success);
    };

    TaskScheduler scheduler;
    scheduler.start(start_config);

    // Register per-group callbacks via configure_group
    const TaskScheduler::Group kGroup = "specialGroup";
    TaskScheduler::GroupConfig group_config;
    group_config.pre_execute_callback = [&cb_data, &kGroup](const TaskScheduler::Group & group,
    TaskScheduler::TaskId id, TaskScheduler::TaskType) {
        cb_data.group_pre_count++;
        // Group callback should only fire for tasks in kGroup
        TEST_ASSERT_EQUAL_STRING(kGroup.c_str(), group.c_str());
        BROOKESIA_LOGI("Group PRE: group='%1%', task_id=%2%", group, id);
    };
    group_config.post_execute_callback = [&cb_data, &kGroup](const TaskScheduler::Group & group,
                                         TaskScheduler::TaskId id, TaskScheduler::TaskType,
    bool success) {
        cb_data.group_post_count++;
        // Group callback should only fire for tasks in kGroup
        TEST_ASSERT_EQUAL_STRING(kGroup.c_str(), group.c_str());
        BROOKESIA_LOGI("Group POST: group='%1%', task_id=%2%, success=%3%", group, id, success);
    };
    TEST_ASSERT_TRUE(scheduler.configure_group(kGroup, group_config));

    // Post one task to the group — both global and group callbacks should fire
    scheduler.post([]() {
        BROOKESIA_LOGI("Grouped task executing");
    }, nullptr, kGroup);
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(1, cb_data.global_pre_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.global_post_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.group_pre_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.group_post_count.load());

    // Post one task without group — only global callbacks should fire
    scheduler.post([]() {
        BROOKESIA_LOGI("Ungrouped task executing");
    });
    vTaskDelay(pdMS_TO_TICKS(100));

    TEST_ASSERT_EQUAL(2, cb_data.global_pre_count.load());
    TEST_ASSERT_EQUAL(2, cb_data.global_post_count.load());
    TEST_ASSERT_EQUAL(1, cb_data.group_pre_count.load());   // Unchanged
    TEST_ASSERT_EQUAL(1, cb_data.group_post_count.load());  // Unchanged

    scheduler.stop();
    BROOKESIA_LOGI("✓ Global + group callbacks fired correctly");
}

TEST_CASE("Test group callback can be updated or cleared via re-configure", "[utils][task_scheduler][callback][group_update]")
{
    BROOKESIA_LOGI("=== TaskScheduler Group Callback Update/Clear via configure_group Test ===");

    struct CallbackData {
        std::atomic<int> v1_count{0};
        std::atomic<int> v2_count{0};
    } cb_data;

    TaskScheduler scheduler;
    scheduler.start();

    const TaskScheduler::Group kGroup = "updateGroup";

    // Step 1: Register v1 callback
    TaskScheduler::GroupConfig config_v1;
    config_v1.pre_execute_callback = [&cb_data, &kGroup](const TaskScheduler::Group & group,
    TaskScheduler::TaskId, TaskScheduler::TaskType) {
        cb_data.v1_count++;
        TEST_ASSERT_EQUAL_STRING(kGroup.c_str(), group.c_str());
        BROOKESIA_LOGI("v1 PRE callback: group='%1%'", group);
    };
    scheduler.configure_group(kGroup, config_v1);

    scheduler.post([]() {
        BROOKESIA_LOGI("Task A");
    }, nullptr, kGroup);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, cb_data.v1_count.load());
    TEST_ASSERT_EQUAL(0, cb_data.v2_count.load());

    // Step 2: Replace with v2 callback
    TaskScheduler::GroupConfig config_v2;
    config_v2.pre_execute_callback = [&cb_data, &kGroup](const TaskScheduler::Group & group,
    TaskScheduler::TaskId, TaskScheduler::TaskType) {
        cb_data.v2_count++;
        TEST_ASSERT_EQUAL_STRING(kGroup.c_str(), group.c_str());
        BROOKESIA_LOGI("v2 PRE callback: group='%1%'", group);
    };
    scheduler.configure_group(kGroup, config_v2);

    scheduler.post([]() {
        BROOKESIA_LOGI("Task B");
    }, nullptr, kGroup);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, cb_data.v1_count.load()); // v1 should not increase
    TEST_ASSERT_EQUAL(1, cb_data.v2_count.load()); // v2 fires

    // Step 3: Clear the callback (nullptr)
    TaskScheduler::GroupConfig config_clear;
    config_clear.pre_execute_callback = nullptr; // explicitly clear
    scheduler.configure_group(kGroup, config_clear);

    scheduler.post([]() {
        BROOKESIA_LOGI("Task C (no callback)");
    }, nullptr, kGroup);
    vTaskDelay(pdMS_TO_TICKS(50));
    TEST_ASSERT_EQUAL(1, cb_data.v1_count.load()); // Unchanged
    TEST_ASSERT_EQUAL(1, cb_data.v2_count.load()); // Unchanged — callback was cleared

    scheduler.stop();
    BROOKESIA_LOGI("✓ Group callback update/clear via configure_group passed");
}

TEST_CASE("Test multi-group concurrent callbacks", "[utils][task_scheduler][callback][multi_group]")
{
    BROOKESIA_LOGI("=== TaskScheduler Multi-Group Concurrent Callbacks Test ===");

    constexpr int kTasksPerGroup = 5;
    constexpr int kUngroupedTasks = 3;
    constexpr int kTotalTasks = kTasksPerGroup * 3 + kUngroupedTasks; // 18

    const TaskScheduler::Group kGroupAlpha = "alpha";
    const TaskScheduler::Group kGroupBeta  = "beta";
    const TaskScheduler::Group kGroupGamma = "gamma";

    // --- Global callback data: tracks per-group call counts across all tasks ---
    struct GlobalData {
        std::atomic<int> total_pre{0};
        std::atomic<int> total_post{0};
        boost::mutex mutex;
        std::map<TaskScheduler::Group, int> pre_per_group;
        std::map<TaskScheduler::Group, int> post_per_group;
    } global_data;

    // --- Per-group data: each group independently verifies its own callback ---
    struct GroupData {
        std::atomic<int> pre_count{0};
        std::atomic<int> post_count{0};
        std::atomic<int> task_exec_count{0};
        std::atomic<int> group_check_fail{0}; // must stay 0
    };
    GroupData alpha_data, beta_data, gamma_data;

    // --- Configure scheduler with 2 parallel workers ---
    TaskScheduler::StartConfig start_config;
    start_config.worker_configs = {
        {.name = "worker0", .stack_size = 8192},
        {.name = "worker1", .stack_size = 8192},
    };
    start_config.pre_execute_callback = [&global_data](const TaskScheduler::Group & group,
                                        TaskScheduler::TaskId id,
    TaskScheduler::TaskType type) {
        global_data.total_pre++;
        {
            boost::lock_guard<boost::mutex> lock(global_data.mutex);
            global_data.pre_per_group[group]++;
        }
        BROOKESIA_LOGI("Global PRE: group='%1%', task_id=%2%, type=%3%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type));
    };
    start_config.post_execute_callback = [&global_data](const TaskScheduler::Group & group,
                                         TaskScheduler::TaskId id,
    TaskScheduler::TaskType type, bool success) {
        global_data.total_post++;
        {
            boost::lock_guard<boost::mutex> lock(global_data.mutex);
            global_data.post_per_group[group]++;
        }
        BROOKESIA_LOGI("Global POST: group='%1%', task_id=%2%, type=%3%, success=%4%",
                       group, id, BROOKESIA_DESCRIBE_TO_STR(type), success);
    };

    TaskScheduler scheduler;
    scheduler.start(start_config);

    // --- Build and register per-group callbacks ---
    // Lambda factory: each group callback verifies the `group` parameter matches its own name.
    auto make_group_config = [](const TaskScheduler::Group & expected_group, GroupData & gd) {
        TaskScheduler::GroupConfig config;
        config.pre_execute_callback = [&expected_group, &gd](const TaskScheduler::Group & group,
                                      TaskScheduler::TaskId id,
        TaskScheduler::TaskType) {
            gd.pre_count++;
            if (group != expected_group) {
                gd.group_check_fail++;
                BROOKESIA_LOGE("Group PRE: expected '%1%', got '%2%' for Task[%3%]",
                               expected_group, group, id);
            }
            BROOKESIA_LOGI("Group PRE: group='%1%', task_id=%2%", group, id);
        };
        config.post_execute_callback = [&expected_group, &gd](const TaskScheduler::Group & group,
                                       TaskScheduler::TaskId id,
        TaskScheduler::TaskType, bool success) {
            gd.post_count++;
            if (group != expected_group) {
                gd.group_check_fail++;
                BROOKESIA_LOGE("Group POST: expected '%1%', got '%2%' for Task[%3%]",
                               expected_group, group, id);
            }
            BROOKESIA_LOGI("Group POST: group='%1%', task_id=%2%, success=%3%", group, id, success);
        };
        return config;
    };

    TEST_ASSERT_TRUE(scheduler.configure_group(kGroupAlpha, make_group_config(kGroupAlpha, alpha_data)));
    TEST_ASSERT_TRUE(scheduler.configure_group(kGroupBeta,  make_group_config(kGroupBeta,  beta_data)));
    TEST_ASSERT_TRUE(scheduler.configure_group(kGroupGamma, make_group_config(kGroupGamma, gamma_data)));

    // --- Post all tasks in rapid succession to stress concurrency ---
    // Interleave group tasks to maximise scheduling overlap.
    for (int i = 0; i < kTasksPerGroup; i++) {
        scheduler.post([&alpha_data, i]() {
            alpha_data.task_exec_count++;
            BROOKESIA_LOGI("Alpha task %1% executing", i);
            vTaskDelay(pdMS_TO_TICKS(5));
        }, nullptr, kGroupAlpha);

        scheduler.post([&beta_data, i]() {
            beta_data.task_exec_count++;
            BROOKESIA_LOGI("Beta task %1% executing", i);
            vTaskDelay(pdMS_TO_TICKS(5));
        }, nullptr, kGroupBeta);

        scheduler.post([&gamma_data, i]() {
            gamma_data.task_exec_count++;
            BROOKESIA_LOGI("Gamma task %1% executing", i);
            vTaskDelay(pdMS_TO_TICKS(5));
        }, nullptr, kGroupGamma);
    }

    // Ungrouped tasks: should trigger global callbacks only, not any group callback.
    for (int i = 0; i < kUngroupedTasks; i++) {
        scheduler.post([i]() {
            BROOKESIA_LOGI("Ungrouped task %1% executing", i);
            vTaskDelay(pdMS_TO_TICKS(5));
        });
    }

    // Wait long enough for all tasks to finish across both workers.
    vTaskDelay(pdMS_TO_TICKS(1000));

    // --- Verify task execution counts ---
    TEST_ASSERT_EQUAL(kTasksPerGroup, alpha_data.task_exec_count.load());
    TEST_ASSERT_EQUAL(kTasksPerGroup, beta_data.task_exec_count.load());
    TEST_ASSERT_EQUAL(kTasksPerGroup, gamma_data.task_exec_count.load());

    // --- Verify per-group callback counts (group callbacks must NOT fire for other groups) ---
    TEST_ASSERT_EQUAL(kTasksPerGroup, alpha_data.pre_count.load());
    TEST_ASSERT_EQUAL(kTasksPerGroup, alpha_data.post_count.load());
    TEST_ASSERT_EQUAL(0, alpha_data.group_check_fail.load());

    TEST_ASSERT_EQUAL(kTasksPerGroup, beta_data.pre_count.load());
    TEST_ASSERT_EQUAL(kTasksPerGroup, beta_data.post_count.load());
    TEST_ASSERT_EQUAL(0, beta_data.group_check_fail.load());

    TEST_ASSERT_EQUAL(kTasksPerGroup, gamma_data.pre_count.load());
    TEST_ASSERT_EQUAL(kTasksPerGroup, gamma_data.post_count.load());
    TEST_ASSERT_EQUAL(0, gamma_data.group_check_fail.load());

    // --- Verify global callback saw every task exactly once ---
    TEST_ASSERT_EQUAL(kTotalTasks, global_data.total_pre.load());
    TEST_ASSERT_EQUAL(kTotalTasks, global_data.total_post.load());

    // --- Verify global callback received correct group labels for each task ---
    {
        boost::lock_guard<boost::mutex> lock(global_data.mutex);
        TEST_ASSERT_EQUAL(kTasksPerGroup,  global_data.pre_per_group[kGroupAlpha]);
        TEST_ASSERT_EQUAL(kTasksPerGroup,  global_data.pre_per_group[kGroupBeta]);
        TEST_ASSERT_EQUAL(kTasksPerGroup,  global_data.pre_per_group[kGroupGamma]);
        TEST_ASSERT_EQUAL(kUngroupedTasks, global_data.pre_per_group[""]);
        TEST_ASSERT_EQUAL(kTasksPerGroup,  global_data.post_per_group[kGroupAlpha]);
        TEST_ASSERT_EQUAL(kTasksPerGroup,  global_data.post_per_group[kGroupBeta]);
        TEST_ASSERT_EQUAL(kTasksPerGroup,  global_data.post_per_group[kGroupGamma]);
        TEST_ASSERT_EQUAL(kUngroupedTasks, global_data.post_per_group[""]);
    }

    BROOKESIA_LOGI("Summary — total_pre=%1%, total_post=%2% | "
                   "alpha(pre=%3%, post=%4%) | beta(pre=%5%, post=%6%) | gamma(pre=%7%, post=%8%) | "
                   "ungrouped(pre=%9%, post=%10%)",
                   global_data.total_pre.load(), global_data.total_post.load(),
                   alpha_data.pre_count.load(), alpha_data.post_count.load(),
                   beta_data.pre_count.load(),  beta_data.post_count.load(),
                   gamma_data.pre_count.load(), gamma_data.post_count.load(),
                   global_data.pre_per_group[""], global_data.post_per_group[""]);

    scheduler.stop();
    BROOKESIA_LOGI("✓ Multi-group concurrent callbacks test passed");
}
