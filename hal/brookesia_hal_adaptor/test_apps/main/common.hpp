/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <string_view>

#ifdef BROOKESIA_LOG_TAG
#   undef BROOKESIA_LOG_TAG
#endif
#define BROOKESIA_LOG_TAG "HalAdaptorTest"

#include "brookesia/lib_utils.hpp"

constexpr size_t DEFAULT_TEST_MEMORY_LEAK_THRESHOLD = 8 * 1024;

void set_memory_leak_threshold(size_t threshold);
size_t get_memory_leak_threshold();

void reset_hal_test_runtime();
std::string_view get_test_board_name();
bool is_test_esp_vocat_board();
bool is_test_esp32p4_function_ev_board();
