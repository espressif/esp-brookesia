/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file audio_recorder.hpp
 * @brief Audio recording interface definitions for HAL devices.
 */
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Abstract recording interface exposed by audio-capable devices.
 */
class AudioRecorderIface {
public:
    static constexpr std::string_view interface_name = "AudioRecorderIface"; ///< Unique interface name used for runtime lookup.

    /**
     * @brief Recording configuration provided by the device implementation.
     */
    struct RecorderConfig {
        uint8_t bits;                          ///< Sample bit width.
        uint8_t channels;                      ///< Number of captured channels.
        uint32_t sample_rate;                  ///< Capture sample rate in Hz.
        std::string mic_layout;                ///< Microphone layout descriptor used by upper layers.
        float general_gain;                    ///< Global input gain applied to the recorder.
        std::map<uint8_t, float> channel_gains; ///< Optional per-channel gain overrides.
    };

    /**
     * @brief Constructs an empty recording interface.
     */
    AudioRecorderIface() = default;

    /**
     * @brief Destroys the recording interface.
     */
    virtual ~AudioRecorderIface() = default;

    /**
     * @brief Returns the implementation-defined native recording handle.
     *
     * @return Opaque handle managed by the device implementation, or `nullptr` when unavailable.
     */
    const void *get_handle() const
    {
        return handle;
    }

    /**
     * @brief Returns the current recording configuration.
     *
     * @return Immutable reference to the recording configuration.
     */
    const RecorderConfig &get_config() const
    {
        return config;
    }

    /**
     * @brief Opens or activates the recording path.
     *
     * @return true on success, otherwise false.
     */
    virtual bool open_recorder() = 0;

    /**
     * @brief Closes or deactivates the recording path.
     *
     * @return true on success, otherwise false.
     */
    virtual bool close_recorder() = 0;

    /**
     * @brief Applies the configured recorder gain values.
     *
     * @return true on success, otherwise false.
     */
    virtual bool set_recorder_gain() = 0;

protected:
    RecorderConfig config; ///< Recording configuration owned by the implementation.
    void *handle = nullptr; ///< Opaque implementation-defined recording handle.
};

BROOKESIA_DESCRIBE_STRUCT(AudioRecorderIface::RecorderConfig, (), (bits, channels, sample_rate, mic_layout, general_gain, channel_gains));

} // namespace esp_brookesia::hal
