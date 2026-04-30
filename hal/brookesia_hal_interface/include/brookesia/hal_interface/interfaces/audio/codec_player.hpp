/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file codec_player.hpp
 * @brief Declares the audio player HAL interface.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Player interface exposed by audio-capable devices.
 */
class AudioCodecPlayerIface: public Interface {
public:
    static constexpr const char *NAME = "AudioCodecPlayer";  ///< Interface registry name.

    /**
     * @brief Dynamic configuration.
     */
    struct Config {
        uint8_t bits;           ///< Sample bit width.
        uint8_t channels;       ///< Number of output channels.
        uint32_t sample_rate;   ///< Output sample rate in Hz.
    };

    /**
     * @brief Construct an audio interface.
     */
    AudioCodecPlayerIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic interfaces.
     */
    virtual ~AudioCodecPlayerIface() = default;

    /**
     * @brief Open the backend.
     *
     * @param[in] config Dynamic configuration.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool open(const Config &config) = 0;

    /**
     * @brief Close the backend.
     */
    virtual void close() = 0;

    /**
     * @brief Set volume.
     *
     * @param[in] volume Requested output volume percentage.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_volume(uint8_t volume) = 0;

    /**
     * @brief Write PCM/encoded payload to the backend.
     *
     * @param[in] data Buffer pointer containing audio payload.
     * @param[in] size Buffer size in bytes.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool write_data(const uint8_t *data, size_t size) = 0;

    /**
     * @brief Check if the PA control is supported.
     *
     * @return `true` if the PA on/off control is supported; otherwise `false`.
     */
    virtual bool is_pa_on_off_supported() = 0;

    /**
     * @brief Set the PA on/off state.
     *
     * @param[in] on `true` to set the PA to be on; `false` to set the PA to be off.
     * @return `true` if the PA on/off state is set successfully; otherwise `false`.
     */
    virtual bool set_pa_on_off(bool on) = 0;

    /**
     * @brief Check if the PA is set on.
     *
     * @return `true` if the PA is set on; otherwise `false`.
     */
    virtual bool is_pa_on() const = 0;
};

} // namespace esp_brookesia::hal
