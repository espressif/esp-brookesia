/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <thread>
#include <string>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_pthread.h"
#include "esp_heap_caps.h"
#include "esp_idf_version.h"
#include "unity.h"
#include "boost/thread.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"

using namespace esp_brookesia::lib_utils;

// Global variables for testing
static std::vector<std::string> g_thread_names;
static std::vector<int> g_thread_priorities;
static std::vector<int> g_thread_cores;

// Testing helper functions
void record_thread_info(const char *label)
{
    TaskHandle_t current_task = xTaskGetCurrentTaskHandle();
    const char *task_name = pcTaskGetName(current_task);
    UBaseType_t priority = uxTaskPriorityGet(current_task);
    BaseType_t core = xPortGetCoreID();

    g_thread_names.push_back(task_name ? task_name : "null");
    g_thread_priorities.push_back(priority);
    g_thread_cores.push_back(core);

    BROOKESIA_LOGI("%s: name=%s, priority=%u, core=%d", label, task_name, priority, core);
}

// ==================== ThreadConfig structure testing ====================

TEST_CASE("Test ThreadConfig default values", "[utils][thread_config][struct]")
{
    BROOKESIA_LOGI("=== ThreadConfig Default Values Test ===");

    ThreadConfig config;

    TEST_ASSERT_TRUE(!config.name.empty());
    TEST_ASSERT_GREATER_OR_EQUAL(-1, config.core_id);
    TEST_ASSERT_GREATER_THAN(0, config.priority);
    TEST_ASSERT_GREATER_THAN(0, config.stack_size);

    BROOKESIA_LOGI("Default config: %s", BROOKESIA_DESCRIBE_TO_STR(config).c_str());
}

TEST_CASE("Test ThreadConfig custom values", "[utils][thread_config][struct]")
{
    BROOKESIA_LOGI("=== ThreadConfig Custom Values Test ===");

    ThreadConfig config = {
        .name = "custom_thread",
        .core_id = 1,
        .priority = 10,
        .stack_size = 4096,
        .stack_in_ext = true
    };

    TEST_ASSERT_TRUE(config.name == "custom_thread");
    TEST_ASSERT_EQUAL(1, config.core_id);
    TEST_ASSERT_EQUAL(10, config.priority);
    TEST_ASSERT_EQUAL(4096, config.stack_size);
    TEST_ASSERT_TRUE(config.stack_in_ext);

    BROOKESIA_LOGI("Custom config: %s", BROOKESIA_DESCRIBE_TO_STR(config).c_str());
}

TEST_CASE("Test ThreadConfig BROOKESIA_DESCRIBE_TO_STR", "[utils][thread_config][describe]")
{
    BROOKESIA_LOGI("=== ThreadConfig BROOKESIA_DESCRIBE_TO_STR Test ===");

    ThreadConfig config = {
        .name = "test_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 2048,
        .stack_in_ext = false
    };

    std::string desc = BROOKESIA_DESCRIBE_TO_STR(config);

    TEST_ASSERT_TRUE(desc.find("name") != std::string::npos);
    TEST_ASSERT_TRUE(desc.find("core_id") != std::string::npos);
    TEST_ASSERT_TRUE(desc.find("priority") != std::string::npos);
    TEST_ASSERT_TRUE(desc.find("stack_size") != std::string::npos);
    TEST_ASSERT_TRUE(desc.find("stack_in_ext") != std::string::npos);

    BROOKESIA_LOGI("Described: %s", desc.c_str());
}

TEST_CASE("Test ThreadConfig with JSON format", "[utils][thread_config][describe][json]")
{
    BROOKESIA_LOGI("=== ThreadConfig JSON Format Test ===");

    ThreadConfig config = {
        .name = "json_thread",
        .core_id = 1,
        .priority = 8,
        .stack_size = 8192,
        .stack_in_ext = true
    };

    std::string json = BROOKESIA_DESCRIBE_TO_STR_WITH_FMT(config, BROOKESIA_DESCRIBE_FORMAT_JSON);

    TEST_ASSERT_TRUE(json.find("\"name\"") != std::string::npos);
    TEST_ASSERT_TRUE(json.find("\"core_id\"") != std::string::npos);
    TEST_ASSERT_TRUE(json.find("{") != std::string::npos);
    TEST_ASSERT_TRUE(json.find("}") != std::string::npos);

    BROOKESIA_LOGI("JSON format: %s", json.c_str());
}

// ==================== ThreadConfig apply/get Test ====================


TEST_CASE("Test ThreadConfig apply and get", "[utils][thread_config][apply]")
{
    BROOKESIA_LOGI("=== ThreadConfig Apply and Get Test ===");

    // Get current config
    ThreadConfig original = ThreadConfig::get_applied_config();
    BROOKESIA_LOGI("Original config: %s", BROOKESIA_DESCRIBE_TO_STR(original).c_str());

    // Apply new config
    ThreadConfig new_config = {
        .name = "test_apply",
        .core_id = 0,
        .priority = 10,
        .stack_size = 4096,
        .stack_in_ext = false
    };
    new_config.apply();

    // Get applied config
    ThreadConfig applied = ThreadConfig::get_applied_config();
    BROOKESIA_LOGI("Applied config: %s", BROOKESIA_DESCRIBE_TO_STR(applied).c_str());

    // Verify config is applied
    TEST_ASSERT_EQUAL(new_config.core_id, applied.core_id);
    TEST_ASSERT_EQUAL(new_config.priority, applied.priority);
    TEST_ASSERT_EQUAL(new_config.stack_size, applied.stack_size);

    // Restore original config
    original.apply();
}

// ==================== ThreadConfigGuard Basic Test ====================

TEST_CASE("Test ThreadConfigGuard basic usage", "[utils][thread_config][guard][basic]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Basic Usage Test ===");

    ThreadConfig original = ThreadConfig::get_applied_config();
    BROOKESIA_LOGI("Original config: %s", BROOKESIA_DESCRIBE_TO_STR(original).c_str());

    {
        ThreadConfig config = {
            .name = "basic_thread",
            .core_id = 0,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        ThreadConfigGuard guard(config);
        BROOKESIA_LOGI("ThreadConfigGuard created");

        // Verify config is applied
        ThreadConfig current = ThreadConfig::get_applied_config();
        TEST_ASSERT_EQUAL(config.priority, current.priority);
        TEST_ASSERT_EQUAL(config.stack_size, current.stack_size);

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    // Verify config is restored
    ThreadConfig restored = ThreadConfig::get_applied_config();
    BROOKESIA_LOGI("Restored config: %s", BROOKESIA_DESCRIBE_TO_STR(restored).c_str());
    TEST_ASSERT_EQUAL(original.priority, restored.priority);
    TEST_ASSERT_EQUAL(original.stack_size, restored.stack_size);
}

TEST_CASE("Test ThreadConfigGuard with default config", "[utils][thread_config][guard][default]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Default Config Test ===");

    ThreadConfig config;

    {
        ThreadConfigGuard guard(config);
        BROOKESIA_LOGI("Using default config");
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    TEST_ASSERT_TRUE(true);
}

// ==================== ThreadConfigGuard Thread Creation Test ====================

TEST_CASE("Test ThreadConfigGuard with std::thread", "[utils][thread_config][guard][std_thread]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with std::thread Test ===");

    g_thread_names.clear();

    ThreadConfig config = {
        .name = "std_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            record_thread_info("std::thread");
        });

        t.join();
    }

    // Verify thread name
    TEST_ASSERT_EQUAL(1, g_thread_names.size());
    TEST_ASSERT_TRUE(g_thread_names[0].find("std_thread") != std::string::npos);
}

TEST_CASE("Test ThreadConfigGuard with multiple threads", "[utils][thread_config][guard][multiple]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Multiple Threads Test ===");

    g_thread_names.clear();

    ThreadConfig config = {
        .name = "multi_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    const int num_threads = 3;
    std::vector<std::thread> threads;

    {
        ThreadConfigGuard guard(config);

        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([i]() {
                BROOKESIA_LOGI("Thread %d running", i);
                record_thread_info("multi_thread");
                vTaskDelay(pdMS_TO_TICKS(10));
            });
        }

        // Wait for all threads to complete
        for (auto &t : threads) {
            t.join();
        }
    }

    TEST_ASSERT_EQUAL(num_threads, g_thread_names.size());
}

// ==================== ThreadConfigGuard Different Config Test ====================

TEST_CASE("Test ThreadConfigGuard with high priority", "[utils][thread_config][guard][priority]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard High Priority Test ===");

    g_thread_priorities.clear();

    ThreadConfig config = {
        .name = "high_prio",
        .core_id = 0,
        .priority = 20,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            record_thread_info("high_prio");
        });

        t.join();
    }

    // Verify priority
    TEST_ASSERT_EQUAL(1, g_thread_priorities.size());
    BROOKESIA_LOGI("Thread priority: %d", g_thread_priorities[0]);
}

TEST_CASE("Test ThreadConfigGuard with large stack", "[utils][thread_config][guard][stack]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Large Stack Test ===");

    ThreadConfig config = {
        .name = "large_stack",
        .core_id = 0,
        .priority = 5,
        .stack_size = 8192,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            // Use some stack space
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer));
            BROOKESIA_LOGI("Large stack thread running");
        });

        t.join();
    }

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test ThreadConfigGuard with core pinning", "[utils][thread_config][guard][core]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Core Pinning Test ===");

    g_thread_cores.clear();

    // Test Core 0
    {
        ThreadConfig config = {
            .name = "core0_thread",
            .core_id = 0,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        {
            ThreadConfigGuard guard(config);

            std::thread t([]() {
                record_thread_info("core0_thread");
            });

            t.join();
        }
    }

    // Verify core
    TEST_ASSERT_EQUAL(1, g_thread_cores.size());
    TEST_ASSERT_EQUAL(0, g_thread_cores[0]);

#if CONFIG_SOC_CPU_CORES_NUM > 1
    // Test Core 1 (if available)
    g_thread_cores.clear();

    {
        ThreadConfig config = {
            .name = "core1_thread",
            .core_id = 1,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        {
            ThreadConfigGuard guard(config);

            std::thread t([]() {
                record_thread_info("core1_thread");
            });

            t.join();
        }
    }

    TEST_ASSERT_EQUAL(1, g_thread_cores.size());
    TEST_ASSERT_EQUAL(1, g_thread_cores[0]);
#endif
}

TEST_CASE("Test ThreadConfigGuard with external stack", "[utils][thread_config][guard][ext_stack]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard External Stack Test ===");

    ThreadConfig config = {
        .name = "ext_stack",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = true
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            BROOKESIA_LOGI("Thread with external stack running");
        });

        t.join();
    }

    TEST_ASSERT_TRUE(true);
}

// ==================== ThreadConfigGuard Nested Test ====================

TEST_CASE("Test ThreadConfigGuard nested scopes", "[utils][thread_config][guard][nested]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Nested Scopes Test ===");

    ThreadConfig config1 = {
        .name = "outer_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    ThreadConfig config2 = {
        .name = "inner_thread",
        .core_id = 0,
        .priority = 10,
        .stack_size = 8192,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard1(config1);
        BROOKESIA_LOGI("Outer guard created");

        std::thread t1([]() {
            BROOKESIA_LOGI("Outer thread running");
        });

        {
            ThreadConfigGuard guard2(config2);
            BROOKESIA_LOGI("Inner guard created");

            std::thread t2([]() {
                BROOKESIA_LOGI("Inner thread running");
            });

            t2.join();
            BROOKESIA_LOGI("Inner thread joined");
        }

        BROOKESIA_LOGI("Inner guard destroyed");
        t1.join();
        BROOKESIA_LOGI("Outer thread joined");
    }

    BROOKESIA_LOGI("Outer guard destroyed");
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test ThreadConfigGuard sequential scopes", "[utils][thread_config][guard][sequential]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Sequential Scopes Test ===");

    SemaphoreHandle_t sem = xSemaphoreCreateBinary();

    // First config
    {
        ThreadConfig config = {
            .name = "first_thread",
            .core_id = 0,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        ThreadConfigGuard guard(config);

        std::thread t([sem]() {
            BROOKESIA_LOGI("First thread running");
            xSemaphoreGive(sem);
        });

        xSemaphoreTake(sem, pdMS_TO_TICKS(1000));
        t.join();
    }

    // Second config
    {
        ThreadConfig config = {
            .name = "second_thread",
            .core_id = 0,
            .priority = 10,
            .stack_size = 8192,
            .stack_in_ext = false
        };

        ThreadConfigGuard guard(config);

        std::thread t([sem]() {
            BROOKESIA_LOGI("Second thread running");
            xSemaphoreGive(sem);
        });

        xSemaphoreTake(sem, pdMS_TO_TICKS(1000));
        t.join();
    }

    vSemaphoreDelete(sem);
    TEST_ASSERT_TRUE(true);
}

// ==================== ThreadConfigGuard Boundary Test ====================

TEST_CASE("Test ThreadConfigGuard with minimum stack", "[utils][thread_config][guard][boundary]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Minimum Stack Test ===");

    ThreadConfig config = {
        .name = "min_stack",
        .core_id = 0,
        .priority = 5,
        .stack_size = 3072,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            BROOKESIA_LOGI("Minimum stack thread running");
        });

        t.join();
    }

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test ThreadConfigGuard with null name", "[utils][thread_config][guard][null_name]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Null Name Test ===");

    ThreadConfig config = {
        .name = "",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            BROOKESIA_LOGI("Thread with null name running");
        });

        t.join();
    }

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test ThreadConfigGuard with empty name", "[utils][thread_config][guard][empty_name]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Empty Name Test ===");

    ThreadConfig config = {
        .name = "",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        std::thread t([]() {
            BROOKESIA_LOGI("Thread with empty name running");
        });

        t.join();
    }

    TEST_ASSERT_TRUE(true);
}

// ==================== ThreadConfigGuard Real World Test ====================

TEST_CASE("Test ThreadConfigGuard real world worker thread", "[utils][thread_config][guard][real_world]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Real World - Worker Thread Test ===");

    ThreadConfig config = {
        .name = "worker",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    SemaphoreHandle_t start_sem = xSemaphoreCreateBinary();
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    int result = 0;

    {
        ThreadConfigGuard guard(config);

        std::thread worker([&result, start_sem, done_sem]() {
            // Wait for start signal
            xSemaphoreTake(start_sem, portMAX_DELAY);

            // Execute work
            BROOKESIA_LOGI("Worker processing...");
            result = 42;
            vTaskDelay(pdMS_TO_TICKS(100));

            // Notify completion
            xSemaphoreGive(done_sem);
        });

        // Send start signal
        xSemaphoreGive(start_sem);

        // Wait for completion
        xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000));
        worker.join();
    }

    vSemaphoreDelete(start_sem);
    vSemaphoreDelete(done_sem);

    TEST_ASSERT_EQUAL(42, result);
}

TEST_CASE("Test ThreadConfigGuard real world producer consumer", "[utils][thread_config][guard][real_world_pc]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Real World - Producer-Consumer Test ===");

    ThreadConfig producer_config = {
        .name = "producer",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    ThreadConfig consumer_config = {
        .name = "consumer",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    SemaphoreHandle_t data_ready = xSemaphoreCreateBinary();
    int shared_data = 0;

    // Producer
    std::thread producer;
    {
        ThreadConfigGuard guard(producer_config);

        producer = std::thread([&shared_data, data_ready]() {
            BROOKESIA_LOGI("Producer: producing data");
            shared_data = 100;
            xSemaphoreGive(data_ready);
            BROOKESIA_LOGI("Producer: data ready");
        });
    }

    // Consumer
    std::thread consumer;
    {
        ThreadConfigGuard guard(consumer_config);

        consumer = std::thread([&shared_data, data_ready]() {
            BROOKESIA_LOGI("Consumer: waiting for data");
            xSemaphoreTake(data_ready, portMAX_DELAY);
            BROOKESIA_LOGI("Consumer: consuming data = %d", shared_data);
            TEST_ASSERT_EQUAL(100, shared_data);
        });
    }

    // Wait for completion
    producer.join();
    consumer.join();
    vSemaphoreDelete(data_ready);
}

// ==================== ThreadConfigGuard Stress Test ====================

TEST_CASE("Test ThreadConfigGuard stress many threads", "[utils][thread_config][guard][stress]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Stress - Many Threads Test ===");

    ThreadConfig config = {
        .name = "stress_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 3072,
        .stack_in_ext = false
    };

    const int num_threads = 10;
    std::vector<std::thread> threads;

    {
        ThreadConfigGuard guard(config);

        for (int i = 0; i < num_threads; i++) {
            threads.emplace_back([i]() {
                BROOKESIA_LOGI("Stress thread %d running", i);
                vTaskDelay(pdMS_TO_TICKS(10 + (i * 5)));
            });
        }

        // Wait for all threads to complete
        for (auto &t : threads) {
            t.join();
        }
    }

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test ThreadConfigGuard stress rapid create destroy", "[utils][thread_config][guard][stress_rapid]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Stress - Rapid Create/Destroy Test ===");

    const int iterations = 5;

    for (int i = 0; i < iterations; i++) {
        ThreadConfig config = {
            .name = "rapid_thread",
            .core_id = 0,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        {
            ThreadConfigGuard guard(config);

            std::thread t([i]() {
                BROOKESIA_LOGI("Rapid thread iteration %d", i);
            });

            t.join();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    TEST_ASSERT_TRUE(true);
}

// ==================== boost::thread Test ====================

TEST_CASE("Test ThreadConfigGuard with boost::thread basic", "[utils][thread_config][guard][boost][basic]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Basic Test ===");

    g_thread_names.clear();

    ThreadConfig config = {
        .name = "boost_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        boost::thread t([]() {
            record_thread_info("boost::thread");
        });

        t.join();
    }

    // Verify thread name
    TEST_ASSERT_EQUAL(1, g_thread_names.size());
    TEST_ASSERT_TRUE(g_thread_names[0].find("boost_thread") != std::string::npos);
}

TEST_CASE("Test ThreadConfigGuard with boost::thread multiple", "[utils][thread_config][guard][boost][multiple]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Multiple Test ===");

    std::thread([]() {
        g_thread_names.clear();

        ThreadConfig config = {
            .name = "boost_multi",
            .core_id = 0,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        const int num_threads = 3;
        boost::thread_group threads;

        {
            ThreadConfigGuard guard(config);

            for (int i = 0; i < num_threads; i++) {
                threads.create_thread([i]() {
                    BROOKESIA_LOGI("boost::thread %d running", i);
                    record_thread_info("boost_multi");
                    vTaskDelay(pdMS_TO_TICKS(10));
                });
            }

            // Wait for all threads to complete
            threads.join_all();
        }

        TEST_ASSERT_EQUAL(num_threads, g_thread_names.size());
    }).join();
}

TEST_CASE("Test ThreadConfigGuard with boost::thread high priority", "[utils][thread_config][guard][boost][priority]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread High Priority Test ===");

    g_thread_priorities.clear();

    ThreadConfig config = {
        .name = "boost_high_prio",
        .core_id = 0,
        .priority = 20,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        boost::thread t([]() {
            record_thread_info("boost_high_prio");
        });

        t.join();
    }

    // Verify priority
    TEST_ASSERT_EQUAL(1, g_thread_priorities.size());
    BROOKESIA_LOGI("boost::thread priority: %d", g_thread_priorities[0]);
}

TEST_CASE("Test ThreadConfigGuard with boost::thread core pinning", "[utils][thread_config][guard][boost][core]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Core Pinning Test ===");

    g_thread_cores.clear();

    // Test Core 0
    {
        ThreadConfig config = {
            .name = "boost_core0",
            .core_id = 0,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        {
            ThreadConfigGuard guard(config);

            boost::thread t([]() {
                record_thread_info("boost_core0");
            });

            t.join();
        }
    }

    // Verify core
    TEST_ASSERT_EQUAL(1, g_thread_cores.size());
    TEST_ASSERT_EQUAL(0, g_thread_cores[0]);

#if CONFIG_FREERTOS_UNICORE == 0
    // Test Core 1 (if available)
    g_thread_cores.clear();

    {
        ThreadConfig config = {
            .name = "boost_core1",
            .core_id = 1,
            .priority = 5,
            .stack_size = 4096,
            .stack_in_ext = false
        };

        {
            ThreadConfigGuard guard(config);

            boost::thread t([]() {
                record_thread_info("boost_core1");
            });

            t.join();
        }
    }

    TEST_ASSERT_EQUAL(1, g_thread_cores.size());
    TEST_ASSERT_EQUAL(1, g_thread_cores[0]);
#endif
}

TEST_CASE("Test ThreadConfigGuard with boost::thread nested", "[utils][thread_config][guard][boost][nested]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Nested Test ===");

    ThreadConfig config1 = {
        .name = "boost_outer",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    ThreadConfig config2 = {
        .name = "boost_inner",
        .core_id = 0,
        .priority = 10,
        .stack_size = 8192,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard1(config1);
        BROOKESIA_LOGI("Outer guard created");

        boost::thread t1([]() {
            BROOKESIA_LOGI("Outer boost::thread running");
        });

        {
            ThreadConfigGuard guard2(config2);
            BROOKESIA_LOGI("Inner guard created");

            boost::thread t2([]() {
                BROOKESIA_LOGI("Inner boost::thread running");
            });

            t2.join();
            BROOKESIA_LOGI("Inner boost::thread joined");
        }

        BROOKESIA_LOGI("Inner guard destroyed");
        t1.join();
        BROOKESIA_LOGI("Outer boost::thread joined");
    }

    BROOKESIA_LOGI("Outer guard destroyed");
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test ThreadConfigGuard with boost::thread interrupt", "[utils][thread_config][guard][boost][interrupt]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Interrupt Test ===");

    ThreadConfig config = {
        .name = "boost_interrupt",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    bool thread_interrupted = false;

    {
        ThreadConfigGuard guard(config);

        boost::thread t([&thread_interrupted]() {
            try {
                BROOKESIA_LOGI("boost::thread running, waiting for interrupt");
                for (int i = 0; i < 100; i++) {
                    boost::this_thread::interruption_point();
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            } catch (boost::thread_interrupted &) {
                thread_interrupted = true;
                BROOKESIA_LOGI("boost::thread interrupted");
            }
        });

        vTaskDelay(pdMS_TO_TICKS(50));
        t.interrupt();
        t.join();
    }

    TEST_ASSERT_TRUE(thread_interrupted);
}

TEST_CASE("Test ThreadConfigGuard with boost::thread real world worker", "[utils][thread_config][guard][boost][real_world]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Real World - Worker Test ===");

    ThreadConfig config = {
        .name = "boost_worker",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    SemaphoreHandle_t start_sem = xSemaphoreCreateBinary();
    SemaphoreHandle_t done_sem = xSemaphoreCreateBinary();
    int result = 0;

    {
        ThreadConfigGuard guard(config);

        boost::thread worker([&result, start_sem, done_sem]() {
            // Wait for start signal
            xSemaphoreTake(start_sem, portMAX_DELAY);

            // Execute work
            BROOKESIA_LOGI("boost::thread worker processing...");
            result = 42;
            vTaskDelay(pdMS_TO_TICKS(100));

            // Notify completion
            xSemaphoreGive(done_sem);
        });

        // Send start signal
        xSemaphoreGive(start_sem);

        // Wait for completion
        xSemaphoreTake(done_sem, pdMS_TO_TICKS(1000));
        worker.join();
    }

    vSemaphoreDelete(start_sem);
    vSemaphoreDelete(done_sem);

    TEST_ASSERT_EQUAL(42, result);
}

TEST_CASE("Test ThreadConfigGuard with boost::thread stress", "[utils][thread_config][guard][boost][stress]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard with boost::thread Stress Test ===");

    std::thread([]() {
        ThreadConfig config = {
            .name = "boost_stress",
            .core_id = 0,
            .priority = 5,
            .stack_size = 3072,
            .stack_in_ext = false
        };

        const int num_threads = 10;
        boost::thread_group threads;

        {
            ThreadConfigGuard guard(config);

            for (int i = 0; i < num_threads; i++) {
                threads.create_thread([i]() {
                    BROOKESIA_LOGI("boost::thread stress %d running", i);
                    vTaskDelay(pdMS_TO_TICKS(10 + (i * 5)));
                });
            }

            // Wait for all threads to complete
            threads.join_all();
        }

        TEST_ASSERT_TRUE(true);
    }).join();
}

TEST_CASE("Test ThreadConfigGuard mixed std and boost threads", "[utils][thread_config][guard][mixed]")
{
    BROOKESIA_LOGI("=== ThreadConfigGuard Mixed std::thread and boost::thread Test ===");

    ThreadConfig config = {
        .name = "mixed_thread",
        .core_id = 0,
        .priority = 5,
        .stack_size = 4096,
        .stack_in_ext = false
    };

    {
        ThreadConfigGuard guard(config);

        // Create std::thread
        std::thread std_t1([]() {
            BROOKESIA_LOGI("std::thread 1 running");
            vTaskDelay(pdMS_TO_TICKS(10));
        });

        std::thread std_t2([]() {
            BROOKESIA_LOGI("std::thread 2 running");
            vTaskDelay(pdMS_TO_TICKS(20));
        });

        // Create boost::thread
        boost::thread boost_t1([]() {
            BROOKESIA_LOGI("boost::thread 1 running");
            vTaskDelay(pdMS_TO_TICKS(15));
        });

        boost::thread boost_t2([]() {
            BROOKESIA_LOGI("boost::thread 2 running");
            vTaskDelay(pdMS_TO_TICKS(25));
        });

        // Wait for all threads to complete
        std_t1.join();
        std_t2.join();
        boost_t1.join();
        boost_t2.join();
    }

    TEST_ASSERT_TRUE(true);
}
