/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdio>
#include <cstring>

#include "unity.h"
#include "unity_test_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "brookesia/hal_interface.hpp"
#include "brookesia/hal_adaptor.hpp"

// Some resources are lazy allocated in the driver, the threadhold is left for that case
#if defined(CONFIG_IDF_TARGET_ESP32P4)
// TODO: esp_board_manager deinit SDcard: Memory leak occurs due to missing LDO release.
#   define TEST_MEMORY_LEAK_THRESHOLD (200)
#else
#   define TEST_MEMORY_LEAK_THRESHOLD (0)
#endif

using namespace esp_brookesia;

int memory_leak_threshold = TEST_MEMORY_LEAK_THRESHOLD;

#if CONFIG_BSP_USB_CONSOLE
extern "C" char unity_input_from_gdb[64];
#endif

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    esp_reent_cleanup();    //clean up some of the newlib's lazy allocations
    unity_utils_evaluate_leaks_direct(memory_leak_threshold);
    memory_leak_threshold = TEST_MEMORY_LEAK_THRESHOLD;
}

extern "C" void app_main(void)
{
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

#if CONFIG_BSP_USB_CONSOLE
extern "C" void __wrap_unity_putc(int c)
{
    if (c == '\r') {
        return;
    }

    (void)fputc(c, stdout);
    if (c == '\n') {
        (void)fflush(stdout);
    }
}

extern "C" void __wrap_unity_flush(void)
{
    (void)fflush(stdout);
}

extern "C" void __wrap_unity_gets(char *dst, size_t len)
{
    if ((dst == nullptr) || (len == 0)) {
        return;
    }

    const size_t gdb_input_len = strlen(unity_input_from_gdb);
    if ((gdb_input_len > 0) && (gdb_input_len < (len - 1))) {
        memcpy(dst, unity_input_from_gdb, gdb_input_len);
        dst[gdb_input_len] = '\n';
        dst[gdb_input_len + 1] = '\0';
        memset(unity_input_from_gdb, 0, sizeof(unity_input_from_gdb));
        return;
    }

    size_t write_index = 0;
    while (true) {
        const int c = fgetc(stdin);
        if (c == EOF) {
            clearerr(stdin);
            vTaskDelay(1);
            continue;
        }

        if ((c == '\r') || (c == '\n')) {
            __wrap_unity_putc('\n');
            dst[write_index] = '\0';
            return;
        }
        if (c == '\b') {
            if (write_index > 0) {
                write_index--;
                __wrap_unity_putc('\b');
                __wrap_unity_putc(' ');
                __wrap_unity_putc('\b');
            }
            continue;
        }
        if ((write_index < (len - 1)) && (c > 0x1f) && (c != 0x7f)) {
            dst[write_index++] = static_cast<char>(c);
            __wrap_unity_putc(c);
        }
    }
}
#endif
