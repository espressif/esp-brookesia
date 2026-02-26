/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
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
#include "general.hpp"

// Some resources are lazy allocated in the driver, the threadhold is left for that case
#if CONFIG_ESP_HOSTED_ENABLED
#   define TEST_MEMORY_LEAK_THRESHOLD (1024)
#else
#   define TEST_MEMORY_LEAK_THRESHOLD (600)
#endif

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
     *   ______   ________  _______   __     __  ______   ______   ________         __       __  ______  ________  ______
     *  /      \ |        \|       \ |  \   |  \|      \ /      \ |        \       |  \  _  |  \|      \|        \|      \
     * |  $$$$$$\| $$$$$$$$| $$$$$$$\| $$   | $$ \$$$$$$|  $$$$$$\| $$$$$$$$       | $$ / \ | $$ \$$$$$$| $$$$$$$$ \$$$$$$
     * | $$___\$$| $$__    | $$__| $$| $$   | $$  | $$  | $$   \$$| $$__           | $$/  $\| $$  | $$  | $$__      | $$
     *  \$$    \ | $$  \   | $$    $$ \$$\ /  $$  | $$  | $$      | $$  \          | $$  $$$\ $$  | $$  | $$  \     | $$
     *  _\$$$$$$\| $$$$$   | $$$$$$$\  \$$\  $$   | $$  | $$   __ | $$$$$          | $$ $$\$$\$$  | $$  | $$$$$     | $$
     * |  \__| $$| $$_____ | $$  | $$   \$$ $$   _| $$_ | $$__/  \| $$_____        | $$$$  \$$$$ _| $$_ | $$       _| $$_
     *  \$$    $$| $$     \| $$  | $$    \$$$   |   $$ \ \$$    $$| $$     \ ______| $$$    \$$$|   $$ \| $$      |   $$ \
     *   \$$$$$$  \$$$$$$$$ \$$   \$$     \$     \$$$$$$  \$$$$$$  \$$$$$$$$|      \\$$      \$$ \$$$$$$ \$$       \$$$$$$
     *                                                                       \$$$$$$
     */
    printf("  ______   ________  _______   __     __  ______   ______   ________         __       __  ______  ________  ______\r\n");
    printf(" /      \\ |        \\|       \\ |  \\   |  \\|      \\ /      \\ |        \\       |  \\  _  |  \\|      \\|        \\|      \\\r\n");
    printf("|  $$$$$$\\| $$$$$$$$| $$$$$$$\\| $$   | $$ \\$$$$$$|  $$$$$$\\| $$$$$$$$       | $$ / \\ | $$ \\$$$$$$| $$$$$$$$ \\$$$$$$\r\n");
    printf("| $$___\\$$| $$__    | $$__| $$| $$   | $$  | $$  | $$   \\$$| $$__           | $$/  $\\| $$  | $$  | $$__      | $$\r\n");
    printf(" \\$$    \\ | $$  \\   | $$    $$ \\$$\\ /  $$  | $$  | $$      | $$  \\          | $$  $$$\\ $$  | $$  | $$  \\     | $$\r\n");
    printf(" _\\$$$$$$\\| $$$$$   | $$$$$$$\\  \\$$\\  $$   | $$  | $$   __ | $$$$$          | $$ $$\\$$\\$$  | $$  | $$$$$     | $$\r\n");
    printf("|  \\__| $$| $$_____ | $$  | $$   \\$$ $$   _| $$_ | $$__/  \\| $$_____        | $$$$  \\$$$$ _| $$_ | $$       _| $$_\r\n");
    printf(" \\$$    $$| $$     \\| $$  | $$    \\$$$   |   $$ \\ \\$$    $$| $$     \\ ______| $$$    \\$$$|   $$ \\| $$      |   $$ \\\r\n");
    printf("  \\$$$$$$  \\$$$$$$$$ \\$$   \\$$     \\$     \\$$$$$$  \\$$$$$$  \\$$$$$$$$|      \\\\$$      \\$$ \\$$$$$$ \\$$       \\$$$$$$\r\n");
    printf("                                                                      \\$$$$$$\r\n");

    auto unity_run_menu_thread = []() {
        unity_run_menu();
    };
    {
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_size = 20 * 1024,
        });
        boost::thread(unity_run_menu_thread).detach();
    }
}
