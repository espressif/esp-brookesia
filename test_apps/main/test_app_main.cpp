/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "unity.h"
#include "unity_test_runner.h"
#include "unity_test_utils_memory.h"
#include "esp_heap_caps.h"

// Some resources are lazy allocated in the LCD driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD  (300)

static size_t before_free_8bit;
static size_t before_free_32bit;

void setUp(void)
{
    before_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    before_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
}

void tearDown(void)
{
    size_t after_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    size_t after_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT);
    unity_utils_check_leak(before_free_8bit, after_free_8bit, "8BIT", TEST_MEMORY_LEAK_THRESHOLD);
    unity_utils_check_leak(before_free_32bit, after_free_32bit, "32BIT", TEST_MEMORY_LEAK_THRESHOLD);
}

extern "C" void app_main(void)
{
    /**
     *
     *  ________   ______   _______           _______                                 __                            __
     * |        \ /      \ |       \         |       \                               |  \                          |  \
     * | $$$$$$$$|  $$$$$$\| $$$$$$$\        | $$$$$$$\  ______    ______    ______  | $$   __   ______    _______  \$$  ______
     * | $$__    | $$___\$$| $$__/ $$ ______ | $$__/ $$ /      \  /      \  /      \ | $$  /  \ /      \  /       \|  \ |      \
     * | $$  \    \$$    \ | $$    $$|      \| $$    $$|  $$$$$$\|  $$$$$$\|  $$$$$$\| $$_/  $$|  $$$$$$\|  $$$$$$$| $$  \$$$$$$\
     * | $$$$$    _\$$$$$$\| $$$$$$$  \$$$$$$| $$$$$$$\| $$   \$$| $$  | $$| $$  | $$| $$   $$ | $$    $$ \$$    \ | $$ /      $$
     * | $$_____ |  \__| $$| $$              | $$__/ $$| $$      | $$__/ $$| $$__/ $$| $$$$$$\ | $$$$$$$$ _\$$$$$$\| $$|  $$$$$$$
     * | $$     \ \$$    $$| $$              | $$    $$| $$       \$$    $$ \$$    $$| $$  \$$\ \$$     \|       $$| $$ \$$    $$
     *  \$$$$$$$$  \$$$$$$  \$$               \$$$$$$$  \$$        \$$$$$$   \$$$$$$  \$$   \$$  \$$$$$$$ \$$$$$$$  \$$  \$$$$$$$
     *
     */
    printf(" ________   ______   _______           _______                                 __                            __\r\n");
    printf("|        \\ /      \\ |       \\         |       \\                               |  \\                          |  \\\r\n");
    printf("| $$$$$$$$|  $$$$$$\\| $$$$$$$\\        | $$$$$$$\\  ______    ______    ______  | $$   __   ______    _______  \\$$  ______\r\n");
    printf("| $$__    | $$___\\$$| $$__/ $$ ______ | $$__/ $$ /      \\  /      \\  /      \\ | $$  /  \\ /      \\  /       \\|  \\ |      \\\r\n");
    printf("| $$  \\    \\$$    \\ | $$    $$|      \\| $$    $$|  $$$$$$\\|  $$$$$$\\|  $$$$$$\\| $$_/  $$|  $$$$$$\\|  $$$$$$$| $$  \\$$$$$$\\\r\n");
    printf("| $$$$$    _\\$$$$$$\\| $$$$$$$  \\$$$$$$| $$$$$$$\\| $$   \\$$| $$  | $$| $$  | $$| $$   $$ | $$    $$ \\$$    \\ | $$ /      $$\r\n");
    printf("| $$_____ |  \\__| $$| $$              | $$__/ $$| $$      | $$__/ $$| $$__/ $$| $$$$$$\\ | $$$$$$$$ _\\$$$$$$\\| $$|  $$$$$$$\r\n");
    printf("| $$     \\ \\$$    $$| $$              | $$    $$| $$       \\$$    $$ \\$$    $$| $$  \\$$\\ \\$$     \\|       $$| $$ \\$$    $$\r\n");
    printf(" \\$$$$$$$$  \\$$$$$$  \\$$               \\$$$$$$$  \\$$        \\$$$$$$   \\$$$$$$  \\$$   \\$$  \\$$$$$$$ \\$$$$$$$  \\$$  \\$$$$$$$\r\n");
    unity_run_menu();
}
