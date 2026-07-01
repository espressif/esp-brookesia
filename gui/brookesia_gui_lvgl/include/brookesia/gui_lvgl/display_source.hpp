/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

#include "brookesia/gui_lvgl/macro_configs.h"

typedef struct _lv_display_t lv_display_t;
typedef struct _lv_indev_t lv_indev_t;

namespace esp_brookesia::gui::lvgl {

inline constexpr std::string_view DISPLAY_SOURCE_NAME = "LVGL";
inline constexpr std::string_view DISPLAY_SOURCE_ROLE = "gui";

struct DisplaySourceConfig {
    std::string source_name = std::string(DISPLAY_SOURCE_NAME);
    std::string source_role = std::string(DISPLAY_SOURCE_ROLE);
    std::string output_name;
    uint32_t frame_timeout_ms = 6000;
    int task_core_id = 0;
    int task_priority = 6;
    uint32_t tick_period_ms = 5;
    uint32_t task_min_delay_ms = 10;
    uint32_t task_max_delay_ms = 100;
    int task_stack_size = 40 * 1024;
    bool stack_in_psram = true;
    uint16_t buffer_height = 20;
    bool require_double_buffer = false;
};

class DisplaySourceImpl;

class DisplaySource {
public:
    DisplaySource(const DisplaySource &) = delete;
    DisplaySource(DisplaySource &&) = delete;
    DisplaySource &operator=(const DisplaySource &) = delete;
    DisplaySource &operator=(DisplaySource &&) = delete;

    static DisplaySource &get_instance()
    {
        static DisplaySource instance;
        return instance;
    }

    bool start(const DisplaySourceConfig &config = {});
    void stop_timers();
    void release_display_service();
    void stop();

    bool is_started() const;
    lv_display_t *display() const;
    lv_indev_t *input() const;
    std::string output_name() const;
    uint32_t source_id() const;
    uint32_t width() const;
    uint32_t height() const;

private:
    DisplaySource();
    ~DisplaySource();

    std::unique_ptr<DisplaySourceImpl> impl_;
};

} // namespace esp_brookesia::gui::lvgl
