/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "common_def.hpp"

namespace hal_test {

struct RgbLightConfig {
    uint8_t channel = 0;
    bool active_low = false;
};

struct DisplayDrawCall {
    uint32_t x1 = 0;
    uint32_t y1 = 0;
    uint32_t x2 = 0;
    uint32_t y2 = 0;
    const void *data = nullptr;
};

void clear_registry();

class MockStatusLedDevice : public esp_brookesia::hal::DeviceImpl<MockStatusLedDevice>,
    public esp_brookesia::hal::StatusLedIface {
public:
    explicit MockStatusLedDevice(const char *name);

    bool probe() override;
    bool check_initialized() const override;
    bool init() override;
    bool deinit() override;
    void start_blink(BlinkType type) override;
    void stop_blink(BlinkType type) override;

    uint32_t probe_count = 0;
    uint32_t init_count = 0;
    uint32_t deinit_count = 0;
    BlinkType last_started = BlinkType::Max;
    BlinkType last_stopped = BlinkType::Max;
    bool initialized = false;

private:
    void *query_interface(uint64_t id) override;
};

class MockAudioDevice : public esp_brookesia::hal::DeviceImpl<MockAudioDevice>,
    public esp_brookesia::hal::AudioPlayerIface,
    public esp_brookesia::hal::AudioRecorderIface {
public:
    explicit MockAudioDevice(const char *name);

    bool probe() override;
    bool check_initialized() const override;
    bool init() override;
    bool deinit() override;
    bool set_volume(uint8_t volume) override;
    uint8_t get_volume() const override;
    bool open_player() override;
    bool close_player() override;
    bool open_recorder() override;
    bool close_recorder() override;
    bool set_recorder_gain() override;

    uint32_t probe_count = 0;
    uint32_t init_count = 0;
    uint32_t deinit_count = 0;
    bool initialized = false;

private:
    uint8_t volume_ = 0;
    bool player_opened_ = false;
    bool recorder_opened_ = false;
    void *query_interface(uint64_t id) override;
};

class MockDisplayDevice : public esp_brookesia::hal::DeviceImpl<MockDisplayDevice>,
    public esp_brookesia::hal::DisplayPanelIface,
    public esp_brookesia::hal::DisplayTouchIface {
public:
    explicit MockDisplayDevice(const char *name);

    bool probe() override;
    bool check_initialized() const override;
    bool init() override;
    bool deinit() override;
    bool set_backlight(int percent) override;
    bool draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data) override;
    bool register_callbacks(const DisplayCallbacks &callbacks) override;
    bool trigger_flush_done();

    uint32_t probe_count = 0;
    uint32_t init_count = 0;
    uint32_t deinit_count = 0;
    bool initialized = false;
    int last_backlight_percent = -1;
    DisplayDrawCall last_draw{};

private:
    void *query_interface(uint64_t id) override;
};

} // namespace hal_test
