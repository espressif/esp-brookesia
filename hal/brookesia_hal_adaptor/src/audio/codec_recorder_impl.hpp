/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <optional>
#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed audio recording HAL interface (obtains codec handle from board manager).
 */
class AudioCodecRecorderImpl : public AudioCodecRecorderIface {
public:
    AudioCodecRecorderImpl(std::optional<AudioCodecRecorderIface::Info> info);
    ~AudioCodecRecorderImpl();

    bool open() override;
    void close() override;
    bool set_general_gain(float gain) override;
    bool set_channel_gains(const std::map<uint8_t, float> &gains) override;
    bool read_data(uint8_t *data, size_t size) override;

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_valid_internal();
    }

private:
    bool set_general_gain_internal(float gain);
    bool set_channel_gains_internal(const std::map<uint8_t, float> &gains);

    bool is_valid_internal() const
    {
        return handles_ != nullptr;
    }

    bool is_opened_internal() const
    {
        return is_opened_;
    }

    mutable boost::mutex mutex_;
    void *handles_ = nullptr;
    bool is_opened_ = false;
};

} // namespace esp_brookesia::hal
