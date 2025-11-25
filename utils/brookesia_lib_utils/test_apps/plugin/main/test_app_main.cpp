/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_check.h"
#include "unity.h"
#include "unity_test_utils.h"

// Some resources are lazy allocated in the driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (0)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

extern "C" void app_main(void)
{


    /**
     *  _______   __       __    __   ______   ______  __    __
     * |       \ |  \     |  \  |  \ /      \ |      \|  \  |  \
     * | $$$$$$$\| $$     | $$  | $$|  $$$$$$\ \$$$$$$| $$\ | $$
     * | $$__/ $$| $$     | $$  | $$| $$ __\$$  | $$  | $$$\| $$
     * | $$    $$| $$     | $$  | $$| $$|    \  | $$  | $$$$\ $$
     * | $$$$$$$ | $$     | $$  | $$| $$ \$$$$  | $$  | $$\$$ $$
     * | $$      | $$_____| $$__/ $$| $$__| $$ _| $$_ | $$ \$$$$
     * | $$      | $$     \\$$    $$ \$$    $$|   $$ \| $$  \$$$
     *  \$$       \$$$$$$$$ \$$$$$$   \$$$$$$  \$$$$$$ \$$   \$$
     */
    printf(" _______   __       __    __   ______   ______  __    __\r\n");
    printf("|       \\ |  \\     |  \\  |  \\ /      \\ |      \\|  \\  |  \\\r\n");
    printf("| $$$$$$$\\| $$     | $$  | $$|  $$$$$$\\ \\$$$$$$| $$\\ | $$\r\n");
    printf("| $$__/ $$| $$     | $$  | $$| $$ __\\$$  | $$  | $$$\\| $$\r\n");
    printf("| $$    $$| $$     | $$  | $$| $$|    \\  | $$  | $$$$\\ $$\r\n");
    printf("| $$$$$$$ | $$     | $$  | $$| $$ \\$$$$  | $$  | $$\\$$ $$\r\n");
    printf("| $$      | $$_____| $$__/ $$| $$__| $$ _| $$_ | $$ \\$$$$\r\n");
    printf("| $$      | $$     \\\\$$    $$ \\$$    $$|   $$ \\| $$  \\$$$\r\n");
    printf(" \\$$       \\$$$$$$$$ \\$$$$$$   \\$$$$$$  \\$$$$$$ \\$$   \\$$\r\n");
    unity_run_menu();
}
