/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

#include "brookesia/hal_adaptor/audio/device.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"

namespace esp_brookesia::hal {

class AudioProcessorCore {
public:
    AudioProcessorCore(
        std::shared_ptr<audio::CodecPlayerIface> player_iface,
        std::shared_ptr<audio::CodecRecorderIface> recorder_iface,
        AudioProcessorConfig config
    );
    ~AudioProcessorCore();

    bool acquire(audio::PlaybackIface::EventCallback callback = nullptr);
    void release();
    void clear_playback_callback();
    bool is_opened() const
    {
        return is_opened_;
    }

    bool play(const std::string &url);
    bool pause();
    bool resume();
    bool stop();

    std::vector<std::string> get_afe_wake_words();
    bool start_encoder(const audio::EncoderDynamicConfig &config, audio::EncoderIface::Callbacks callbacks);
    int read_encoded_data(uint8_t *data, size_t size);
    void stop_encoder();
    void pause_encoder();
    void resume_encoder();
    bool is_encoder_started() const
    {
        return is_encoder_started_;
    }
    bool is_encoder_paused() const
    {
        return is_encoder_paused_;
    }

    bool start_decoder(const audio::DecoderDynamicConfig &config);
    void stop_decoder();
    bool is_decoder_started() const
    {
        return is_decoder_started_;
    }
    bool feed_decoder_data(const uint8_t *data, size_t size);

private:
    bool open_common(audio::PlaybackIface::EventCallback callback);
    void close_common();
    void on_playback_event(uint8_t state);
    void on_recorder_event(void *event);
    void on_recorder_input_data(const uint8_t *data, size_t size);
    void emit_afe_event(audio::AfeEvent event);

    std::shared_ptr<audio::CodecPlayerIface> player_iface_;
    std::shared_ptr<audio::CodecRecorderIface> recorder_iface_;
    AudioProcessorConfig config_;

    mutable std::mutex mutex_;
    size_t open_ref_count_ = 0;
    audio::PlaybackIface::EventCallback playback_callback_;
    audio::EncoderIface::Callbacks encoder_callbacks_;

    void *playback_handle_ = nullptr;
    void *recorder_handle_ = nullptr;
    void *feeder_handle_ = nullptr;
    std::atomic_bool is_opened_ = false;
    std::atomic_bool is_encoder_started_ = false;
    std::atomic_bool is_encoder_paused_ = false;
    std::atomic_bool is_decoder_started_ = false;
    std::string afe_model_partition_label_;
    std::string afe_mn_language_;
};

class AudioPlaybackImpl: public audio::PlaybackIface {
public:
    explicit AudioPlaybackImpl(std::shared_ptr<AudioProcessorCore> core);
    ~AudioPlaybackImpl() override;

    bool open(EventCallback callback) override;
    void close() override;
    bool play(const std::string &url) override;
    bool pause() override;
    bool resume() override;
    bool stop() override;
    bool is_opened() const override
    {
        return is_opened_;
    }

private:
    std::shared_ptr<AudioProcessorCore> core_;
    std::atomic_bool is_opened_ = false;
};

class AudioEncoderImpl: public audio::EncoderIface {
public:
    explicit AudioEncoderImpl(std::shared_ptr<AudioProcessorCore> core);
    ~AudioEncoderImpl() override;

    std::vector<std::string> get_afe_wake_words() override;
    bool start(const audio::EncoderDynamicConfig &config, Callbacks callbacks) override;
    int read_encoded_data(uint8_t *data, size_t size) override;
    void stop() override;
    void pause() override;
    void resume() override;
    bool is_started() const override;
    bool is_paused() const override;

private:
    std::shared_ptr<AudioProcessorCore> core_;
};

class AudioDecoderImpl: public audio::DecoderIface {
public:
    explicit AudioDecoderImpl(std::shared_ptr<AudioProcessorCore> core);
    ~AudioDecoderImpl() override;

    bool start(const audio::DecoderDynamicConfig &config) override;
    void stop() override;
    bool is_started() const override;
    bool feed_data(const uint8_t *data, size_t size) override;

private:
    std::shared_ptr<AudioProcessorCore> core_;
};

} // namespace esp_brookesia::hal
