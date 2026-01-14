/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/service_audio.hpp"

struct DisplayPeripheralConfig {
    uint32_t h_res;
    uint32_t v_res;
    bool flag_swap_color_bytes;
};

using DisplayBitmapFlushDoneCallback = std::function<bool()>;
struct DisplayCallbacks {
    DisplayBitmapFlushDoneCallback bitmap_flush_done;
};

bool board_manager_init();

bool board_audio_peripheral_init(esp_brookesia::service::Audio::PeripheralConfig &config);

bool board_display_backlight_set(int percent);
bool board_display_peripheral_init(DisplayPeripheralConfig &config);
bool board_display_draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data);
bool board_display_register_callbacks(DisplayCallbacks &callbacks);
