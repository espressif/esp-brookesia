/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/log.hpp"
#include <string>
#include <vector>

using namespace esp_brookesia::lib_utils;

// Global counters for testing
static int g_cleanup_counter = 0;
static std::string g_last_message;
static std::vector<int> g_call_sequence;

// Test helper functions
void simple_cleanup()
{
    g_cleanup_counter++;
    BROOKESIA_LOGI("simple_cleanup called, counter = %1%", g_cleanup_counter);
}

void cleanup_with_message(const std::string &msg)
{
    g_last_message = msg;
    BROOKESIA_LOGI("cleanup_with_message: %1%", msg);
}

void cleanup_with_multiple_args(int id, const std::string &name, bool flag)
{
    g_call_sequence.push_back(id);
    BROOKESIA_LOGI("cleanup_with_multiple_args: id=%1%, name=%2%, flag=%3%", id, name, flag ? "true" : "false");
}

int add_numbers(int a, int b)
{
    int result = a + b;
    BROOKESIA_LOGI("add_numbers: %1% + %2% = %3%", a, b, result);
    return result;
}

// Test class
class TestResource {
public:
    TestResource(int id) : id_(id), is_released_(false)
    {
        BROOKESIA_LOGI("TestResource(%1%) constructed", id_);
    }

    ~TestResource()
    {
        BROOKESIA_LOGI("TestResource(%1%) destructed, is_released=%2%", id_, is_released_ ? "true" : "false");
    }

    void release()
    {
        is_released_ = true;
        g_cleanup_counter++;
        BROOKESIA_LOGI("TestResource(%1%) released", id_);
    }

    int get_id() const
    {
        return id_;
    }

private:
    int id_;
    bool is_released_;
};

TEST_CASE("Test basic usage", "[utils][function_guard][basic]")
{
    BROOKESIA_LOGI("=== FunctionGuard Basic Usage Test ===");

    g_cleanup_counter = 0;

    {
        FunctionGuard guard(simple_cleanup);
        BROOKESIA_LOGI("Inside scope, counter = %1%", g_cleanup_counter);
        TEST_ASSERT_EQUAL(0, g_cleanup_counter);
    }

    // After leaving scope, cleanup should be called
    BROOKESIA_LOGI("Outside scope, counter = %1%", g_cleanup_counter);
    TEST_ASSERT_EQUAL(1, g_cleanup_counter);
}

TEST_CASE("Test with arguments", "[utils][function_guard][args]")
{
    BROOKESIA_LOGI("=== FunctionGuard with Arguments Test ===");

    g_last_message = "";

    {
        FunctionGuard guard(cleanup_with_message, std::string("Test message"));
        TEST_ASSERT_TRUE(g_last_message.empty());
    }

    // After leaving scope, cleanup should be called
    TEST_ASSERT_EQUAL_STRING("Test message", g_last_message.c_str());
}

TEST_CASE("Test with multiple arguments", "[utils][function_guard][multiple_args]")
{
    BROOKESIA_LOGI("=== FunctionGuard with Multiple Arguments Test ===");

    g_call_sequence.clear();

    {
        FunctionGuard guard(cleanup_with_multiple_args, 42, std::string("TestName"), true);
        TEST_ASSERT_TRUE(g_call_sequence.empty());
    }

    // Verify function was called with correct arguments
    TEST_ASSERT_EQUAL(1, g_call_sequence.size());
    TEST_ASSERT_EQUAL(42, g_call_sequence[0]);
}

TEST_CASE("Test release", "[utils][function_guard][release]")
{
    BROOKESIA_LOGI("=== FunctionGuard Release Test ===");

    g_cleanup_counter = 0;

    {
        FunctionGuard guard(simple_cleanup);
        guard.release();  // Release the guard
        BROOKESIA_LOGI("Guard released");
    }

    // After leaving scope, cleanup should NOT be called
    BROOKESIA_LOGI("Counter after scope = %1%", g_cleanup_counter);
    TEST_ASSERT_EQUAL(0, g_cleanup_counter);
}

TEST_CASE("Test with lambda", "[utils][function_guard][lambda]")
{
    BROOKESIA_LOGI("=== FunctionGuard with Lambda Test ===");

    int counter = 0;

    {
        FunctionGuard guard([&counter]() {
            counter++;
            BROOKESIA_LOGI("Lambda cleanup called, counter = %1%", counter);
        });

        TEST_ASSERT_EQUAL(0, counter);
    }

    TEST_ASSERT_EQUAL(1, counter);
}

TEST_CASE("Test with lambda and capture", "[utils][function_guard][lambda_capture]")
{
    BROOKESIA_LOGI("=== FunctionGuard with Lambda Capture Test ===");

    std::string result;

    {
        std::string prefix = "Cleanup: ";
        FunctionGuard guard([&result, prefix]() {
            result = prefix + "Done";
            BROOKESIA_LOGI("Lambda with capture: %1%", result);
        });

        TEST_ASSERT_TRUE(result.empty());
    }

    TEST_ASSERT_EQUAL_STRING("Cleanup: Done", result.c_str());
}

TEST_CASE("Test with member function", "[utils][function_guard][member_func]")
{
    BROOKESIA_LOGI("=== FunctionGuard with Member Function Test ===");

    g_cleanup_counter = 0;
    TestResource resource(100);

    {
        FunctionGuard guard([&resource]() {
            resource.release();
        });

        TEST_ASSERT_EQUAL(0, g_cleanup_counter);
    }

    TEST_ASSERT_EQUAL(1, g_cleanup_counter);
}

TEST_CASE("Test multiple instances", "[utils][function_guard][multiple]")
{
    BROOKESIA_LOGI("=== FunctionGuard Multiple Instances Test ===");

    g_call_sequence.clear();

    {
        FunctionGuard guard1([&]() {
            g_call_sequence.push_back(1);
            BROOKESIA_LOGI("Guard 1 cleanup");
        });

        FunctionGuard guard2([&]() {
            g_call_sequence.push_back(2);
            BROOKESIA_LOGI("Guard 2 cleanup");
        });

        FunctionGuard guard3([&]() {
            g_call_sequence.push_back(3);
            BROOKESIA_LOGI("Guard 3 cleanup");
        });

        TEST_ASSERT_TRUE(g_call_sequence.empty());
    }

    // Destruction order should be 3, 2, 1 (LIFO)
    TEST_ASSERT_EQUAL(3, g_call_sequence.size());
    TEST_ASSERT_EQUAL(3, g_call_sequence[0]);
    TEST_ASSERT_EQUAL(2, g_call_sequence[1]);
    TEST_ASSERT_EQUAL(1, g_call_sequence[2]);
}

TEST_CASE("Test with exception safety", "[utils][function_guard][exception]")
{
    BROOKESIA_LOGI("=== FunctionGuard Exception Safety Test ===");

    g_cleanup_counter = 0;

    try {
        FunctionGuard guard(simple_cleanup);
        BROOKESIA_LOGI("Before exception");
        // Note: Exceptions may not be supported in embedded environments
        // This just demonstrates FunctionGuard's exception safety
        // throw std::runtime_error("Test exception");
    } catch (...) {
        BROOKESIA_LOGI("Exception caught");
    }

    // Cleanup should be called even if an exception occurs
    // Since we commented out the throw, cleanup should execute normally
    TEST_ASSERT_EQUAL(1, g_cleanup_counter);
}

TEST_CASE("Test with return value function", "[utils][function_guard][return_value]")
{
    BROOKESIA_LOGI("=== FunctionGuard with Return Value Function Test ===");

    int result = 0;

    {
        FunctionGuard guard([&result]() {
            result = add_numbers(10, 20);
        });

        TEST_ASSERT_EQUAL(0, result);
    }

    TEST_ASSERT_EQUAL(30, result);
}

TEST_CASE("Test nested scopes", "[utils][function_guard][nested]")
{
    BROOKESIA_LOGI("=== FunctionGuard Nested Scopes Test ===");

    g_call_sequence.clear();

    {
        FunctionGuard outer_guard([&]() {
            g_call_sequence.push_back(1);
            BROOKESIA_LOGI("Outer guard cleanup");
        });

        {
            FunctionGuard inner_guard([&]() {
                g_call_sequence.push_back(2);
                BROOKESIA_LOGI("Inner guard cleanup");
            });

            TEST_ASSERT_TRUE(g_call_sequence.empty());
        }

        // Inner guard should have executed
        TEST_ASSERT_EQUAL(1, g_call_sequence.size());
        TEST_ASSERT_EQUAL(2, g_call_sequence[0]);
    }

    // Outer guard should also execute
    TEST_ASSERT_EQUAL(2, g_call_sequence.size());
    TEST_ASSERT_EQUAL(2, g_call_sequence[0]);
    TEST_ASSERT_EQUAL(1, g_call_sequence[1]);
}

TEST_CASE("Test conditional release", "[utils][function_guard][conditional]")
{
    BROOKESIA_LOGI("=== FunctionGuard Conditional Release Test ===");

    g_cleanup_counter = 0;

    // Test 1: Condition is true, release guard
    {
        FunctionGuard guard(simple_cleanup);
        bool success = true;

        if (success) {
            guard.release();
            BROOKESIA_LOGI("Operation succeeded, guard released");
        }
    }
    TEST_ASSERT_EQUAL(0, g_cleanup_counter);

    // Test 2: Condition is false, do not release guard
    {
        FunctionGuard guard(simple_cleanup);
        bool success = false;

        if (success) {
            guard.release();
        } else {
            BROOKESIA_LOGI("Operation failed, guard will execute cleanup");
        }
    }
    TEST_ASSERT_EQUAL(1, g_cleanup_counter);
}

TEST_CASE("Test with std::function", "[utils][function_guard][std_function]")
{
    BROOKESIA_LOGI("=== FunctionGuard with std::function Test ===");

    int counter = 0;
    std::function<void()> cleanup_func = [&counter]() {
        counter += 10;
        BROOKESIA_LOGI("std::function cleanup, counter = %1%", counter);
    };

    {
        FunctionGuard guard(cleanup_func);
        TEST_ASSERT_EQUAL(0, counter);
    }

    TEST_ASSERT_EQUAL(10, counter);
}

TEST_CASE("Test move semantics", "[utils][function_guard][move]")
{
    BROOKESIA_LOGI("=== FunctionGuard Move Semantics Test ===");

    g_call_sequence.clear();

    {
        std::vector<int> data = {1, 2, 3, 4, 5};

        FunctionGuard guard([data = std::move(data)]() mutable {
            g_call_sequence = std::move(data);
            BROOKESIA_LOGI("Moved data size: %1%", g_call_sequence.size());
        });

        TEST_ASSERT_TRUE(g_call_sequence.empty());
    }

    TEST_ASSERT_EQUAL(5, g_call_sequence.size());
    TEST_ASSERT_EQUAL(1, g_call_sequence[0]);
    TEST_ASSERT_EQUAL(5, g_call_sequence[4]);
}

TEST_CASE("Test real world file handle", "[utils][function_guard][real_world]")
{
    BROOKESIA_LOGI("=== FunctionGuard Real World - File Handle Test ===");

    bool file_closed = false;

    // Simulate file operations
    {
        // Simulate opening a file
        int file_handle = 42;
        BROOKESIA_LOGI("File opened: handle = %1%", file_handle);

        FunctionGuard file_guard([&file_closed, file_handle]() {
            // Simulate closing the file
            file_closed = true;
            BROOKESIA_LOGI("File closed: handle = %1%", file_handle);
        });

        // Perform file operations
        BROOKESIA_LOGI("Performing file operations...");
        TEST_ASSERT_FALSE(file_closed);
    }

    // File should be closed
    TEST_ASSERT_TRUE(file_closed);
}

TEST_CASE("Test real world mutex lock", "[utils][function_guard][real_world_mutex]")
{
    BROOKESIA_LOGI("=== FunctionGuard Real World - Mutex Lock Test ===");

    bool mutex_locked = false;
    bool mutex_unlocked = false;

    {
        // Simulate acquiring lock
        mutex_locked = true;
        BROOKESIA_LOGI("Mutex locked");

        FunctionGuard lock_guard([&mutex_unlocked]() {
            // Simulate releasing lock
            mutex_unlocked = true;
            BROOKESIA_LOGI("Mutex unlocked");
        });

        // Execute critical section code
        BROOKESIA_LOGI("In critical section");
        TEST_ASSERT_TRUE(mutex_locked);
        TEST_ASSERT_FALSE(mutex_unlocked);
    }

    // Lock should be released
    TEST_ASSERT_TRUE(mutex_unlocked);
}
