/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file audio_player.hpp
 * @brief Audio playback interface definitions for HAL devices.
 */
#pragma once

#include <cstdint>
#include <string_view>
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Abstract playback interface exposed by audio-capable devices.
 */
class AudioPlayerIface {
public:
    static constexpr std::string_view interface_name = "AudioPlayerIface"; ///< Unique interface name used for runtime lookup.

    /**
     * @brief Playback configuration provided by the device implementation.
     */
    struct PlayerConfig {
        uint8_t volume_default = 75; ///< Default output volume in percent.
        uint8_t volume_min = 0;      ///< Minimum supported output volume in percent.
        uint8_t volume_max = 100;    ///< Maximum supported output volume in percent.
    };

    /**
     * @brief Constructs an empty playback interface.
     */
    AudioPlayerIface() = default;

    /**
     * @brief Destroys the playback interface.
     */
    virtual ~AudioPlayerIface() = default;

    /**
     * @brief Returns the implementation-defined native playback handle.
     *
     * @return Opaque handle managed by the device implementation, or `nullptr` when unavailable.
     */
    const void *get_handle() const
    {
        return handle;
    }

    /**
     * @brief Returns the current playback configuration.
     *
     * @return Immutable reference to the playback configuration.
     */
    const PlayerConfig &get_config() const
    {
        return config;
    }

    /**
     * @brief Sets the playback volume.
     *
     * @param volume Requested output volume in percent.
     * @return true on success, otherwise false.
     */
    virtual bool set_volume(uint8_t volume) = 0;

    /**
     * @brief Returns the current playback volume.
     *
     * @return Current output volume in percent.
     */
    virtual uint8_t get_volume() const = 0;

    /**
     * @brief Opens or activates the playback path.
     *
     * @return true on success, otherwise false.
     */
    virtual bool open_player() = 0;

    /**
     * @brief Closes or deactivates the playback path.
     *
     * @return true on success, otherwise false.
     */
    virtual bool close_player() = 0;

protected:
    PlayerConfig config; ///< Playback configuration owned by the implementation.
    void *handle = nullptr; ///< Opaque implementation-defined playback handle.
};

BROOKESIA_DESCRIBE_STRUCT(AudioPlayerIface::PlayerConfig, (), (volume_default, volume_min, volume_max));

} // namespace esp_brookesia::hal
