/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <chrono>
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

#define BROOKESIA_TEST_CASE(id, name, tags) BOOST_AUTO_TEST_CASE(id)

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

#ifndef TEST_ASSERT_EQUAL
#define TEST_ASSERT_EQUAL(expected, actual) BOOST_TEST((actual) == (expected))
#endif

#ifndef TEST_ASSERT_EQUAL_UINT32
#define TEST_ASSERT_EQUAL_UINT32(expected, actual) BOOST_TEST((actual) == (expected))
#endif

#ifndef TEST_ASSERT_EQUAL_INT
#define TEST_ASSERT_EQUAL_INT(expected, actual) BOOST_TEST((actual) == (expected))
#endif

#ifndef TEST_ASSERT_EQUAL_size_t
#define TEST_ASSERT_EQUAL_size_t(expected, actual) BOOST_TEST((actual) == (expected))
#endif

#ifndef TEST_ASSERT_EQUAL_STRING
#define TEST_ASSERT_EQUAL_STRING(expected, actual) BOOST_TEST(std::string(actual) == std::string(expected))
#endif

#ifndef TEST_ASSERT_NOT_EQUAL
#define TEST_ASSERT_NOT_EQUAL(unexpected, actual) BOOST_TEST((actual) != (unexpected))
#endif

#ifndef TEST_ASSERT_GREATER_THAN
#define TEST_ASSERT_GREATER_THAN(threshold, actual) BOOST_TEST((actual) > (threshold))
#endif

#ifndef TEST_ASSERT_GREATER_OR_EQUAL
#define TEST_ASSERT_GREATER_OR_EQUAL(threshold, actual) BOOST_TEST((actual) >= (threshold))
#endif

#ifndef TEST_IGNORE_MESSAGE
#define TEST_IGNORE_MESSAGE(message) BOOST_TEST_MESSAGE(message)
#endif

namespace esp_brookesia::lib_utils::test_adapter {

inline void delay_ms(uint32_t delay_ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
}

} // namespace esp_brookesia::lib_utils::test_adapter

#endif
