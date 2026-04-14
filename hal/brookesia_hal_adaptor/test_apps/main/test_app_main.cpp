/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "unity.h"
#include "unity_test_utils.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

// Some resources are lazy allocated in the driver, the threadhold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (0)

using namespace esp_brookesia;

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
    hal::init_device(hal::StorageDevice::DEVICE_NAME);
    hal::init_device(hal::AudioDevice::DEVICE_NAME);

    /**
     *  __    __   ______   __                ______   _______    ______   _______  ________   ______   _______
     * |  \  |  \ /      \ |  \              /      \ |       \  /      \ |       \|        \ /      \ |       \
     * | $$  | $$|  $$$$$$\| $$             |  $$$$$$\| $$$$$$$\|  $$$$$$\| $$$$$$$\\$$$$$$$$|  $$$$$$\| $$$$$$$\
     * | $$__| $$| $$__| $$| $$             | $$__| $$| $$  | $$| $$__| $$| $$__/ $$  | $$   | $$  | $$| $$__| $$
     * | $$    $$| $$    $$| $$             | $$    $$| $$  | $$| $$    $$| $$    $$  | $$   | $$  | $$| $$    $$
     * | $$$$$$$$| $$$$$$$$| $$             | $$$$$$$$| $$  | $$| $$$$$$$$| $$$$$$$   | $$   | $$  | $$| $$$$$$$\
     * | $$  | $$| $$  | $$| $$_____        | $$  | $$| $$__/ $$| $$  | $$| $$        | $$   | $$__/ $$| $$  | $$
     * | $$  | $$| $$  | $$| $$     \ ______| $$  | $$| $$    $$| $$  | $$| $$        | $$    \$$    $$| $$  | $$
     *  \$$   \$$ \$$   \$$ \$$$$$$$$|      \\$$   \$$ \$$$$$$$  \$$   \$$ \$$         \$$     \$$$$$$  \$$   \$$
     *                                \$$$$$$
     */
    printf(" __    __   ______   __                ______   _______    ______   _______  ________   ______   _______\r\n");
    printf("|  \\  |  \\ /      \\ |  \\              /      \\ |       \\  /      \\ |       \\|        \\ /      \\ |       \\\r\n");
    printf("| $$  | $$|  $$$$$$\\| $$             |  $$$$$$\\| $$$$$$$\\|  $$$$$$\\| $$$$$$$\\\\$$$$$$$$|  $$$$$$\\| $$$$$$$\\\r\n");
    printf("| $$__| $$| $$__| $$| $$             | $$__| $$| $$  | $$| $$__| $$| $$__/ $$  | $$   | $$  | $$| $$__| $$\r\n");
    printf("| $$    $$| $$    $$| $$             | $$    $$| $$  | $$| $$    $$| $$    $$  | $$   | $$  | $$| $$    $$\r\n");
    printf("| $$$$$$$$| $$$$$$$$| $$             | $$$$$$$$| $$  | $$| $$$$$$$$| $$$$$$$   | $$   | $$  | $$| $$$$$$$\\\r\n");
    printf("| $$  | $$| $$  | $$| $$_____        | $$  | $$| $$__/ $$| $$  | $$| $$        | $$   | $$__/ $$| $$  | $$\r\n");
    printf("| $$  | $$| $$  | $$| $$     \\ ______| $$  | $$| $$    $$| $$  | $$| $$        | $$    \\$$    $$| $$  | $$\r\n");
    printf(" \\$$   \\$$ \\$$   \\$$ \\$$$$$$$$|      \\\\$$   \\$$ \\$$$$$$$  \\$$   \\$$ \\$$         \\$$     \\$$$$$$  \\$$   \\$$\r\n");
    printf("                               \\$$$$$$\r\n");
    unity_run_menu();
}
