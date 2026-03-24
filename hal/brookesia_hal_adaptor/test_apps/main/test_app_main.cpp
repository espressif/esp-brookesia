/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_newlib.h"
#include "unity.h"
#include "unity_test_utils.h"

#include "common.hpp"

// Some resources are lazy allocated in the driver, the threshold is left for that case.
#define TEST_MEMORY_LEAK_THRESHOLD (0)

void setUp(void)
{
    reset_hal_test_runtime();
    set_memory_leak_threshold(TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    reset_hal_test_runtime();
    esp_reent_cleanup();    // clean up some of newlib's lazy allocations
    unity_utils_evaluate_leaks_direct(get_memory_leak_threshold());
    set_memory_leak_threshold(TEST_MEMORY_LEAK_THRESHOLD);
}

extern "C" void app_main(void)
{
    reset_hal_test_runtime();
    set_memory_leak_threshold(TEST_MEMORY_LEAK_THRESHOLD);
    unity_run_menu();
}
