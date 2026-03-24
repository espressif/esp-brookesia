/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <array>

#include "unity.h"
#include "audio_device.hpp"
#include "lcd_display_device.hpp"
#include "esp_board_device.h"
#include "esp_board_manager.h"
#include "esp_board_manager_includes.h"

#include "common.hpp"

using namespace esp_brookesia::hal;

namespace {

constexpr char kAudioDeviceName[] = "AudioDevice";
constexpr char kDisplayDeviceName[] = "LcdDisplay";

struct ExpectedAudioConfig {
    uint8_t bits;
    uint8_t channels;
    uint32_t sample_rate;
    const char *mic_layout;
};

struct AudioCodecHandlesCompat {
    void *codec_dev;
};

void prepare_test(size_t leak_threshold = DEFAULT_TEST_MEMORY_LEAK_THRESHOLD)
{
    reset_hal_test_runtime();
    set_memory_leak_threshold(leak_threshold);

    const bool is_supported_board = is_test_esp_vocat_board() || is_test_esp32p4_function_ev_board();
    TEST_ASSERT_TRUE_MESSAGE(
        is_supported_board,
        "This test app only supports esp_vocat boards and esp32_p4_function_ev"
    );
    TEST_ASSERT_TRUE_MESSAGE(!get_test_board_name().empty(), "Board name is empty");
}

ExpectedAudioConfig get_expected_audio_config()
{
    if (is_test_esp_vocat_board()) {
        return {32, 2, 16000, "RMNN"};
    }

    if (is_test_esp32p4_function_ev_board()) {
        return {16, 2, 16000, "MR"};
    }

    TEST_FAIL_MESSAGE("Unsupported board for audio adaptor assertions");
    return {};
}

bool on_bitmap_flush_done()
{
    return false;
}

} // namespace

TEST_CASE("HAL adaptor devices are registered and initialize from registry", "[hal][adaptor][registry]")
{
    prepare_test();

    const auto audio_device = get_device(kAudioDeviceName);
    const auto display_device = get_device(kDisplayDeviceName);

    TEST_ASSERT_NOT_NULL(audio_device.get());
    TEST_ASSERT_NOT_NULL(display_device.get());
    TEST_ASSERT_TRUE(audio_device->probe());
    TEST_ASSERT_TRUE(display_device->probe());

    TEST_ASSERT_TRUE(Device::init_device_from_registry());
    TEST_ASSERT_TRUE(audio_device->check_initialized());
    TEST_ASSERT_TRUE(display_device->check_initialized());
}

TEST_CASE("Audio adaptor exposes player and recorder interfaces with board config", "[hal][adaptor][audio]")
{
    prepare_test();

    TEST_ASSERT_TRUE(Device::init_device_from_registry());

    auto *player = get_interface<AudioPlayerIface>(kAudioDeviceName);
    auto *recorder = get_interface<AudioRecorderIface>(kAudioDeviceName);

    TEST_ASSERT_NOT_NULL(player);
    TEST_ASSERT_NOT_NULL(recorder);
    TEST_ASSERT_NOT_NULL(player->get_handle());
    TEST_ASSERT_NOT_NULL(recorder->get_handle());

    const auto &player_cfg = player->get_config();
    const auto &recorder_cfg = recorder->get_config();
    const auto expected = get_expected_audio_config();

    TEST_ASSERT_EQUAL_UINT8(75, player_cfg.volume_default);
    TEST_ASSERT_EQUAL_UINT8(0, player_cfg.volume_min);
    TEST_ASSERT_EQUAL_UINT8(100, player_cfg.volume_max);

    TEST_ASSERT_EQUAL_UINT8(expected.bits, recorder_cfg.bits);
    TEST_ASSERT_EQUAL_UINT8(expected.channels, recorder_cfg.channels);
    TEST_ASSERT_EQUAL_UINT32(expected.sample_rate, recorder_cfg.sample_rate);
    TEST_ASSERT_EQUAL_STRING(expected.mic_layout, recorder_cfg.mic_layout.c_str());

    void *play_handles = nullptr;
    void *rec_handles = nullptr;
    TEST_ASSERT_EQUAL_HEX32(
        ESP_OK,
        esp_board_device_get_handle("audio_dac", &play_handles)
    );
    TEST_ASSERT_EQUAL_HEX32(
        ESP_OK,
        esp_board_device_get_handle("audio_adc", &rec_handles)
    );
    TEST_ASSERT_NOT_NULL(play_handles);
    TEST_ASSERT_NOT_NULL(rec_handles);
    TEST_ASSERT_EQUAL_PTR(static_cast<AudioCodecHandlesCompat *>(play_handles)->codec_dev, player->get_handle());
    TEST_ASSERT_EQUAL_PTR(static_cast<AudioCodecHandlesCompat *>(rec_handles)->codec_dev, recorder->get_handle());
}

TEST_CASE("LCD adaptor exposes panel and touch interfaces with board config", "[hal][adaptor][display]")
{
    prepare_test();

    TEST_ASSERT_TRUE(Device::init_device_from_registry());

    auto *panel = get_interface<DisplayPanelIface>(kDisplayDeviceName);
    auto *touch = get_interface<DisplayTouchIface>(kDisplayDeviceName);

    TEST_ASSERT_NOT_NULL(panel);
    TEST_ASSERT_NOT_NULL(touch);
    TEST_ASSERT_NOT_NULL(touch->get_handle());

    const auto &panel_cfg = panel->get_config();
    TEST_ASSERT_GREATER_THAN_UINT32(0, panel_cfg.h_res);
    TEST_ASSERT_GREATER_THAN_UINT32(0, panel_cfg.v_res);

#if CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_SPI_SUPPORT
    TEST_ASSERT_TRUE(panel_cfg.flag_swap_color_bytes);
#elif CONFIG_ESP_BOARD_DEV_DISPLAY_LCD_SUB_DSI_SUPPORT
    TEST_ASSERT_FALSE(panel_cfg.flag_swap_color_bytes);
#else
    TEST_FAIL_MESSAGE("Unsupported display LCD subtype in test configuration");
#endif

    void *touch_handle = nullptr;
    TEST_ASSERT_EQUAL_HEX32(
        ESP_OK,
        esp_board_manager_get_device_handle("lcd_touch", &touch_handle)
    );
    TEST_ASSERT_EQUAL_PTR(touch_handle, touch->get_handle());
}

TEST_CASE("HAL adaptor devices support runtime operations and clean reinit", "[hal][adaptor][lifecycle]")
{
    prepare_test(12 * 1024);

    TEST_ASSERT_TRUE(Device::init_device_from_registry());

    auto *panel = get_interface<DisplayPanelIface>(kDisplayDeviceName);
    auto *touch = get_interface<DisplayTouchIface>(kDisplayDeviceName);
    auto *player = get_interface<AudioPlayerIface>(kAudioDeviceName);
    const auto audio_device = get_device(kAudioDeviceName);
    const auto display_device = get_device(kDisplayDeviceName);

    TEST_ASSERT_NOT_NULL(panel);
    TEST_ASSERT_NOT_NULL(touch);
    TEST_ASSERT_NOT_NULL(player);
    TEST_ASSERT_NOT_NULL(audio_device.get());
    TEST_ASSERT_NOT_NULL(display_device.get());

    DisplayPanelIface::DisplayCallbacks callbacks;
    callbacks.bitmap_flush_done = on_bitmap_flush_done;
    TEST_ASSERT_TRUE(panel->register_callbacks(callbacks));
    TEST_ASSERT_TRUE(panel->set_backlight(0));
    TEST_ASSERT_TRUE(panel->set_backlight(50));
    TEST_ASSERT_TRUE(panel->set_backlight(100));

    const std::array<uint16_t, 4> pixels = {0xFFFF, 0xF800, 0x07E0, 0x001F};
    TEST_ASSERT_TRUE(panel->draw_bitmap(0, 0, 2, 2, pixels.data()));

    TEST_ASSERT_TRUE(audio_device->deinit());
    TEST_ASSERT_TRUE(display_device->deinit());
    TEST_ASSERT_FALSE(audio_device->check_initialized());
    TEST_ASSERT_FALSE(display_device->check_initialized());
    TEST_ASSERT_NULL(player->get_handle());
    TEST_ASSERT_NULL(touch->get_handle());

    const auto &panel_cfg = panel->get_config();
    TEST_ASSERT_EQUAL_UINT32(0, panel_cfg.h_res);
    TEST_ASSERT_EQUAL_UINT32(0, panel_cfg.v_res);

    TEST_ASSERT_TRUE(Device::init_device_from_registry());
    TEST_ASSERT_TRUE(audio_device->check_initialized());
    TEST_ASSERT_TRUE(display_device->check_initialized());
}
