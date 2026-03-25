/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "test_fixtures.hpp"

namespace hal_test {

using namespace esp_brookesia::hal;

void clear_registry()
{
    Device::Registry::release_all_instances();
    Device::Registry::remove_all_plugins();
}

MockStatusLedDevice::MockStatusLedDevice(const char *name)
    : DeviceImpl<MockStatusLedDevice>(name)
{
}

bool MockStatusLedDevice::probe()
{
    ++probe_count;
    return true;
}

bool MockStatusLedDevice::check_initialized() const
{
    return initialized;
}

bool MockStatusLedDevice::init()
{
    ++init_count;
    initialized = true;
    return true;
}

bool MockStatusLedDevice::deinit()
{
    ++deinit_count;
    initialized = false;
    return true;
}

void MockStatusLedDevice::start_blink(BlinkType type)
{
    last_started = type;
}

void MockStatusLedDevice::stop_blink(BlinkType type)
{
    last_stopped = type;
}

void *MockStatusLedDevice::query_interface(uint64_t id)
{
    return build_table<StatusLedIface>(id);
}

MockAudioDevice::MockAudioDevice(const char *name)
    : DeviceImpl<MockAudioDevice>(name)
{
    AudioPlayerIface::config.volume_default = 42;
    AudioPlayerIface::config.volume_min = 5;
    AudioPlayerIface::config.volume_max = 95;
    AudioRecorderIface::config.bits = 16;
    AudioRecorderIface::config.channels = 1;
    AudioRecorderIface::config.sample_rate = 16000;
    AudioRecorderIface::config.mic_layout = "M";
    AudioRecorderIface::config.general_gain = 24.0F;
    AudioRecorderIface::config.channel_gains = {{0, 24.0F}};

    volume_ = AudioPlayerIface::config.volume_default;
}

bool MockAudioDevice::probe()
{
    ++probe_count;
    return true;
}

bool MockAudioDevice::check_initialized() const
{
    return initialized;
}

bool MockAudioDevice::init()
{
    ++init_count;
    initialized = true;
    return true;
}

bool MockAudioDevice::deinit()
{
    ++deinit_count;
    initialized = false;
    return true;
}

bool MockAudioDevice::set_volume(uint8_t volume)
{
    if ((volume < AudioPlayerIface::config.volume_min) || (volume > AudioPlayerIface::config.volume_max)) {
        return false;
    }

    volume_ = volume;
    return true;
}

uint8_t MockAudioDevice::get_volume() const
{
    return volume_;
}

bool MockAudioDevice::open_player()
{
    player_opened_ = true;
    AudioPlayerIface::handle = reinterpret_cast<void *>(0xCAFE1001);
    return true;
}

bool MockAudioDevice::close_player()
{
    player_opened_ = false;
    AudioPlayerIface::handle = nullptr;
    return true;
}

bool MockAudioDevice::open_recorder()
{
    recorder_opened_ = true;
    AudioRecorderIface::handle = reinterpret_cast<void *>(0xCAFE1002);
    return true;
}

bool MockAudioDevice::close_recorder()
{
    recorder_opened_ = false;
    AudioRecorderIface::handle = nullptr;
    return true;
}

bool MockAudioDevice::set_recorder_gain()
{
    return recorder_opened_;
}

void *MockAudioDevice::query_interface(uint64_t id)
{
    return build_table<AudioPlayerIface, AudioRecorderIface>(id);
}

MockDisplayDevice::MockDisplayDevice(const char *name)
    : DeviceImpl<MockDisplayDevice>(name)
{
    config_.h_res = 320;
    config_.v_res = 240;
    config_.flag_swap_color_bytes = true;
    display_touch_handles_ = reinterpret_cast<void *>(0xCAFE2001);
    display_lcd_handles_ = reinterpret_cast<void *>(0xCAFE2002);
    display_lcd_cfg_ = reinterpret_cast<void *>(0xCAFE2003);
}

bool MockDisplayDevice::probe()
{
    ++probe_count;
    return true;
}

bool MockDisplayDevice::check_initialized() const
{
    return initialized;
}

bool MockDisplayDevice::init()
{
    ++init_count;
    initialized = true;
    return true;
}

bool MockDisplayDevice::deinit()
{
    ++deinit_count;
    initialized = false;
    return true;
}

bool MockDisplayDevice::set_backlight(int percent)
{
    last_backlight_percent = percent;
    return true;
}

bool MockDisplayDevice::draw_bitmap(uint32_t x1, uint32_t y1, uint32_t x2, uint32_t y2, const void *data)
{
    last_draw.x1 = x1;
    last_draw.y1 = y1;
    last_draw.x2 = x2;
    last_draw.y2 = y2;
    last_draw.data = data;
    return true;
}

bool MockDisplayDevice::register_callbacks(const DisplayCallbacks &callbacks)
{
    display_callbacks_ = callbacks;
    return true;
}

bool MockDisplayDevice::trigger_flush_done()
{
    if (!display_callbacks_.bitmap_flush_done) {
        return false;
    }
    return display_callbacks_.bitmap_flush_done();
}

void *MockDisplayDevice::query_interface(uint64_t id)
{
    return build_table<DisplayPanelIface, DisplayTouchIface>(id);
}

} // namespace hal_test
