/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <cstddef>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "brookesia/hal_linux/macro_configs.h"

#if !BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/channel_layout.h"
#include "libavutil/error.h"
#include "libavutil/opt.h"
#include "libavutil/samplefmt.h"
#include "libswresample/swresample.h"
#include "portaudio.h"
}
#endif

#if !BROOKESIA_HAL_LINUX_AUDIO_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_player.hpp"
#include "brookesia/hal_interface/interfaces/audio/codec_recorder.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"
#include "brookesia/hal_linux/audio/device.hpp"

namespace esp_brookesia::hal {

namespace {

audio::CodecRecorderIface::Info make_recorder_info()
{
    return audio::CodecRecorderIface::Info{
        .bits = 16,
        .channels = 2,
        .sample_rate = 16000,
        .mic_layout = "LR",
        .general_gain = 1.0f,
        .channel_gains = {
            {0, 1.0f},
            {1, 1.0f},
        },
    };
}

class AudioOutputControl {
public:
    void set_volume(uint8_t volume)
    {
        std::lock_guard lock(mutex_);
        volume_ = std::min<uint8_t>(volume, 100);
    }

    bool set_pa_on_off(bool on)
    {
        std::lock_guard lock(mutex_);
        pa_on_ = on;
        return true;
    }

    bool is_pa_on() const
    {
        std::lock_guard lock(mutex_);
        return pa_on_;
    }

    float get_gain() const
    {
        std::lock_guard lock(mutex_);
        if (!pa_on_) {
            return 0.0f;
        }
        return static_cast<float>(volume_) / 100.0f;
    }

private:
    mutable std::mutex mutex_;
    uint8_t volume_ = 100;
    bool pa_on_ = true;
};

std::shared_ptr<AudioOutputControl> ensure_audio_output_control(std::shared_ptr<AudioOutputControl> output_control)
{
    return output_control != nullptr ? std::move(output_control) : std::make_shared<AudioOutputControl>();
}

} // namespace

class AudioCodecPlayerLinuxStub: public audio::CodecPlayerIface {
public:
    explicit AudioCodecPlayerLinuxStub(std::shared_ptr<AudioOutputControl> output_control)
        : output_control_(ensure_audio_output_control(std::move(output_control)))
    {
    }

    bool open(const Config &config) override
    {
        std::lock_guard lock(mutex_);
        if ((config.bits == 0) || (config.channels == 0) || (config.sample_rate == 0)) {
            BROOKESIA_LOGE(
                "Invalid player config: bits(%1%), channels(%2%), sample_rate(%3%)",
                config.bits, config.channels, config.sample_rate
            );
            return false;
        }

        config_ = config;
        is_opened_ = true;
        total_bytes_written_ = 0;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = false;
    }

    bool set_volume(uint8_t volume) override
    {
        output_control_->set_volume(volume);
        return true;
    }

    bool write_data(const uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if (data == nullptr) {
            BROOKESIA_LOGE("Player write data pointer is null");
            return false;
        }

        total_bytes_written_ += size;
        return true;
    }

    bool is_pa_on_off_supported() override
    {
        return true;
    }

    bool set_pa_on_off(bool on) override
    {
        return output_control_->set_pa_on_off(on);
    }

    bool is_pa_on() const override
    {
        return output_control_->is_pa_on();
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<AudioOutputControl> output_control_;
    Config config_{};
    bool is_opened_ = false;
    size_t total_bytes_written_ = 0;
};

class AudioCodecRecorderLinuxStub: public audio::CodecRecorderIface {
public:
    AudioCodecRecorderLinuxStub()
        : audio::CodecRecorderIface(make_recorder_info())
    {
    }

    bool open() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = true;
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = false;
    }

    bool read_data(uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if (data == nullptr) {
            BROOKESIA_LOGE("Recorder read data pointer is null");
            return false;
        }

        std::fill_n(data, size, static_cast<uint8_t>(0x5A));
        return true;
    }

    bool set_general_gain(float gain) override
    {
        std::lock_guard lock(mutex_);
        const_cast<Info &>(get_info()).general_gain = gain;
        return true;
    }

    bool set_channel_gains(const std::map<uint8_t, float> &gains) override
    {
        std::lock_guard lock(mutex_);
        const_cast<Info &>(get_info()).channel_gains = gains;
        return true;
    }

private:
    std::mutex mutex_;
    bool is_opened_ = false;
};

class AudioPlaybackLinuxStub: public audio::PlaybackIface {
public:
    bool open(EventCallback callback) override
    {
        {
            std::lock_guard lock(mutex_);
            callback_ = std::move(callback);
            state_ = audio::PlayState::Idle;
            is_opened_ = true;
        }
        notify(audio::PlayState::Idle);
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        is_opened_ = false;
        state_ = audio::PlayState::Idle;
        current_url_.clear();
        callback_ = nullptr;
    }

    bool play(const std::string &url) override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || url.empty()) {
                BROOKESIA_LOGE("Invalid play URL request: opened(%1%), url_empty(%2%)", is_opened_, url.empty());
                return false;
            }
            current_url_ = url;
            state_ = audio::PlayState::Playing;
        }
        notify(audio::PlayState::Playing);
        return true;
    }

    bool pause() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || (state_ != audio::PlayState::Playing)) {
                return false;
            }
            state_ = audio::PlayState::Paused;
        }
        notify(audio::PlayState::Paused);
        return true;
    }

    bool resume() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || (state_ != audio::PlayState::Paused)) {
                return false;
            }
            state_ = audio::PlayState::Playing;
        }
        notify(audio::PlayState::Playing);
        return true;
    }

    bool stop() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                return false;
            }
            state_ = audio::PlayState::Idle;
            current_url_.clear();
        }
        notify(audio::PlayState::Idle);
        return true;
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

private:
    void notify(audio::PlayState state)
    {
        EventCallback callback;
        {
            std::lock_guard lock(mutex_);
            callback = callback_;
        }
        if (callback) {
            callback(state);
        }
    }

    mutable std::mutex mutex_;
    EventCallback callback_;
    std::string current_url_;
    bool is_opened_ = false;
    audio::PlayState state_ = audio::PlayState::Idle;
};

class AudioEncoderLinuxStub: public audio::EncoderIface {
public:
    std::vector<std::string> get_afe_wake_words() override
    {
        return {};
    }

    bool start(const audio::EncoderDynamicConfig &config, Callbacks callbacks) override
    {
        if ((config.general.channels == 0) || (config.general.sample_bits == 0) || (config.general.sample_rate == 0)) {
            BROOKESIA_LOGE("Invalid encoder config");
            return false;
        }

        {
            std::lock_guard lock(mutex_);
            callbacks_ = std::move(callbacks);
            is_started_ = true;
            is_paused_ = false;
        }

        if (config.enable_afe) {
            notify_afe(audio::AfeEvent::WakeStart);
        }
        return true;
    }

    int read_encoded_data(uint8_t *data, size_t size) override
    {
        if ((data == nullptr) || (size == 0)) {
            return -1;
        }

        {
            std::lock_guard lock(mutex_);
            if (!is_started_) {
                return -1;
            }
            if (is_paused_) {
                return 0;
            }
        }

        notify_recorder_data();

        const std::array<uint8_t, 6> encoder_data = {0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5};
        if (size < encoder_data.size()) {
            return -1;
        }
        std::memcpy(data, encoder_data.data(), encoder_data.size());
        return static_cast<int>(encoder_data.size());
    }

    void stop() override
    {
        std::lock_guard lock(mutex_);
        is_started_ = false;
        is_paused_ = false;
        callbacks_ = {};
    }

    void pause() override
    {
        std::lock_guard lock(mutex_);
        if (is_started_) {
            is_paused_ = true;
        }
    }

    void resume() override
    {
        std::lock_guard lock(mutex_);
        if (is_started_) {
            is_paused_ = false;
        }
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

    bool is_paused() const override
    {
        std::lock_guard lock(mutex_);
        return is_paused_;
    }

private:
    void notify_afe(audio::AfeEvent event)
    {
        AfeEventCallback callback;
        {
            std::lock_guard lock(mutex_);
            callback = callbacks_.afe_event;
        }
        if (callback) {
            callback(event);
        }
    }

    void notify_recorder_data()
    {
        DataCallback recorder_callback;
        {
            std::lock_guard lock(mutex_);
            recorder_callback = callbacks_.recorder_data;
        }

        const std::array<uint8_t, 8> recorder_data = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17};
        if (recorder_callback) {
            recorder_callback(recorder_data.data(), recorder_data.size());
        }
    }

    mutable std::mutex mutex_;
    Callbacks callbacks_;
    bool is_started_ = false;
    bool is_paused_ = false;
};

class AudioDecoderLinuxStub: public audio::DecoderIface {
public:
    bool start(const audio::DecoderDynamicConfig &config) override
    {
        if ((config.general.channels == 0) || (config.general.sample_bits == 0) || (config.general.sample_rate == 0)) {
            BROOKESIA_LOGE("Invalid decoder config");
            return false;
        }

        std::lock_guard lock(mutex_);
        is_started_ = true;
        return true;
    }

    void stop() override
    {
        std::lock_guard lock(mutex_);
        is_started_ = false;
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

    bool feed_data(const uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        return is_started_ && (data != nullptr) && (size > 0);
    }

private:
    mutable std::mutex mutex_;
    bool is_started_ = false;
};

#if !BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
class PortAudioRuntime {
public:
    static bool acquire()
    {
        std::lock_guard lock(mutex_);
        if (ref_count_ == 0) {
            const PaError error = Pa_Initialize();
            if (error != paNoError) {
                BROOKESIA_LOGE("Failed to initialize PortAudio: %1%", Pa_GetErrorText(error));
                return false;
            }
        }
        ref_count_++;
        return true;
    }

    static void release()
    {
        std::lock_guard lock(mutex_);
        if (ref_count_ == 0) {
            return;
        }
        ref_count_--;
        if (ref_count_ == 0) {
            Pa_Terminate();
        }
    }

private:
    static inline std::mutex mutex_;
    static inline size_t ref_count_ = 0;
};

int16_t scale_pcm16_sample(int16_t sample, float gain)
{
    const int32_t scaled = static_cast<int32_t>(sample * gain);
    return static_cast<int16_t>(std::clamp<int32_t>(scaled, -32768, 32767));
}

void apply_pcm_float_gain(float *samples, size_t sample_count, float gain)
{
    if ((samples == nullptr) || (gain >= 0.999f && gain <= 1.001f)) {
        return;
    }
    for (size_t i = 0; i < sample_count; i++) {
        samples[i] = std::clamp(samples[i] * gain, -1.0f, 1.0f);
    }
}

class AudioCodecPlayerLinuxReal: public audio::CodecPlayerIface {
public:
    explicit AudioCodecPlayerLinuxReal(std::shared_ptr<AudioOutputControl> output_control)
        : output_control_(ensure_audio_output_control(std::move(output_control)))
    {
    }

    ~AudioCodecPlayerLinuxReal() override
    {
        close();
    }

    bool open(const Config &config) override
    {
        close();
        if ((config.bits != 16) || (config.channels == 0) || (config.sample_rate == 0)) {
            BROOKESIA_LOGE(
                "Unsupported player config: bits(%1%), channels(%2%), sample_rate(%3%)",
                config.bits, config.channels, config.sample_rate
            );
            return false;
        }
        if (!PortAudioRuntime::acquire()) {
            return false;
        }

        PaStream *stream = nullptr;
        const PaError error = Pa_OpenDefaultStream(
                                  &stream, 0, config.channels, paInt16, config.sample_rate,
                                  paFramesPerBufferUnspecified, nullptr, nullptr
                              );
        if (error != paNoError) {
            BROOKESIA_LOGE("Failed to open PortAudio output stream: %1%", Pa_GetErrorText(error));
            PortAudioRuntime::release();
            return false;
        }
        const PaError start_error = Pa_StartStream(stream);
        if (start_error != paNoError) {
            BROOKESIA_LOGE("Failed to start PortAudio output stream: %1%", Pa_GetErrorText(start_error));
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
            return false;
        }

        std::lock_guard lock(mutex_);
        stream_ = stream;
        config_ = config;
        is_opened_ = true;
        return true;
    }

    void close() override
    {
        PaStream *stream = nullptr;
        {
            std::lock_guard lock(mutex_);
            stream = stream_;
            stream_ = nullptr;
            is_opened_ = false;
        }
        if (stream != nullptr) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
        }
    }

    bool set_volume(uint8_t volume) override
    {
        output_control_->set_volume(volume);
        return true;
    }

    bool write_data(const uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if ((stream_ == nullptr) || (data == nullptr) || (config_.bits != 16) || (config_.channels == 0)) {
            return false;
        }
        const size_t bytes_per_frame = config_.channels * sizeof(int16_t);
        const size_t frames = size / bytes_per_frame;
        if (frames == 0) {
            return true;
        }
        const size_t sample_count = frames * config_.channels;
        std::vector<int16_t> scaled_samples(sample_count);
        const auto *samples = reinterpret_cast<const int16_t *>(data);
        const float gain = output_control_->get_gain();
        for (size_t i = 0; i < sample_count; i++) {
            scaled_samples[i] = scale_pcm16_sample(samples[i], gain);
        }
        const PaError error = Pa_WriteStream(stream_, scaled_samples.data(), static_cast<unsigned long>(frames));
        if (error == paOutputUnderflowed) {
            BROOKESIA_LOGW("PortAudio output underflowed, continuing playback");
            return true;
        }
        if (error != paNoError) {
            BROOKESIA_LOGE("Failed to write PortAudio output stream: %1%", Pa_GetErrorText(error));
            return false;
        }
        return true;
    }

    bool is_pa_on_off_supported() override
    {
        return true;
    }

    bool set_pa_on_off(bool on) override
    {
        return output_control_->set_pa_on_off(on);
    }

    bool is_pa_on() const override
    {
        return output_control_->is_pa_on();
    }

private:
    mutable std::mutex mutex_;
    std::shared_ptr<AudioOutputControl> output_control_;
    PaStream *stream_ = nullptr;
    Config config_{};
    bool is_opened_ = false;
};

class AudioCodecRecorderLinuxReal: public audio::CodecRecorderIface {
public:
    AudioCodecRecorderLinuxReal()
        : audio::CodecRecorderIface(make_recorder_info())
    {
    }

    ~AudioCodecRecorderLinuxReal() override
    {
        close();
    }

    bool open() override
    {
        close();
        if (!PortAudioRuntime::acquire()) {
            return false;
        }
        PaStream *stream = nullptr;
        const auto &info = get_info();
        const PaError error = Pa_OpenDefaultStream(
                                  &stream, info.channels, 0, paInt16, info.sample_rate,
                                  paFramesPerBufferUnspecified, nullptr, nullptr
                              );
        if (error != paNoError) {
            BROOKESIA_LOGE("Failed to open PortAudio input stream: %1%", Pa_GetErrorText(error));
            PortAudioRuntime::release();
            return false;
        }
        const PaError start_error = Pa_StartStream(stream);
        if (start_error != paNoError) {
            BROOKESIA_LOGE("Failed to start PortAudio input stream: %1%", Pa_GetErrorText(start_error));
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
            return false;
        }

        std::lock_guard lock(mutex_);
        stream_ = stream;
        is_opened_ = true;
        return true;
    }

    void close() override
    {
        PaStream *stream = nullptr;
        {
            std::lock_guard lock(mutex_);
            stream = stream_;
            stream_ = nullptr;
            is_opened_ = false;
        }
        if (stream != nullptr) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
        }
    }

    bool read_data(uint8_t *data, size_t size) override
    {
        std::lock_guard lock(mutex_);
        if ((stream_ == nullptr) || (data == nullptr)) {
            return false;
        }
        const auto &info = get_info();
        const size_t bytes_per_frame = info.channels * sizeof(int16_t);
        const size_t frames = size / bytes_per_frame;
        if (frames == 0) {
            return true;
        }
        const PaError error = Pa_ReadStream(stream_, data, static_cast<unsigned long>(frames));
        if ((error != paNoError) && (error != paInputOverflowed)) {
            BROOKESIA_LOGE("Failed to read PortAudio input stream: %1%", Pa_GetErrorText(error));
            return false;
        }
        return true;
    }

    bool set_general_gain(float gain) override
    {
        std::lock_guard lock(mutex_);
        const_cast<Info &>(get_info()).general_gain = gain;
        return true;
    }

    bool set_channel_gains(const std::map<uint8_t, float> &gains) override
    {
        std::lock_guard lock(mutex_);
        const_cast<Info &>(get_info()).channel_gains = gains;
        return true;
    }

private:
    std::mutex mutex_;
    PaStream *stream_ = nullptr;
    bool is_opened_ = false;
};

namespace {

std::string av_error_to_string(int error)
{
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer = {};
    av_strerror(error, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

audio::CodecFormat normalize_codec(audio::CodecFormat format)
{
    return format;
}

AVCodecID to_av_codec_id(audio::CodecFormat format)
{
    switch (normalize_codec(format)) {
    case audio::CodecFormat::OPUS:
        return AV_CODEC_ID_OPUS;
    case audio::CodecFormat::G711A:
        return AV_CODEC_ID_PCM_ALAW;
    case audio::CodecFormat::PCM:
    case audio::CodecFormat::Max:
    default:
        return AV_CODEC_ID_NONE;
    }
}

std::filesystem::path get_media_file_system_root()
{
    if (const char *fs_root = std::getenv("BROOKESIA_HAL_LINUX_FS_ROOT")) {
        return std::filesystem::path(fs_root);
    }
    return std::filesystem::temp_directory_path() / "esp-brookesia" / "hal_linux" / "fs";
}

std::string resolve_file_url(const std::string &url)
{
    std::string path = url;
    constexpr std::string_view file_scheme_double = "file://";
    constexpr std::string_view file_scheme_single = "file:";
    if (path.starts_with(file_scheme_double)) {
        path = path.substr(file_scheme_double.size());
        if (!path.starts_with('/')) {
            path = "/" + path;
        }
    } else if (path.starts_with(file_scheme_single)) {
        path = path.substr(file_scheme_single.size());
    }

    constexpr std::string_view littlefs_prefix = "/littlefs/";
    constexpr std::string_view spiffs_prefix = "/spiffs/";
    if (path.starts_with(littlefs_prefix) || path.starts_with(spiffs_prefix)) {
        return (get_media_file_system_root() / path.substr(1)).string();
    }
    return path;
}

int get_layout_channels(const AVChannelLayout &layout)
{
    return layout.nb_channels > 0 ? layout.nb_channels : 1;
}

void default_channel_layout(AVChannelLayout &layout, int channels)
{
    av_channel_layout_uninit(&layout);
    av_channel_layout_default(&layout, channels);
}

bool sample_format_supported(const AVCodec *codec, AVSampleFormat format)
{
    if ((codec == nullptr) || (codec->sample_fmts == nullptr)) {
        return true;
    }
    for (const AVSampleFormat *sample_format = codec->sample_fmts; *sample_format != AV_SAMPLE_FMT_NONE;
            sample_format++) {
        if (*sample_format == format) {
            return true;
        }
    }
    return false;
}

AVSampleFormat choose_sample_format(const AVCodec *codec)
{
    if (sample_format_supported(codec, AV_SAMPLE_FMT_S16)) {
        return AV_SAMPLE_FMT_S16;
    }
    if (sample_format_supported(codec, AV_SAMPLE_FMT_FLTP)) {
        return AV_SAMPLE_FMT_FLTP;
    }
    if (sample_format_supported(codec, AV_SAMPLE_FMT_FLT)) {
        return AV_SAMPLE_FMT_FLT;
    }
    return (codec != nullptr && codec->sample_fmts != nullptr) ? codec->sample_fmts[0] : AV_SAMPLE_FMT_S16;
}

} // namespace

class AudioOutputStream {
public:
    ~AudioOutputStream()
    {
        close();
    }

    bool open(int channels, int sample_rate, PaSampleFormat format)
    {
        close();
        if ((channels <= 0) || (sample_rate <= 0) || !PortAudioRuntime::acquire()) {
            return false;
        }
        PaStream *stream = nullptr;
        const PaError error = Pa_OpenDefaultStream(
                                  &stream, 0, channels, format, sample_rate,
                                  paFramesPerBufferUnspecified, nullptr, nullptr
                              );
        if (error != paNoError) {
            BROOKESIA_LOGE("Failed to open PortAudio output stream: %1%", Pa_GetErrorText(error));
            PortAudioRuntime::release();
            return false;
        }
        const PaError start_error = Pa_StartStream(stream);
        if (start_error != paNoError) {
            BROOKESIA_LOGE("Failed to start PortAudio output stream: %1%", Pa_GetErrorText(start_error));
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
            return false;
        }
        stream_ = stream;
        channels_ = channels;
        sample_rate_ = sample_rate;
        format_ = format;
        return true;
    }

    void close()
    {
        if (stream_ == nullptr) {
            return;
        }
        Pa_StopStream(stream_);
        Pa_CloseStream(stream_);
        stream_ = nullptr;
        PortAudioRuntime::release();
    }

    bool write(const void *data, size_t frames)
    {
        if ((stream_ == nullptr) || (data == nullptr) || (frames == 0)) {
            return stream_ != nullptr;
        }
        const PaError error = Pa_WriteStream(stream_, data, static_cast<unsigned long>(frames));
        if (error == paOutputUnderflowed) {
            BROOKESIA_LOGW("PortAudio output underflowed, continuing playback");
            return true;
        }
        if (error != paNoError) {
            BROOKESIA_LOGE("Failed to write PortAudio output stream: %1%", Pa_GetErrorText(error));
            return false;
        }
        return true;
    }

    bool is_opened() const
    {
        return stream_ != nullptr;
    }

    int channels() const
    {
        return channels_;
    }

    int sample_rate() const
    {
        return sample_rate_;
    }

    PaSampleFormat format() const
    {
        return format_;
    }

private:
    PaStream *stream_ = nullptr;
    int channels_ = 0;
    int sample_rate_ = 0;
    PaSampleFormat format_ = paFloat32;
};

class DecodedFrameWriter {
public:
    explicit DecodedFrameWriter(std::shared_ptr<AudioOutputControl> output_control = nullptr)
        : output_control_(ensure_audio_output_control(std::move(output_control)))
    {
    }

    ~DecodedFrameWriter()
    {
        close();
    }

    void close()
    {
        output_.close();
        if (swr_ != nullptr) {
            swr_free(&swr_);
        }
        av_channel_layout_uninit(&output_layout_);
        output_channels_ = 0;
        output_sample_rate_ = 0;
    }

    bool write(AVFrame *frame)
    {
        if (frame == nullptr) {
            return false;
        }
        const int channels = get_layout_channels(frame->ch_layout);
        const int sample_rate = frame->sample_rate > 0 ? frame->sample_rate : 16000;
        if (!ensure_output(frame, channels, sample_rate)) {
            return false;
        }

        const int max_samples = swr_get_out_samples(swr_, frame->nb_samples);
        if (max_samples <= 0) {
            return false;
        }
        std::vector<float> output(static_cast<size_t>(max_samples) * output_channels_);
        uint8_t *output_data = reinterpret_cast<uint8_t *>(output.data());
        const int converted_samples = swr_convert(
                                          swr_, &output_data, max_samples,
                                          const_cast<const uint8_t **>(frame->extended_data), frame->nb_samples
                                      );
        if (converted_samples < 0) {
            BROOKESIA_LOGE("Failed to convert decoded audio: %1%", av_error_to_string(converted_samples));
            return false;
        }
        apply_pcm_float_gain(output.data(), static_cast<size_t>(converted_samples) * output_channels_,
                             output_control_->get_gain());
        return output_.write(output.data(), static_cast<size_t>(converted_samples));
    }

private:
    bool ensure_output(AVFrame *frame, int channels, int sample_rate)
    {
        if (output_.is_opened() && (output_channels_ == channels) && (output_sample_rate_ == sample_rate)) {
            return true;
        }
        close();
        default_channel_layout(output_layout_, channels);
        AVChannelLayout input_layout = {};
        if (frame->ch_layout.nb_channels > 0) {
            av_channel_layout_copy(&input_layout, &frame->ch_layout);
        } else {
            av_channel_layout_default(&input_layout, channels);
        }
        const int swr_error = swr_alloc_set_opts2(
                                  &swr_, &output_layout_, AV_SAMPLE_FMT_FLT, sample_rate,
                                  &input_layout, static_cast<AVSampleFormat>(frame->format), sample_rate, 0, nullptr
                              );
        av_channel_layout_uninit(&input_layout);
        if (swr_error < 0 || swr_ == nullptr) {
            BROOKESIA_LOGE("Failed to allocate resampler: %1%", av_error_to_string(swr_error));
            return false;
        }
        const int init_error = swr_init(swr_);
        if (init_error < 0) {
            BROOKESIA_LOGE("Failed to initialize resampler: %1%", av_error_to_string(init_error));
            return false;
        }
        if (!output_.open(channels, sample_rate, paFloat32)) {
            return false;
        }
        output_channels_ = channels;
        output_sample_rate_ = sample_rate;
        return true;
    }

    AudioOutputStream output_;
    std::shared_ptr<AudioOutputControl> output_control_;
    SwrContext *swr_ = nullptr;
    AVChannelLayout output_layout_ = {};
    int output_channels_ = 0;
    int output_sample_rate_ = 0;
};

class AudioPlaybackLinuxReal: public audio::PlaybackIface {
public:
    explicit AudioPlaybackLinuxReal(std::shared_ptr<AudioOutputControl> output_control)
        : output_control_(ensure_audio_output_control(std::move(output_control)))
    {
    }

    ~AudioPlaybackLinuxReal() override
    {
        close();
    }

    bool open(EventCallback callback) override
    {
        {
            std::lock_guard lock(mutex_);
            callback_ = std::move(callback);
            state_ = audio::PlayState::Idle;
            is_opened_ = true;
        }
        notify(audio::PlayState::Idle);
        return true;
    }

    void close() override
    {
        stop();
        std::lock_guard lock(mutex_);
        callback_ = nullptr;
        is_opened_ = false;
    }

    bool play(const std::string &url) override
    {
        if (url.empty()) {
            return false;
        }
        stop_worker(false);
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                return false;
            }
            current_url_ = url;
            stop_requested_ = false;
            paused_ = false;
            state_ = audio::PlayState::Playing;
        }
        notify(audio::PlayState::Playing);
        worker_ = std::thread([this, url]() {
            run_playback(url);
        });
        return true;
    }

    bool pause() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || (state_ != audio::PlayState::Playing)) {
                return false;
            }
            paused_ = true;
            state_ = audio::PlayState::Paused;
        }
        notify(audio::PlayState::Paused);
        return true;
    }

    bool resume() override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || (state_ != audio::PlayState::Paused)) {
                return false;
            }
            paused_ = false;
            state_ = audio::PlayState::Playing;
        }
        cv_.notify_all();
        notify(audio::PlayState::Playing);
        return true;
    }

    bool stop() override
    {
        return stop_worker(true);
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

private:
    bool stop_worker(bool notify_idle)
    {
        {
            std::lock_guard lock(mutex_);
            stop_requested_ = true;
            paused_ = false;
        }
        cv_.notify_all();
        if (worker_.joinable()) {
            worker_.join();
        }
        {
            std::lock_guard lock(mutex_);
            current_url_.clear();
            state_ = audio::PlayState::Idle;
            stop_requested_ = false;
        }
        if (notify_idle) {
            notify(audio::PlayState::Idle);
        }
        return true;
    }

    bool wait_if_paused_or_stopped()
    {
        std::unique_lock lock(mutex_);
        cv_.wait(lock, [this]() {
            return stop_requested_ || !paused_;
        });
        return stop_requested_;
    }

    bool is_stop_requested() const
    {
        std::lock_guard lock(mutex_);
        return stop_requested_;
    }

    void run_playback(const std::string &url)
    {
        decode_and_play(url);
        bool should_notify_idle = false;
        {
            std::lock_guard lock(mutex_);
            should_notify_idle = !stop_requested_ && (state_ != audio::PlayState::Idle);
            if (should_notify_idle) {
                state_ = audio::PlayState::Idle;
            }
        }
        if (should_notify_idle) {
            notify(audio::PlayState::Idle);
        }
    }

    bool decode_and_play(const std::string &url)
    {
        const std::string path = resolve_file_url(url);
        AVFormatContext *format_context = nullptr;
        int error = avformat_open_input(&format_context, path.c_str(), nullptr, nullptr);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to open audio URL '%1%': %2%", url, av_error_to_string(error));
            return false;
        }
        auto format_guard = lib_utils::FunctionGuard([&format_context]() {
            avformat_close_input(&format_context);
        });

        error = avformat_find_stream_info(format_context, nullptr);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to read stream info: %1%", av_error_to_string(error));
            return false;
        }
        const int stream_index = av_find_best_stream(format_context, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (stream_index < 0) {
            BROOKESIA_LOGE("Audio stream not found in URL: %1%", url);
            return false;
        }

        AVStream *stream = format_context->streams[stream_index];
        const AVCodec *codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (codec == nullptr) {
            BROOKESIA_LOGE("Decoder not found for codec id: %1%", static_cast<int>(stream->codecpar->codec_id));
            return false;
        }
        AVCodecContext *codec_context = avcodec_alloc_context3(codec);
        if (codec_context == nullptr) {
            return false;
        }
        auto codec_guard = lib_utils::FunctionGuard([&codec_context]() {
            avcodec_free_context(&codec_context);
        });
        error = avcodec_parameters_to_context(codec_context, stream->codecpar);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to copy codec parameters: %1%", av_error_to_string(error));
            return false;
        }
        error = avcodec_open2(codec_context, codec, nullptr);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to open decoder: %1%", av_error_to_string(error));
            return false;
        }

        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        if ((packet == nullptr) || (frame == nullptr)) {
            av_packet_free(&packet);
            av_frame_free(&frame);
            return false;
        }
        auto packet_guard = lib_utils::FunctionGuard([&packet]() {
            av_packet_free(&packet);
        });
        auto frame_guard = lib_utils::FunctionGuard([&frame]() {
            av_frame_free(&frame);
        });

        DecodedFrameWriter writer(output_control_);
        while (!is_stop_requested() && av_read_frame(format_context, packet) >= 0) {
            if (packet->stream_index != stream_index) {
                av_packet_unref(packet);
                continue;
            }
            if (wait_if_paused_or_stopped()) {
                av_packet_unref(packet);
                break;
            }
            if (!send_packet_and_write(codec_context, packet, frame, writer)) {
                av_packet_unref(packet);
                break;
            }
            av_packet_unref(packet);
        }
        avcodec_send_packet(codec_context, nullptr);
        while (!is_stop_requested()) {
            error = avcodec_receive_frame(codec_context, frame);
            if (error == AVERROR_EOF || error == AVERROR(EAGAIN)) {
                break;
            }
            if (error < 0) {
                break;
            }
            writer.write(frame);
            av_frame_unref(frame);
        }
        return true;
    }

    static bool send_packet_and_write(
        AVCodecContext *codec_context, AVPacket *packet, AVFrame *frame, DecodedFrameWriter &writer
    )
    {
        int error = avcodec_send_packet(codec_context, packet);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to send packet to decoder: %1%", av_error_to_string(error));
            return false;
        }
        while (true) {
            error = avcodec_receive_frame(codec_context, frame);
            if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
                break;
            }
            if (error < 0) {
                BROOKESIA_LOGE("Failed to receive decoded frame: %1%", av_error_to_string(error));
                return false;
            }
            if (!writer.write(frame)) {
                av_frame_unref(frame);
                return false;
            }
            av_frame_unref(frame);
        }
        return true;
    }

    void notify(audio::PlayState state)
    {
        EventCallback callback;
        {
            std::lock_guard lock(mutex_);
            callback = callback_;
        }
        if (callback) {
            callback(state);
        }
    }

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::shared_ptr<AudioOutputControl> output_control_;
    EventCallback callback_;
    std::string current_url_;
    bool is_opened_ = false;
    bool paused_ = false;
    bool stop_requested_ = false;
    audio::PlayState state_ = audio::PlayState::Idle;
};

class AudioEncoderLinuxReal: public audio::EncoderIface {
public:
    ~AudioEncoderLinuxReal() override
    {
        stop();
    }

    std::vector<std::string> get_afe_wake_words() override
    {
        return {};
    }

    bool start(const audio::EncoderDynamicConfig &config, Callbacks callbacks) override
    {
        stop();
        if ((config.general.channels == 0) || (config.general.sample_bits != 16) ||
                (config.general.sample_rate == 0)) {
            BROOKESIA_LOGE("Invalid encoder config");
            return false;
        }
        if ((config.type != audio::CodecFormat::PCM) && !open_encoder(config)) {
            cleanup_encoder();
            return false;
        }
        if (!PortAudioRuntime::acquire()) {
            cleanup_encoder();
            return false;
        }

        const uint32_t bytes_per_frame = config.general.channels * sizeof(int16_t);
        const uint32_t requested_frames = std::max<uint32_t>(1, config.fetch_data_size / bytes_per_frame);
        frames_per_buffer_ = encoder_frame_size_ > 0 ? encoder_frame_size_ : requested_frames;

        PaStream *stream = nullptr;
        const PaError error = Pa_OpenDefaultStream(
                                  &stream, config.general.channels, 0, paInt16, config.general.sample_rate,
                                  frames_per_buffer_, nullptr, nullptr
                              );
        if (error != paNoError) {
            BROOKESIA_LOGE("Failed to open PortAudio input stream: %1%", Pa_GetErrorText(error));
            PortAudioRuntime::release();
            cleanup_encoder();
            return false;
        }
        const PaError start_error = Pa_StartStream(stream);
        if (start_error != paNoError) {
            BROOKESIA_LOGE("Failed to start PortAudio input stream: %1%", Pa_GetErrorText(start_error));
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
            cleanup_encoder();
            return false;
        }

        {
            std::lock_guard lock(mutex_);
            input_stream_ = stream;
            config_ = config;
            callbacks_ = std::move(callbacks);
            stop_requested_ = false;
            is_started_ = true;
            is_paused_ = false;
        }
        return true;
    }

    int read_encoded_data(uint8_t *data, size_t size) override
    {
        if ((data == nullptr) || (size == 0)) {
            return -1;
        }

        {
            std::lock_guard lock(mutex_);
            if (!is_started_) {
                return -1;
            }
            if (is_paused_ || stop_requested_) {
                return 0;
            }
            if (!encoded_packets_.empty()) {
                return pop_encoded_packet_locked(data, size);
            }
        }

        const size_t sample_count = static_cast<size_t>(frames_per_buffer_) * config_.general.channels;
        std::vector<int16_t> pcm(sample_count);
        PaStream *stream = nullptr;
        {
            std::lock_guard lock(mutex_);
            stream = input_stream_;
        }
        if (stream == nullptr) {
            return -1;
        }

        const PaError error = Pa_ReadStream(stream, pcm.data(), frames_per_buffer_);
        if ((error != paNoError) && (error != paInputOverflowed)) {
            BROOKESIA_LOGE("Failed to read PortAudio input stream: %1%", Pa_GetErrorText(error));
            return -1;
        }

        notify_recorder_data(reinterpret_cast<const uint8_t *>(pcm.data()), pcm.size() * sizeof(int16_t));
        if (config_.type == audio::CodecFormat::PCM) {
            const size_t data_size = pcm.size() * sizeof(int16_t);
            if (size < data_size) {
                return -1;
            }
            std::memcpy(data, pcm.data(), data_size);
            return static_cast<int>(data_size);
        }

        encode_and_queue(pcm, frames_per_buffer_);
        {
            std::lock_guard lock(mutex_);
            if (encoded_packets_.empty()) {
                return 0;
            }
            return pop_encoded_packet_locked(data, size);
        }
    }

    void stop() override
    {
        {
            std::lock_guard lock(mutex_);
            stop_requested_ = true;
            is_paused_ = false;
        }

        PaStream *stream = nullptr;
        {
            std::lock_guard lock(mutex_);
            stream = input_stream_;
            input_stream_ = nullptr;
            is_started_ = false;
            is_paused_ = false;
            callbacks_ = {};
            encoded_packets_.clear();
        }
        if (stream != nullptr) {
            Pa_StopStream(stream);
            Pa_CloseStream(stream);
            PortAudioRuntime::release();
        }
        cleanup_encoder();
    }

    void pause() override
    {
        std::lock_guard lock(mutex_);
        if (is_started_) {
            is_paused_ = true;
        }
    }

    void resume() override
    {
        std::lock_guard lock(mutex_);
        if (is_started_) {
            is_paused_ = false;
        }
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

    bool is_paused() const override
    {
        std::lock_guard lock(mutex_);
        return is_paused_;
    }

private:
    bool open_encoder(const audio::EncoderDynamicConfig &config)
    {
        const AVCodecID codec_id = to_av_codec_id(config.type);
        const AVCodec *codec = avcodec_find_encoder(codec_id);
        if (codec == nullptr) {
            BROOKESIA_LOGE("Encoder not found for codec id: %1%", static_cast<int>(codec_id));
            return false;
        }
        encoder_context_ = avcodec_alloc_context3(codec);
        if (encoder_context_ == nullptr) {
            return false;
        }
        encoder_context_->sample_rate = config.general.sample_rate;
        default_channel_layout(encoder_context_->ch_layout, config.general.channels);
        encoder_context_->sample_fmt = choose_sample_format(codec);
        encoder_context_->bit_rate = 24000;
        if (config.type == audio::CodecFormat::OPUS) {
            if (const auto *opus_extra = std::get_if<audio::EncoderExtraConfigOpus>(&config.extra)) {
                encoder_context_->bit_rate = opus_extra->bitrate;
            }
        }

        const int error = avcodec_open2(encoder_context_, codec, nullptr);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to open audio encoder: %1%", av_error_to_string(error));
            return false;
        }
        encoder_frame_size_ = encoder_context_->frame_size > 0 ? encoder_context_->frame_size : 320;
        default_channel_layout(input_layout_, config.general.channels);
        return true;
    }

    void cleanup_encoder()
    {
        if (swr_ != nullptr) {
            swr_free(&swr_);
        }
        if (encoder_context_ != nullptr) {
            avcodec_free_context(&encoder_context_);
        }
        av_channel_layout_uninit(&input_layout_);
        encoder_frame_size_ = 0;
    }

    bool ensure_swr()
    {
        if ((encoder_context_ == nullptr) || (encoder_context_->sample_fmt == AV_SAMPLE_FMT_S16)) {
            return true;
        }
        if (swr_ != nullptr) {
            return true;
        }
        const int error = swr_alloc_set_opts2(
                              &swr_, &encoder_context_->ch_layout, encoder_context_->sample_fmt,
                              encoder_context_->sample_rate, &input_layout_, AV_SAMPLE_FMT_S16,
                              encoder_context_->sample_rate, 0, nullptr
                          );
        if (error < 0 || swr_ == nullptr) {
            BROOKESIA_LOGE("Failed to allocate encoder resampler: %1%", av_error_to_string(error));
            return false;
        }
        const int init_error = swr_init(swr_);
        if (init_error < 0) {
            BROOKESIA_LOGE("Failed to initialize encoder resampler: %1%", av_error_to_string(init_error));
            return false;
        }
        return true;
    }

    void encode_and_queue(const std::vector<int16_t> &pcm, int frames)
    {
        if ((encoder_context_ == nullptr) || !ensure_swr()) {
            return;
        }
        AVFrame *frame = av_frame_alloc();
        if (frame == nullptr) {
            return;
        }
        auto frame_guard = lib_utils::FunctionGuard([&frame]() {
            av_frame_free(&frame);
        });

        frame->nb_samples = frames;
        frame->format = encoder_context_->sample_fmt;
        frame->sample_rate = encoder_context_->sample_rate;
        av_channel_layout_copy(&frame->ch_layout, &encoder_context_->ch_layout);
        int error = av_frame_get_buffer(frame, 0);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to allocate encoder frame: %1%", av_error_to_string(error));
            return;
        }
        error = av_frame_make_writable(frame);
        if (error < 0) {
            return;
        }

        if (encoder_context_->sample_fmt == AV_SAMPLE_FMT_S16) {
            std::memcpy(frame->data[0], pcm.data(), pcm.size() * sizeof(int16_t));
        } else {
            const uint8_t *input_data = reinterpret_cast<const uint8_t *>(pcm.data());
            const int converted = swr_convert(swr_, frame->data, frames, &input_data, frames);
            if (converted < 0) {
                BROOKESIA_LOGE("Failed to convert encoder input: %1%", av_error_to_string(converted));
                return;
            }
        }

        send_frame_and_queue(frame);
    }

    void send_frame_and_queue(AVFrame *frame)
    {
        int error = avcodec_send_frame(encoder_context_, frame);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to send audio encoder frame: %1%", av_error_to_string(error));
            return;
        }
        receive_packets_to_queue();
    }

    void flush_encoder()
    {
        if (encoder_context_ == nullptr) {
            return;
        }
        avcodec_send_frame(encoder_context_, nullptr);
        receive_packets_to_queue();
    }

    void receive_packets_to_queue()
    {
        AVPacket *packet = av_packet_alloc();
        if (packet == nullptr) {
            return;
        }
        auto packet_guard = lib_utils::FunctionGuard([&packet]() {
            av_packet_free(&packet);
        });
        while (true) {
            const int error = avcodec_receive_packet(encoder_context_, packet);
            if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
                break;
            }
            if (error < 0) {
                BROOKESIA_LOGE("Failed to receive audio encoder packet: %1%", av_error_to_string(error));
                break;
            }
            std::vector<uint8_t> packet_data(packet->size);
            std::memcpy(packet_data.data(), packet->data, packet_data.size());
            {
                std::lock_guard lock(mutex_);
                encoded_packets_.push_back(std::move(packet_data));
            }
            av_packet_unref(packet);
        }
    }

    void notify_recorder_data(const uint8_t *data, size_t size)
    {
        DataCallback callback;
        {
            std::lock_guard lock(mutex_);
            callback = callbacks_.recorder_data;
        }
        if (callback) {
            callback(data, size);
        }
    }

    int pop_encoded_packet_locked(uint8_t *data, size_t size)
    {
        auto packet = std::move(encoded_packets_.front());
        encoded_packets_.pop_front();
        if (size < packet.size()) {
            return -1;
        }
        std::memcpy(data, packet.data(), packet.size());
        return static_cast<int>(packet.size());
    }

    mutable std::mutex mutex_;
    Callbacks callbacks_;
    audio::EncoderDynamicConfig config_{};
    PaStream *input_stream_ = nullptr;
    AVCodecContext *encoder_context_ = nullptr;
    SwrContext *swr_ = nullptr;
    AVChannelLayout input_layout_ = {};
    std::deque<std::vector<uint8_t>> encoded_packets_;
    unsigned long frames_per_buffer_ = 0;
    int encoder_frame_size_ = 0;
    bool is_started_ = false;
    bool is_paused_ = false;
    bool stop_requested_ = false;
};

class AudioDecoderLinuxReal: public audio::DecoderIface {
public:
    ~AudioDecoderLinuxReal() override
    {
        stop();
    }

    bool start(const audio::DecoderDynamicConfig &config) override
    {
        stop();
        if ((config.general.channels == 0) || (config.general.sample_bits != 16) ||
                (config.general.sample_rate == 0)) {
            return false;
        }
        config_ = config;
        if (config.type == audio::CodecFormat::PCM) {
            if (!pcm_output_.open(config.general.channels, config.general.sample_rate, paInt16)) {
                return false;
            }
        } else if (!open_decoder(config.type)) {
            return false;
        }
        is_started_ = true;
        return true;
    }

    void stop() override
    {
        is_started_ = false;
        pcm_output_.close();
        decoded_writer_.close();
        if (decoder_context_ != nullptr) {
            avcodec_free_context(&decoder_context_);
        }
    }

    bool is_started() const override
    {
        return is_started_;
    }

    bool feed_data(const uint8_t *data, size_t size) override
    {
        if (!is_started_ || (data == nullptr) || (size == 0)) {
            return false;
        }
        if (config_.type == audio::CodecFormat::PCM) {
            const size_t bytes_per_frame = config_.general.channels * sizeof(int16_t);
            return pcm_output_.write(data, size / bytes_per_frame);
        }
        AVPacket *packet = av_packet_alloc();
        AVFrame *frame = av_frame_alloc();
        if ((packet == nullptr) || (frame == nullptr)) {
            av_packet_free(&packet);
            av_frame_free(&frame);
            return false;
        }
        auto packet_guard = lib_utils::FunctionGuard([&packet]() {
            av_packet_free(&packet);
        });
        auto frame_guard = lib_utils::FunctionGuard([&frame]() {
            av_frame_free(&frame);
        });
        packet->data = const_cast<uint8_t *>(data);
        packet->size = static_cast<int>(size);
        int error = avcodec_send_packet(decoder_context_, packet);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to send audio decoder packet: %1%", av_error_to_string(error));
            return false;
        }
        while (true) {
            error = avcodec_receive_frame(decoder_context_, frame);
            if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
                break;
            }
            if (error < 0) {
                BROOKESIA_LOGE("Failed to receive audio decoder frame: %1%", av_error_to_string(error));
                return false;
            }
            if (!decoded_writer_.write(frame)) {
                av_frame_unref(frame);
                return false;
            }
            av_frame_unref(frame);
        }
        return true;
    }

private:
    bool open_decoder(audio::CodecFormat format)
    {
        const AVCodecID codec_id = to_av_codec_id(format);
        const AVCodec *codec = avcodec_find_decoder(codec_id);
        if (codec == nullptr) {
            BROOKESIA_LOGE("Decoder not found for codec id: %1%", static_cast<int>(codec_id));
            return false;
        }
        decoder_context_ = avcodec_alloc_context3(codec);
        if (decoder_context_ == nullptr) {
            return false;
        }
        decoder_context_->sample_rate = config_.general.sample_rate;
        default_channel_layout(decoder_context_->ch_layout, config_.general.channels);
        const int error = avcodec_open2(decoder_context_, codec, nullptr);
        if (error < 0) {
            BROOKESIA_LOGE("Failed to open audio decoder: %1%", av_error_to_string(error));
            return false;
        }
        return true;
    }

    audio::DecoderDynamicConfig config_{};
    AudioOutputStream pcm_output_;
    DecodedFrameWriter decoded_writer_;
    AVCodecContext *decoder_context_ = nullptr;
    bool is_started_ = false;
};
#endif // !BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB

std::string AudioLinuxDevice::get_playback_iface_name()
{
    return audio::PlaybackIface::get_default_instance_name();
}

std::string AudioLinuxDevice::get_encoder_iface_name(size_t id)
{
    return audio::EncoderIface::get_default_instance_name(id);
}

std::string AudioLinuxDevice::get_decoder_iface_name(size_t id)
{
    return audio::DecoderIface::get_default_instance_name(id);
}

bool AudioLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> AudioLinuxDevice::get_interface_specs() const
{
    return {
        {audio::CodecPlayerIface::NAME, PLAYER_IFACE_NAME},
        {audio::CodecRecorderIface::NAME, RECORDER_IFACE_NAME},
        {audio::PlaybackIface::NAME, get_playback_iface_name()},
        {audio::EncoderIface::NAME, get_encoder_iface_name(0)},
        {audio::DecoderIface::NAME, get_decoder_iface_name(0)},
    };
}

bool AudioLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto output_control = std::make_shared<AudioOutputControl>();
    auto make_player_iface = [&output_control]() -> std::shared_ptr<audio::CodecPlayerIface> {
#if BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
        return std::make_shared<AudioCodecPlayerLinuxStub>(output_control);
#else
        return std::make_shared<AudioCodecPlayerLinuxReal>(output_control);
#endif
    };
    auto make_recorder_iface = []() -> std::shared_ptr<audio::CodecRecorderIface> {
#if BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
        return std::make_shared<AudioCodecRecorderLinuxStub>();
#else
        return std::make_shared<AudioCodecRecorderLinuxReal>();
#endif
    };
    auto make_playback_iface = [&output_control]() -> std::shared_ptr<audio::PlaybackIface> {
#if BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
        return std::make_shared<AudioPlaybackLinuxStub>();
#else
        return std::make_shared<AudioPlaybackLinuxReal>(output_control);
#endif
    };
    auto make_encoder_iface = []() -> std::shared_ptr<audio::EncoderIface> {
#if BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
        return std::make_shared<AudioEncoderLinuxStub>();
#else
        return std::make_shared<AudioEncoderLinuxReal>();
#endif
    };
    auto make_decoder_iface = []() -> std::shared_ptr<audio::DecoderIface> {
#if BROOKESIA_HAL_LINUX_MEDIA_BACKEND_STUB
        return std::make_shared<AudioDecoderLinuxStub>();
#else
        return std::make_shared<AudioDecoderLinuxReal>();
#endif
    };
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        player_iface_ = make_player_iface(), false, "Failed to create audio player linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        recorder_iface_ = make_recorder_iface(), false, "Failed to create audio recorder linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        playback_iface_ = make_playback_iface(), false, "Failed to create audio playback linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        encoder_iface_ = make_encoder_iface(), false, "Failed to create audio encoder linux"
    );
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        decoder_iface_ = make_decoder_iface(), false, "Failed to create audio decoder linux"
    );

    interfaces_.emplace(PLAYER_IFACE_NAME, player_iface_);
    interfaces_.emplace(RECORDER_IFACE_NAME, recorder_iface_);
    interfaces_.emplace(get_playback_iface_name(), playback_iface_);
    interfaces_.emplace(get_encoder_iface_name(0), encoder_iface_);
    interfaces_.emplace(get_decoder_iface_name(0), decoder_iface_);

    return true;
}

void AudioLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(get_decoder_iface_name(0));
    interfaces_.erase(get_encoder_iface_name(0));
    interfaces_.erase(get_playback_iface_name());
    interfaces_.erase(RECORDER_IFACE_NAME);
    interfaces_.erase(PLAYER_IFACE_NAME);
    decoder_iface_.reset();
    encoder_iface_.reset();
    playback_iface_.reset();
    recorder_iface_.reset();
    player_iface_.reset();
}

#if BROOKESIA_HAL_LINUX_ENABLE_AUDIO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, AudioLinuxDevice, AudioLinuxDevice::DEVICE_NAME, AudioLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_AUDIO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
