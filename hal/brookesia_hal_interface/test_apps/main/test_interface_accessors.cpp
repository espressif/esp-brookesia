/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <array>

#include "unity.h"

#include "test_fixtures.hpp"

using namespace esp_brookesia::hal;
using namespace hal_test;

TEST_CASE("HAL status LED interface records blink types", "[hal][interface][status_led]")
{
    MockStatusLedDevice led("status_led");

    led.start_blink(StatusLedIface::BlinkType::WifiConnected);
    led.stop_blink(StatusLedIface::BlinkType::WifiDisconnected);

    TEST_ASSERT_EQUAL(static_cast<int>(StatusLedIface::BlinkType::WifiConnected), static_cast<int>(led.last_started));
    TEST_ASSERT_EQUAL(static_cast<int>(StatusLedIface::BlinkType::WifiDisconnected), static_cast<int>(led.last_stopped));
}

TEST_CASE("HAL audio interfaces expose io handles and configs", "[hal][interface][audio]")
{
    MockAudioDevice audio("audio");

    auto &player = audio.AudioPlayerIface::get_config();
    auto &recorder = audio.AudioRecorderIface::get_config();

    TEST_ASSERT_EQUAL_UINT8(42, player.volume_default);
    TEST_ASSERT_EQUAL_UINT8(5, player.volume_min);
    TEST_ASSERT_EQUAL_UINT8(95, player.volume_max);

    TEST_ASSERT_EQUAL_UINT8(16, recorder.bits);
    TEST_ASSERT_EQUAL_UINT8(1, recorder.channels);
    TEST_ASSERT_EQUAL_UINT32(16000, recorder.sample_rate);
    TEST_ASSERT_EQUAL_STRING("M", recorder.mic_layout.c_str());
    TEST_ASSERT_EQUAL_FLOAT(24.0F, recorder.general_gain);
    TEST_ASSERT_EQUAL_FLOAT(24.0F, recorder.channel_gains.at(0));
}

TEST_CASE("HAL display interfaces expose config touch handle draw and callback state", "[hal][interface][display]")
{
    MockDisplayDevice display("display");
    uint32_t callback_count = 0;
    bool callback_result = true;
    DisplayPanelIface::DisplayCallbacks callbacks;
    callbacks.bitmap_flush_done = [&callback_count, &callback_result]() {
        ++callback_count;
        return callback_result;
    };

    TEST_ASSERT_TRUE(display.register_callbacks(callbacks));
    TEST_ASSERT_TRUE(display.set_backlight(80));

    const std::array<uint16_t, 4> pixels = {0xFFFF, 0xF800, 0x07E0, 0x001F};
    TEST_ASSERT_TRUE(display.draw_bitmap(1, 2, 3, 4, pixels.data()));

    const auto &cfg = display.get_config();
    TEST_ASSERT_EQUAL_UINT32(320, cfg.h_res);
    TEST_ASSERT_EQUAL_UINT32(240, cfg.v_res);
    TEST_ASSERT_TRUE(cfg.flag_swap_color_bytes);

    TEST_ASSERT_EQUAL_PTR(reinterpret_cast<void *>(0xCAFE2001), display.get_handle());
    TEST_ASSERT_EQUAL(80, display.last_backlight_percent);
    TEST_ASSERT_EQUAL_UINT32(1, display.last_draw.x1);
    TEST_ASSERT_EQUAL_UINT32(2, display.last_draw.y1);
    TEST_ASSERT_EQUAL_UINT32(3, display.last_draw.x2);
    TEST_ASSERT_EQUAL_UINT32(4, display.last_draw.y2);
    TEST_ASSERT_EQUAL_PTR(pixels.data(), display.last_draw.data);

    TEST_ASSERT_TRUE(display.trigger_flush_done());
    TEST_ASSERT_EQUAL_UINT32(1, callback_count);

    callback_result = false;
    TEST_ASSERT_FALSE(display.trigger_flush_done());
    TEST_ASSERT_EQUAL_UINT32(2, callback_count);
}
