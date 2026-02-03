/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string>
#include <vector>
#include <cfloat>
#include <cstdint>
#include <functional>
#include "unity.h"
#include "brookesia/lib_utils/log.hpp"
#include "brookesia/lib_utils/time_profiler.hpp"

class LogTestClass {
public:
    LogTestClass() = default;

    void print()
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGI("File: %1%", __FILE__);
        BROOKESIA_LOGI("Line: %1%", __LINE__);
        BROOKESIA_LOGI("Time: %1%", __DATE__);

        auto lambda_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_LOGI("File: %1%", __FILE__);
            BROOKESIA_LOGI("Line: %1%", __LINE__);
            BROOKESIA_LOGI("Time: %1%", __DATE__);
        };
        lambda_func();
    }
};

TEST_CASE("Test basic functions", "[utils][log][basic]")
{
    BROOKESIA_LOG_TRACE_GUARD();

    LogTestClass log_test_class;
    log_test_class.print();

    auto lambda_func = []() {
        BROOKESIA_LOG_TRACE_GUARD();

        BROOKESIA_LOGT("This is a trace message");
        BROOKESIA_LOGD("This is a debug message");
        BROOKESIA_LOGI("This is an info message");
        BROOKESIA_LOGW("This is a warning message");
        BROOKESIA_LOGE("This is an error message");
    };
    lambda_func();

    BROOKESIA_LOGT("This is a trace message");
    BROOKESIA_LOGD("This is a debug message");
    BROOKESIA_LOGI("This is an info message");
    BROOKESIA_LOGW("This is a warning message");
    BROOKESIA_LOGE("This is an error message");
}

TEST_CASE("Test log level filtering", "[utils][log][level]")
{
    // Print all log levels to verify filtering
    // These messages will be filtered based on BROOKESIA_UTILS_LOG_LEVEL
    BROOKESIA_LOGT("[LOG_LEVEL_VERIFY] Trace level message");
    BROOKESIA_LOGD("[LOG_LEVEL_VERIFY] Debug level message");
    BROOKESIA_LOGI("[LOG_LEVEL_VERIFY] Info level message");
    BROOKESIA_LOGW("[LOG_LEVEL_VERIFY] Warning level message");
    BROOKESIA_LOGE("[LOG_LEVEL_VERIFY] Error level message");

    // Print current log level for verification
    printf("[LOG_LEVEL_VERIFY] Current log level: %d\n", BROOKESIA_UTILS_LOG_LEVEL);
}

#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_INFO
TEST_CASE("Test with integer types", "[utils][log][integer]")
{
    BROOKESIA_LOGI("=== Integer Types Test ===");

    // Integer types
    int8_t   i8  = -128;
    uint8_t  u8  = 255;
    int16_t  i16 = -32768;
    uint16_t u16 = 65535;
    int32_t  i32 = -2147483648;
    uint32_t u32 = 4294967295U;
    int64_t  i64 = -9223372036854775807LL;
    uint64_t u64 = 18446744073709551615ULL;

    // Print integer types
    BROOKESIA_LOGI("int8_t:   %1%", static_cast<int>(i8));
    BROOKESIA_LOGI("uint8_t:  %1%", static_cast<unsigned int>(u8));
    BROOKESIA_LOGI("int16_t:  %1%", i16);
    BROOKESIA_LOGI("uint16_t: %1%", u16);
    BROOKESIA_LOGI("int32_t:  %1%", i32);
    BROOKESIA_LOGI("uint32_t: %1%", u32);
    BROOKESIA_LOGI("int64_t:  %1%", i64);
    BROOKESIA_LOGI("uint64_t: %1%", u64);

    // Different bases
    BROOKESIA_LOGI("Hex: 0x%1$x, Dec: %1%, Oct: %1$o", 255);

    // Multiple parameters
    BROOKESIA_LOGI("Multiple: a=%1%, b=%2%, c=%3%", 10, 20, 30);

    // Use format specifier to force integer
    BROOKESIA_LOGI("int8_t with %%d:  %1$d", i8);
    BROOKESIA_LOGI("uint8_t with %%u: %1$u", u8);
}

TEST_CASE("Test with floating point types", "[utils][log][float]")
{
    BROOKESIA_LOGI("=== Floating Point Types Test ===");

    float  f32 = 3.14159f;
    double f64 = 2.718281828459045;

    BROOKESIA_LOGI("float:  %1%", f32);
    BROOKESIA_LOGI("double: %1%", f64);

    // Scientific notation
    BROOKESIA_LOGI("Scientific: %1$e", 123456.789);

    // Precision control
    BROOKESIA_LOGI("Precision: %1$.2f", 3.14159);
    BROOKESIA_LOGI("Precision: %1$.6f", 3.14159);

    // Special values
    BROOKESIA_LOGI("Zero: %1%", 0.0);
    BROOKESIA_LOGI("Negative: %1%", -123.456);
}

TEST_CASE("Test with string types", "[utils][log][string]")
{
    BROOKESIA_LOGI("=== String Types Test ===");

    // C string
    const char *c_str = "Hello, World!";
    BROOKESIA_LOGI("C string: %1%", c_str);

    // C++ string
    std::string cpp_str = "C++ String";
    BROOKESIA_LOGI("C++ string: %1%", cpp_str);

    // Character
    char ch = 'A';
    BROOKESIA_LOGI("Char: %1%", ch);

    // Empty string
    BROOKESIA_LOGI("Empty string: '%1%'", "");

    // Contains special characters
    BROOKESIA_LOGI("Special chars: %1%", "Tab:\t Newline:\n Quote:\" Backslash:\\");

    // Multiple strings
    BROOKESIA_LOGI("Multiple strings: %1% %2% %3%", "First", "Second", "Third");
}

TEST_CASE("Test with pointer types", "[utils][log][pointer]")
{
    BROOKESIA_LOGI("=== Pointer Types Test ===");

    int value = 42;
    int *ptr = &value;
    void *void_ptr = ptr;
    uint8_t *uint8_ptr = reinterpret_cast<uint8_t *>(&value);
    int8_t *int8_ptr = reinterpret_cast<int8_t *>(&value);
    const uint8_t *const_uint8_ptr = reinterpret_cast<const uint8_t *>(&value);
    const int8_t *const_int8_ptr = reinterpret_cast<const int8_t *>(&value);

    BROOKESIA_LOGI("Pointer: %1%", ptr);
    BROOKESIA_LOGI("Void pointer: %1%", void_ptr);
    BROOKESIA_LOGI("Nullptr: %1%", nullptr);
    BROOKESIA_LOGI("uint8_ptr: %1%", uint8_ptr);
    BROOKESIA_LOGI("int8_ptr: %1%", int8_ptr);
    BROOKESIA_LOGI("const_uint8_ptr: %1%", const_uint8_ptr);
    BROOKESIA_LOGI("const_int8_ptr: %1%", const_int8_ptr);

    // Value at pointer
    BROOKESIA_LOGI("Value at pointer: %1%", *ptr);

    // Object pointer
    LogTestClass obj;
    LogTestClass *obj_ptr = &obj;
    BROOKESIA_LOGI("Object pointer: %1%", static_cast<void *>(obj_ptr));
}

TEST_CASE("Test with boolean types", "[utils][log][boolean]")
{
    BROOKESIA_LOGI("=== Boolean Types Test ===");

    bool true_val = true;
    bool false_val = false;

    BROOKESIA_LOGI("Boolean true: %1%", true_val);
    BROOKESIA_LOGI("Boolean false: %1%", false_val);

    // Conditional expression
    int a = 10, b = 20;
    BROOKESIA_LOGI("Comparison (a < b): %1%", (a < b));
    BROOKESIA_LOGI("Comparison (a > b): %1%", (a > b));
}

TEST_CASE("Test with format modifiers", "[utils][log][format]")
{
    BROOKESIA_LOGI("=== Format Modifiers Test ===");

    int num = 42;

    // Width and alignment
    BROOKESIA_LOGI("Width 10, right: '%1$10d'", num);
    BROOKESIA_LOGI("Width 10, left:  '%1$-10d'", num);

    // Padding
    BROOKESIA_LOGI("Zero padding: %1$05d", num);

    // Sign
    BROOKESIA_LOGI("Always show sign: %1$+d", num);
    BROOKESIA_LOGI("Space for positive: %1$ d", num);

    // Combined
    BROOKESIA_LOGI("Combined: '%1$+010d'", num);
}

TEST_CASE("Test with positional arguments", "[utils][log][positional]")
{
    BROOKESIA_LOGI("=== Positional Arguments Test ===");

    // Positional arguments
    BROOKESIA_LOGI("Normal order: %1% %2% %3%", "first", "second", "third");
    BROOKESIA_LOGI("Reverse order: %3% %2% %1%", "first", "second", "third");
    BROOKESIA_LOGI("Repeat: %1% %2% %1% %2%", "A", "B");

    // Mixed use
    int x = 10, y = 20, z = 30;
    BROOKESIA_LOGI("x=%1%, y=%2%, z=%3%, sum=%4%", x, y, z, x + y + z);

    // Use format specifier
    BROOKESIA_LOGI("Formatted: %1$+05d, %2$+05d, %3$+05d", x, y, z);
}

TEST_CASE("Test with complex expressions", "[utils][log][complex]")
{
    BROOKESIA_LOGI("=== Complex Expressions Test ===");

    // Arithmetic expressions
    BROOKESIA_LOGI("Arithmetic: 10 + 20 = %1%", 10 + 20);
    BROOKESIA_LOGI("Arithmetic: 10 * 20 = %1%", 10 * 20);

    // Function call
    auto square = [](int x) {
        return x * x;
    };
    BROOKESIA_LOGI("Function result: square(5) = %1%", square(5));

    // Ternary operator
    int value = 42;
    BROOKESIA_LOGI("Ternary: value is %1%", (value > 0 ? "positive" : "negative"));

    // Type conversion
    BROOKESIA_LOGI("Cast: (int)3.14 = %1%", static_cast<int>(3.14));
}

TEST_CASE("Test with edge cases", "[utils][log][edge]")
{
    BROOKESIA_LOGI("=== Edge Cases Test ===");

    // No arguments
    BROOKESIA_LOGI("No arguments");

    // Too many arguments (should be ignored, no exception)
    BROOKESIA_LOGI("One placeholder: %1%", 1, 2, 3, 4, 5);

    // Too few arguments (should keep placeholders, no exception)
    BROOKESIA_LOGI("Three placeholders: %1% %2% %3%", 1);

    // Empty string format
    BROOKESIA_LOGI("");

    // Only placeholders
    BROOKESIA_LOGI("%1%", 42);

    // Escape percent sign
    BROOKESIA_LOGI("Percent sign: 100%%");

    // Very long string
    std::string long_str(200, 'X');
    BROOKESIA_LOGI("Long string: %1%", long_str);
}

TEST_CASE("Test with mixed types", "[utils][log][mixed]")
{
    BROOKESIA_LOGI("=== Mixed Types Test ===");

    const char *name = "Device";
    int id = 12345;
    float temperature = 25.6f;
    bool status = true;
    void *addr = reinterpret_cast<void *>(0xDEADBEEF);

    BROOKESIA_LOGI("Device Info:");
    BROOKESIA_LOGI("  Name: %1%", name);
    BROOKESIA_LOGI("  ID: %1%", id);
    BROOKESIA_LOGI("  Temperature: %1$.1f°C", temperature);
    BROOKESIA_LOGI("  Status: %1%", status ? "Online" : "Offline");
    BROOKESIA_LOGI("  Address: %1%", addr);

    // Output all information in one line
    BROOKESIA_LOGI("Summary: %1% (ID:%2%, Temp:%3$.1f°C, Status:%4%, Addr:%5%)",
                   name, id, temperature, status, addr);
}

TEST_CASE("Test LogTraceGuard basic", "[utils][log][trace_guard]")
{
    BROOKESIA_LOGI("=== LogTraceGuard Basic Test ===");

    {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Inside guarded scope");
    }

    BROOKESIA_LOGI("Outside guarded scope");
}

TEST_CASE("Test LogTraceGuard with this pointer", "[utils][log][trace_guard_this]")
{
    BROOKESIA_LOGI("=== LogTraceGuard with This Pointer Test ===");

    LogTestClass obj;
    obj.print();
}

TEST_CASE("Test LogTraceGuard nested", "[utils][log][trace_guard_nested]")
{
    BROOKESIA_LOGI("=== LogTraceGuard Nested Test ===");

    auto outer_func = []() {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Outer function");

        auto inner_func = []() {
            BROOKESIA_LOG_TRACE_GUARD();
            BROOKESIA_LOGI("Inner function");
        };
        inner_func();

        BROOKESIA_LOGI("Back to outer function");
    };

    outer_func();
}

TEST_CASE("Test all log levels", "[utils][log][levels]")
{
    BROOKESIA_LOGI("=== All Log Levels Test ===");

    BROOKESIA_LOGT("Trace level message");
    BROOKESIA_LOGD("Debug level message");
    BROOKESIA_LOGI("Info level message");
    BROOKESIA_LOGW("Warning level message");
    BROOKESIA_LOGE("Error level message");

    // With arguments
    int value = 42;
    BROOKESIA_LOGT("Trace with value: %1%", value);
    BROOKESIA_LOGD("Debug with value: %1%", value);
    BROOKESIA_LOGI("Info with value: %1%", value);
    BROOKESIA_LOGW("Warning with value: %1%", value);
    BROOKESIA_LOGE("Error with value: %1%", value);
}

TEST_CASE("Test with std::string", "[utils][log][std_string]")
{
    BROOKESIA_LOGI("=== std::string Test ===");

    std::string str1 = "Hello";
    std::string str2 = "World";

    BROOKESIA_LOGI("String 1: %1%", str1);
    BROOKESIA_LOGI("String 2: %1%", str2);
    BROOKESIA_LOGI("Combined: %1% %2%", str1, str2);

    // Empty string
    std::string empty;
    BROOKESIA_LOGI("Empty string: '%1%'", empty);

    // Long string
    std::string long_str(100, 'A');
    BROOKESIA_LOGI("Long string (100 chars): %1%", long_str);
}

TEST_CASE("Test with containers", "[utils][log][containers]")
{
    BROOKESIA_LOGI("=== Containers Test ===");

    std::vector<int> vec = {1, 2, 3, 4, 5};
    BROOKESIA_LOGI("Vector size: %1%", vec.size());
    BROOKESIA_LOGI("Vector first: %1%, last: %2%", vec.front(), vec.back());

    // Iterate and output
    for (size_t i = 0; i < vec.size(); i++) {
        BROOKESIA_LOGI("  vec[%1%] = %2%", i, vec[i]);
    }
}

TEST_CASE("Test with arithmetic expressions", "[utils][log][arithmetic]")
{
    BROOKESIA_LOGI("=== Arithmetic Expressions Test ===");

    int a = 10, b = 20;

    BROOKESIA_LOGI("Addition: %1% + %2% = %3%", a, b, a + b);
    BROOKESIA_LOGI("Subtraction: %1% - %2% = %3%", a, b, a - b);
    BROOKESIA_LOGI("Multiplication: %1% * %2% = %3%", a, b, a * b);
    BROOKESIA_LOGI("Division: %1% / %2% = %3%", b, a, b / a);
    BROOKESIA_LOGI("Modulo: %1% %% %2% = %3%", b, a, b % a);

    // Complex expression
    BROOKESIA_LOGI("Complex: (%1% + %2%) * %3% = %4%", a, b, 2, (a + b) * 2);
}

TEST_CASE("Test with comparison operators", "[utils][log][comparison]")
{
    BROOKESIA_LOGI("=== Comparison Operators Test ===");

    int x = 10, y = 20;

    BROOKESIA_LOGI("x=%1%, y=%2%", x, y);
    BROOKESIA_LOGI("x == y: %1%", x == y);
    BROOKESIA_LOGI("x != y: %1%", x != y);
    BROOKESIA_LOGI("x < y: %1%", x < y);
    BROOKESIA_LOGI("x > y: %1%", x > y);
    BROOKESIA_LOGI("x <= y: %1%", x <= y);
    BROOKESIA_LOGI("x >= y: %1%", x >= y);
}

TEST_CASE("Test with logical operators", "[utils][log][logical]")
{
    BROOKESIA_LOGI("=== Logical Operators Test ===");

    bool t = true, f = false;

    BROOKESIA_LOGI("true=%1%, false=%2%", t, f);
    BROOKESIA_LOGI("true && true: %1%", t && t);
    BROOKESIA_LOGI("true && false: %1%", t && f);
    BROOKESIA_LOGI("true || false: %1%", t || f);
    BROOKESIA_LOGI("false || false: %1%", f || f);
    BROOKESIA_LOGI("!true: %1%", !t);
    BROOKESIA_LOGI("!false: %1%", !f);
}

TEST_CASE("Test with bitwise operators", "[utils][log][bitwise]")
{
    BROOKESIA_LOGI("=== Bitwise Operators Test ===");

    uint8_t a = 0b10101010;
    uint8_t b = 0b11001100;

    BROOKESIA_LOGI("a = 0x%1$02x (%1$d)", static_cast<unsigned int>(a));
    BROOKESIA_LOGI("b = 0x%1$02x (%1$d)", static_cast<unsigned int>(b));
    BROOKESIA_LOGI("a & b = 0x%1$02x", static_cast<unsigned int>(a & b));
    BROOKESIA_LOGI("a | b = 0x%1$02x", static_cast<unsigned int>(a | b));
    BROOKESIA_LOGI("a ^ b = 0x%1$02x", static_cast<unsigned int>(a ^ b));
    BROOKESIA_LOGI("~a = 0x%1$02x", static_cast<unsigned int>(~a));
    BROOKESIA_LOGI("a << 2 = 0x%1$02x", static_cast<unsigned int>(a << 2));
    BROOKESIA_LOGI("a >> 2 = 0x%1$02x", static_cast<unsigned int>(a >> 2));
}

TEST_CASE("Test with hex and octal", "[utils][log][hex_oct]")
{
    BROOKESIA_LOGI("=== Hex and Octal Test ===");

    int num = 255;

    BROOKESIA_LOGI("Decimal: %1%", num);
    BROOKESIA_LOGI("Hex (lowercase): 0x%1$x", num);
    BROOKESIA_LOGI("Hex (uppercase): 0X%1$X", num);
    BROOKESIA_LOGI("Octal: 0%1$o", num);

    // Multiple numbers
    BROOKESIA_LOGI("Dec: %1%, Hex: 0x%1$x, Oct: 0%1$o", 64);
}

TEST_CASE("Test with width and precision", "[utils][log][width_precision]")
{
    BROOKESIA_LOGI("=== Width and Precision Test ===");

    // Integer width
    BROOKESIA_LOGI("Width 5: '%1$5d'", 42);
    BROOKESIA_LOGI("Width 5 (left): '%1$-5d'", 42);
    BROOKESIA_LOGI("Width 5 (zero pad): '%1$05d'", 42);

    // Floating point precision
    double pi = 3.141592653589793;
    BROOKESIA_LOGI("Default: %1%", pi);
    BROOKESIA_LOGI("Precision 2: %1$.2f", pi);
    BROOKESIA_LOGI("Precision 5: %1$.5f", pi);
    BROOKESIA_LOGI("Precision 10: %1$.10f", pi);

    // String width
    BROOKESIA_LOGI("String width 10: '%1$10s'", "Hello");
    BROOKESIA_LOGI("String width 10 (left): '%1$-10s'", "Hello");
}

TEST_CASE("Test with scientific notation", "[utils][log][scientific]")
{
    BROOKESIA_LOGI("=== Scientific Notation Test ===");

    double large = 1234567890.0;
    double small = 0.000000123;

    BROOKESIA_LOGI("Large number: %1%", large);
    BROOKESIA_LOGI("Large (scientific): %1$e", large);
    BROOKESIA_LOGI("Large (scientific, uppercase): %1$E", large);

    BROOKESIA_LOGI("Small number: %1%", small);
    BROOKESIA_LOGI("Small (scientific): %1$e", small);
    BROOKESIA_LOGI("Small (scientific, uppercase): %1$E", small);
}

TEST_CASE("Test with sign modifiers", "[utils][log][sign]")
{
    BROOKESIA_LOGI("=== Sign Modifiers Test ===");

    int positive = 42;
    int negative = -42;

    BROOKESIA_LOGI("Positive (default): %1%", positive);
    BROOKESIA_LOGI("Positive (with +): %1$+d", positive);
    BROOKESIA_LOGI("Positive (with space): %1$ d", positive);

    BROOKESIA_LOGI("Negative (default): %1%", negative);
    BROOKESIA_LOGI("Negative (with +): %1$+d", negative);
    BROOKESIA_LOGI("Negative (with space): %1$ d", negative);
}

TEST_CASE("Test with lambda functions", "[utils][log][lambda]")
{
    BROOKESIA_LOGI("=== Lambda Functions Test ===");

    auto simple_lambda = []() {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Simple lambda");
    };
    simple_lambda();

    auto lambda_with_params = [](int x, int y) {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Lambda with params: x=%1%, y=%2%, sum=%3%", x, y, x + y);
    };
    lambda_with_params(10, 20);

    auto lambda_with_capture = [value = 100]() {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Lambda with capture: value=%1%", value);
    };
    lambda_with_capture();
}

TEST_CASE("Test with nested functions", "[utils][log][nested]")
{
    BROOKESIA_LOGI("=== Nested Functions Test ===");

    auto level1 = []() {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Level 1");

        auto level2 = []() {
            BROOKESIA_LOG_TRACE_GUARD();
            BROOKESIA_LOGI("Level 2");

            auto level3 = []() {
                BROOKESIA_LOG_TRACE_GUARD();
                BROOKESIA_LOGI("Level 3");
            };
            level3();
        };
        level2();
    };
    level1();
}

TEST_CASE("Test with recursive function", "[utils][log][recursive]")
{
    BROOKESIA_LOGI("=== Recursive Function Test ===");

    std::function<int(int)> factorial = [&factorial](int n) -> int {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Calculating factorial(%1%)", n);

        if (n <= 1)
        {
            return 1;
        }
        return n * factorial(n - 1);
    };

    int result = factorial(5);
    BROOKESIA_LOGI("factorial(5) = %1%", result);
    TEST_ASSERT_EQUAL(120, result);
}

TEST_CASE("Test with exception handling", "[utils][log][exception]")
{
    BROOKESIA_LOGI("=== Exception Handling Test ===");

    try {
        BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_LOGI("Before potential exception");

        // In embedded environment, exceptions may not be supported
        // throw std::runtime_error("Test exception");

        BROOKESIA_LOGI("After potential exception (no exception thrown)");
    } catch (const std::exception &e) {
        BROOKESIA_LOGE("Caught exception: %1%", e.what());
    }
}

TEST_CASE("Test with performance", "[utils][log][performance]")
{
    BROOKESIA_LOGI("=== Performance Test ===");

    const int iterations = 100;

    BROOKESIA_LOGI("Starting %1% log iterations...", iterations);

    {
        BROOKESIA_TIME_PROFILER_SCOPE("Test with performance");
        for (int i = 0; i < iterations; i++) {
            BROOKESIA_TIME_PROFILER_SCOPE("single iteration");
            BROOKESIA_LOGD("Iteration %1%", i);
        }
    }

    BROOKESIA_LOGI("Completed %1% iterations", iterations);

    BROOKESIA_TIME_PROFILER_REPORT();
    BROOKESIA_TIME_PROFILER_CLEAR();
}

TEST_CASE("Test with special characters", "[utils][log][special_chars]")
{
    BROOKESIA_LOGI("=== Special Characters Test ===");

    BROOKESIA_LOGI("Tab:\t<-- tab here");
    BROOKESIA_LOGI("Newline:\n<-- newline above");
    BROOKESIA_LOGI("Carriage return:\r<-- CR");
    BROOKESIA_LOGI("Backslash: \\");
    BROOKESIA_LOGI("Single quote: '");
    BROOKESIA_LOGI("Double quote: \"");
    BROOKESIA_LOGI("Question mark: ?");
    BROOKESIA_LOGI("Percent: %%");

    // Unicode (if supported)
    BROOKESIA_LOGI("Unicode: \u4E2D\u6587");  // 中文
}

TEST_CASE("Test with boundary values", "[utils][log][boundary]")
{
    BROOKESIA_LOGI("=== Boundary Values Test ===");

    // Integer boundary
    BROOKESIA_LOGI("INT8_MIN:  %1%", static_cast<int>(INT8_MIN));
    BROOKESIA_LOGI("INT8_MAX:  %1%", static_cast<int>(INT8_MAX));
    BROOKESIA_LOGI("UINT8_MAX: %1%", static_cast<unsigned int>(UINT8_MAX));

    BROOKESIA_LOGI("INT16_MIN:  %1%", INT16_MIN);
    BROOKESIA_LOGI("INT16_MAX:  %1%", INT16_MAX);
    BROOKESIA_LOGI("UINT16_MAX: %1%", UINT16_MAX);

    BROOKESIA_LOGI("INT32_MIN:  %1%", INT32_MIN);
    BROOKESIA_LOGI("INT32_MAX:  %1%", INT32_MAX);
    BROOKESIA_LOGI("UINT32_MAX: %1%", UINT32_MAX);

    // Floating point boundary
    BROOKESIA_LOGI("Float min: %1$e", FLT_MIN);
    BROOKESIA_LOGI("Float max: %1$e", FLT_MAX);
    BROOKESIA_LOGI("Double min: %1$e", DBL_MIN);
    BROOKESIA_LOGI("Double max: %1$e", DBL_MAX);
}

TEST_CASE("Test with zero values", "[utils][log][zero]")
{
    BROOKESIA_LOGI("=== Zero Values Test ===");

    int zero_int = 0;
    float zero_float = 0.0f;
    double zero_double = 0.0;
    void *null_ptr = nullptr;

    BROOKESIA_LOGI("Zero int: %1%", zero_int);
    BROOKESIA_LOGI("Zero float: %1%", zero_float);
    BROOKESIA_LOGI("Zero double: %1%", zero_double);
    BROOKESIA_LOGI("Null pointer: %1%", null_ptr);
}

TEST_CASE("Test with negative values", "[utils][log][negative]")
{
    BROOKESIA_LOGI("=== Negative Values Test ===");

    int neg_int = -12345;
    float neg_float = -3.14f;
    double neg_double = -2.718;

    BROOKESIA_LOGI("Negative int: %1%", neg_int);
    BROOKESIA_LOGI("Negative float: %1%", neg_float);
    BROOKESIA_LOGI("Negative double: %1%", neg_double);

    // Negative numbers in various formats
    BROOKESIA_LOGI("Negative hex: 0x%1$x", neg_int);
    BROOKESIA_LOGI("Negative scientific: %1$e", neg_double);
}

TEST_CASE("Test with very long messages", "[utils][log][long_message]")
{
    BROOKESIA_LOGI("=== Very Long Messages Test ===");

    std::string long_msg = "This is a very long message that contains a lot of text. ";
    for (int i = 0; i < 5; i++) {
        long_msg += "More text here. ";
    }

    BROOKESIA_LOGI("Long message: %1%", long_msg);

    // Very long string
    std::string very_long(500, 'X');
    BROOKESIA_LOGI("Very long string (500 chars): %1%", very_long);
}

TEST_CASE("Test with multiple log calls", "[utils][log][multiple_calls]")
{
    BROOKESIA_LOGI("=== Multiple Log Calls Test ===");

    for (int i = 0; i < 10; i++) {
        BROOKESIA_LOGI("Log call %1%: value=%2%", i, i * 10);
    }
}

TEST_CASE("Test log macro edge cases", "[utils][log][macro_edge]")
{
    BROOKESIA_LOGI("=== Log Macro Edge Cases Test ===");

    // No parameters
    BROOKESIA_LOGI("No parameters");

    // Single parameter
    BROOKESIA_LOGI("One param: %1%", 42);

    // Multiple parameters
    BROOKESIA_LOGI("Multiple params: %1%, %2%, %3%, %4%, %5%", 1, 2, 3, 4, 5);

    // Parameter repeated use
    BROOKESIA_LOGI("Repeat: %1% %1% %1%", "Hello");

    // Parameter out of order
    BROOKESIA_LOGI("Out of order: %3% %1% %2%", "A", "B", "C");
}
#endif // BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_INFO
