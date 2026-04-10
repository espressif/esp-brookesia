/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file codec_player.hpp
 * @brief Declares the audio playback HAL interface.
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
 * @brief Playback interface exposed by audio-capable devices.
 */
class AudioCodecPlayerIface: public Interface {
public:
    static constexpr const char *NAME = "AudioCodecPlayer";  ///< Interface registry name.

    /**
     * @brief Static playback capability information.
     */
    struct Info {
        uint8_t volume_default; ///< Default volume percentage.
        uint8_t volume_min;     ///< Minimum supported volume percentage.
        uint8_t volume_max;     ///< Maximum supported volume percentage.
    };

    /**
     * @brief Dynamic playback configuration.
     */
    struct Config {
        uint8_t bits;           ///< Sample bit width.
        uint8_t channels;       ///< Number of output channels.
        uint32_t sample_rate;   ///< Output sample rate in Hz.
    };

    /**
     * @brief Construct an audio playback interface.
     *
     * @param[in] info Static playback capability information.
     */
    AudioCodecPlayerIface(Info info)
        : Interface(NAME)
        , info_(std::move(info))
    {
    }

    /**
     * @brief Virtual destructor for polymorphic playback interfaces.
     */
    virtual ~AudioCodecPlayerIface() = default;

    /**
     * @brief Open the playback backend.
     *
     * @param[in] config Dynamic playback configuration.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool open(const Config &config) = 0;

    /**
     * @brief Close the playback backend.
     */
    virtual void close() = 0;

    /**
     * @brief Set playback volume.
     *
     * @param[in] volume Requested output volume percentage.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool set_volume(uint8_t volume) = 0;

    /**
     * @brief Get current playback volume.
     *
     * @param[out] volume Current output volume percentage.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool get_volume(uint8_t &volume) = 0;

    /**
     * @brief Set playback to mute.
     *
     * @note Calling this interface will mute the playback. Even if the minimum volume is not zero,
     *       the volume will be muted to zero.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool mute() = 0;

    /**
     * @brief Unmute playback.
     *
     * @note Calling this interface will set the volume to the last set value. If the last set value is zero,
     *       the volume will be restored to the default value.
     *
     * @return `true` on success; otherwise `false`.
     */
    virtual bool unmute() = 0;

    /**
     * @brief Write PCM/encoded payload to the playback backend.
     *
     * @param[in] data Buffer pointer containing audio payload.
     * @param[in] size Buffer size in bytes.
     * @return `true` on success; otherwise `false`.
     */
    virtual bool write_data(const uint8_t *data, size_t size) = 0;

    /**
     * @brief Get static playback capability information.
     *
     * @return Playback information.
     */
    const Info &get_info() const
    {
        return info_;
    }

private:
    Info info_{};
};

BROOKESIA_DESCRIBE_STRUCT(AudioCodecPlayerIface::Info, (), (volume_default, volume_min, volume_max));

} // namespace esp_brookesia::hal
