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
    uint32_t frame_timeout_ms = static_cast<uint32_t>(
                                    BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_FRAME_TIMEOUT_MS
                                );
    int task_core_id = BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_CORE_ID;
    int task_priority = BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_PRIORITY;
    uint32_t tick_period_ms = static_cast<uint32_t>(
                                  BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TICK_PERIOD_MS
                              );
    uint32_t task_min_delay_ms = static_cast<uint32_t>(
                                     BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_MIN_DELAY_MS
                                 );
    uint32_t task_max_delay_ms = static_cast<uint32_t>(
                                     BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_MAX_DELAY_MS
                                 );
    int task_stack_size = BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_STACK_SIZE;
    bool stack_in_psram = (BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_STACK_IN_PSRAM != 0);
    uint16_t buffer_height = static_cast<uint16_t>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_BUFFER_HEIGHT);
    bool require_double_buffer = (BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_REQUIRE_DOUBLE_BUFFER != 0);
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
