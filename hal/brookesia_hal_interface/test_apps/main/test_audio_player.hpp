/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/audio/codec_player.hpp"

namespace esp_brookesia {

class TestAudioCodecPlayerIface: public hal::AudioCodecPlayerIface {
public:
    static constexpr const char *NAME = "TestAudioPlayer:Player";

    TestAudioCodecPlayerIface(hal::AudioCodecPlayerIface::Info info) : hal::AudioCodecPlayerIface(info) {}
    ~TestAudioCodecPlayerIface() = default;

    bool open(const hal::AudioCodecPlayerIface::Config &config) override;
    void close() override;
    bool set_volume(uint8_t volume) override;
    bool get_volume(uint8_t &volume) override;
    bool mute() override;
    bool unmute() override;
    bool write_data(const uint8_t *data, size_t size) override;
};

class TestAudioPlayerDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestAudioPlayer";

    TestAudioPlayerDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
