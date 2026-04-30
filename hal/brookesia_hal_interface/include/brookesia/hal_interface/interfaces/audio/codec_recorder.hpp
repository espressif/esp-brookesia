/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file codec_recorder.hpp
 * @brief Declares the audio recording HAL interface.
 */

namespace esp_brookesia::hal {

/**
 * @brief Recording interface exposed by audio-capable devices.
 */
class AudioCodecRecorderIface: public Interface {
public:
    static constexpr const char *NAME = "AudioCodecRecorder";  ///< Interface registry name.

    /**
     * @brief Static recording capability information.
     */
    struct Info {
        uint8_t bits;                           ///< Sample bit width.
        uint8_t channels;                       ///< Number of capture channels.
        uint32_t sample_rate;                   ///< Capture sample rate in Hz.
        std::string mic_layout;                 ///< Microphone layout descriptor.
        float general_gain;                     ///< Global input gain.
        std::map<uint8_t, float> channel_gains; ///< Per-channel gain overrides.
    };

    /**
     * @brief Construct an audio recording interface.
     *
     * @param[in] info Static recording capability information.
     */
    AudioCodecRecorderIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic recording interfaces.
     */
    virtual ~AudioCodecRecorderIface() = default;

    /**
     * @brief Open the recording backend.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool open() = 0;

    /**
     * @brief Close the recording backend.
     */
    virtual void close() = 0;

    /**
     * @brief Read captured audio payload.
     *
     * @param[out] data Destination buffer for captured bytes.
     * @param[in] size Requested byte count.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool read_data(uint8_t *data, size_t size) = 0;

    /**
     * @brief Set the general gain.
     *
     * @param[in] gain The gain to set.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_general_gain(float gain) = 0;

    /**
     * @brief Set the channel gains.
     *
     * @param[in] gains The channel gains to set.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_channel_gains(const std::map<uint8_t, float> &gains) = 0;

    /**
     * @brief Get static recording capability information.
     *
     * @return Recording information.
     */
    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_STRUCT(
    AudioCodecRecorderIface::Info, (), (bits, channels, sample_rate, mic_layout, general_gain, channel_gains)
);

} // namespace esp_brookesia::hal
