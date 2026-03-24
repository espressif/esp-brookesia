/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "unity.h"
#include "unity_test_utils.h"

#include "test_fixtures.hpp"

#define TEST_MEMORY_LEAK_THRESHOLD (384)

void setUp(void)
{
    hal_test::clear_registry();
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    hal_test::clear_registry();
    esp_reent_cleanup();
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

extern "C" void app_main(void)
{
    printf("\nHAL Interface test_apps\n");
    unity_run_menu();
}
