/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <functional>
#include "brookesia/service_helper/audio.hpp"

struct DisplayPeripheralConfig {
    uint32_t h_res;
    uint32_t v_res;
    bool flag_swap_color_bytes;
};

using DisplayBitmapFlushDoneCallback = std::function<bool()>;
struct DisplayCallbacks {
    DisplayBitmapFlushDoneCallback bitmap_flush_done;
};

class Board {
public:
    Board(const Board &) = delete;
    Board &operator=(const Board &) = delete;

    static Board &get_instance()
    {
        static Board instance;
        return instance;
    }

    bool init();

    bool is_initialized() const
    {
        return is_initialized_;
    }

    // Audio
    bool init_audio(esp_brookesia::service::helper::Audio::PeripheralConfig &config);

    // Display
    bool init_display(DisplayPeripheralConfig &config);
    bool set_display_backlight(int percent);
    bool draw_display_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data);
    bool register_display_callbacks(DisplayCallbacks &callbacks);

private:
    Board() = default;
    ~Board() = default;

    bool is_initialized_ = false;
    void *display_lcd_handles_ = nullptr;
    void *display_lcd_cfg_ = nullptr;
    DisplayCallbacks display_callbacks_{};
};
