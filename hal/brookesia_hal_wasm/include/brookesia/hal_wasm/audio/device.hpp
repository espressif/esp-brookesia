/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"

namespace esp_brookesia::hal {

class AudioCodecPlayerWasmImpl;
class AudioPlaybackWasmImpl;
class AudioEncoderWasmImpl;
class AudioDecoderWasmImpl;

class AudioWasmDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "AudioWasm";
    static constexpr const char *PLAYER_IFACE_NAME = "AudioWasm:Player";

    AudioWasmDevice(const AudioWasmDevice &) = delete;
    AudioWasmDevice &operator=(const AudioWasmDevice &) = delete;
    AudioWasmDevice(AudioWasmDevice &&) = delete;
    AudioWasmDevice &operator=(AudioWasmDevice &&) = delete;

    static AudioWasmDevice &get_instance()
    {
        static AudioWasmDevice instance;
        return instance;
    }

    static std::string get_playback_iface_name();
    static std::string get_encoder_iface_name(size_t id);
    static std::string get_decoder_iface_name(size_t id);

private:
    AudioWasmDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~AudioWasmDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<AudioCodecPlayerWasmImpl> player_iface_;
    std::shared_ptr<AudioPlaybackWasmImpl> playback_iface_;
    std::shared_ptr<AudioEncoderWasmImpl> encoder_iface_;
    std::shared_ptr<AudioDecoderWasmImpl> decoder_iface_;
};

} // namespace esp_brookesia::hal
