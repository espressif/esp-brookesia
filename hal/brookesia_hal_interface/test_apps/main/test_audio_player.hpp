/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/device.hpp"

namespace esp_brookesia {

class TestAudioCodecPlayerIface: public hal::AudioCodecPlayerIface {
public:
    static constexpr const char *NAME = "TestAudioPlayer:Player";

    explicit TestAudioCodecPlayerIface(bool pa_supported)
        : hal::AudioCodecPlayerIface()
        , pa_supported_(pa_supported)
    {
    }
    ~TestAudioCodecPlayerIface() = default;

    bool open(const hal::AudioCodecPlayerIface::Config &config) override;
    void close() override;
    bool set_volume(uint8_t volume) override;
    bool write_data(const uint8_t *data, size_t size) override;
    bool is_pa_on_off_supported() override
    {
        return pa_supported_;
    }
    bool set_pa_on_off(bool on) override;
    bool is_pa_on() const override
    {
        return pa_on_;
    }

    bool is_opened() const
    {
        return is_opened_;
    }
    uint8_t get_last_volume() const
    {
        return last_volume_;
    }
    size_t get_total_bytes_written() const
    {
        return total_bytes_written_;
    }

private:
    bool pa_supported_ = false;
    bool pa_on_ = true;
    bool is_opened_ = false;
    uint8_t last_volume_ = 0;
    size_t total_bytes_written_ = 0;
};

class TestAudioPlayerDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestAudioPlayer";

    TestAudioPlayerDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    static void set_pa_supported(bool pa_supported);
};

} // namespace esp_brookesia
