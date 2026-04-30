/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_adaptor/macro_configs.h"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Board-backed audio playback HAL interface (obtains codec handle from board manager).
 */
class AudioCodecPlayerImpl : public AudioCodecPlayerIface {
public:
    AudioCodecPlayerImpl();
    ~AudioCodecPlayerImpl();

    bool open(const AudioCodecPlayerIface::Config &config) override;
    void close() override;
    bool set_volume(uint8_t volume) override;
    bool write_data(const uint8_t *data, size_t size) override;

    bool is_pa_on_off_supported() override
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_pa_control_valid_internal();
    }
    bool set_pa_on_off(bool on) override;
    bool is_pa_on() const override
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_pa_on_;
    }

    bool is_valid() const
    {
        boost::lock_guard<boost::mutex> lock(mutex_);
        return is_codec_valid_internal();
    }

private:
    bool setup_codec();
    bool setup_pa_control();

    bool set_volume_internal(uint8_t volume, bool force = false);
    bool set_pa_on_off_internal(bool on, bool force = false);

    bool is_codec_valid_internal() const
    {
        return codec_handles_ != nullptr;
    }

    bool is_codec_opened_internal() const
    {
        return is_codec_opened_;
    }

    bool is_pa_control_valid_internal() const
    {
        return pa_control_handle_ != nullptr;
    }

    mutable boost::mutex mutex_;

    void *codec_handles_ = nullptr;
    uint8_t volume_ = 0;
    bool is_codec_opened_ = false;

    void *pa_control_handle_ = nullptr;
    bool pa_control_active_level_ = true;
    bool is_pa_on_ = true;
};

} // namespace esp_brookesia::hal
