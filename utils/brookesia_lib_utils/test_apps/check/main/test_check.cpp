/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "brookesia/lib_utils/check.hpp"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "esp_err.h"
#include <boost/thread.hpp>
#include <string>
#include <cstring>
#include <functional>
#include <atomic>
#include <stdexcept>

using namespace esp_brookesia::lib_utils;

// ==================== Helper functions ====================

static int test_return_value = 0;
static bool test_goto_executed = false;

static int test_null_return_func(void *ptr)
{
    BROOKESIA_CHECK_NULL_RETURN(ptr, -1, "Pointer is NULL");
    return 0;
}

static void test_null_exit_func(void *ptr)
{
    BROOKESIA_CHECK_NULL_EXIT(ptr, "Pointer is NULL");
    test_return_value = 1;
}

static int test_null_goto_func(void *ptr)
{
    test_goto_executed = false;
    BROOKESIA_CHECK_NULL_GOTO(ptr, error_handler, "Pointer is NULL");
    return 0;

error_handler:
    test_goto_executed = true;
    return -1;
}

static int test_false_return_func(bool condition)
{
    BROOKESIA_CHECK_FALSE_RETURN(condition, -1, "Condition is false");
    return 0;
}

static void test_false_exit_func(bool condition)
{
    BROOKESIA_CHECK_FALSE_EXIT(condition, "Condition is false");
    test_return_value = 1;
}

static int test_false_goto_func(bool condition)
{
    test_goto_executed = false;
    BROOKESIA_CHECK_FALSE_GOTO(condition, error_handler, "Condition is false");
    return 0;

error_handler:
    test_goto_executed = true;
    return -1;
}

static int test_error_return_func(esp_err_t err)
{
    BROOKESIA_CHECK_ESP_ERR_RETURN(err, -1, "ESP error occurred");
    return 0;
}

static void test_error_exit_func(esp_err_t err)
{
    BROOKESIA_CHECK_ESP_ERR_EXIT(err, "ESP error occurred");
    test_return_value = 1;
}

static int test_error_goto_func(esp_err_t err)
{
    test_goto_executed = false;
    BROOKESIA_CHECK_ESP_ERR_GOTO(err, error_handler, "ESP error occurred");
    return 0;

error_handler:
    test_goto_executed = true;
    return -1;
}

static int test_exception_return_func(bool throw_exception)
{
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        if (throw_exception) throw std::runtime_error("Test exception"),
        -1,
        "Exception occurred"
    );
    return 0;
}

static void test_exception_exit_func(bool throw_exception)
{
    BROOKESIA_CHECK_EXCEPTION_EXIT(
        if (throw_exception) throw std::runtime_error("Test exception"),
        "Exception occurred"
    );
    test_return_value = 1;
}

static int test_exception_goto_func(bool throw_exception)
{
    test_goto_executed = false;
    BROOKESIA_CHECK_EXCEPTION_GOTO(
        if (throw_exception) throw std::runtime_error("Test exception"),
        error_handler,
        "Exception occurred"
    );
    return 0;

error_handler:
    test_goto_executed = true;
    return -1;
}

static int test_value_return_func(int value, int min, int max)
{
    BROOKESIA_CHECK_OUT_RANGE_RETURN(value, min, max, -1, "Value out of range");
    return 0;
}

static void test_value_exit_func(int value, int min, int max)
{
    BROOKESIA_CHECK_OUT_RANGE_EXIT(value, min, max, "Value out of range");
    test_return_value = 1;
}

static int test_value_goto_func(int value, int min, int max)
{
    test_goto_executed = false;
    BROOKESIA_CHECK_OUT_RANGE_GOTO(value, min, max, error_handler, "Value out of range");
    return 0;

error_handler:
    test_goto_executed = true;
    return -1;
}

// ==================== Test cases: NULL check ====================

TEST_CASE("Test CHECK_NULL_RETURN with valid pointer", "[utils][check_value][null_return]")
{
    BROOKESIA_LOGI("=== CHECK_NULL_RETURN Valid Pointer Test ===");

    int value = 42;
    int result = test_null_return_func(&value);
    TEST_ASSERT_EQUAL(0, result);
}

TEST_CASE("Test CHECK_NULL_RETURN with NULL pointer", "[utils][check_value][null_return]")
{
    BROOKESIA_LOGI("=== CHECK_NULL_RETURN NULL Pointer Test ===");

    int result = test_null_return_func(NULL);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_NULL_EXIT with valid pointer", "[utils][check_value][null_exit]")
{
    BROOKESIA_LOGI("=== CHECK_NULL_EXIT Valid Pointer Test ===");

    test_return_value = 0;
    int value = 42;
    test_null_exit_func(&value);
    TEST_ASSERT_EQUAL(1, test_return_value);
}

TEST_CASE("Test CHECK_NULL_EXIT with NULL pointer", "[utils][check_value][null_exit]")
{
    BROOKESIA_LOGI("=== CHECK_NULL_EXIT NULL Pointer Test ===");

    test_return_value = 0;
    test_null_exit_func(NULL);
    TEST_ASSERT_EQUAL(0, test_return_value);
}

TEST_CASE("Test CHECK_NULL_GOTO with valid pointer", "[utils][check_value][null_goto]")
{
    BROOKESIA_LOGI("=== CHECK_NULL_GOTO Valid Pointer Test ===");

    int value = 42;
    int result = test_null_goto_func(&value);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_FALSE(test_goto_executed);
}

TEST_CASE("Test CHECK_NULL_GOTO with NULL pointer", "[utils][check_value][null_goto]")
{
    BROOKESIA_LOGI("=== CHECK_NULL_GOTO NULL Pointer Test ===");

    int result = test_null_goto_func(NULL);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_TRUE(test_goto_executed);
}

// ==================== Test cases: FALSE check ====================

TEST_CASE("Test CHECK_FALSE_RETURN with true condition", "[utils][check_value][false_return]")
{
    BROOKESIA_LOGI("=== CHECK_FALSE_RETURN True Condition Test ===");

    int result = test_false_return_func(true);
    TEST_ASSERT_EQUAL(0, result);
}

TEST_CASE("Test CHECK_FALSE_RETURN with false condition", "[utils][check_value][false_return]")
{
    BROOKESIA_LOGI("=== CHECK_FALSE_RETURN False Condition Test ===");

    int result = test_false_return_func(false);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_FALSE_EXIT with true condition", "[utils][check_value][false_exit]")
{
    BROOKESIA_LOGI("=== CHECK_FALSE_EXIT True Condition Test ===");

    test_return_value = 0;
    test_false_exit_func(true);
    TEST_ASSERT_EQUAL(1, test_return_value);
}

TEST_CASE("Test CHECK_FALSE_EXIT with false condition", "[utils][check_value][false_exit]")
{
    BROOKESIA_LOGI("=== CHECK_FALSE_EXIT False Condition Test ===");

    test_return_value = 0;
    test_false_exit_func(false);
    TEST_ASSERT_EQUAL(0, test_return_value);
}

TEST_CASE("Test CHECK_FALSE_GOTO with true condition", "[utils][check_value][false_goto]")
{
    BROOKESIA_LOGI("=== CHECK_FALSE_GOTO True Condition Test ===");

    int result = test_false_goto_func(true);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_FALSE(test_goto_executed);
}

TEST_CASE("Test CHECK_FALSE_GOTO with false condition", "[utils][check_value][false_goto]")
{
    BROOKESIA_LOGI("=== CHECK_FALSE_GOTO False Condition Test ===");

    int result = test_false_goto_func(false);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_TRUE(test_goto_executed);
}

// ==================== Test cases: ESP_ERROR check ====================

TEST_CASE("Test CHECK_ERROR_RETURN with ESP_OK", "[utils][check_value][error_return]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_RETURN ESP_OK Test ===");

    int result = test_error_return_func(ESP_OK);
    TEST_ASSERT_EQUAL(0, result);
}

TEST_CASE("Test CHECK_ERROR_RETURN with ESP_FAIL", "[utils][check_value][error_return]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_RETURN ESP_FAIL Test ===");

    int result = test_error_return_func(ESP_FAIL);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_ERROR_RETURN with ESP_ERR_NO_MEM", "[utils][check_value][error_return]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_RETURN ESP_ERR_NO_MEM Test ===");

    int result = test_error_return_func(ESP_ERR_NO_MEM);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_ERROR_EXIT with ESP_OK", "[utils][check_value][error_exit]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_EXIT ESP_OK Test ===");

    test_return_value = 0;
    test_error_exit_func(ESP_OK);
    TEST_ASSERT_EQUAL(1, test_return_value);
}

TEST_CASE("Test CHECK_ERROR_EXIT with ESP_FAIL", "[utils][check_value][error_exit]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_EXIT ESP_FAIL Test ===");

    test_return_value = 0;
    test_error_exit_func(ESP_FAIL);
    TEST_ASSERT_EQUAL(0, test_return_value);
}

TEST_CASE("Test CHECK_ERROR_GOTO with ESP_OK", "[utils][check_value][error_goto]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_GOTO ESP_OK Test ===");

    int result = test_error_goto_func(ESP_OK);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_FALSE(test_goto_executed);
}

TEST_CASE("Test CHECK_ERROR_GOTO with ESP_FAIL", "[utils][check_value][error_goto]")
{
    BROOKESIA_LOGI("=== CHECK_ERROR_GOTO ESP_FAIL Test ===");

    int result = test_error_goto_func(ESP_FAIL);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_TRUE(test_goto_executed);
}

// ==================== Test cases: EXCEPTION check ====================

TEST_CASE("Test CHECK_EXCEPTION_RETURN without exception", "[utils][check_value][exception_return]")
{
    BROOKESIA_LOGI("=== CHECK_EXCEPTION_RETURN No Exception Test ===");

    int result = test_exception_return_func(false);
    TEST_ASSERT_EQUAL(0, result);
}

TEST_CASE("Test CHECK_EXCEPTION_RETURN with exception", "[utils][check_value][exception_return]")
{
    BROOKESIA_LOGI("=== CHECK_EXCEPTION_RETURN With Exception Test ===");

    int result = test_exception_return_func(true);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_EXCEPTION_EXIT without exception", "[utils][check_value][exception_exit]")
{
    BROOKESIA_LOGI("=== CHECK_EXCEPTION_EXIT No Exception Test ===");

    test_return_value = 0;
    test_exception_exit_func(false);
    TEST_ASSERT_EQUAL(1, test_return_value);
}

TEST_CASE("Test CHECK_EXCEPTION_EXIT with exception", "[utils][check_value][exception_exit]")
{
    BROOKESIA_LOGI("=== CHECK_EXCEPTION_EXIT With Exception Test ===");

    test_return_value = 0;
    test_exception_exit_func(true);
    TEST_ASSERT_EQUAL(0, test_return_value);
}

TEST_CASE("Test CHECK_EXCEPTION_GOTO without exception", "[utils][check_value][exception_goto]")
{
    BROOKESIA_LOGI("=== CHECK_EXCEPTION_GOTO No Exception Test ===");

    int result = test_exception_goto_func(false);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_FALSE(test_goto_executed);
}

TEST_CASE("Test CHECK_EXCEPTION_GOTO with exception", "[utils][check_value][exception_goto]")
{
    BROOKESIA_LOGI("=== CHECK_EXCEPTION_GOTO With Exception Test ===");

    int result = test_exception_goto_func(true);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_TRUE(test_goto_executed);
}

// ==================== Test cases: VALUE range check ====================

TEST_CASE("Test CHECK_VALUE with value in range", "[utils][check_value][value]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE In Range Test ===");

    int value = 50;
    BROOKESIA_CHECK_OUT_RANGE(value, 0, 100, "Value out of range");
    // If there is no crash or log error, the test passes
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test CHECK_VALUE with value below range", "[utils][check_value][value]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE Below Range Test ===");

    int value = -10;
    BROOKESIA_CHECK_OUT_RANGE(value, 0, 100, "Value out of range");
    // Should output error log but not interrupt execution
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test CHECK_VALUE with value above range", "[utils][check_value][value]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE Above Range Test ===");

    int value = 150;
    BROOKESIA_CHECK_OUT_RANGE(value, 0, 100, "Value out of range");
    // Should output error log but not interrupt execution
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test CHECK_VALUE_RETURN with value in range", "[utils][check_value][value_return]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_RETURN In Range Test ===");

    int result = test_value_return_func(50, 0, 100);
    TEST_ASSERT_EQUAL(0, result);
}

TEST_CASE("Test CHECK_VALUE_RETURN with value below range", "[utils][check_value][value_return]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_RETURN Below Range Test ===");

    int result = test_value_return_func(-10, 0, 100);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_VALUE_RETURN with value above range", "[utils][check_value][value_return]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_RETURN Above Range Test ===");

    int result = test_value_return_func(150, 0, 100);
    TEST_ASSERT_EQUAL(-1, result);
}

TEST_CASE("Test CHECK_VALUE_RETURN with boundary values", "[utils][check_value][value_return]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_RETURN Boundary Test ===");

    // Test boundary values
    TEST_ASSERT_EQUAL(0, test_value_return_func(0, 0, 100));    // min
    TEST_ASSERT_EQUAL(0, test_value_return_func(100, 0, 100));  // max
    TEST_ASSERT_EQUAL(-1, test_value_return_func(-1, 0, 100));  // min - 1
    TEST_ASSERT_EQUAL(-1, test_value_return_func(101, 0, 100)); // max + 1
}

TEST_CASE("Test CHECK_VALUE_EXIT with value in range", "[utils][check_value][value_exit]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_EXIT In Range Test ===");

    test_return_value = 0;
    test_value_exit_func(50, 0, 100);
    TEST_ASSERT_EQUAL(1, test_return_value);
}

TEST_CASE("Test CHECK_VALUE_EXIT with value out of range", "[utils][check_value][value_exit]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_EXIT Out of Range Test ===");

    test_return_value = 0;
    test_value_exit_func(150, 0, 100);
    TEST_ASSERT_EQUAL(0, test_return_value);
}

TEST_CASE("Test CHECK_VALUE_GOTO with value in range", "[utils][check_value][value_goto]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_GOTO In Range Test ===");

    int result = test_value_goto_func(50, 0, 100);
    TEST_ASSERT_EQUAL(0, result);
    TEST_ASSERT_FALSE(test_goto_executed);
}

TEST_CASE("Test CHECK_VALUE_GOTO with value out of range", "[utils][check_value][value_goto]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE_GOTO Out of Range Test ===");

    int result = test_value_goto_func(150, 0, 100);
    TEST_ASSERT_EQUAL(-1, result);
    TEST_ASSERT_TRUE(test_goto_executed);
}

// ==================== Test cases: different data types ====================

TEST_CASE("Test CHECK_VALUE with different types", "[utils][check_value][value_types]")
{
    BROOKESIA_LOGI("=== CHECK_VALUE Different Types Test ===");

    // unsigned int
    unsigned int u_value = 50;
    BROOKESIA_CHECK_OUT_RANGE(u_value, 0, 100, "Value out of range");

    // float
    float f_value = 50.5f;
    BROOKESIA_CHECK_OUT_RANGE(f_value, 0.0f, 100.0f, "Value out of range");

    // double
    double d_value = 50.5;
    BROOKESIA_CHECK_OUT_RANGE(d_value, 0.0, 100.0, "Value out of range");

    // char
    char c_value = 'M';
    BROOKESIA_CHECK_OUT_RANGE(c_value, 'A', 'Z', "Value out of range");

    TEST_ASSERT_TRUE(true);
}

// ==================== Test cases: complex scenarios ====================

TEST_CASE("Test multiple checks in sequence", "[utils][check_value][complex]")
{
    BROOKESIA_LOGI("=== Multiple Checks Test ===");

    int *ptr = new int(42);
    bool condition = true;
    int value = 50;

    BROOKESIA_CHECK_NULL_EXIT(ptr, "Pointer is NULL");
    BROOKESIA_CHECK_FALSE_EXIT(condition, "Condition is false");
    BROOKESIA_CHECK_OUT_RANGE_EXIT(value, 0, 100, "Value out of range");

    delete ptr;
    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test nested checks", "[utils][check_value][complex]")
{
    BROOKESIA_LOGI("=== Nested Checks Test ===");

    auto outer_func = [](int *ptr) -> int {
        BROOKESIA_CHECK_NULL_RETURN(ptr, -1, "Pointer is NULL");

        auto inner_func = [](int value) -> int {
            BROOKESIA_CHECK_OUT_RANGE_RETURN(value, 0, 100, -1, "Value out of range");
            return 0;
        };

        return inner_func(*ptr);
    };

    int value = 50;
    TEST_ASSERT_EQUAL(0, outer_func(&value));

    value = 150;
    TEST_ASSERT_EQUAL(-1, outer_func(&value));

    TEST_ASSERT_EQUAL(-1, outer_func(nullptr));
}

TEST_CASE("Test with string pointers", "[utils][check_value][string]")
{
    BROOKESIA_LOGI("=== String Pointer Check Test ===");

    const char *valid_str = "Hello";
    const char *null_str = nullptr;

    auto check_string = [](const char *str) -> int {
        BROOKESIA_CHECK_NULL_RETURN(str, -1, "String is NULL");
        BROOKESIA_CHECK_FALSE_RETURN(strlen(str) > 0, -1, "String is empty");
        return 0;
    };

    TEST_ASSERT_EQUAL(0, check_string(valid_str));
    TEST_ASSERT_EQUAL(-1, check_string(null_str));
    TEST_ASSERT_EQUAL(-1, check_string(""));
}

TEST_CASE("Test with array bounds checking", "[utils][check_value][array]")
{
    BROOKESIA_LOGI("=== Array Bounds Check Test ===");

    int array[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const int array_size = 10;

    auto safe_access = [&](int index) -> int {
        BROOKESIA_CHECK_OUT_RANGE_RETURN(index, 0, array_size - 1, -1, "Index out of range");
        return array[index];
    };

    TEST_ASSERT_EQUAL(0, safe_access(0));
    TEST_ASSERT_EQUAL(5, safe_access(5));
    TEST_ASSERT_EQUAL(9, safe_access(9));
    TEST_ASSERT_EQUAL(-1, safe_access(-1));
    TEST_ASSERT_EQUAL(-1, safe_access(10));
}

// ==================== Test cases: performance testing ====================

TEST_CASE("Test performance with many checks", "[utils][check_value][performance]")
{
    BROOKESIA_LOGI("=== Performance Test ===");

    const int iterations = 1000;
    int success_count = 0;

    for (int i = 0; i < iterations; i++) {
        int value = i % 200;  // 0-199
        BROOKESIA_CHECK_OUT_RANGE(value, 0, 100, "Value out of range");

        if (value >= 0 && value <= 100) {
            success_count++;
        }
    }

    BROOKESIA_LOGI("Completed %1% iterations, %2% in range", iterations, success_count);
    TEST_ASSERT_TRUE(success_count > 0);
}

// ==================== Test cases: boundary conditions ====================

TEST_CASE("Test with extreme values", "[utils][check_value][extreme]")
{
    BROOKESIA_LOGI("=== Extreme Values Test ===");

    // INT_MAX / INT_MIN
    int max_val = 2147483647;
    int min_val = -2147483648;

    BROOKESIA_CHECK_OUT_RANGE(max_val, min_val, max_val, "Value out of range");
    BROOKESIA_CHECK_OUT_RANGE(0, min_val, max_val, "Value out of range");

    // Negative range
    int neg_value = -50;
    BROOKESIA_CHECK_OUT_RANGE(neg_value, -100, 0, "Value out of range");

    TEST_ASSERT_TRUE(true);
}

TEST_CASE("Test with same min and max", "[utils][check_value][edge]")
{
    BROOKESIA_LOGI("=== Same Min Max Test ===");

    int value = 42;
    BROOKESIA_CHECK_OUT_RANGE(value, 42, 42, "Value out of range");  // Only 42 is valid value

    auto check_exact = [](int val) -> int {
        BROOKESIA_CHECK_OUT_RANGE_RETURN(val, 42, 42, -1, "Value out of range");
        return 0;
    };

    TEST_ASSERT_EQUAL(0, check_exact(42));
    TEST_ASSERT_EQUAL(-1, check_exact(41));
    TEST_ASSERT_EQUAL(-1, check_exact(43));
}

// ==================== Test cases: multiple goto labels ====================

TEST_CASE("Test multiple goto labels", "[utils][check_value][goto]")
{
    BROOKESIA_LOGI("=== Multiple Goto Labels Test ===");

    auto complex_check = [](int *ptr, int value) -> int {
        BROOKESIA_CHECK_NULL_GOTO(ptr, error_null, "Pointer is NULL");
        BROOKESIA_CHECK_OUT_RANGE_GOTO(value, 0, 100, error_range, "Value out of range");
        BROOKESIA_CHECK_FALSE_GOTO(value % 2 == 0, error_odd, "Value is odd");
        return 0;

error_odd:
        BROOKESIA_LOGW("Value is odd: %d", value);
        return -3;

error_range:
        BROOKESIA_LOGW("Value out of range: %d", value);
        return -2;

error_null:
        BROOKESIA_LOGW("Pointer is NULL");
        return -1;
    };

    int value = 50;
    TEST_ASSERT_EQUAL(0, complex_check(&value, 50));
    TEST_ASSERT_EQUAL(-3, complex_check(&value, 51));
    TEST_ASSERT_EQUAL(-2, complex_check(&value, 150));
    TEST_ASSERT_EQUAL(-1, complex_check(nullptr, 50));
}

// ==================== Test cases: exception type testing ====================

TEST_CASE("Test different exception types", "[utils][check_value][exception]")
{
    BROOKESIA_LOGI("=== Different Exception Types Test ===");

    // std::runtime_error
    auto test_runtime = [](bool throw_it) -> int {
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            if (throw_it) throw std::runtime_error("Runtime error"),
            -1,
            "Exception occurred"
        );
        return 0;
    };
    TEST_ASSERT_EQUAL(-1, test_runtime(true));
    TEST_ASSERT_EQUAL(0, test_runtime(false));

    // std::logic_error
    auto test_logic = [](bool throw_it) -> int {
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            if (throw_it) throw std::logic_error("Logic error"),
            -2,
            "Exception occurred"
        );
        return 0;
    };
    TEST_ASSERT_EQUAL(-2, test_logic(true));
    TEST_ASSERT_EQUAL(0, test_logic(false));

    // std::invalid_argument
    auto test_invalid_arg = [](bool throw_it) -> int {
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            if (throw_it) throw std::invalid_argument("Invalid argument"),
            -3,
            "Exception occurred"
        );
        return 0;
    };
    TEST_ASSERT_EQUAL(-3, test_invalid_arg(true));
    TEST_ASSERT_EQUAL(0, test_invalid_arg(false));

    // std::out_of_range
    auto test_out_of_range = [](bool throw_it) -> int {
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            if (throw_it) throw std::out_of_range("Out of range"),
            -4,
            "Exception occurred"
        );
        return 0;
    };
    TEST_ASSERT_EQUAL(-4, test_out_of_range(true));
    TEST_ASSERT_EQUAL(0, test_out_of_range(false));
}

TEST_CASE("Test exception in complex expression", "[utils][check_value][exception]")
{
    BROOKESIA_LOGI("=== Exception in Complex Expression Test ===");

    auto risky_func = [](int value) -> int {
        if (value < 0)
        {
            throw std::runtime_error("Negative value");
        }
        if (value > 100)
        {
            throw std::out_of_range("Too large");
        }
        return value * 2;
    };

    auto safe_wrapper = [&](int value) -> int {
        int result = 0;
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            result = risky_func(value),
            -1,
            "Exception occurred"
        );
        return result;
    };

    TEST_ASSERT_EQUAL(100, safe_wrapper(50));
    TEST_ASSERT_EQUAL(-1, safe_wrapper(-10));
    TEST_ASSERT_EQUAL(-1, safe_wrapper(150));
}

// ==================== Test cases: ESP_ERR various error codes ====================

TEST_CASE("Test various ESP error codes", "[utils][check_value][esp_err]")
{
    BROOKESIA_LOGI("=== Various ESP Error Codes Test ===");

    auto test_err = [](esp_err_t err) -> int {
        BROOKESIA_CHECK_ESP_ERR_RETURN(err, static_cast<int>(err), "ESP error occurred");
        return 0;
    };

    TEST_ASSERT_EQUAL(0, test_err(ESP_OK));
    TEST_ASSERT_EQUAL(ESP_FAIL, test_err(ESP_FAIL));
    TEST_ASSERT_EQUAL(ESP_ERR_NO_MEM, test_err(ESP_ERR_NO_MEM));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, test_err(ESP_ERR_INVALID_ARG));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_STATE, test_err(ESP_ERR_INVALID_STATE));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_SIZE, test_err(ESP_ERR_INVALID_SIZE));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_FOUND, test_err(ESP_ERR_NOT_FOUND));
    TEST_ASSERT_EQUAL(ESP_ERR_NOT_SUPPORTED, test_err(ESP_ERR_NOT_SUPPORTED));
    TEST_ASSERT_EQUAL(ESP_ERR_TIMEOUT, test_err(ESP_ERR_TIMEOUT));
}

// ==================== Test cases: range check boundary conditions ====================

TEST_CASE("Test range check with negative ranges", "[utils][check_value][range]")
{
    BROOKESIA_LOGI("=== Range Check Negative Ranges Test ===");

    auto check_range = [](int value, int min, int max) -> int {
        BROOKESIA_CHECK_OUT_RANGE_RETURN(value, min, max, -1, "Value out of range");
        return 0;
    };

    // Negative range
    TEST_ASSERT_EQUAL(0, check_range(-50, -100, 0));
    TEST_ASSERT_EQUAL(0, check_range(-100, -100, 0));
    TEST_ASSERT_EQUAL(0, check_range(0, -100, 0));
    TEST_ASSERT_EQUAL(-1, check_range(-101, -100, 0));
    TEST_ASSERT_EQUAL(-1, check_range(1, -100, 0));

    // Cross zero range
    TEST_ASSERT_EQUAL(0, check_range(0, -50, 50));
    TEST_ASSERT_EQUAL(0, check_range(-25, -50, 50));
    TEST_ASSERT_EQUAL(0, check_range(25, -50, 50));
    TEST_ASSERT_EQUAL(-1, check_range(-51, -50, 50));
    TEST_ASSERT_EQUAL(-1, check_range(51, -50, 50));
}

TEST_CASE("Test range check with floating point", "[utils][check_value][range]")
{
    BROOKESIA_LOGI("=== Range Check Floating Point Test ===");

    auto check_float = [](float value, float min, float max) -> int {
        BROOKESIA_CHECK_OUT_RANGE_RETURN(value, min, max, -1, "Value out of range");
        return 0;
    };

    TEST_ASSERT_EQUAL(0, check_float(50.5f, 0.0f, 100.0f));
    TEST_ASSERT_EQUAL(0, check_float(0.0f, 0.0f, 100.0f));
    TEST_ASSERT_EQUAL(0, check_float(100.0f, 0.0f, 100.0f));
    TEST_ASSERT_EQUAL(-1, check_float(-0.1f, 0.0f, 100.0f));
    TEST_ASSERT_EQUAL(-1, check_float(100.1f, 0.0f, 100.0f));

    auto check_double = [](double value, double min, double max) -> int {
        BROOKESIA_CHECK_OUT_RANGE_RETURN(value, min, max, -1, "Value out of range");
        return 0;
    };

    TEST_ASSERT_EQUAL(0, check_double(3.14159, 0.0, 10.0));
    TEST_ASSERT_EQUAL(-1, check_double(-0.001, 0.0, 10.0));
    TEST_ASSERT_EQUAL(-1, check_double(10.001, 0.0, 10.0));
}

// ==================== Test cases: combined checks ====================

TEST_CASE("Test combined checks in real scenario", "[utils][check_value][combined]")
{
    BROOKESIA_LOGI("=== Combined Checks Real Scenario Test ===");

    struct Config {
        int *data;
        size_t size;
        bool enabled;
    };

    auto validate_config = [](const Config * cfg) -> int {
        BROOKESIA_CHECK_NULL_RETURN(cfg, -1, "Config is NULL");
        BROOKESIA_CHECK_NULL_RETURN(cfg->data, -2, "Data is NULL");
        BROOKESIA_CHECK_FALSE_RETURN(cfg->enabled, -3, "Config is disabled");
        BROOKESIA_CHECK_OUT_RANGE_RETURN(cfg->size, 1, 1000, -4, "Size out of range");
        return 0;
    };

    int data[10];
    Config valid_cfg = {data, 10, true};
    TEST_ASSERT_EQUAL(0, validate_config(&valid_cfg));

    TEST_ASSERT_EQUAL(-1, validate_config(nullptr));

    Config null_data_cfg = {nullptr, 10, true};
    TEST_ASSERT_EQUAL(-2, validate_config(&null_data_cfg));

    Config disabled_cfg = {data, 10, false};
    TEST_ASSERT_EQUAL(-3, validate_config(&disabled_cfg));

    Config invalid_size_cfg = {data, 0, true};
    TEST_ASSERT_EQUAL(-4, validate_config(&invalid_size_cfg));

    Config large_size_cfg = {data, 2000, true};
    TEST_ASSERT_EQUAL(-4, validate_config(&large_size_cfg));
}

// ==================== Test cases: recursive checks ====================

TEST_CASE("Test recursive checks", "[utils][check_value][recursive]")
{
    BROOKESIA_LOGI("=== Recursive Checks Test ===");

    std::function<int(int, int)> factorial = [&](int n, int depth) -> int {
        BROOKESIA_CHECK_OUT_RANGE_RETURN(n, 0, 20, -1, "n out of range");
        BROOKESIA_CHECK_OUT_RANGE_RETURN(depth, 0, 100, -2, "Depth out of range");

        if (n <= 1)
        {
            return 1;
        }

        int sub_result = factorial(n - 1, depth + 1);
        BROOKESIA_CHECK_FALSE_EXECUTE(sub_result >= 0, {
            return sub_result;
        });

        return n * sub_result;
    };

    TEST_ASSERT_EQUAL(1, factorial(0, 0));
    TEST_ASSERT_EQUAL(1, factorial(1, 0));
    TEST_ASSERT_EQUAL(120, factorial(5, 0));
    TEST_ASSERT_EQUAL(-1, factorial(-1, 0));
    TEST_ASSERT_EQUAL(-1, factorial(25, 0));
}

// ==================== Test cases: thread safety (basic) ====================

TEST_CASE("Test thread safety basic", "[utils][check_value][thread]")
{
    BROOKESIA_LOGI("=== Thread Safety Basic Test ===");

    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};

    auto worker = [&](int id) {
        for (int i = 0; i < 100; i++) {
            int value = (id * 100 + i) % 200;

            auto check_func = [](int val) -> int {
                BROOKESIA_CHECK_OUT_RANGE_RETURN(val, 0, 100, -1, "Value out of range");
                return 0;
            };

            if (check_func(value) == 0) {
                success_count++;
            } else {
                fail_count++;
            }
        }
    };

    // Use boost::thread
    const int num_threads = 4;
    boost::thread_group threads;

    for (int i = 0; i < num_threads; i++) {
        ThreadConfigGuard guard({
            .stack_size = 4096,
        });
        threads.create_thread([&worker, i]() {
            worker(i);
        });
    }

    // Wait for all threads to complete
    threads.join_all();

    BROOKESIA_LOGI("Success: %1%, Fail: %2%", success_count.load(), fail_count.load());
    TEST_ASSERT_EQUAL(400, success_count.load() + fail_count.load());
}

TEST_CASE("Test thread safety with exceptions", "[utils][check_value][thread]")
{
    BROOKESIA_LOGI("=== Thread Safety With Exceptions Test ===");

    std::atomic<int> exception_caught{0};
    std::atomic<int> no_exception{0};

    auto worker = [&](int id) {
        for (int i = 0; i < 50; i++) {
            bool should_throw = ((id * 50 + i) % 3 == 0);

            auto risky_func = [](bool throw_it) -> int {
                BROOKESIA_CHECK_EXCEPTION_RETURN(
                    if (throw_it) throw std::runtime_error("Test exception"),
                    -1,
                    "Exception occurred"
                );
                return 0;
            };

            if (risky_func(should_throw) == -1) {
                exception_caught++;
            } else {
                no_exception++;
            }
        }
    };

    // Use boost::thread
    const int num_threads = 4;
    boost::thread_group threads;

    for (int i = 0; i < num_threads; i++) {
        ThreadConfigGuard guard({
            .stack_size = 4096,
        });
        threads.create_thread([&worker, i]() {
            worker(i);
        });
    }

    // Wait for all threads to complete
    threads.join_all();

    BROOKESIA_LOGI("Exception caught: %1%, No exception: %2%",
                   exception_caught.load(), no_exception.load());
    TEST_ASSERT_EQUAL(200, exception_caught.load() + no_exception.load());
}

TEST_CASE("Test thread safety with multiple check types", "[utils][check_value][thread]")
{
    BROOKESIA_LOGI("=== Thread Safety Multiple Check Types Test ===");

    std::atomic<int> null_checks{0};
    std::atomic<int> false_checks{0};
    std::atomic<int> range_checks{0};
    std::atomic<int> total_passed{0};

    auto worker = [&](int id) {
        int data[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

        for (int i = 0; i < 30; i++) {
            int check_type = (id * 30 + i) % 3;

            switch (check_type) {
            case 0: {
                // NULL check
                int *ptr = (i % 2 == 0) ? &data[0] : nullptr;
                auto check_null = [](int *p) -> int {
                    BROOKESIA_CHECK_NULL_RETURN(p, -1, "Pointer is NULL");
                    return 0;
                };
                if (check_null(ptr) == 0) {
                    null_checks++;
                    total_passed++;
                }
                break;
            }
            case 1: {
                // FALSE check
                bool condition = (i % 2 == 0);
                auto check_false = [](bool cond) -> int {
                    BROOKESIA_CHECK_FALSE_RETURN(cond, -1, "Condition is false");
                    return 0;
                };
                if (check_false(condition) == 0) {
                    false_checks++;
                    total_passed++;
                }
                break;
            }
            case 2: {
                // Range check
                int value = (id * 30 + i) % 150;
                auto check_range = [](int val) -> int {
                    BROOKESIA_CHECK_OUT_RANGE_RETURN(val, 0, 100, -1, "Value out of range");
                    return 0;
                };
                if (check_range(value) == 0) {
                    range_checks++;
                    total_passed++;
                }
                break;
            }
            }
        }
    };

    // Use boost::thread
    const int num_threads = 4;
    boost::thread_group threads;

    for (int i = 0; i < num_threads; i++) {
        ThreadConfigGuard guard({
            .stack_size = 4096,
        });
        threads.create_thread([&worker, i]() {
            worker(i);
        });
    }

    // Wait for all threads to complete
    threads.join_all();

    BROOKESIA_LOGI("NULL checks: %1%, FALSE checks: %2%, Range checks: %3%, Total passed: %4%",
                   null_checks.load(), false_checks.load(),
                   range_checks.load(), total_passed.load());

    // Each thread executes 30 times, with 4 threads, a total of 120 checks
    TEST_ASSERT_TRUE(total_passed.load() > 0);
    TEST_ASSERT_TRUE(total_passed.load() <= 120);
}
