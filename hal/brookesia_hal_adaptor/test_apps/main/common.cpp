/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>
#include <string_view>

#include "audio_device.hpp"
#include "lcd_display_device.hpp"
#include "esp_board_device.h"
#include "esp_board_manager_includes.h"

#include "common.hpp"

using namespace esp_brookesia::hal;

static size_t memory_leak_threshold = 0;

namespace {

constexpr std::array<const char *, 5> kBoardDeviceNames = {
    "lcd_brightness",
    "lcd_touch",
    "display_lcd",
    "audio_adc",
    "audio_dac",
};

void deinit_board_device_if_needed(const char *name)
{
    (void)esp_board_device_deinit(name);
}

} // namespace

void set_memory_leak_threshold(size_t threshold)
{
    memory_leak_threshold = threshold;
}

size_t get_memory_leak_threshold()
{
    return memory_leak_threshold;
}

void reset_hal_test_runtime()
{
    if (auto display = get_device("LcdDisplay")) {
        display->deinit();
    }
    if (auto audio = get_device("AudioDevice")) {
        audio->deinit();
    }

    for (const auto *name : kBoardDeviceNames) {
        deinit_board_device_if_needed(name);
    }
}

std::string_view get_test_board_name()
{
    return (g_esp_board_info.name != nullptr) ? std::string_view(g_esp_board_info.name) : std::string_view();
}

bool is_test_esp_vocat_board()
{
    const auto board_name = get_test_board_name();
    return board_name == "esp_vocat_board_v1_0" || board_name == "esp_vocat_board_v1_2";
}

bool is_test_esp32p4_function_ev_board()
{
    return get_test_board_name() == "esp32_p4_function_ev";
}
