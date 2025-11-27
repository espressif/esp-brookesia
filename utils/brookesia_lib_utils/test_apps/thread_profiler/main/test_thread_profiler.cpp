/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <memory>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/thread_profiler.hpp"

using namespace esp_brookesia::lib_utils;

// Task creation type
enum class TaskType {
    FreeRTOS,   // xTaskCreate
    StdThread,  // std::thread
    BoostThread // boost::thread
};

// Test task handles for cleanup
static ThreadProfiler &profiler = ThreadProfiler::get_instance();
static std::vector<TaskHandle_t> test_task_handles;
static std::vector<std::unique_ptr<std::thread>> test_std_threads;
static std::vector<std::unique_ptr<boost::thread>> test_boost_threads;

// Atomic counters for test tasks
static std::atomic<bool> g_thread_need_stop{false};
static std::atomic<int> g_callback_counter{0};
static std::atomic<int> g_high_cpu_task_counter{0};

// Helper function to reset counters
void reset_counters()
{
    g_callback_counter = 0;
    g_high_cpu_task_counter = 0;
}

// Helper function to take a snapshot (using static methods)
std::shared_ptr<ThreadProfiler::ProfileSnapshot> take_snapshot_helper()
{
    auto start_result = ThreadProfiler::sample_tasks();
    if (!start_result) {
        return nullptr;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    auto end_result = ThreadProfiler::sample_tasks();
    if (!end_result) {
        return nullptr;
    }
    return ThreadProfiler::take_snapshot(*start_result, *end_result);
}

// Helper function to create dummy test tasks
void create_test_task(
    const char *name, UBaseType_t priority, uint32_t stack_size, void (*task_func)(void *),
    TaskType type = TaskType::FreeRTOS
)
{
    switch (type) {
    case TaskType::FreeRTOS: {
        TaskHandle_t handle = nullptr;
        xTaskCreate(task_func, name, stack_size, reinterpret_cast<void *>(type), priority, &handle);
        test_task_handles.push_back(handle);
        BROOKESIA_LOGI("Created FreeRTOS task: %1%", name);
        break;
    }
    case TaskType::StdThread: {
        ThreadConfigGuard guard({
            .name = name,
            .priority = static_cast<size_t>(priority),
            .stack_size = static_cast<size_t>(stack_size),
        });
        auto thread = std::make_unique<std::thread>(task_func, reinterpret_cast<void *>(type));
        test_std_threads.push_back(std::move(thread));
        BROOKESIA_LOGI("Created std::thread: %1%", name);
        break;
    }
    case TaskType::BoostThread: {
        ThreadConfigGuard guard({
            .name = name,
            .priority = static_cast<size_t>(priority),
            .stack_size = static_cast<size_t>(stack_size),
        });
        auto thread = std::make_unique<boost::thread>(task_func, (void *)type);
        test_boost_threads.push_back(std::move(thread));
        BROOKESIA_LOGI("Created boost::thread: %1%", name);
        break;
    }
    }
}

// Helper function to cleanup test tasks
void cleanup_test_tasks()
{
    g_thread_need_stop = true;

    // Cleanup FreeRTOS tasks
    for (auto handle : test_task_handles) {
        if (handle) {
            vTaskDelete(handle);
        }
    }
    test_task_handles.clear();

    // Cleanup std::thread tasks
    for (auto &thread : test_std_threads) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    test_std_threads.clear();

    // Cleanup boost::thread tasks
    for (auto &thread : test_boost_threads) {
        if (thread && thread->joinable()) {
            thread->join();
        }
    }
    test_boost_threads.clear();

    g_thread_need_stop = false;

    profiler.reset_profiling();
}

// Dummy task functions
void idle_task(void *pvParameters)
{
    TaskType task_type = static_cast<TaskType>(reinterpret_cast<uintptr_t>(pvParameters));
    while (!g_thread_need_stop || (task_type == TaskType::FreeRTOS)) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void busy_task(void *pvParameters)
{
    TaskType task_type = static_cast<TaskType>(reinterpret_cast<uintptr_t>(pvParameters));
    while (!g_thread_need_stop || (task_type == TaskType::FreeRTOS)) {
        // Busy loop to consume CPU
        for (int i = 0; i < 10000; i++) {
            g_high_cpu_task_counter++;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void periodic_task(void *pvParameters)
{
    TaskType task_type = static_cast<TaskType>(reinterpret_cast<uintptr_t>(pvParameters));
    while (!g_thread_need_stop || (task_type == TaskType::FreeRTOS)) {
        // Busy loop to consume CPU
        for (int i = 0; i < 10000; i++) {
            g_high_cpu_task_counter++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_CASE("Test basic configuration", "[utils][thread_profiler][basic][config]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Basic Configuration Test ===");

    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 500,
        .profiling_interval_ms = 1000,
        .primary_sort = ThreadProfiler::PrimarySortBy::CoreId,
        .secondary_sort = ThreadProfiler::SecondarySortBy::CpuPercent,
        .enable_auto_logging = false,
    };

    bool result = profiler.configure_profiling(config);
    TEST_ASSERT_TRUE(result);

    auto retrieved_config = profiler.get_profiling_config();
    TEST_ASSERT_FALSE(retrieved_config.enable_auto_logging);
    TEST_ASSERT_EQUAL(500, retrieved_config.sampling_duration_ms);
    TEST_ASSERT_EQUAL(1000, retrieved_config.profiling_interval_ms);
    TEST_ASSERT_EQUAL(static_cast<int>(ThreadProfiler::PrimarySortBy::CoreId), static_cast<int>(retrieved_config.primary_sort));
    TEST_ASSERT_EQUAL(static_cast<int>(ThreadProfiler::SecondarySortBy::CpuPercent), static_cast<int>(retrieved_config.secondary_sort));
}

#if BROOKESIA_UTILS_THREAD_PROFILER_AVAILABLE
TEST_CASE("Test snapshot with custom tasks", "[utils][thread_profiler][basic][custom_tasks]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Snapshot with Custom Tasks Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take snapshot using static methods
    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    // Check if our test tasks are in the snapshot
    bool found_test_task = false;
    for (const auto &task : snapshot->tasks) {
        if (task.name.find("Idle") != std::string::npos) {
            found_test_task = true;
            BROOKESIA_LOGI("Found test task: %1%, CPU: %2%%%", task.name.c_str(), task.cpu_percent);
        }
    }
    TEST_ASSERT_TRUE(found_test_task);

    cleanup_test_tasks();
}

TEST_CASE("Test print snapshot", "[utils][thread_profiler][basic][print]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Print Snapshot Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take snapshot using static methods
    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    // This should not crash
    ThreadProfiler::sort_tasks(snapshot->tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(*snapshot, config.primary_sort, config.secondary_sort);

    cleanup_test_tasks();
}

// ============================================================================
// Sorting Tests
// ============================================================================

TEST_CASE("Test all sorting methods", "[utils][thread_profiler][comprehensive][all_sorts]")
{
    BROOKESIA_LOGI("=== ThreadProfiler All Sorting Methods Test ===");

    reset_counters();

    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run

    // Test different secondary sorting methods (all with CoreId primary sort)
    const ThreadProfiler::SecondarySortBy secondary_sorts[] = {
        ThreadProfiler::SecondarySortBy::CpuPercent,
        ThreadProfiler::SecondarySortBy::Priority,
        ThreadProfiler::SecondarySortBy::StackUsage,
        ThreadProfiler::SecondarySortBy::Name
    };

    const char *sort_names[] = {
        "CoreId+CpuPercent",
        "CoreId+Priority",
        "CoreId+StackUsage",
        "CoreId+Name"
    };

    for (size_t i = 0; i < sizeof(secondary_sorts) / sizeof(secondary_sorts[0]); i++) {
        BROOKESIA_LOGI("--- Testing sort by: %1% ---", sort_names[i]);

        ThreadProfiler::ProfilingConfig config = {
            .primary_sort = ThreadProfiler::PrimarySortBy::CoreId,
            .secondary_sort = secondary_sorts[i],
            .enable_auto_logging = false,
        };
        profiler.configure_profiling(config);

        auto snapshot = take_snapshot_helper();
        TEST_ASSERT_NOT_NULL(snapshot.get());
        TEST_ASSERT_GREATER_THAN(0, snapshot->tasks.size());

        ThreadProfiler::sort_tasks(snapshot->tasks, config.primary_sort, config.secondary_sort);
        ThreadProfiler::print_snapshot(*snapshot, config.primary_sort, config.secondary_sort);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // Also test with primary sort disabled
    BROOKESIA_LOGI("--- Testing sort by: None+CpuPercent ---");
    ThreadProfiler::ProfilingConfig config_no_primary = {
        .primary_sort = ThreadProfiler::PrimarySortBy::None,
        .secondary_sort = ThreadProfiler::SecondarySortBy::CpuPercent,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config_no_primary);

    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());
    TEST_ASSERT_GREATER_THAN(0, snapshot->tasks.size());

    ThreadProfiler::sort_tasks(snapshot->tasks, config_no_primary.primary_sort, config_no_primary.secondary_sort);
    ThreadProfiler::print_snapshot(*snapshot, config_no_primary.primary_sort, config_no_primary.secondary_sort);

    cleanup_test_tasks();
}

// ============================================================================
// Query Tests
// ============================================================================

TEST_CASE("Test get task by name", "[utils][thread_profiler][query][name]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Get Task by Name Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take snapshot using static methods
    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());
    TEST_ASSERT_GREATER_THAN(0, snapshot->tasks.size());

    // Try to find Idle task
    ThreadProfiler::TaskInfo task_info;
    bool found = ThreadProfiler::get_task_by_name(*snapshot, "Idle", task_info);
    if (found) {
        BROOKESIA_LOGI("Found Idle task: CPU=%1%%%, Core=%2%",
                       task_info.cpu_percent,
                       task_info.core_id);
        TEST_ASSERT_TRUE(task_info.name == std::string("Idle"));
    } else {
        // Try Busy or Periodic task
        found = ThreadProfiler::get_task_by_name(*snapshot, "Busy", task_info);
        if (!found) {
            found = ThreadProfiler::get_task_by_name(*snapshot, "Periodic", task_info);
        }
        TEST_ASSERT_TRUE(found);
    }

    // Try to find non-existent task
    found = ThreadProfiler::get_task_by_name(*snapshot, "NonExistentTask123", task_info);
    TEST_ASSERT_FALSE(found);

    cleanup_test_tasks();
}

TEST_CASE("Test get tasks above threshold", "[utils][thread_profiler][query][threshold]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Get Tasks Above Threshold Test ===");

    reset_counters();
    // Create test tasks with different priorities
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take snapshot using static methods
    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    BROOKESIA_LOGI("--- Print Whole Snapshot ---");
    ThreadProfiler::sort_tasks(snapshot->tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(*snapshot, config.primary_sort, config.secondary_sort);

    // Test CPU threshold
    BROOKESIA_LOGI("--- Testing CPU Threshold ---");
    auto high_cpu_tasks = ThreadProfiler::get_tasks_above_threshold(*snapshot, ThreadProfiler::ThresholdType::CpuPercent, 5);
    BROOKESIA_LOGI("Tasks with >= 5%% CPU: %1%", high_cpu_tasks.size());
    for (const auto &task : high_cpu_tasks) {
        TEST_ASSERT_GREATER_OR_EQUAL(5, task.cpu_percent);
        BROOKESIA_LOGI("  %1%: %2%%%", task.name.c_str(), task.cpu_percent);
    }
    ThreadProfiler::ProfileSnapshot filtered_snapshot = *snapshot;
    filtered_snapshot.tasks = high_cpu_tasks;
    ThreadProfiler::sort_tasks(filtered_snapshot.tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(filtered_snapshot, config.primary_sort, config.secondary_sort);

    // Test Priority threshold
    BROOKESIA_LOGI("--- Testing Priority Threshold ---");
    auto high_pri_tasks = ThreadProfiler::get_tasks_above_threshold(*snapshot, ThreadProfiler::ThresholdType::Priority, 5);
    BROOKESIA_LOGI("Tasks with priority >= 5: %1%", high_pri_tasks.size());
    for (const auto &task : high_pri_tasks) {
        TEST_ASSERT_GREATER_OR_EQUAL(5, task.priority);
        BROOKESIA_LOGI("  %1%: priority=%2%", task.name.c_str(), task.priority);
    }
    filtered_snapshot.tasks = high_pri_tasks;
    ThreadProfiler::sort_tasks(filtered_snapshot.tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(filtered_snapshot, config.primary_sort, config.secondary_sort);

    // Test StackUsage threshold
    BROOKESIA_LOGI("--- Testing StackUsage Threshold ---");
    auto low_stack_tasks = ThreadProfiler::get_tasks_above_threshold(*snapshot, ThreadProfiler::ThresholdType::StackUsage, 1024);
    BROOKESIA_LOGI("Tasks with stack HWM <= 1024 bytes: %1%", low_stack_tasks.size());
    for (const auto &task : low_stack_tasks) {
        TEST_ASSERT_LESS_OR_EQUAL(1024, task.stack_high_water_mark);
        BROOKESIA_LOGI("  %1%: stack_hwm=%2% bytes", task.name.c_str(), task.stack_high_water_mark);
    }
    filtered_snapshot.tasks = low_stack_tasks;
    ThreadProfiler::sort_tasks(filtered_snapshot.tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(filtered_snapshot, config.primary_sort, config.secondary_sort);

    // Test CpuPercent threshold (explicit)
    BROOKESIA_LOGI("--- Testing CpuPercent Threshold (>= 10%%) ---");
    auto default_tasks = ThreadProfiler::get_tasks_above_threshold(*snapshot, ThreadProfiler::ThresholdType::CpuPercent, 10);
    BROOKESIA_LOGI("Tasks with >= 10%% CPU: %1%", default_tasks.size());
    for (const auto &task : default_tasks) {
        TEST_ASSERT_GREATER_OR_EQUAL(10, task.cpu_percent);
        BROOKESIA_LOGI("  %1%: %2%%%", task.name.c_str(), task.cpu_percent);
    }
    filtered_snapshot.tasks = default_tasks;
    ThreadProfiler::sort_tasks(filtered_snapshot.tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(filtered_snapshot, config.primary_sort, config.secondary_sort);

    cleanup_test_tasks();
}

TEST_CASE("Test get latest snapshot", "[utils][thread_profiler][query][latest]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Get Latest Snapshot Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run

    // No snapshot yet
    auto snapshot1 = profiler.get_profiling_latest_snapshot();
    TEST_ASSERT_NULL(snapshot1.get());

    // Take a snapshot
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take snapshot manually using static methods
    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    // Note: get_profiling_latest_snapshot() only returns snapshot when profiling is active
    // So we need to start profiling to get latest snapshot
    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();
    profiler.start_profiling(scheduler, 500, 1000);
    vTaskDelay(pdMS_TO_TICKS(2000));
    profiler.stop_profiling();

    // Now we should have a latest snapshot
    auto snapshot2 = profiler.get_profiling_latest_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot2.get());
    TEST_ASSERT_GREATER_THAN(0, snapshot2->tasks.size());

    cleanup_test_tasks();
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_CASE("Test callback on snapshot", "[utils][thread_profiler][callback][basic]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Callback Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register callback - RAII scope
        auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
            BROOKESIA_LOGI("Callback invoked, task count: %1%", snapshot.tasks.size());
        });

        bool result = profiler.start_profiling(scheduler);
        TEST_ASSERT_TRUE(result);
        TEST_ASSERT_TRUE(profiler.is_profiling());

        // Wait for at least 2 callbacks
        vTaskDelay(pdMS_TO_TICKS(2000));

        profiler.stop_profiling();

        TEST_ASSERT_GREATER_OR_EQUAL(2, g_callback_counter.load());
        BROOKESIA_LOGI("Callback invoked %1% times", g_callback_counter.load());

        // conn will auto-disconnect here (RAII)
    }

    cleanup_test_tasks();
}

TEST_CASE("Test callback for high CPU detection", "[utils][thread_profiler][callback][high_cpu]")
{
    BROOKESIA_LOGI("=== ThreadProfiler High CPU Detection Callback Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 1000,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    std::atomic<int> high_cpu_detected{0};

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register callback - RAII scope
        auto conn = profiler.connect_profiling_signal([&high_cpu_detected](const ThreadProfiler::ProfileSnapshot & snapshot) {
            for (const auto &task : snapshot.tasks) {
                if (task.cpu_percent > 30) {
                    high_cpu_detected++;
                    BROOKESIA_LOGW("High CPU task detected: %1% (%2%%%)", task.name.c_str(), task.cpu_percent);
                }
            }
        });

        profiler.start_profiling(scheduler);

        vTaskDelay(pdMS_TO_TICKS(1500));

        profiler.stop_profiling();

        BROOKESIA_LOGI("High CPU tasks detected: %1%", high_cpu_detected.load());

        // conn will auto-disconnect here (RAII)
    }

    cleanup_test_tasks();
}

// ============================================================================
// Threshold Signal Tests
// ============================================================================

TEST_CASE("Test multiple threshold callbacks", "[utils][thread_profiler][threshold][signal][multiple]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Multiple Threshold Callbacks Test ===");

    reset_counters();
    create_test_task("Task1", 5, 2048, idle_task);
    create_test_task("Task2", 10, 2048, busy_task);

    vTaskDelay(pdMS_TO_TICKS(100));
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 500,
        .profiling_interval_ms = 1000,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    std::atomic<int> cpu_callback_count{0};
    std::atomic<int> priority_callback_count{0};

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register multiple threshold callbacks - RAII scope
        auto cpu_conn = profiler.connect_threshold_signal(
                            ThreadProfiler::ThresholdType::CpuPercent,
                            10,
        [&](const std::vector<ThreadProfiler::TaskInfo> &tasks) {
            cpu_callback_count++;
            BROOKESIA_LOGI("CPU threshold (>= 10%%) triggered, %1% tasks detected", tasks.size());
            ThreadProfiler::ProfileSnapshot filtered_snapshot;
            filtered_snapshot.tasks = tasks;
            ThreadProfiler::sort_tasks(filtered_snapshot.tasks, config.primary_sort, config.secondary_sort);
            ThreadProfiler::print_snapshot(filtered_snapshot, config.primary_sort, config.secondary_sort);
        }
                        );

        auto priority_conn = profiler.connect_threshold_signal(
                                 ThreadProfiler::ThresholdType::Priority,
                                 8,
        [&](const std::vector<ThreadProfiler::TaskInfo> &tasks) {
            priority_callback_count++;
            BROOKESIA_LOGI("Priority threshold (>= 8) triggered, %1% tasks detected", tasks.size());
            ThreadProfiler::ProfileSnapshot filtered_snapshot;
            filtered_snapshot.tasks = tasks;
            ThreadProfiler::sort_tasks(filtered_snapshot.tasks, config.primary_sort, config.secondary_sort);
            ThreadProfiler::print_snapshot(filtered_snapshot, config.primary_sort, config.secondary_sort);
        }
                             );

        profiler.start_profiling(scheduler);

        vTaskDelay(pdMS_TO_TICKS(2500));

        profiler.stop_profiling();

        BROOKESIA_LOGI("--- Print Whole Snapshot ---");
        auto latest_snapshot = profiler.get_profiling_latest_snapshot();
        if (latest_snapshot) {
            ThreadProfiler::sort_tasks(latest_snapshot->tasks, config.primary_sort, config.secondary_sort);
            ThreadProfiler::print_snapshot(*latest_snapshot, config.primary_sort, config.secondary_sort);
        }

        BROOKESIA_LOGI("CPU callbacks: %1%, Priority callbacks: %2%",
                       cpu_callback_count.load(), priority_callback_count.load());
        TEST_ASSERT_GREATER_THAN(0, cpu_callback_count.load());
        TEST_ASSERT_GREATER_THAN(0, priority_callback_count.load());

        // cpu_conn and priority_conn will auto-disconnect here (RAII)
    }

    cleanup_test_tasks();
}

// ============================================================================
// TaskScheduler Integration Tests
// ============================================================================

TEST_CASE("Test start profiling with scheduler", "[utils][thread_profiler][scheduler][start]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Start Profiling with Scheduler Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    bool result = profiler.start_profiling(scheduler);
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_TRUE(profiler.is_profiling());

    vTaskDelay(pdMS_TO_TICKS(5000));

    profiler.stop_profiling();
    TEST_ASSERT_FALSE(profiler.is_profiling());


    cleanup_test_tasks();
}

TEST_CASE("Test auto logging", "[utils][thread_profiler][scheduler][auto_log]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Auto Logging Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 500,
        .enable_auto_logging = true,  // Enable auto logging
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    profiler.start_profiling(scheduler);

    // Let it run and auto-log
    vTaskDelay(pdMS_TO_TICKS(2500));

    profiler.stop_profiling();

    cleanup_test_tasks();
}

TEST_CASE("Test profiling with custom period", "[utils][thread_profiler][scheduler][custom_period]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Custom Period Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register callback - RAII scope
        auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
        });

        // Use custom period of 800ms
        profiler.start_profiling(scheduler, 500, 1000);

        vTaskDelay(pdMS_TO_TICKS(3000));

        profiler.stop_profiling();

        // Should have been called ~3 times (2500ms / 800ms)
        BROOKESIA_LOGI("Callback count: %1%", g_callback_counter.load());
        TEST_ASSERT_GREATER_OR_EQUAL(2, g_callback_counter.load());
        TEST_ASSERT_LESS_OR_EQUAL(4, g_callback_counter.load());

        // conn will auto-disconnect here (RAII)
    }

    cleanup_test_tasks();
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE("Test start profiling when already profiling", "[utils][thread_profiler][error][already_profiling]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Already Profiling Test ===");
    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    bool result1 = profiler.start_profiling(scheduler);
    TEST_ASSERT_TRUE(result1);

    // Try to start again
    bool result2 = profiler.start_profiling(scheduler);
    TEST_ASSERT_TRUE(result2);

    profiler.stop_profiling();
}

TEST_CASE("Test start profiling with stopped scheduler", "[utils][thread_profiler][error][stopped_scheduler]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Stopped Scheduler Test ===");
    std::shared_ptr<TaskScheduler> scheduler = std::make_shared<TaskScheduler>();

    // Scheduler is not started
    bool result = profiler.start_profiling(scheduler);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_FALSE(profiler.is_profiling());
}

// ============================================================================
// Comprehensive Tests
// ============================================================================

TEST_CASE("Test comprehensive profiling workflow", "[utils][thread_profiler][comprehensive][workflow]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Comprehensive Workflow Test ===");

    reset_counters();

    // Create test tasks
    create_test_task("TestTask1", 5, 2048, idle_task);
    create_test_task("TestTask2", 10, 2048, periodic_task);

    vTaskDelay(pdMS_TO_TICKS(100));
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 500,
        .profiling_interval_ms = 1000,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // 1. Manual snapshot
    auto snapshot1 = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot1.get());
    ThreadProfiler::sort_tasks(snapshot1->tasks, config.primary_sort, config.secondary_sort);
    ThreadProfiler::print_snapshot(*snapshot1, config.primary_sort, config.secondary_sort);

    // 2. Query specific task
    ThreadProfiler::TaskInfo test_task_info;
    bool found = ThreadProfiler::get_task_by_name(*snapshot1, "TestTask1", test_task_info);
    if (found) {
        BROOKESIA_LOGI("TestTask1: CPU=%1%%%, Priority=%2%",
                       test_task_info.cpu_percent, test_task_info.priority);
    }

    // 3. Get high CPU tasks
    auto high_cpu = ThreadProfiler::get_tasks_above_threshold(*snapshot1, ThreadProfiler::ThresholdType::CpuPercent, 10);
    BROOKESIA_LOGI("High CPU tasks: %1%", high_cpu.size());

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // 4. Register callback - RAII scope
        auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
        });

        // 5. Start periodic profiling
        profiler.start_profiling(scheduler);

        vTaskDelay(pdMS_TO_TICKS(4000));

        // 6. Get latest snapshot
        auto latest = profiler.get_profiling_latest_snapshot();
        TEST_ASSERT_NOT_NULL(latest.get());

        // 7. Stop profiling
        profiler.stop_profiling();

        TEST_ASSERT_GREATER_OR_EQUAL(2, g_callback_counter.load());

        // conn will auto-disconnect here (RAII)
    }

    cleanup_test_tasks();
}

TEST_CASE("Test multiple snapshot cycles", "[utils][thread_profiler][comprehensive][multiple_cycles]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Multiple Snapshot Cycles Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {};
    profiler.configure_profiling(config);

    const int cycle_count = 5;
    for (int i = 0; i < cycle_count; i++) {
        BROOKESIA_LOGI("Snapshot cycle %1%", i + 1);

        auto snapshot = take_snapshot_helper();
        TEST_ASSERT_NOT_NULL(snapshot.get());
        TEST_ASSERT_GREATER_THAN(0, snapshot->tasks.size());

        BROOKESIA_LOGI("  Tasks: %1%, CPU: %2%%%",
                       snapshot->stats.total_tasks,
                       snapshot->stats.total_cpu_percent);

        vTaskDelay(pdMS_TO_TICKS(100));
    }

    cleanup_test_tasks();
}

TEST_CASE("Test async profiling does not block scheduler", "[utils][thread_profiler][comprehensive][async]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Async Non-Blocking Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task);
    create_test_task("Busy", 5, 2048, busy_task);

    vTaskDelay(pdMS_TO_TICKS(100));
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 500,  // 1000ms sample interval
        .profiling_interval_ms = 1000,  // 1000ms sample interval
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Track scheduler responsiveness with a high-frequency task
    std::atomic<int> scheduler_task_count{0};
    std::atomic<int> profiler_callback_count{0};

    // Post a fast recurring task to verify scheduler is not blocked
    auto verify_task_start = std::chrono::steady_clock::now();
    std::function<void()> verify_task = [&]() {
        scheduler_task_count++;
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - verify_task_start
                       ).count();

        // Run for 3 seconds
        if (elapsed < 3000) {
            scheduler->post_delayed(verify_task, 100);
        }
    };
    scheduler->post(verify_task);

    {
        // Register profiling callback - RAII scope
        auto conn = profiler.connect_profiling_signal([&](const ThreadProfiler::ProfileSnapshot & snapshot) {
            profiler_callback_count++;
            BROOKESIA_LOGI("Profiler callback %1%, tasks: %2%",
                           profiler_callback_count.load(), snapshot.tasks.size());
        });

        // Start profiling with 1 second period
        bool started = profiler.start_profiling(scheduler);
        TEST_ASSERT_TRUE(started);

        // Wait for profiling cycles
        vTaskDelay(pdMS_TO_TICKS(3500));

        profiler.stop_profiling();

        BROOKESIA_LOGI("Scheduler task executed %1% times", scheduler_task_count.load());
        BROOKESIA_LOGI("Profiler callback executed %1% times", profiler_callback_count.load());

        // Verify scheduler remained responsive
        // Expected: ~30 times (3000ms / 100ms)
        // Allow some tolerance for system load
        TEST_ASSERT_GREATER_THAN(20, scheduler_task_count.load());

        // Verify profiler callbacks executed
        // Expected: ~3 times (3000ms / 1000ms)
        TEST_ASSERT_GREATER_OR_EQUAL(2, profiler_callback_count.load());

        // conn will auto-disconnect here (RAII)
    }

    cleanup_test_tasks();
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_CASE("Test snapshot performance", "[utils][thread_profiler][performance][snapshot]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Snapshot Performance Test ===");

    reset_counters();
    // Create test tasks
    create_test_task("Idle", 5, 2048, idle_task, TaskType::FreeRTOS);
    create_test_task("Busy", 5, 2048, busy_task, TaskType::StdThread);
    create_test_task("Periodic", 5, 2048, periodic_task, TaskType::BoostThread);

    vTaskDelay(pdMS_TO_TICKS(100)); // Let tasks run
    ThreadProfiler::ProfilingConfig config = {};
    profiler.configure_profiling(config);

    // First call: returns nullopt
    auto start_result = ThreadProfiler::sample_tasks();
    TEST_ASSERT_NOT_NULL(start_result.get());

    vTaskDelay(pdMS_TO_TICKS(500));

    // Second call: returns snapshot
    auto end_result = ThreadProfiler::sample_tasks();
    TEST_ASSERT_NOT_NULL(end_result.get());

    auto start = std::chrono::steady_clock::now();
    auto snapshot = ThreadProfiler::take_snapshot(*start_result, *end_result);
    auto end = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    TEST_ASSERT_NOT_NULL(snapshot.get());
    BROOKESIA_LOGI("Snapshot time: %lld ms", elapsed);

    // Incremental snapshot should be very fast (< 50ms)
    TEST_ASSERT_LESS_THAN(50, elapsed);

    cleanup_test_tasks();
}

// ============================================================================
// Stress Tests
// ============================================================================

TEST_CASE("Test profiling with many tasks", "[utils][thread_profiler][stress][many_tasks]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Many Tasks Stress Test ===");

    // Create many test tasks
    for (int i = 0; i < 10; i++) {
        char name[16];
        snprintf(name, sizeof(name), "StressTask%d", i);
        create_test_task(name, 5 + (i % 3), 2048, i % 2 == 0 ? idle_task : periodic_task);
    }

    vTaskDelay(pdMS_TO_TICKS(200));
    ThreadProfiler::ProfilingConfig config = {};
    profiler.configure_profiling(config);

    // Take snapshot using static methods
    auto snapshot = take_snapshot_helper();
    TEST_ASSERT_NOT_NULL(snapshot.get());
    TEST_ASSERT_GREATER_THAN(10, snapshot->tasks.size());

    BROOKESIA_LOGI("Total tasks detected: %1%", snapshot->stats.total_tasks);

    cleanup_test_tasks();
}

TEST_CASE("Test long running profiling", "[utils][thread_profiler][stress][long_running]")
{
    BROOKESIA_LOGI("=== ThreadProfiler Long Running Test ===");

    reset_counters();
    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register callback - RAII scope
        auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
        });

        profiler.start_profiling(scheduler);

        // Run for 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));

        profiler.stop_profiling();

        // Should have been called ~10 times
        BROOKESIA_LOGI("Callbacks during 5s: %1%", g_callback_counter.load());
        TEST_ASSERT_GREATER_OR_EQUAL(8, g_callback_counter.load());

        // conn will auto-disconnect here (RAII)
    }

    profiler.reset_profiling();
}

// ============================================================================
// SignalConnection RAII Tests
// ============================================================================

TEST_CASE("Test SignalConnection RAII auto-disconnect", "[utils][thread_profiler][signal_connection][raii]")
{
    BROOKESIA_LOGI("=== SignalConnection RAII Auto-Disconnect Test ===");

    reset_counters();

    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Test 1: Connection auto-disconnects when leaving scope
    BROOKESIA_LOGI("Test 1: Auto-disconnect on scope exit");
    {
        auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
            BROOKESIA_LOGI("Callback in scope, count=%1%", g_callback_counter.load());
        });

        profiler.start_profiling(scheduler);
        vTaskDelay(pdMS_TO_TICKS(1500));
        profiler.stop_profiling();

        int count_in_scope = g_callback_counter.load();
        TEST_ASSERT_GREATER_THAN(0, count_in_scope);
        BROOKESIA_LOGI("Callback count in scope: %1%", count_in_scope);

        // conn will auto-disconnect here when leaving scope
    }

    // Test 2: Verify callback is no longer called after scope exit
    BROOKESIA_LOGI("Test 2: Verify callback disconnected after scope");
    int count_after_scope = g_callback_counter.load();

    profiler.start_profiling(scheduler);
    vTaskDelay(pdMS_TO_TICKS(1500));
    profiler.stop_profiling();

    // Callback should NOT increase (connection was auto-disconnected)
    TEST_ASSERT_EQUAL(count_after_scope, g_callback_counter.load());
    BROOKESIA_LOGI("✓ Callback correctly disconnected (RAII verified)");

    profiler.reset_profiling();
}

TEST_CASE("Test SignalConnection manual disconnect", "[utils][thread_profiler][signal_connection][manual]")
{
    BROOKESIA_LOGI("=== SignalConnection Manual Disconnect Test ===");

    reset_counters();

    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Register callback
    auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
        g_callback_counter++;
        BROOKESIA_LOGI("Callback triggered, count=%1%", g_callback_counter.load());
    });

    // Test 1: Callback active before disconnect
    profiler.start_profiling(scheduler);
    vTaskDelay(pdMS_TO_TICKS(1500));
    profiler.stop_profiling();

    int count_before_disconnect = g_callback_counter.load();
    TEST_ASSERT_GREATER_THAN(0, count_before_disconnect);
    BROOKESIA_LOGI("Callback count before disconnect: %1%", count_before_disconnect);

    // Test 2: Manual disconnect
    BROOKESIA_LOGI("Manually disconnecting...");
    conn.disconnect();

    // Test 3: Verify callback is no longer called after disconnect
    profiler.start_profiling(scheduler);
    vTaskDelay(pdMS_TO_TICKS(1500));
    profiler.stop_profiling();

    TEST_ASSERT_EQUAL(count_before_disconnect, g_callback_counter.load());
    BROOKESIA_LOGI("✓ Callback correctly stopped after manual disconnect");

    profiler.reset_profiling();
}

TEST_CASE("Test SignalConnection move semantics", "[utils][thread_profiler][signal_connection][move]")
{
    BROOKESIA_LOGI("=== SignalConnection Move Semantics Test ===");

    reset_counters();

    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Test move semantics
    ThreadProfiler::SignalConnection moved_conn;

    {
        // Register callback in inner scope
        auto conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
            BROOKESIA_LOGI("Callback triggered, count=%1%", g_callback_counter.load());
        });

        // Move connection to outer scope
        moved_conn = std::move(conn);

        // Original conn is now invalid, moved_conn owns the connection
        // conn should not disconnect on scope exit
    }

    // Test: Callback should still work after move (moved_conn is still valid)
    profiler.start_profiling(scheduler);
    vTaskDelay(pdMS_TO_TICKS(1500));
    profiler.stop_profiling();

    TEST_ASSERT_GREATER_THAN(0, g_callback_counter.load());
    BROOKESIA_LOGI("✓ Callback still works after move (count=%1%)", g_callback_counter.load());

    int count_before_final_disconnect = g_callback_counter.load();

    // Manually disconnect moved_conn
    moved_conn.disconnect();

    // Verify callback is disconnected
    profiler.start_profiling(scheduler);
    vTaskDelay(pdMS_TO_TICKS(1500));
    profiler.stop_profiling();

    TEST_ASSERT_EQUAL(count_before_final_disconnect, g_callback_counter.load());
    BROOKESIA_LOGI("✓ Callback correctly disconnected after moving");

    profiler.reset_profiling();
}

TEST_CASE("Test SignalConnection multiple callbacks RAII", "[utils][thread_profiler][signal_connection][multiple_raii]")
{
    BROOKESIA_LOGI("=== SignalConnection Multiple Callbacks RAII Test ===");

    reset_counters();

    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    std::atomic<int> callback1_count{0};
    std::atomic<int> callback2_count{0};
    std::atomic<int> callback3_count{0};

    // Callback 1: Lives entire test
    auto conn1 = profiler.connect_profiling_signal([&](const ThreadProfiler::ProfileSnapshot & snapshot) {
        callback1_count++;
        BROOKESIA_LOGI("Callback 1 triggered, count=%1%", callback1_count.load());
    });

    profiler.start_profiling(scheduler);

    // Phase 1: All callbacks active
    {
        auto conn2 = profiler.connect_profiling_signal([&](const ThreadProfiler::ProfileSnapshot & snapshot) {
            callback2_count++;
            BROOKESIA_LOGI("Callback 2 triggered, count=%1%", callback2_count.load());
        });

        {
            auto conn3 = profiler.connect_profiling_signal([&](const ThreadProfiler::ProfileSnapshot & snapshot) {
                callback3_count++;
                BROOKESIA_LOGI("Callback 3 triggered, count=%1%", callback3_count.load());
            });

            vTaskDelay(pdMS_TO_TICKS(1500));

            // All 3 callbacks should be called
            TEST_ASSERT_GREATER_THAN(0, callback1_count.load());
            TEST_ASSERT_GREATER_THAN(0, callback2_count.load());
            TEST_ASSERT_GREATER_THAN(0, callback3_count.load());
            BROOKESIA_LOGI("Phase 1: callback1=%1%, callback2=%2%, callback3=%3%",
                           callback1_count.load(), callback2_count.load(), callback3_count.load());

            // conn3 auto-disconnects here
        }

        // Phase 2: Only conn1 and conn2 active (conn3 disconnected)
        int count3_after_scope = callback3_count.load();
        vTaskDelay(pdMS_TO_TICKS(1500));

        TEST_ASSERT_GREATER_THAN(0, callback1_count.load());
        TEST_ASSERT_GREATER_THAN(0, callback2_count.load());
        TEST_ASSERT_EQUAL(count3_after_scope, callback3_count.load()); // Should not increase
        BROOKESIA_LOGI("Phase 2: callback1=%1%, callback2=%2%, callback3=%3% (stopped)",
                       callback1_count.load(), callback2_count.load(), callback3_count.load());

        // conn2 auto-disconnects here
    }

    // Phase 3: Only conn1 active (conn2 and conn3 disconnected)
    int count2_after_scope = callback2_count.load();
    int count3_final = callback3_count.load();
    vTaskDelay(pdMS_TO_TICKS(1500));

    TEST_ASSERT_GREATER_THAN(0, callback1_count.load());
    TEST_ASSERT_EQUAL(count2_after_scope, callback2_count.load()); // Should not increase
    TEST_ASSERT_EQUAL(count3_final, callback3_count.load()); // Should not increase
    BROOKESIA_LOGI("Phase 3: callback1=%1%, callback2=%2% (stopped), callback3=%3% (stopped)",
                   callback1_count.load(), callback2_count.load(), callback3_count.load());

    profiler.stop_profiling();

    // Cleanup: conn1 still needs manual disconnect or will auto-disconnect at end of function
    conn1.disconnect();

    profiler.reset_profiling();
    BROOKESIA_LOGI("✓ Multiple connections RAII verified - each disconnected at correct scope");
}

TEST_CASE("Test SignalConnection connected() check", "[utils][thread_profiler][signal_connection][connected]")
{
    BROOKESIA_LOGI("=== SignalConnection connected() Check Test ===");

    reset_counters();

    ThreadProfiler::ProfilingConfig config = {
        .sampling_duration_ms = 100,
        .profiling_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Test 1: Newly created connection is not connected
    ThreadProfiler::SignalConnection conn;
    TEST_ASSERT_FALSE(conn.connected());
    BROOKESIA_LOGI("Default connection: connected=%d", conn.connected());

    // Test 2: After registration, connection is connected
    conn = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
        g_callback_counter++;
    });
    TEST_ASSERT_TRUE(conn.connected());
    BROOKESIA_LOGI("After registration: connected=%d", conn.connected());

    // Test 3: After manual disconnect, connection is not connected
    conn.disconnect();
    TEST_ASSERT_FALSE(conn.connected());
    BROOKESIA_LOGI("After disconnect: connected=%d", conn.connected());

    // Test 4: After reset_profiling, connection is not connected
    auto conn2 = profiler.connect_profiling_signal([](const ThreadProfiler::ProfileSnapshot & snapshot) {
        g_callback_counter++;
    });
    TEST_ASSERT_TRUE(conn2.connected());

    profiler.reset_profiling(); // Should disconnect all slots

    TEST_ASSERT_FALSE(conn2.connected());
    BROOKESIA_LOGI("After reset_profiling: connected=%d", conn2.connected());

    BROOKESIA_LOGI("✓ connected() check verified");
}
#endif // CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
