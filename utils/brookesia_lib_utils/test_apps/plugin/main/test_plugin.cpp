/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string>
#include <memory>
#include <future>
#include <vector>
#include <chrono>
#include <atomic>
#include "unity.h"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "test_class.hpp"
#include "test_plugin_macro_a.hpp"
#include "test_plugin_macro_a_custom.hpp"
#include "test_plugin_macro_singleton_a.hpp"
#include "test_plugin_macro_singleton_b_custom.hpp"

using namespace esp_brookesia::lib_utils;

constexpr int MACRO_PLUGIN_COUNT = 4;

// ==================== Test cases ====================

TEST_CASE("Test macro registration", "[utils][plugin_registry][macro]")
{
    BROOKESIA_LOGI("=== PluginRegistry Macro Registration Test ===");

    auto plugins = PluginRegistry<IPlugin>::get_all_instances();
    for (const auto &[name, plugin] : plugins) {
        BROOKESIA_LOGI("  - %1%: %2%", name, plugin->get_name());
    }

    // // Verify plugin count
    TEST_ASSERT_EQUAL(MACRO_PLUGIN_COUNT, PluginRegistry<IPlugin>::get_plugin_count());

    {
        // Plugin was registered at file scope (see above)
        // Verify registration success
        auto plugin = PluginRegistry<IPlugin>::get_instance(PLUGIN_MACRO_A_NAME);
        TEST_ASSERT_NOT_NULL(plugin);
        TEST_ASSERT_EQUAL(MACRO_A_VALUE, plugin->get_value());
    }
    {
        // Plugin was registered at file scope (see above)
        // Verify registration success
        auto plugin = PluginRegistry<IPlugin>::get_instance(PLUGIN_MACRO_A_CUSTOM_NAME);
        TEST_ASSERT_NOT_NULL(plugin);
        TEST_ASSERT_EQUAL(MACRO_A_CUSTOM_VALUE, plugin->get_value());
    }

    // Clean up - release instance but keep registration (file scope registered)
    PluginRegistry<IPlugin>::release_instance(PLUGIN_MACRO_A_NAME);
    PluginRegistry<IPlugin>::release_instance(PLUGIN_MACRO_A_CUSTOM_NAME);
}

TEST_CASE("Test singleton macro registration", "[utils][plugin_registry][macro][singleton]")
{
    BROOKESIA_LOGI("=== PluginRegistry Singleton Macro Registration Test ===");

    {
        // Singleton plugin was registered at file scope (see above)
        // Verify registration success
        auto plugin = PluginRegistry<IPlugin>::get_instance(PLUGIN_MACRO_SINGLETON_A_NAME);
        TEST_ASSERT_NOT_NULL(plugin);
        TEST_ASSERT_EQUAL_STRING("PluginSingletonA", plugin->get_name().c_str());
        TEST_ASSERT_EQUAL(MACRO_SINGLETON_A_DEFAULT_VALUE, plugin->get_value());

        // Verify it's the same instance as the singleton
        auto &singleton_ref = PluginSingletonA::get_instance();
        TEST_ASSERT_EQUAL(plugin.get(), &singleton_ref);

        // Get again, verify same cached instance
        auto plugin_again = PluginRegistry<IPlugin>::get_instance(PLUGIN_MACRO_SINGLETON_A_NAME);
        TEST_ASSERT_NOT_NULL(plugin_again);
        TEST_ASSERT_EQUAL(plugin.get(), plugin_again.get());
    }
    {
        // Singleton plugin was registered at file scope (see above)
        // Verify registration success
        auto plugin = PluginRegistry<IPlugin>::get_instance(PLUGIN_MACRO_SINGLETON_B_CUSTOM_NAME);
        TEST_ASSERT_NOT_NULL(plugin);
        TEST_ASSERT_EQUAL_STRING("PluginSingletonB", plugin->get_name().c_str());
        TEST_ASSERT_EQUAL(MACRO_SINGLETON_B_DEFAULT_VALUE, plugin->get_value());

        // Verify it's the same instance as the singleton
        auto &singleton_ref = PluginSingletonB::get_instance();
        TEST_ASSERT_EQUAL(plugin.get(), &singleton_ref);

        // Get again, verify same cached instance
        auto plugin_again = PluginRegistry<IPlugin>::get_instance(PLUGIN_MACRO_SINGLETON_B_CUSTOM_NAME);
        TEST_ASSERT_NOT_NULL(plugin_again);
        TEST_ASSERT_EQUAL(plugin.get(), plugin_again.get());
    }

    // Clean up - release instance but keep registration (file scope registered)
    PluginRegistry<IPlugin>::release_instance(PLUGIN_MACRO_SINGLETON_A_NAME);
    PluginRegistry<IPlugin>::release_instance(PLUGIN_MACRO_SINGLETON_B_CUSTOM_NAME);
}

TEST_CASE("Test basic registration and retrieval", "[utils][plugin_registry][basic]")
{
    BROOKESIA_LOGI("=== PluginRegistry Basic Registration Test ===");

    // Register plugins
    PluginRegistry<IPlugin>::register_plugin<PluginA>("plugin_a", []() {
        return std::make_unique<PluginA>();
    });

    // Get by name
    auto plugin = PluginRegistry<IPlugin>::get_instance("plugin_a");
    TEST_ASSERT_NOT_NULL(plugin);
    TEST_ASSERT_EQUAL_STRING("PluginA", plugin->get_name().c_str());
    TEST_ASSERT_EQUAL(PLUGIN_A_DEFAULT_VALUE, plugin->get_value());

    // Get again, verify same cached instance (singleton pattern)
    auto plugin_again = PluginRegistry<IPlugin>::get_instance("plugin_a");
    TEST_ASSERT_NOT_NULL(plugin_again);
    TEST_ASSERT_EQUAL(plugin.get(), plugin_again.get());  // Compare underlying pointers

    // Clean up - remove only the plugin used in this test
    PluginRegistry<IPlugin>::remove_plugin("plugin_a");
}

TEST_CASE("Test multiple plugins", "[utils][plugin_registry][multiple]")
{
    BROOKESIA_LOGI("=== PluginRegistry Multiple Plugins Test ===");

    // Register multiple plugins
    PluginRegistry<IPlugin>::register_plugin<PluginA>("plugin_a", []() {
        return std::make_unique<PluginA>();
    });

    PluginRegistry<IPlugin>::register_plugin<PluginB>("plugin_b", []() {
        return std::make_unique<PluginB>();
    });

    // Get and verify
    auto plugin_a = PluginRegistry<IPlugin>::get_instance("plugin_a");
    auto plugin_b = PluginRegistry<IPlugin>::get_instance("plugin_b");

    TEST_ASSERT_NOT_NULL(plugin_a);
    TEST_ASSERT_NOT_NULL(plugin_b);
    TEST_ASSERT_EQUAL(PLUGIN_A_DEFAULT_VALUE, plugin_a->get_value());
    TEST_ASSERT_EQUAL(PLUGIN_B_DEFAULT_VALUE, plugin_b->get_value());

    // Verify plugin count, including the two plugins registered at file scope
    TEST_ASSERT_EQUAL(MACRO_PLUGIN_COUNT + 2, PluginRegistry<IPlugin>::get_plugin_count());

    // Clean up - remove only the plugins used in this test
    PluginRegistry<IPlugin>::remove_plugin("plugin_a");
    PluginRegistry<IPlugin>::remove_plugin("plugin_b");
}

TEST_CASE("Test with constructor arguments", "[utils][plugin_registry][constructor]")
{
    BROOKESIA_LOGI("=== PluginRegistry Constructor Arguments Test ===");

    // Register plugins with constructor arguments
    PluginRegistry<IPlugin>::register_plugin<PluginA>("plugin_a_custom", []() {
        return std::make_unique<PluginA>(999);
    });

    PluginRegistry<IPlugin>::register_plugin<PluginC>("plugin_c", []() {
        return std::make_unique<PluginC>("CustomC", 777);
    });

    // Verify
    auto plugin_a = PluginRegistry<IPlugin>::get_instance("plugin_a_custom");
    auto plugin_c = PluginRegistry<IPlugin>::get_instance("plugin_c");

    TEST_ASSERT_EQUAL(999, plugin_a->get_value());
    TEST_ASSERT_EQUAL_STRING("CustomC", plugin_c->get_name().c_str());
    TEST_ASSERT_EQUAL(777, plugin_c->get_value());

    // Clean up - remove only the plugins used in this test
    PluginRegistry<IPlugin>::remove_plugin("plugin_a_custom");
    PluginRegistry<IPlugin>::remove_plugin("plugin_c");
}

TEST_CASE("Test factory function execution", "[utils][plugin_registry][factory]")
{
    BROOKESIA_LOGI("=== PluginRegistry Factory Function Execution Test ===");

    // Register plugins (factory function)
    PluginRegistry<IPlugin>::register_plugin<PluginA>("factory_a", []() {
        BROOKESIA_LOGI("Factory function called - creating instance NOW!");
        return std::make_unique<PluginA>(999);
    });

    BROOKESIA_LOGI("Plugin registered, factory function not called yet");

    // First get_instance() call, call factory function
    BROOKESIA_LOGI("First get_instance() call:");
    auto plugin1 = PluginRegistry<IPlugin>::get_instance("factory_a");
    TEST_ASSERT_NOT_NULL(plugin1);
    TEST_ASSERT_EQUAL(999, plugin1->get_value());

    // Second get_instance() call, return cached instance (singleton pattern)
    BROOKESIA_LOGI("Second get_instance() call:");
    auto plugin2 = PluginRegistry<IPlugin>::get_instance("factory_a");
    TEST_ASSERT_NOT_NULL(plugin2);
    TEST_ASSERT_EQUAL(999, plugin2->get_value());
    TEST_ASSERT_EQUAL(plugin1.get(), plugin2.get());  // Same cached instance - compare underlying pointers

    // Clean up - remove only the plugin used in this test
    PluginRegistry<IPlugin>::remove_plugin("factory_a");
}

TEST_CASE("Test thread safety", "[utils][plugin_registry][thread_safety]")
{
    BROOKESIA_LOGI("=== PluginRegistry Thread Safety Test ===");

    // Register multiple plugins
    PluginRegistry<IPlugin>::register_plugin<PluginA>("plugin_a", []() {
        return std::make_unique<PluginA>(100);
    });

    PluginRegistry<IPlugin>::register_plugin<PluginB>("plugin_b", []() {
        return std::make_unique<PluginB>(200);
    });

    PluginRegistry<IPlugin>::register_plugin<PluginC>("plugin_c", []() {
        return std::make_unique<PluginC>("C", 300);
    });

    // Use atomic variable to count successful operations
    std::atomic<int> success_count{0};
    std::atomic<int> error_count{0};

    // Create multiple asynchronous tasks for concurrent testing
    const int num_threads = 10;
    const int operations_per_thread = 10;
    std::vector<std::future<void>> futures;

    BROOKESIA_LOGI("Starting %1% threads, each performing %2% operations", num_threads, operations_per_thread);

    for (int t = 0; t < num_threads; t++) {
        ThreadConfigGuard config_guard({
            .stack_size = 5 * 1024,
        });
        futures.push_back(std::async(std::launch::async, [ &, t]() {
            for (int i = 0; i < operations_per_thread; i++) {
                try {
                    // Test 1: Concurrent get by name
                    auto plugin_a = PluginRegistry<IPlugin>::get_instance("plugin_a");
                    if (plugin_a && plugin_a->get_value() == 100) {
                        success_count++;
                    } else {
                        error_count++;
                    }

                    // Test 2: Concurrent get by name
                    auto plugin_b = PluginRegistry<IPlugin>::get_instance("plugin_b");
                    if (plugin_b && plugin_b->get_value() == 200) {
                        success_count++;
                    } else {
                        error_count++;
                    }

                    // Test 3: Concurrent query plugin count, including the two plugins registered at file scope
                    if (PluginRegistry<IPlugin>::get_plugin_count() == (3 + MACRO_PLUGIN_COUNT)) {
                        success_count++;
                    } else {
                        error_count++;
                    }

                    // Short sleep, increase concurrent conflict probability
                    std::this_thread::sleep_for(std::chrono::microseconds(10));

                } catch (const std::exception &e) {
                    BROOKESIA_LOGE("Thread %1% exception: %2%", t, e.what());
                    error_count++;
                }
            }
        }));
    }

    // Wait for all threads to complete
    BROOKESIA_LOGI("Waiting for all threads to complete...");
    for (auto &future : futures) {
        future.wait();
    }

    // Verify results
    int expected_success = num_threads * operations_per_thread * 3;  // Each loop 3 operations
    BROOKESIA_LOGI("Thread safety test completed:");
    BROOKESIA_LOGI("  Expected operations: %1%", expected_success);
    BROOKESIA_LOGI("  Successful operations: %1%", success_count.load());
    BROOKESIA_LOGI("  Failed operations: %1%", error_count.load());

    // Assert: all operations should succeed
    TEST_ASSERT_EQUAL(expected_success, success_count.load());
    TEST_ASSERT_EQUAL(0, error_count.load());

    // Verify plugin state is not corrupted, including the two plugins registered at file scope
    TEST_ASSERT_EQUAL((3 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());
    auto plugin_a = PluginRegistry<IPlugin>::get_instance("plugin_a");
    TEST_ASSERT_NOT_NULL(plugin_a);
    TEST_ASSERT_EQUAL(100, plugin_a->get_value());

    // Clean up - remove only the plugins used in this test
    PluginRegistry<IPlugin>::remove_plugin("plugin_a");
    PluginRegistry<IPlugin>::remove_plugin("plugin_b");
    PluginRegistry<IPlugin>::remove_plugin("plugin_c");
}

TEST_CASE("Test concurrent registration and removal", "[utils][plugin_registry][concurrent_ops]")
{
    BROOKESIA_LOGI("=== PluginRegistry Concurrent Registration and Removal Test ===");

    std::atomic<int> registration_count{0};
    std::atomic<int> removal_count{0};
    std::atomic<int> query_count{0};

    const int num_threads = 8;
    const int operations_per_thread = 50;
    std::vector<std::future<void>> futures;

    BROOKESIA_LOGI("Starting %1% threads for concurrent operations", num_threads);

    // Thread 1-3: Concurrent registration
    for (int t = 0; t < 3; t++) {
        ThreadConfigGuard config_guard({
            .stack_size = 5 * 1024,
        });
        futures.push_back(std::async(std::launch::async, [ &, t]() {
            for (int i = 0; i < operations_per_thread; i++) {
                std::string name = "plugin_" + std::to_string(t) + "_" + std::to_string(i);
                PluginRegistry<IPlugin>::register_plugin<PluginA>(name, []() {
                    return std::make_unique<PluginA>(999);
                });
                registration_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        }));
    }

    // Thread 4-5: Concurrent query
    for (int t = 0; t < 2; t++) {
        ThreadConfigGuard config_guard({
            .stack_size = 5 * 1024,
        });
        futures.push_back(std::async(std::launch::async, [&]() {
            for (int i = 0; i < operations_per_thread * 2; i++) {
                // Query plugin count
                (void)PluginRegistry<IPlugin>::get_plugin_count();
                query_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(5));
            }
        }));
    }

    // Thread 6-7: Concurrent deletion (later started)
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    for (int t = 0; t < 2; t++) {
        ThreadConfigGuard config_guard({
            .stack_size = 5 * 1024,
        });
        futures.push_back(std::async(std::launch::async, [ &, t]() {
            for (int i = 0; i < operations_per_thread / 2; i++) {
                std::string name = "plugin_" + std::to_string(t) + "_" + std::to_string(i);
                PluginRegistry<IPlugin>::remove_plugin(name);
                removal_count++;
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            }
        }));
    }

    // Thread 8: Concurrent get instance
    {
        ThreadConfigGuard config_guard({
            .stack_size = 5 * 1024,
        });
        futures.push_back(std::async(std::launch::async, [&]() {
            for (int i = 0; i < operations_per_thread; i++) {
                // Concurrent get different plugins
                auto p = PluginRegistry<IPlugin>::get_instance("plugin_0_0");
                (void)p; // Use p to avoid unused warning
                std::this_thread::sleep_for(std::chrono::microseconds(20));
            }
        }));
    }

    // Wait for all threads to complete
    BROOKESIA_LOGI("Waiting for all concurrent operations to complete...");
    for (auto &future : futures) {
        future.wait();
    }

    // Verify results
    BROOKESIA_LOGI("Concurrent operations completed:");
    BROOKESIA_LOGI("  Registrations: %1%", registration_count.load());
    BROOKESIA_LOGI("  Removals: %1%", removal_count.load());
    BROOKESIA_LOGI("  Queries: %1%", query_count.load());
    BROOKESIA_LOGI("  Final plugin count: %1%", PluginRegistry<IPlugin>::get_plugin_count());

    // Verify no crashes, data consistency
    auto final_count = PluginRegistry<IPlugin>::get_plugin_count();
    BROOKESIA_LOGI("  Final count: %1%", final_count);

    // Basic consistency check
    TEST_ASSERT_TRUE(registration_count.load() > 0);
    TEST_ASSERT_TRUE(query_count.load() > 0);
    // final_count is size_t, always >= 0, only verify reasonable range
    TEST_ASSERT_TRUE(final_count <= 150);  // Maximum 150 plugins registered

    // Clean up - remove all dynamically registered plugins
    // Remove plugins registered by threads 0-2 (plugin_0_*, plugin_1_*, plugin_2_*)
    for (int t = 0; t < 3; t++) {
        for (int i = 0; i < operations_per_thread; i++) {
            std::string name = "plugin_" + std::to_string(t) + "_" + std::to_string(i);
            PluginRegistry<IPlugin>::remove_plugin(name);
        }
    }
}

TEST_CASE("Test edge cases", "[utils][plugin_registry][edge_cases]")
{
    BROOKESIA_LOGI("=== PluginRegistry Edge Cases Test ===");

    // Get non-existent plugin
    auto non_existent = PluginRegistry<IPlugin>::get_instance("non_existent");
    TEST_ASSERT_NULL(non_existent);

    // Remove non-existent plugin
    PluginRegistry<IPlugin>::remove_plugin("non_existent");

    // Empty plugin count, including the two plugins registered at file scope
    TEST_ASSERT_EQUAL((0 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());

    // Clean up - no plugins were registered in this test
}

TEST_CASE("Test complex scenario", "[utils][plugin_registry][complex]")
{
    BROOKESIA_LOGI("=== PluginRegistry Complex Scenario Test ===");

    // Scenario: register multiple plugins, perform various operations

    // 1. Register multiple plugins
    PluginRegistry<IPlugin>::register_plugin<PluginA>("service_a", []() {
        return std::make_unique<PluginA>(100);
    });

    PluginRegistry<IPlugin>::register_plugin<PluginB>("service_b", []() {
        return std::make_unique<PluginB>(200);
    });

    PluginRegistry<IPlugin>::register_plugin<PluginA>("service_a_backup", []() {
        return std::make_unique<PluginA>(101);
    });

    // 2. Get and use plugins
    auto service_a = PluginRegistry<IPlugin>::get_instance("service_a");
    TEST_ASSERT_EQUAL(100, service_a->get_value());

    // 3. Verify plugin count
    TEST_ASSERT_EQUAL((3 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());

    // 4. Get plugin again (will return cached instance)
    auto service_a2 = PluginRegistry<IPlugin>::get_instance("service_a");
    TEST_ASSERT_EQUAL(service_a.get(), service_a2.get());  // Same cached instance - compare underlying pointers

    // 5. Remove plugin
    PluginRegistry<IPlugin>::remove_plugin("service_b");

    // 6. Verify final state
    TEST_ASSERT_EQUAL((2 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());

    // 7. Get all instances
    auto all_instances = PluginRegistry<IPlugin>::get_all_instances();
    TEST_ASSERT_EQUAL((2 + MACRO_PLUGIN_COUNT), all_instances.size());
    BROOKESIA_LOGI("All instances:");
    for (const auto &[name, instance] : all_instances) {
        BROOKESIA_LOGI("  - %1%: %2%", name, instance->get_name());
    }

    BROOKESIA_LOGI("Complex scenario test completed successfully");

    // Clean up - remove only the plugins used in this test
    // Note: service_b was already removed above
    PluginRegistry<IPlugin>::remove_plugin("service_a");
    PluginRegistry<IPlugin>::remove_plugin("service_a_backup");
}

TEST_CASE("Test remove operations", "[utils][plugin_registry][remove]")
{
    BROOKESIA_LOGI("=== PluginRegistry Remove Operations Test ===");

    // Register plugins
    PluginRegistry<IPlugin>::register_plugin<PluginA>("plugin_a", []() {
        return std::make_unique<PluginA>();
    });

    PluginRegistry<IPlugin>::register_plugin<PluginB>("plugin_b", []() {
        return std::make_unique<PluginB>();
    });

    // Get all instances
    auto all_instances = PluginRegistry<IPlugin>::get_all_instances();
    TEST_ASSERT_EQUAL((2 + MACRO_PLUGIN_COUNT), all_instances.size());
    BROOKESIA_LOGI("All instances:");
    for (const auto &[name, instance] : all_instances) {
        BROOKESIA_LOGI("  - %1%: %2%", name, instance->get_name());
    }

    TEST_ASSERT_EQUAL((2 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());

    // Remove single plugin
    PluginRegistry<IPlugin>::remove_plugin("plugin_a");
    TEST_ASSERT_EQUAL((1 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());

    // Try to remove non-existent plugin (no error, but not affect count)
    PluginRegistry<IPlugin>::remove_plugin("non_existent");
    TEST_ASSERT_EQUAL((1 + MACRO_PLUGIN_COUNT), PluginRegistry<IPlugin>::get_plugin_count());

    // Clear all
    PluginRegistry<IPlugin>::remove_all_plugins();
    TEST_ASSERT_EQUAL(0, PluginRegistry<IPlugin>::get_plugin_count());
}
