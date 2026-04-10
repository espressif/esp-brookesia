/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <future>
#include <thread>
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "unity.h"
#include "boost/thread.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"

using namespace esp_brookesia::lib_utils;

// ==================== Helper ====================

// ThreadConfig::get_current_config().stack_size returns the FreeRTOS high-water-mark
// (minimum free stack ever observed), NOT the allocated stack size. Before comparing a
// "configured" ThreadConfig against a "current" ThreadConfig via operator==, normalize
// the expected stack_size to the actual high-water-mark so the comparison is valid for
// all other fields (name, core_id, priority, stack_in_ext).
static void assert_config_matches_current(ThreadConfig expected)
{
    auto current = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
    BROOKESIA_LOGI("Expected: %1%, Current: %2%", expected, current);
    expected.stack_size = current.stack_size;
    TEST_ASSERT_TRUE(expected == current);
}

// ==================== BROOKESIA_THREAD_GET_CURRENT_CONFIG ====================

TEST_CASE("BROOKESIA_THREAD_GET_CURRENT_CONFIG on main task",
          "[utils][thread_config][current_config]")
{
    BROOKESIA_LOGI("=== BROOKESIA_THREAD_GET_CURRENT_CONFIG on Main Task ===");

    ThreadConfig current = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
    BROOKESIA_LOGI("Main task: %1%", current);

    TEST_ASSERT_FALSE(current.name.empty());
    TEST_ASSERT_GREATER_OR_EQUAL(-1, current.core_id);
    TEST_ASSERT_GREATER_THAN(0, current.priority);
    // stack_hwm > 0 proves the task exists and has remaining stack
    TEST_ASSERT_GREATER_THAN(0, current.stack_size);
}

// ==================== BROOKESIA_THREAD_CONFIG_GUARD: applied config ====================

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD applies and restores config",
          "[utils][thread_config][guard][applied]")
{
    BROOKESIA_LOGI("=== Guard Applies and Restores Applied Config ===");

    ThreadConfig original = BROOKESIA_THREAD_GET_APPLIED_CONFIG();
    BROOKESIA_LOGI("Original: %1%", original);

    {
        BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{
            .name = "guard_test", .core_id = 0, .priority = 12, .stack_size = 5120
        });

        ThreadConfig applied = BROOKESIA_THREAD_GET_APPLIED_CONFIG();
        BROOKESIA_LOGI("While active: %1%", applied);
        TEST_ASSERT_EQUAL(0,    applied.core_id);
        TEST_ASSERT_EQUAL(12,   applied.priority);
        TEST_ASSERT_EQUAL(5120, applied.stack_size);
    }

    ThreadConfig restored = BROOKESIA_THREAD_GET_APPLIED_CONFIG();
    BROOKESIA_LOGI("Restored: %1%", restored);
    TEST_ASSERT_TRUE(original == restored);
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD with inline initializer",
          "[utils][thread_config][guard][inline]")
{
    BROOKESIA_LOGI("=== Guard with Inline Initializer ===");

    ThreadConfig original = BROOKESIA_THREAD_GET_APPLIED_CONFIG();

    {
        BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{.priority = 8, .stack_size = 4096});
        TEST_ASSERT_EQUAL(8,    BROOKESIA_THREAD_GET_APPLIED_CONFIG().priority);
        TEST_ASSERT_EQUAL(4096, BROOKESIA_THREAD_GET_APPLIED_CONFIG().stack_size);
    }

    TEST_ASSERT_TRUE(original == BROOKESIA_THREAD_GET_APPLIED_CONFIG());
}

// ==================== Guard with std::thread ====================

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates priority to std::thread",
          "[utils][thread_config][guard][std_thread][priority]")
{
    BROOKESIA_LOGI("=== Guard Priority -> std::thread ===");

    ThreadConfig config{.name = "prio_std", .core_id = 0, .priority = 15, .stack_size = 4096};

    {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates name to std::thread",
          "[utils][thread_config][guard][std_thread][name]")
{
    BROOKESIA_LOGI("=== Guard Name -> std::thread ===");

    ThreadConfig config{.name = "named_std", .core_id = 0};

    {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates core_id to std::thread",
          "[utils][thread_config][guard][std_thread][core]")
{
    BROOKESIA_LOGI("=== Guard Core -> std::thread ===");

    {
        ThreadConfig config{.name = "core-1_std", .core_id = -1};

        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }

    for (int i = 0; i < CONFIG_SOC_CPU_CORES_NUM; i++) {
        ThreadConfig config{.name = "core" + std::to_string(i) + "_std", .core_id = i};
        BROOKESIA_THREAD_CONFIG_GUARD(config);
        std::thread([config]() {
            assert_config_matches_current(config);
        }).join();
    }
}

#if CONFIG_SPIRAM
TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates stack_in_ext to std::thread",
          "[utils][thread_config][guard][std_thread][stack_ext]")
{
    BROOKESIA_LOGI("=== Guard Stack-In-Ext -> std::thread ===");

    {
        ThreadConfig config{
            .name = "ext_std", .core_id = 0, .stack_in_ext = true
        };

        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}
#endif

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD applied config is restored after std::thread exits",
          "[utils][thread_config][guard][std_thread][restore]")
{
    BROOKESIA_LOGI("=== Applied Config Restored After std::thread Exits ===");

    ThreadConfig original = BROOKESIA_THREAD_GET_APPLIED_CONFIG();

    {
        ThreadConfig config{.name = "restore_std", .core_id = 0, .priority = 18, .stack_size = 6144};

        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }

    TEST_ASSERT_TRUE(original == BROOKESIA_THREAD_GET_APPLIED_CONFIG());
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD multiple std::threads share same config",
          "[utils][thread_config][guard][std_thread][multiple]")
{
    BROOKESIA_LOGI("=== Multiple std::threads Share Same Config ===");

    ThreadConfig config{.name = "multi_std", .core_id = 0, .priority = 9, .stack_size = 4096};
    std::vector<std::thread> threads;

    {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        for (int i = 0; i < 4; i++) {
            threads.emplace_back([config, i]() {
                BROOKESIA_LOGI("Thread %d", i);
                assert_config_matches_current(config);
            });
        }

        for (auto &t : threads) {
            t.join();
        }
    }
}

// ==================== Guard with boost::thread ====================

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates priority to boost::thread",
          "[utils][thread_config][guard][boost][priority]")
{
    BROOKESIA_LOGI("=== Guard Priority -> boost::thread ===");

    ThreadConfig config{.name = "prio_boost", .core_id = 0, .priority = 16, .stack_size = 4096};

    {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        boost::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates name to boost::thread",
          "[utils][thread_config][guard][boost][name]")
{
    BROOKESIA_LOGI("=== Guard Name -> boost::thread ===");

    ThreadConfig config{.name = "named_boost", .core_id = 0};

    {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        boost::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD propagates core_id to boost::thread",
          "[utils][thread_config][guard][boost][core]")
{
    BROOKESIA_LOGI("=== Guard Core -> boost::thread ===");

    {
        ThreadConfig config{.name = "core-1_boost", .core_id = -1};

        BROOKESIA_THREAD_CONFIG_GUARD(config);

        boost::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }

    for (int i = 0; i < CONFIG_SOC_CPU_CORES_NUM; i++) {
        ThreadConfig config{.name = "core" + std::to_string(i) + "_boost", .core_id = i};
        BROOKESIA_THREAD_CONFIG_GUARD(config);
        boost::thread([config]() {
            assert_config_matches_current(config);
        }).join();
    }
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD multiple boost::threads share same config",
          "[utils][thread_config][guard][boost][multiple]")
{
    BROOKESIA_LOGI("=== Multiple boost::threads Share Same Config ===");

    // Run in a std::thread to guarantee sufficient stack for boost::thread_group
    std::thread([]() {
        ThreadConfig config{.name = "multi_boost", .core_id = 0, .priority = 7, .stack_size = 4096};
        boost::thread_group group;

        {
            BROOKESIA_THREAD_CONFIG_GUARD(config);

            for (int i = 0; i < 4; i++) {
                group.create_thread([config, i]() {
                    BROOKESIA_LOGI("boost::thread %d", i);
                    assert_config_matches_current(config);
                });
            }

            group.join_all();
        }
    }).join();
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD applied config restored after boost::thread exits",
          "[utils][thread_config][guard][boost][restore]")
{
    BROOKESIA_LOGI("=== Applied Config Restored After boost::thread Exits ===");

    ThreadConfig original = BROOKESIA_THREAD_GET_APPLIED_CONFIG();

    {
        ThreadConfig config{.name = "restore_boost", .core_id = 0, .priority = 17, .stack_size = 6144};

        BROOKESIA_THREAD_CONFIG_GUARD(config);

        boost::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }

    TEST_ASSERT_TRUE(original == BROOKESIA_THREAD_GET_APPLIED_CONFIG());
}

// ==================== Nested and sequential guards ====================

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD nested scopes each thread sees its guard",
          "[utils][thread_config][guard][nested]")
{
    BROOKESIA_LOGI("=== Nested Guards: each thread sees its guard's config ===");

    ThreadConfig outer_cfg{.name = "outer", .core_id = 0, .priority = 6,  .stack_size = 4096};
    ThreadConfig inner_cfg{.name = "inner", .core_id = 0, .priority = 14, .stack_size = 4096};

    {
        BROOKESIA_THREAD_CONFIG_GUARD(outer_cfg);

        // outer_t is created while the outer guard is still active
        std::thread outer_t([outer_cfg]() {
            assert_config_matches_current(outer_cfg);
        });

        {
            BROOKESIA_THREAD_CONFIG_GUARD(inner_cfg);

            std::thread inner_t([inner_cfg]() {
                assert_config_matches_current(inner_cfg);
            });
            inner_t.join();
        }

        outer_t.join();
    }
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD nested scopes restores in LIFO order",
          "[utils][thread_config][guard][nested][lifo]")
{
    BROOKESIA_LOGI("=== Nested Guards Restore in LIFO Order ===");

    ThreadConfig original = BROOKESIA_THREAD_GET_APPLIED_CONFIG();

    {
        BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{.priority = 6,  .stack_size = 4096});
        TEST_ASSERT_EQUAL(6,  BROOKESIA_THREAD_GET_APPLIED_CONFIG().priority);
        {
            BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{.priority = 10, .stack_size = 4096});
            TEST_ASSERT_EQUAL(10, BROOKESIA_THREAD_GET_APPLIED_CONFIG().priority);
            {
                BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{.priority = 14, .stack_size = 4096});
                TEST_ASSERT_EQUAL(14, BROOKESIA_THREAD_GET_APPLIED_CONFIG().priority);
            }
            TEST_ASSERT_EQUAL(10, BROOKESIA_THREAD_GET_APPLIED_CONFIG().priority);
        }
        TEST_ASSERT_EQUAL(6,  BROOKESIA_THREAD_GET_APPLIED_CONFIG().priority);
    }

    TEST_ASSERT_TRUE(original == BROOKESIA_THREAD_GET_APPLIED_CONFIG());
}

TEST_CASE("BROOKESIA_THREAD_CONFIG_GUARD sequential scopes each thread sees its guard",
          "[utils][thread_config][guard][sequential]")
{
    BROOKESIA_LOGI("=== Sequential Guards: each thread sees its guard's config ===");

    {
        ThreadConfig cfg{.name = "seq1", .core_id = 0, .priority = 6, .stack_size = 4096};
        BROOKESIA_THREAD_CONFIG_GUARD(cfg);

        std::thread t([cfg]() {
            assert_config_matches_current(cfg);
        });
        t.join();
    }

    {
        ThreadConfig cfg{.name = "seq2", .core_id = 0, .priority = 11, .stack_size = 4096};
        BROOKESIA_THREAD_CONFIG_GUARD(cfg);

        std::thread t([cfg]() {
            assert_config_matches_current(cfg);
        });
        t.join();
    }
}

// ==================== Boundary conditions ====================

TEST_CASE("boundary: BROOKESIA_THREAD_CONFIG_GUARD with empty name",
          "[utils][thread_config][guard][boundary][empty_name]")
{
    BROOKESIA_LOGI("=== Boundary: Empty Name ===");

    {
        BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{
            .name = "", .core_id = 0
        });

        std::thread t([]() {
            ThreadConfig current = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
            BROOKESIA_LOGI("Empty-name thread: %1%", current);
            TEST_ASSERT_FALSE(current.name.empty());  // FreeRTOS always assigns a name
        });
        t.join();
    }
}

TEST_CASE("boundary: BROOKESIA_THREAD_CONFIG_GUARD with minimum stack",
          "[utils][thread_config][guard][boundary][min_stack]")
{
    BROOKESIA_LOGI("=== Boundary: Minimum Stack Size ===");

    {
        // 3072 is the minimum stack size accepted by the FreeRTOS pthread port
        BROOKESIA_THREAD_CONFIG_GUARD(ThreadConfig{
            .name = "min_stk", .core_id = 0, .priority = 5, .stack_size = 3072
        });

        std::thread t([]() {
            ThreadConfig current = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
            BROOKESIA_LOGI("Min-stack thread: %1%", current);
            TEST_ASSERT_GREATER_THAN(0, current.stack_size);
        });
        t.join();
    }
}

TEST_CASE("boundary: BROOKESIA_THREAD_CONFIG_GUARD with high priority",
          "[utils][thread_config][guard][boundary][high_priority]")
{
    BROOKESIA_LOGI("=== Boundary: High Priority ===");

    // configMAX_PRIORITIES - 2: leave room for system tasks at the top
    const size_t high_prio = configMAX_PRIORITIES - 2;
    ThreadConfig config{.name = "high_prio", .core_id = 0, .priority = high_prio, .stack_size = 4096};

    {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}

#if !CONFIG_SPIRAM
TEST_CASE("boundary: BROOKESIA_THREAD_CONFIG_GUARD with external stack while PSRAM is not available",
          "[utils][thread_config][guard][boundary][ext_stack_while_not_available]")
{
    BROOKESIA_LOGI("=== Boundary: External Stack While PSRAM Not Available ===");

    ThreadConfig config{.name = "ext_stack", .core_id = 0, .stack_in_ext = true};
    BROOKESIA_THREAD_CONFIG_GUARD(config);

    std::thread t([config]() {
        auto current = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
        BROOKESIA_LOGI("Current: %1%", current);
        TEST_ASSERT_FALSE(current.stack_in_ext);
    });
    t.join();
}
#endif

TEST_CASE("boundary: BROOKESIA_THREAD_CONFIG_GUARD with core id exceeds the number of cores",
          "[utils][thread_config][guard][boundary][core_id_exceeds_the_number_of_cores]")
{
    BROOKESIA_LOGI("=== Boundary: Core ID Exceeds the Number of Cores ===");

    ThreadConfig config{.name = "core_id_exceeds_the_number_of_cores", .core_id = CONFIG_SOC_CPU_CORES_NUM};
    BROOKESIA_THREAD_CONFIG_GUARD(config);

    std::thread t([config]() {
        auto current = BROOKESIA_THREAD_GET_CURRENT_CONFIG();
        BROOKESIA_LOGI("Current: %1%", current);
    });
    t.join();
}

TEST_CASE("boundary: BROOKESIA_THREAD_CONFIG_GUARD rapid create/destroy",
          "[utils][thread_config][guard][boundary][rapid]")
{
    BROOKESIA_LOGI("=== Boundary: Rapid Create/Destroy ===");

    ThreadConfig original = BROOKESIA_THREAD_GET_APPLIED_CONFIG();
    ThreadConfig config{.name = "rapid", .core_id = 0};

    for (int i = 0; i < 10; i++) {
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([config]() {
            assert_config_matches_current(config);
        });
        t.join();
    }

    // Applied config must be fully restored after all guards are gone
    ThreadConfig restored = BROOKESIA_THREAD_GET_APPLIED_CONFIG();
    BROOKESIA_LOGI("Restored: %1%", restored);
    TEST_ASSERT_TRUE(original == restored);
}

TEST_CASE("boundary: BROOKESIA_THREAD_GET_CURRENT_CONFIG in any thread type",
          "[utils][thread_config][guard][boundary][no_crash]")
{
    BROOKESIA_LOGI("=== Boundary: get_current_config Does Not Crash ===");

    ThreadConfig config{.priority = 5, .stack_size = 4096};

    // std::thread
    {
        config.name = "std_thread";
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        std::thread t([&]() {
            assert_config_matches_current(config);
        });
        t.join();
    }

    // std::async
    {
        config.name = "std_async";
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        auto future = std::async(std::launch::async, [&]() {
            assert_config_matches_current(config);
        });
        future.get();
    }

    // boost::thread
    {
        config.name = "boost_thread";
        BROOKESIA_THREAD_CONFIG_GUARD(config);

        boost::thread t([&]() {
            assert_config_matches_current(config);
        });
        t.join();
    }
}
