/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <string>
#include <thread>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_utils_memory.h"

#define BROOKESIA_TEST_CASE(id, name, tags) TEST_CASE(name, tags)

namespace esp_brookesia::lib_utils::test_adapter {

inline void delay_ms(uint32_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

} // namespace esp_brookesia::lib_utils::test_adapter

#else

#if defined(BROOKESIA_LIB_UTILS_TEST_ADAPTER_MAIN) && !defined(BOOST_TEST_NO_MAIN)
#define BOOST_TEST_NO_MAIN
#endif

#if defined(BROOKESIA_LIB_UTILS_TEST_ADAPTER_MAIN) && \
    defined(BROOKESIA_LIB_UTILS_TEST_ADAPTER_USE_INCLUDED_BOOST_TEST)
#include "boost/test/included/unit_test.hpp"
#else
#include "boost/test/unit_test.hpp"
#endif

#ifndef TEST_ASSERT_TRUE
#define TEST_ASSERT_TRUE(condition) BOOST_TEST((condition))
#endif

#ifndef TEST_ASSERT_TRUE_MESSAGE
#define TEST_ASSERT_TRUE_MESSAGE(condition, message) BOOST_REQUIRE_MESSAGE((condition), (message))
#endif

#ifndef TEST_ASSERT_FALSE
#define TEST_ASSERT_FALSE(condition) BOOST_TEST(!(condition))
#endif

#ifndef TEST_ASSERT_FALSE_MESSAGE
#define TEST_ASSERT_FALSE_MESSAGE(condition, message) BOOST_REQUIRE_MESSAGE(!(condition), (message))
#endif

#ifndef TEST_ASSERT_NOT_NULL
#define TEST_ASSERT_NOT_NULL(value) BOOST_REQUIRE((value) != nullptr)
#endif

#ifndef TEST_ASSERT_NOT_NULL_MESSAGE
#define TEST_ASSERT_NOT_NULL_MESSAGE(value, message) BOOST_REQUIRE_MESSAGE((value) != nullptr, (message))
#endif

#ifndef TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected equality")
#endif

#ifndef TEST_ASSERT_EQUAL_MESSAGE
#define TEST_ASSERT_EQUAL_MESSAGE(expected, actual, message) BOOST_REQUIRE_MESSAGE((actual) == (expected), (message))
#endif

#ifndef TEST_ASSERT_EQUAL_UINT32
#define TEST_ASSERT_EQUAL_UINT32(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected uint32 equality")
#endif

#ifndef TEST_ASSERT_EQUAL_UINT16
#define TEST_ASSERT_EQUAL_UINT16(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected uint16 equality")
#endif

#ifndef TEST_ASSERT_EQUAL_UINT8
#define TEST_ASSERT_EQUAL_UINT8(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected uint8 equality")
#endif

#ifndef TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_EQUAL_INT(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected int equality")
#endif

#ifndef TEST_ASSERT_EQUAL_INT32
#define TEST_ASSERT_EQUAL_INT32(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected int32 equality")
#endif

#ifndef TEST_ASSERT_EQUAL_INT16
#define TEST_ASSERT_EQUAL_INT16(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected int16 equality")
#endif

#ifndef TEST_ASSERT_EQUAL_INT8
#define TEST_ASSERT_EQUAL_INT8(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected int8 equality")
#endif

#ifndef TEST_ASSERT_EQUAL_size_t
#define TEST_ASSERT_EQUAL_size_t(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected size_t equality")
#endif

#ifndef TEST_ASSERT_EQUAL_STRING
#define TEST_ASSERT_EQUAL_STRING(expected, actual) BOOST_TEST(std::string(actual) == std::string(expected))
#endif

#ifndef TEST_ASSERT_EQUAL_STRING_MESSAGE
#define TEST_ASSERT_EQUAL_STRING_MESSAGE(expected, actual, message) \
    BOOST_TEST(std::string(actual) == std::string(expected), (message))
#endif

#ifndef TEST_ASSERT_EQUAL_FLOAT
#define TEST_ASSERT_EQUAL_FLOAT(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected float equality")
#endif

#ifndef TEST_ASSERT_EQUAL_DOUBLE
#define TEST_ASSERT_EQUAL_DOUBLE(expected, actual) BOOST_REQUIRE_MESSAGE((actual) == (expected), "Expected double equality")
#endif

#ifndef TEST_ASSERT_NOT_EQUAL
#define TEST_ASSERT_NOT_EQUAL(unexpected, actual) BOOST_REQUIRE_MESSAGE((actual) != (unexpected), "Expected inequality")
#endif

#ifndef TEST_ASSERT_GREATER_THAN
#define TEST_ASSERT_GREATER_THAN(threshold, actual) BOOST_REQUIRE_MESSAGE((actual) > (threshold), "Expected greater-than")
#endif

#ifndef TEST_ASSERT_GREATER_OR_EQUAL
#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual) BOOST_REQUIRE_MESSAGE((actual) >= (threshold), "Expected greater-or-equal")
#endif

#ifndef TEST_ASSERT_GREATER_OR_EQUAL_UINT32
#define TEST_ASSERT_GREATER_OR_EQUAL_UINT32(threshold, actual) \
    BOOST_REQUIRE_MESSAGE((actual) >= (threshold), "Expected uint32 greater-or-equal")
#endif

#ifndef TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE
#define TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(threshold, actual, message) BOOST_REQUIRE_MESSAGE((actual) >= (threshold), (message))
#endif

#ifndef TEST_IGNORE_MESSAGE
#define TEST_IGNORE_MESSAGE(message) BOOST_TEST_MESSAGE(message)
#endif

namespace esp_brookesia::lib_utils::test_adapter {

class TestCaseLogger {
public:
    TestCaseLogger(const char *case_name, const char *case_id)
    {
        std::printf("[TEST] Running: %s (%s)\n", case_name, case_id);
        std::fflush(stdout);
    }
};

template <typename DeltaT, typename ExpectedT, typename ActualT>
inline bool is_within_abs(
    DeltaT delta,
    ExpectedT expected,
    ActualT actual
)
{
    return std::fabs(
               static_cast<long double>(actual) - static_cast<long double>(expected)
           ) <= static_cast<long double>(delta);
}

inline void delay_ms(uint32_t delay_ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}

} // namespace esp_brookesia::lib_utils::test_adapter

#define BROOKESIA_TEST_CASE(id, name, tags)                                                                  \
    struct id##_fixture {                                                                                    \
        id##_fixture()                                                                                       \
            : logger_((name), (#id))                                                                         \
        {                                                                                                    \
        }                                                                                                    \
        ::esp_brookesia::lib_utils::test_adapter::TestCaseLogger logger_;                                   \
    };                                                                                                       \
    BOOST_FIXTURE_TEST_CASE(id, id##_fixture)

#ifndef TEST_ASSERT_FLOAT_WITHIN
#define TEST_ASSERT_FLOAT_WITHIN(delta, expected, actual)                                                    \
    BOOST_TEST(::esp_brookesia::lib_utils::test_adapter::is_within_abs((delta), (expected), (actual)))
#endif

#ifndef TEST_ASSERT_DOUBLE_WITHIN
#define TEST_ASSERT_DOUBLE_WITHIN(delta, expected, actual)                                                   \
    BOOST_TEST(::esp_brookesia::lib_utils::test_adapter::is_within_abs((delta), (expected), (actual)))
#endif

#endif
