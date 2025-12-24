/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
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
     *   ______   ________  _______   __     __  ______   ______   ________         __    __  __     __   ______
     *  /      \ |        \|       \ |  \   |  \|      \ /      \ |        \       |  \  |  \|  \   |  \ /      \
     * |  $$$$$$\| $$$$$$$$| $$$$$$$\| $$   | $$ \$$$$$$|  $$$$$$\| $$$$$$$$       | $$\ | $$| $$   | $$|  $$$$$$\
     * | $$___\$$| $$__    | $$__| $$| $$   | $$  | $$  | $$   \$$| $$__           | $$$\| $$| $$   | $$| $$___\$$
     *  \$$    \ | $$  \   | $$    $$ \$$\ /  $$  | $$  | $$      | $$  \          | $$$$\ $$ \$$\ /  $$ \$$    \
     *  _\$$$$$$\| $$$$$   | $$$$$$$\  \$$\  $$   | $$  | $$   __ | $$$$$          | $$\$$ $$  \$$\  $$  _\$$$$$$\
     * |  \__| $$| $$_____ | $$  | $$   \$$ $$   _| $$_ | $$__/  \| $$_____        | $$ \$$$$   \$$ $$  |  \__| $$
     *  \$$    $$| $$     \| $$  | $$    \$$$   |   $$ \ \$$    $$| $$     \ ______| $$  \$$$    \$$$    \$$    $$
     *   \$$$$$$  \$$$$$$$$ \$$   \$$     \$     \$$$$$$  \$$$$$$  \$$$$$$$$|      \\$$   \$$     \$      \$$$$$$
     *                                                                       \$$$$$$
     */
    printf("  ______   ________  _______   __     __  ______   ______   ________         __    __  __     __   ______\r\n");
    printf(" /      \\ |        \\|       \\ |  \\   |  \\|      \\ /      \\ |        \\       |  \\  |  \\|  \\   |  \\ /      \\\r\n");
    printf("|  $$$$$$\\| $$$$$$$$| $$$$$$$\\| $$   | $$ \\$$$$$$|  $$$$$$\\| $$$$$$$$       | $$\\ | $$| $$   | $$|  $$$$$$\\\r\n");
    printf("| $$___\\$$| $$__    | $$__| $$| $$   | $$  | $$  | $$   \\$$| $$__           | $$$\\| $$| $$   | $$| $$___\\$$\r\n");
    printf(" \\$$    \\ | $$  \\   | $$    $$ \\$$\\ /  $$  | $$  | $$      | $$  \\          | $$$$\\ $$ \\$$\\ /  $$ \\$$    \\\r\n");
    printf(" _\\$$$$$$\\| $$$$$   | $$$$$$$\\  \\$$\\  $$   | $$  | $$   __ | $$$$$          | $$\\$$ $$  \\$$\\  $$  _\\$$$$$$\\\r\n");
    printf("|  \\__| $$| $$_____ | $$  | $$   \\$$ $$   _| $$_ | $$__/  \\| $$_____        | $$ \\$$$$   \\$$ $$  |  \\__| $$\r\n");
    printf(" \\$$    $$| $$     \\| $$  | $$    \\$$$   |   $$ \\ \\$$    $$| $$     \\ ______| $$  \\$$$    \\$$$    \\$$    $$\r\n");
    printf("  \\$$$$$$  \\$$$$$$$$ \\$$   \\$$     \\$     \\$$$$$$  \\$$$$$$  \\$$$$$$$$|      \\\\$$   \\$$     \\$      \\$$$$$$\r\n");
    printf("                                                                      \\$$$$$$\r\n");
    unity_run_menu();
}
