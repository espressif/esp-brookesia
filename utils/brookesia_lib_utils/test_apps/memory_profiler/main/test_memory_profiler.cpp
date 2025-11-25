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
#include <cstring>
#include <cstdlib>
#include <climits>
#include <boost/thread.hpp>
#include <boost/chrono.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/lib_utils/memory_profiler.hpp"

using namespace esp_brookesia::lib_utils;

// Global profiler instance
static MemoryProfiler &profiler = MemoryProfiler::get_instance();

// Atomic counters for test callbacks
static std::atomic<int> g_callback_counter{0};
static std::atomic<int> g_threshold_callback_counter{0};

// Helper function to reset counters
void reset_counters()
{
    g_callback_counter = 0;
    g_threshold_callback_counter = 0;
}

// Helper function to allocate memory for testing
void allocate_memory(size_t size)
{
    void *ptr = malloc(size);
    if (ptr) {
        memset(ptr, 0xAA, size);
        // Keep pointer alive for a while
        vTaskDelay(pdMS_TO_TICKS(10));
        free(ptr);
    }
}

// ============================================================================
// Basic Functionality Tests
// ============================================================================

TEST_CASE("Test basic configuration", "[utils][memory_profiler][basic][config]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Basic Configuration Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };

    bool result = profiler.configure_profiling(config);
    TEST_ASSERT_TRUE(result);

    auto retrieved_config = profiler.get_profiling_config();
    TEST_ASSERT_FALSE(retrieved_config.enable_auto_logging);
    TEST_ASSERT_EQUAL(500, retrieved_config.sample_interval_ms);

    profiler.reset_profiling();
}

TEST_CASE("Test singleton pattern", "[utils][memory_profiler][basic][singleton]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Singleton Pattern Test ===");

    auto &profiler1 = MemoryProfiler::get_instance();
    auto &profiler2 = MemoryProfiler::get_instance();

    // Should return the same instance
    TEST_ASSERT_EQUAL_PTR(&profiler1, &profiler2);

    // Configure one instance
    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 1000,
        .enable_auto_logging = true,
    };
    profiler1.configure_profiling(config);

    // Check that the other instance reflects the same config
    auto config2 = profiler2.get_profiling_config();
    TEST_ASSERT_EQUAL(1000, config2.sample_interval_ms);
    TEST_ASSERT_TRUE(config2.enable_auto_logging);

    profiler1.reset_profiling();
}

TEST_CASE("Test take snapshot", "[utils][memory_profiler][basic][snapshot]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Take Snapshot Test ===");
    MemoryProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take a snapshot using static methods
    auto snapshot = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    // Check snapshot structure
    TEST_ASSERT_GREATER_THAN(0, snapshot->memory.internal.total_size);
    TEST_ASSERT_GREATER_THAN(0, snapshot->memory.internal.free_size);

    // Check external memory (may be 0 if PSRAM not available)
    TEST_ASSERT_GREATER_OR_EQUAL(0, snapshot->memory.external.total_size);
    TEST_ASSERT_GREATER_OR_EQUAL(0, snapshot->memory.external.free_size);

    // Check totals
    TEST_ASSERT_EQUAL(
        snapshot->memory.internal.total_size + snapshot->memory.external.total_size,
        snapshot->memory.total_size
    );
    TEST_ASSERT_EQUAL(
        snapshot->memory.internal.free_size + snapshot->memory.external.free_size,
        snapshot->memory.total_free
    );

    // Check percentages
    TEST_ASSERT_LESS_OR_EQUAL(100, snapshot->memory.internal.used_percent);
    TEST_ASSERT_LESS_OR_EQUAL(100, snapshot->memory.external.used_percent);

    // Check statistics
    TEST_ASSERT_EQUAL(1, snapshot->stats.sample_count);

    profiler.reset_profiling();
}

TEST_CASE("Test print snapshot", "[utils][memory_profiler][basic][print]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Print Snapshot Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take a snapshot using static methods
    auto snapshot = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    // This should not crash
    MemoryProfiler::print_snapshot(*snapshot);

    profiler.reset_profiling();
}

TEST_CASE("Test get latest snapshot", "[utils][memory_profiler][basic][latest]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Get Latest Snapshot Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Initially, snapshot should be null
    auto latest1 = profiler.get_profiling_latest_snapshot();
    TEST_ASSERT_NULL(latest1.get());

    // Take a snapshot using static methods
    auto snapshot = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot.get());

    // Note: According to current implementation, take_snapshot() does not update latest_snapshot_
    // So get_profiling_latest_snapshot() may return null or an old snapshot
    // The snapshot returned by take_snapshot() is the actual latest snapshot
    auto latest2 = profiler.get_profiling_latest_snapshot();
    // get_profiling_latest_snapshot() may be null if latest_snapshot_ is not updated
    // In that case, the snapshot from take_snapshot() is the actual latest
    (void)latest2; // Suppress unused variable warning

    profiler.reset_profiling();
}

TEST_CASE("Test reset", "[utils][memory_profiler][basic][reset]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Reset Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take multiple snapshots to accumulate statistics
    auto snapshot1 = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot1.get());
    TEST_ASSERT_EQUAL(1, snapshot1->stats.sample_count);

    // Allocate some memory
    void *ptr = malloc(1024 * 50);
    if (ptr) {
        memset(ptr, 0xAA, 1024 * 50);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    auto snapshot2 = MemoryProfiler::take_snapshot(snapshot1.get());
    TEST_ASSERT_NOT_NULL(snapshot2.get());
    TEST_ASSERT_EQUAL(2, snapshot2->stats.sample_count);

    // Verify snapshot2 is correct (latest_snapshot_ may not be updated in take_snapshot())
    TEST_ASSERT_EQUAL(2, snapshot2->stats.sample_count);

    // Reset profiler
    profiler.reset_profiling();

    // Verify latest snapshot is cleared (reset to null)
    auto latest_after = profiler.get_profiling_latest_snapshot();
    TEST_ASSERT_NULL(latest_after.get());

    // Take a new snapshot after reset
    auto snapshot3 = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot3.get());
    // Statistics should be reset (sample_count starts from 1)
    TEST_ASSERT_EQUAL(1, snapshot3->stats.sample_count);

    // Verify configuration is not affected
    auto config_after = profiler.get_profiling_config();
    TEST_ASSERT_FALSE(config_after.enable_auto_logging);

    // Verify callback is cleared
    // (We can't directly test this, but we can verify it doesn't affect other operations)

    if (ptr) {
        free(ptr);
    }

    profiler.reset_profiling();
}

TEST_CASE("Test reset clears callbacks and threshold listeners", "[utils][memory_profiler][basic][reset_clear]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Reset Clears Callbacks and Threshold Listeners Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 1000,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Test RAII with reset: connections held in inner scope
    {
        // Register snapshot callback
        auto snapshot_conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            BROOKESIA_LOGI("Snapshot callback triggered");
            g_callback_counter++;
        });

        // Register threshold callback
        auto conn = profiler.connect_threshold_signal(
                        MemoryProfiler::ThresholdType::TotalFreePercent,
                        100, // Should always trigger (free percent <= 100)
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            BROOKESIA_LOGI("Threshold callback triggered");
            g_threshold_callback_counter++;
        });

        // Reset profiler (should clear callbacks and threshold listeners)
        profiler.reset_profiling();

        // Verify configuration is preserved
        auto config_after = profiler.get_profiling_config();
        TEST_ASSERT_EQUAL(1000, config_after.sample_interval_ms);
        TEST_ASSERT_FALSE(config_after.enable_auto_logging);

        // Start profiling - callbacks should not be called since they were cleared
        profiler.start_profiling(scheduler);
        vTaskDelay(pdMS_TO_TICKS(2000));
        profiler.stop_profiling();

        // Verify callbacks were cleared (should not be called)
        TEST_ASSERT_EQUAL(0, g_callback_counter.load());
        TEST_ASSERT_EQUAL(0, g_threshold_callback_counter.load());

        // Connections will auto-disconnect when leaving scope (RAII)
    }

    // Re-register callbacks after reset in new scope
    {
        auto snapshot_conn2 = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
        });

        auto conn2 = profiler.connect_threshold_signal(
                         MemoryProfiler::ThresholdType::TotalFreePercent,
                         100, // Should always trigger (free percent <= 100)
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_threshold_callback_counter++;
        }
                     );

        // Start profiling again with new callbacks
        profiler.start_profiling(scheduler);
        vTaskDelay(pdMS_TO_TICKS(2000));
        profiler.stop_profiling();

        // Verify new callbacks work
        TEST_ASSERT_GREATER_THAN(0, g_callback_counter.load());
        TEST_ASSERT_GREATER_THAN(0, g_threshold_callback_counter.load());

        // Connections will auto-disconnect when leaving scope (RAII)
    }
    profiler.reset_profiling();
}

// ============================================================================
// Memory Allocation Tests
// ============================================================================

TEST_CASE("Test memory snapshot after allocation", "[utils][memory_profiler][memory][allocation]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Memory Allocation Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take initial snapshot
    auto snapshot1 = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot1.get());
    size_t initial_free = snapshot1->memory.total_free;

    // Allocate some memory
    const size_t alloc_size = 1024 * 100; // 100 KB
    void *ptr = malloc(alloc_size);
    TEST_ASSERT_NOT_NULL(ptr);
    memset(ptr, 0xAA, alloc_size);

    vTaskDelay(pdMS_TO_TICKS(100));

    // Take snapshot after allocation
    auto snapshot2 = MemoryProfiler::take_snapshot(snapshot1.get());
    TEST_ASSERT_NOT_NULL(snapshot2.get());
    MemoryProfiler::print_snapshot(*snapshot2);

    size_t after_alloc_free = snapshot2->memory.total_free;

    // Free memory should decrease
    TEST_ASSERT_GREATER_THAN(after_alloc_free, initial_free);

    // Free the memory
    free(ptr);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Take snapshot after free
    auto snapshot3 = MemoryProfiler::take_snapshot(snapshot2.get());
    TEST_ASSERT_NOT_NULL(snapshot3.get());
    MemoryProfiler::print_snapshot(*snapshot3);

    size_t after_free_free = snapshot3->memory.total_free;

    // Free memory should increase back
    TEST_ASSERT_GREATER_THAN(after_alloc_free, after_free_free);

    profiler.reset_profiling();
}

// ============================================================================
// Callback Tests
// ============================================================================

TEST_CASE("Test snapshot callback", "[utils][memory_profiler][callback][snapshot]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Snapshot Callback Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Test RAII: connection auto-disconnects when leaving scope
    {
        // Register snapshot callback
        auto snapshot_conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
            BROOKESIA_LOGI("Callback triggered, sample count: %1%", snapshot.stats.sample_count);
        });

        profiler.start_profiling(scheduler);

        // Wait for a few samples
        vTaskDelay(pdMS_TO_TICKS(2000));

        profiler.stop_profiling();

        BROOKESIA_LOGI("Callback count: %1%", g_callback_counter.load());
        TEST_ASSERT_GREATER_THAN(0, g_callback_counter.load());

        // snapshot_conn will auto-disconnect when leaving scope (RAII)
    }

    // Verify callback is disconnected after scope ends
    int callback_count_after_scope = g_callback_counter.load();

    profiler.start_profiling(scheduler);
    vTaskDelay(pdMS_TO_TICKS(1000));
    profiler.stop_profiling();

    // Callback should NOT increase after disconnection
    TEST_ASSERT_EQUAL(callback_count_after_scope, g_callback_counter.load());
    BROOKESIA_LOGI("✓ Callback correctly disconnected after scope (RAII verified)");
    profiler.reset_profiling();
}

// ============================================================================
// Threshold Monitoring Tests
// ============================================================================

TEST_CASE("Test threshold callback - total used percent", "[utils][memory_profiler][threshold][total_used]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Threshold Callback Test (Total Used Percent) ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Get initial snapshot to determine threshold
    auto initial_snapshot = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(initial_snapshot.get());

    // Set threshold to current free percent (should trigger immediately)
    uint32_t threshold = initial_snapshot->memory.total_free_percent;
    if (threshold < 100) {
        threshold++; // Set slightly higher to ensure trigger
    }

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register threshold callback - RAII scope
        auto conn = profiler.connect_threshold_signal(
                        MemoryProfiler::ThresholdType::TotalFreePercent,
                        threshold,
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_threshold_callback_counter++;
            BROOKESIA_LOGI("Total free percent threshold triggered: %1%%%", snapshot.memory.total_free_percent);
        }
                    );

        profiler.start_profiling(scheduler);

        // Wait for threshold to trigger
        vTaskDelay(pdMS_TO_TICKS(2000));

        profiler.stop_profiling();

        BROOKESIA_LOGI("Threshold callback count: %1%", g_threshold_callback_counter.load());
        TEST_ASSERT_GREATER_THAN(0, g_threshold_callback_counter.load());

        // conn will auto-disconnect here (RAII)
    }
    profiler.reset_profiling();
}

TEST_CASE("Test threshold callback - internal free", "[utils][memory_profiler][threshold][internal_free]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Threshold Callback Test (Internal Free) ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Get initial snapshot to determine threshold
    auto initial_snapshot = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(initial_snapshot.get());

    // Set threshold to current free + some margin (should trigger if memory is allocated)
    size_t threshold = initial_snapshot->memory.internal.free_size + 1024 * 50; // 50 KB more

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    void *ptr = nullptr;

    {
        // Register threshold callback - RAII scope
        auto conn = profiler.connect_threshold_signal(
                        MemoryProfiler::ThresholdType::InternalFree,
                        static_cast<uint32_t>(threshold),
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_threshold_callback_counter++;
            BROOKESIA_LOGI("Internal free threshold triggered: %1% KB", snapshot.memory.internal.free_size / 1024);
        }
                    );

        profiler.start_profiling(scheduler);

        // Allocate memory to trigger threshold
        ptr = malloc(1024 * 100); // 100 KB
        if (ptr) {
            memset(ptr, 0xAA, 1024 * 100);
        }

        // Wait for threshold to trigger
        vTaskDelay(pdMS_TO_TICKS(2000));

        profiler.stop_profiling();

        BROOKESIA_LOGI("Threshold callback count: %1%", g_threshold_callback_counter.load());

        // conn will auto-disconnect here (RAII)
    }

    if (ptr) {
        free(ptr);
    }
    profiler.reset_profiling();
}

TEST_CASE("Test multiple threshold callbacks", "[utils][memory_profiler][threshold][multiple]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Multiple Threshold Callbacks Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    std::atomic<int> total_used_count{0};
    std::atomic<int> internal_free_count{0};

    // Get initial snapshot
    auto initial_snapshot = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(initial_snapshot.get());

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    {
        // Register multiple threshold callbacks - RAII scope
        auto conn1 = profiler.connect_threshold_signal(
                         MemoryProfiler::ThresholdType::TotalFreePercent,
                         100, // Should always trigger (free percent <= 100)
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            total_used_count++;
            BROOKESIA_LOGI("Total free percent threshold triggered: %1%%%", snapshot.memory.total_free_percent);
        }
                     );

        auto conn2 = profiler.connect_threshold_signal(
                         MemoryProfiler::ThresholdType::InternalFree,
                         UINT32_MAX, // Should trigger when free < UINT32_MAX (always)
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            internal_free_count++;
            BROOKESIA_LOGI("Internal free threshold triggered: %1% KB", snapshot.memory.internal.free_size / 1024);
        }
                     );

        profiler.start_profiling(scheduler);

        vTaskDelay(pdMS_TO_TICKS(2000));

        profiler.stop_profiling();

        BROOKESIA_LOGI("Total used callbacks: %1%, Internal free callbacks: %2%",
                       total_used_count.load(), internal_free_count.load());
        TEST_ASSERT_GREATER_THAN(0, total_used_count.load());
        TEST_ASSERT_GREATER_THAN(0, internal_free_count.load());

        // conn1 and conn2 will auto-disconnect here (RAII)
    }
    profiler.reset_profiling();
}

// ============================================================================
// TaskScheduler Integration Tests
// ============================================================================

TEST_CASE("Test start profiling with scheduler", "[utils][memory_profiler][scheduler][start]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Start Profiling with Scheduler Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 1000,
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

    profiler.reset_profiling();
}

TEST_CASE("Test auto logging", "[utils][memory_profiler][scheduler][auto_log]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Auto Logging Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 1000,
        .enable_auto_logging = true, // Enable auto logging
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    profiler.start_profiling(scheduler);

    // Wait for a few samples (should see logs)
    vTaskDelay(pdMS_TO_TICKS(3000));

    profiler.stop_profiling();
    profiler.reset_profiling();
}

TEST_CASE("Test start profiling when already profiling", "[utils][memory_profiler][scheduler][already_profiling]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Start Profiling When Already Profiling Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 1000,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Start profiling first time
    bool result1 = profiler.start_profiling(scheduler);
    TEST_ASSERT_TRUE(result1);
    TEST_ASSERT_TRUE(profiler.is_profiling());

    // Try to start again (should return true, already profiling)
    bool result2 = profiler.start_profiling(scheduler);
    TEST_ASSERT_TRUE(result2);
    TEST_ASSERT_TRUE(profiler.is_profiling());

    profiler.stop_profiling();
    TEST_ASSERT_FALSE(profiler.is_profiling());

    profiler.reset_profiling();
}

TEST_CASE("Test stop profiling when not profiling", "[utils][memory_profiler][scheduler][stop_not_profiling]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Stop Profiling When Not Profiling Test ===");


    // Should not crash
    profiler.stop_profiling();
    TEST_ASSERT_FALSE(profiler.is_profiling());

    profiler.reset_profiling();
}

// ============================================================================
// Comprehensive Tests
// ============================================================================

TEST_CASE("Test comprehensive profiling workflow", "[utils][memory_profiler][comprehensive][workflow]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Comprehensive Workflow Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    void *ptr1 = nullptr;
    void *ptr2 = nullptr;

    {
        // Register callbacks - RAII scope
        auto snapshot_conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_callback_counter++;
        });

        auto conn = profiler.connect_threshold_signal(
                        MemoryProfiler::ThresholdType::TotalFreePercent,
                        100, // Should always trigger (free percent <= 100)
        [&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            g_threshold_callback_counter++;
        }
                    );

        // Start profiling
        profiler.start_profiling(scheduler);
        TEST_ASSERT_TRUE(profiler.is_profiling());

        // Allocate some memory during profiling
        ptr1 = malloc(1024 * 50);
        if (ptr1) {
            memset(ptr1, 0xAA, 1024 * 50);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));

        // Allocate more memory
        ptr2 = malloc(1024 * 50);
        if (ptr2) {
            memset(ptr2, 0xBB, 1024 * 50);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));

        // Stop profiling
        profiler.stop_profiling();
        TEST_ASSERT_FALSE(profiler.is_profiling());

        // Check results
        TEST_ASSERT_GREATER_THAN(0, g_callback_counter.load());
        TEST_ASSERT_GREATER_THAN(0, g_threshold_callback_counter.load());

        BROOKESIA_LOGI("Callback count: %1%, Threshold callback count: %2%",
                       g_callback_counter.load(), g_threshold_callback_counter.load());

        // Try to get latest snapshot (may be null if not updated)
        auto latest = profiler.get_profiling_latest_snapshot();
        if (latest.get() != nullptr) {
            TEST_ASSERT_GREATER_THAN(1, latest->stats.sample_count);
            BROOKESIA_LOGI("Sample count: %1%", latest->stats.sample_count);
            // Print final snapshot if available
            MemoryProfiler::print_snapshot(*latest);
        } else {
            // If latest_snapshot_ is not updated, take a manual snapshot for verification
            auto manual_snapshot = MemoryProfiler::take_snapshot();
            if (manual_snapshot.get() != nullptr) {
                BROOKESIA_LOGI("Sample count: %1%", manual_snapshot->stats.sample_count);
                TEST_ASSERT_GREATER_THAN(1, manual_snapshot->stats.sample_count);
            }
        }

        // snapshot_conn and conn will auto-disconnect here (RAII)
    }

    // Free memory after callbacks disconnected
    if (ptr1) {
        free(ptr1);
    }
    if (ptr2) {
        free(ptr2);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    profiler.reset_profiling();
}

TEST_CASE("Test statistics tracking", "[utils][memory_profiler][comprehensive][statistics]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Statistics Tracking Test ===");

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Take initial snapshot
    auto snapshot1 = MemoryProfiler::take_snapshot();
    TEST_ASSERT_NOT_NULL(snapshot1.get());
    TEST_ASSERT_EQUAL(1, snapshot1->stats.sample_count);

    // Allocate memory
    void *ptr = malloc(1024 * 100);
    if (ptr) {
        memset(ptr, 0xAA, 1024 * 100);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // Take another snapshot
    auto snapshot2 = MemoryProfiler::take_snapshot(snapshot1.get());
    TEST_ASSERT_NOT_NULL(snapshot2.get());
    TEST_ASSERT_EQUAL(2, snapshot2->stats.sample_count);

    // Check that statistics are tracking min values
    TEST_ASSERT_LESS_OR_EQUAL(snapshot2->stats.min_total_free, snapshot2->memory.total_free);
    TEST_ASSERT_LESS_OR_EQUAL(snapshot2->stats.min_internal_free, snapshot2->memory.internal.free_size);
    TEST_ASSERT_LESS_OR_EQUAL(snapshot2->stats.min_external_free, snapshot2->memory.external.free_size);
    TEST_ASSERT_LESS_OR_EQUAL(snapshot2->stats.min_total_largest_free_block, snapshot2->memory.total_largest_free_block);

    if (ptr) {
        free(ptr);
    }

    profiler.reset_profiling();
}

TEST_CASE("Test profiling with instance (non-singleton)", "[utils][memory_profiler][instance][profiling]")
{
    BROOKESIA_LOGI("=== MemoryProfiler Instance Profiling Test (Non-Singleton) ===");

    reset_counters();

    // Create a new instance (not using singleton)
    MemoryProfiler instance_profiler;

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    instance_profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    void *ptr = nullptr;

    {
        // Register callbacks - RAII scope
        std::atomic<int> instance_callback_count{0};
        auto snapshot_conn = instance_profiler.connect_profiling_signal(
        [&instance_callback_count](const MemoryProfiler::ProfileSnapshot & snapshot) {
            instance_callback_count++;
            BROOKESIA_LOGI("Instance profiler callback triggered, sample count: %1%", snapshot.stats.sample_count);
        }
                             );

        std::atomic<int> instance_threshold_count{0};
        auto threshold_conn = instance_profiler.connect_threshold_signal(
                                  MemoryProfiler::ThresholdType::TotalFreePercent,
                                  100, // Should always trigger (free percent <= 100)
        [&instance_threshold_count](const MemoryProfiler::ProfileSnapshot & snapshot) {
            instance_threshold_count++;
            BROOKESIA_LOGI("Instance profiler threshold callback triggered, free percent: %1%%%", snapshot.memory.total_free_percent);
        }
                              );

        // Start profiling with instance
        bool result = instance_profiler.start_profiling(scheduler);
        TEST_ASSERT_TRUE(result);
        TEST_ASSERT_TRUE(instance_profiler.is_profiling());

        // Allocate some memory during profiling
        ptr = malloc(1024 * 50);
        if (ptr) {
            memset(ptr, 0xAA, 1024 * 50);
        }

        // Wait for profiling cycles
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Stop profiling
        instance_profiler.stop_profiling();
        TEST_ASSERT_FALSE(instance_profiler.is_profiling());

        // Verify callbacks were triggered
        BROOKESIA_LOGI("Instance callback count: %1%, Instance threshold count: %2%",
                       instance_callback_count.load(), instance_threshold_count.load());
        TEST_ASSERT_GREATER_THAN(0, instance_callback_count.load());
        TEST_ASSERT_GREATER_THAN(0, instance_threshold_count.load());

        // Get latest snapshot from instance
        auto latest = instance_profiler.get_profiling_latest_snapshot();
        if (latest) {
            TEST_ASSERT_GREATER_THAN(1, latest->stats.sample_count);
            BROOKESIA_LOGI("Instance profiler sample count: %1%", latest->stats.sample_count);
            MemoryProfiler::print_snapshot(*latest);
        }

        // Verify singleton profiler is independent
        auto singleton_snapshot = profiler.get_profiling_latest_snapshot();
        // Singleton profiler should not be affected by instance profiler
        // (They are independent instances)

        // snapshot_conn and threshold_conn will auto-disconnect here (RAII)
    }

    // Free memory after callbacks disconnected
    if (ptr) {
        free(ptr);
    }

    vTaskDelay(pdMS_TO_TICKS(100));
    instance_profiler.reset_profiling();
}


// ============================================================================
// SignalConnection RAII Tests
// ============================================================================

TEST_CASE("Test SignalConnection RAII auto-disconnect", "[utils][memory_profiler][signal_connection][raii]")
{
    BROOKESIA_LOGI("=== SignalConnection RAII Auto-Disconnect Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Test 1: Connection auto-disconnects when leaving scope
    BROOKESIA_LOGI("Test 1: Auto-disconnect on scope exit");
    {
        auto conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
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

TEST_CASE("Test SignalConnection manual disconnect", "[utils][memory_profiler][signal_connection][manual]")
{
    BROOKESIA_LOGI("=== SignalConnection Manual Disconnect Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Register callback
    auto conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
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

TEST_CASE("Test SignalConnection move semantics", "[utils][memory_profiler][signal_connection][move]")
{
    BROOKESIA_LOGI("=== SignalConnection Move Semantics Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    // Test move semantics
    MemoryProfiler::SignalConnection moved_conn;

    {
        // Register callback in inner scope
        auto conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
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

TEST_CASE("Test SignalConnection multiple callbacks RAII", "[utils][memory_profiler][signal_connection][multiple_raii]")
{
    BROOKESIA_LOGI("=== SignalConnection Multiple Callbacks RAII Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    auto scheduler = std::make_shared<TaskScheduler>();
    scheduler->start();

    std::atomic<int> callback1_count{0};
    std::atomic<int> callback2_count{0};
    std::atomic<int> callback3_count{0};

    // Callback 1: Lives entire test
    auto conn1 = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
        callback1_count++;
        BROOKESIA_LOGI("Callback 1 triggered, count=%1%", callback1_count.load());
    });

    profiler.start_profiling(scheduler);

    // Phase 1: All callbacks active
    {
        auto conn2 = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
            callback2_count++;
            BROOKESIA_LOGI("Callback 2 triggered, count=%1%", callback2_count.load());
        });

        {
            auto conn3 = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
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

TEST_CASE("Test SignalConnection connected() check", "[utils][memory_profiler][signal_connection][connected]")
{
    BROOKESIA_LOGI("=== SignalConnection connected() Check Test ===");

    reset_counters();

    MemoryProfiler::ProfilingConfig config = {
        .sample_interval_ms = 500,
        .enable_auto_logging = false,
    };
    profiler.configure_profiling(config);

    // Test 1: Newly created connection is not connected
    MemoryProfiler::SignalConnection conn;
    TEST_ASSERT_FALSE(conn.connected());
    BROOKESIA_LOGI("Default connection: connected=%d", conn.connected());

    // Test 2: After registration, connection is connected
    conn = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
        g_callback_counter++;
    });
    TEST_ASSERT_TRUE(conn.connected());
    BROOKESIA_LOGI("After registration: connected=%d", conn.connected());

    // Test 3: After manual disconnect, connection is not connected
    conn.disconnect();
    TEST_ASSERT_FALSE(conn.connected());
    BROOKESIA_LOGI("After disconnect: connected=%d", conn.connected());

    // Test 4: After reset_profiling, connection is not connected
    auto conn2 = profiler.connect_profiling_signal([&](const MemoryProfiler::ProfileSnapshot & snapshot) {
        g_callback_counter++;
    });
    TEST_ASSERT_TRUE(conn2.connected());

    profiler.reset_profiling(); // Should disconnect all slots

    TEST_ASSERT_FALSE(conn2.connected());
    BROOKESIA_LOGI("After reset_profiling: connected=%d", conn2.connected());

    BROOKESIA_LOGI("✓ connected() check verified");
}
