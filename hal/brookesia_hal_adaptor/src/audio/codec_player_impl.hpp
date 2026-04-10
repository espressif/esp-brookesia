/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <optional>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/audio/codec_player.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed audio playback HAL interface (obtains codec handle from board manager).
 */
class AudioCodecPlayerImpl : public AudioCodecPlayerIface {
public:
    AudioCodecPlayerImpl(std::optional<AudioCodecPlayerIface::Info> info);
    ~AudioCodecPlayerImpl();

    bool open(const AudioCodecPlayerIface::Config &config) override;
    void close() override;
    bool set_volume(uint8_t volume) override;
    bool get_volume(uint8_t &volume) override;
    bool mute() override;
    bool unmute() override;
    bool write_data(const uint8_t *data, size_t size) override;

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_valid_internal();
    }

private:
    bool is_valid_internal() const
    {
        return handles_ != nullptr;
    }

    bool is_opened_internal() const
    {
        return is_opened_;
    }

    bool set_volume_internal(uint8_t volume, bool need_map = true);

    bool get_volume_internal(uint8_t &volume) const;

    bool mute_internal();

    mutable boost::mutex mutex_;
    void *handles_ = nullptr;
    bool is_opened_ = false;
    uint8_t volume_ = std::numeric_limits<uint8_t>::max();             // Initialized to max to indicate not set
    uint8_t volume_before_mute_ = 0; // external volume saved before mute()
};

} // namespace esp_brookesia::hal
