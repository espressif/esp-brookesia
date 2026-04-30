/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Singleton HAL device that aggregates codec-based audio playback and recording for board-managed audio hardware.
 */
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed audio device: registers codec player and recorder HAL interfaces after board bring-up.
 *
 * Obtained via get_instance(). Not copyable or movable.
 */
class AudioDevice : public Device {
public:
    /** @brief Logical device name passed to the base @ref Device constructor. */
    static constexpr const char *DEVICE_NAME = "Audio";
    /** @brief Registry key for the codec player HAL implementation (`"Audio:CodecPlayer"`). */
    static constexpr const char *CODEC_PLAYER_IMPL_NAME = "Audio:CodecPlayer";
    /** @brief Registry key for the codec recorder HAL implementation (`"Audio:CodecRecorder"`). */
    static constexpr const char *CODEC_RECORDER_IMPL_NAME = "Audio:CodecRecorder";

    AudioDevice(const AudioDevice &) = delete;
    AudioDevice &operator=(const AudioDevice &) = delete;
    AudioDevice(AudioDevice &&) = delete;
    AudioDevice &operator=(AudioDevice &&) = delete;

    /**
     * @brief Overrides default static recording capability information used when constructing the codec recorder implementation.
     *
     * @param[in] info Codec recorder capability descriptor (format, channels, gains, etc.).
     *
     * @return `true` if the value was stored; `false` on invalid input or if the recorder is already initialized.
     */
    bool set_codec_recorder_info(AudioCodecRecorderIface::Info info);

    /**
     * @brief Returns the process-wide singleton audio device.
     *
     * @return Reference to the unique @ref AudioDevice instance.
     */
    static AudioDevice &get_instance()
    {
        static AudioDevice instance;
        return instance;
    }

private:
    AudioDevice()
        : Device(std::string(DEVICE_NAME))
    {
    }
    ~AudioDevice() = default;

    bool probe() override;
    bool on_init() override;
    void on_deinit() override;

    bool init_codec_player();
    void deinit_codec_player();

    bool init_codec_recorder();
    void deinit_codec_recorder();

    std::optional<AudioCodecRecorderIface::Info> codec_recorder_info_ = std::nullopt;
};

} // namespace esp_brookesia::hal
