/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"

namespace esp_brookesia::hal {

class AudioLinuxDevice: public Device {
public:
    static constexpr const char *DEVICE_NAME = "AudioLinux";
    static constexpr const char *PLAYER_IFACE_NAME = "AudioLinux:Player";
    static constexpr const char *RECORDER_IFACE_NAME = "AudioLinux:Recorder";

    AudioLinuxDevice(const AudioLinuxDevice &) = delete;
    AudioLinuxDevice &operator=(const AudioLinuxDevice &) = delete;
    AudioLinuxDevice(AudioLinuxDevice &&) = delete;
    AudioLinuxDevice &operator=(AudioLinuxDevice &&) = delete;

    static AudioLinuxDevice &get_instance()
    {
        static AudioLinuxDevice instance;
        return instance;
    }

    static std::string get_playback_iface_name();
    static std::string get_encoder_iface_name(size_t id);
    static std::string get_decoder_iface_name(size_t id);

private:
    AudioLinuxDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~AudioLinuxDevice() = default;

    bool probe() override;
    std::vector<InterfaceSpec> get_interface_specs() const override;
    bool on_init() override;
    void on_deinit() override;

    std::shared_ptr<audio::CodecPlayerIface> player_iface_;
    std::shared_ptr<audio::CodecRecorderIface> recorder_iface_;
    std::shared_ptr<audio::PlaybackIface> playback_iface_;
    std::shared_ptr<audio::EncoderIface> encoder_iface_;
    std::shared_ptr<audio::DecoderIface> decoder_iface_;
};

} // namespace esp_brookesia::hal
