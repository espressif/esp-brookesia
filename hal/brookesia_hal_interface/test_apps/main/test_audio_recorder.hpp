/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/audio/codec_recorder.hpp"

namespace esp_brookesia {

class TestAudioCodecRecorderIface: public hal::AudioCodecRecorderIface {
public:
    static constexpr const char *NAME = "TestAudioRecorder:Recorder";

    TestAudioCodecRecorderIface(hal::AudioCodecRecorderIface::Info info) : hal::AudioCodecRecorderIface(std::move(info)) {}
    ~TestAudioCodecRecorderIface() = default;

    bool open() override;
    void close() override;
    bool read_data(uint8_t *data, size_t size) override;
    bool set_general_gain(float gain) override;
    bool set_channel_gains(const std::map<uint8_t, float> &gains) override;
};

class TestAudioRecorderDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestAudioRecorder";

    TestAudioRecorderDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia
