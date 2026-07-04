/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "brookesia/hal_linux/macro_configs.h"

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavdevice/avdevice.h"
#include "libavformat/avformat.h"
#include "libavutil/error.h"
#include "libavutil/imgutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"
#include "libavutil/time.h"
#include "libswscale/swscale.h"
}
#endif

#if !BROOKESIA_HAL_LINUX_VIDEO_DEVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"
#include "brookesia/hal_linux/video/device.hpp"

namespace esp_brookesia::hal {

namespace {

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
constexpr uint8_t V4L2_BUFFER_COUNT_DEFAULT = 2;
constexpr uint32_t REAL_FRAME_DRAIN_PACKET_LIMIT = 120;
constexpr size_t V4L2_RT_BUFFER_MIN_BYTES = 1024 * 1024;
#endif

bool validate_encoder_config(const video::EncoderConfig &config, std::string *error_message)
{
    if (config.sinks.empty()) {
        if (error_message != nullptr) {
            *error_message = "Encoder sink list is empty";
        }
        return false;
    }

    const auto invalid_sink = std::find_if(config.sinks.begin(), config.sinks.end(), [](const auto & sink) {
        return (sink.format == video::EncoderSinkFormat::Max) || (sink.width == 0) || (sink.height == 0) ||
               (sink.fps == 0);
    });
    if (invalid_sink != config.sinks.end()) {
        if (error_message != nullptr) {
            *error_message = "Encoder sink configuration is invalid";
        }
        return false;
    }
    if (config.enable_stream_mode && config.sinks.size() != 1) {
        if (error_message != nullptr) {
            *error_message = "Linux encoder stream mode supports exactly one sink";
        }
        return false;
    }
    if (config.source && config.source->v4l2_buffer_count && (config.source->v4l2_buffer_count.value() == 0)) {
        if (error_message != nullptr) {
            *error_message = "V4L2 buffer count must be greater than 0";
        }
        return false;
    }

    if (error_message != nullptr) {
        error_message->clear();
    }
    return true;
}

bool validate_decoder_config(const video::DecoderConfig &config, std::string *error_message)
{
    if ((config.width == 0) || (config.height == 0) || (config.source_format == video::DecoderSourceFormat::Max) ||
            (config.sink_format == video::DecoderSinkFormat::Max)) {
        if (error_message != nullptr) {
            *error_message = "Decoder configuration is invalid";
        }
        return false;
    }

    if (error_message != nullptr) {
        error_message->clear();
    }
    return true;
}

void set_error(std::string *error_message, const std::string &message)
{
    if (error_message != nullptr) {
        *error_message = message;
    }
}

std::vector<uint8_t> make_stub_encoder_frame(size_t sink_index, const video::EncoderSinkInfo &sink_info)
{
    static std::atomic<uint8_t> frame_counter {0};
    const uint8_t phase = frame_counter.fetch_add(1);

    const size_t pixel_count = static_cast<size_t>(sink_info.width) * static_cast<size_t>(sink_info.height);
    std::vector<uint8_t> frame;
    if (sink_info.format == video::EncoderSinkFormat::RGB565) {
        frame.resize(pixel_count * 2);
        for (uint16_t y = 0; y < sink_info.height; ++y) {
            for (uint16_t x = 0; x < sink_info.width; ++x) {
                const uint8_t r = static_cast<uint8_t>((x + phase * 3) & 0xFF);
                const uint8_t g = static_cast<uint8_t>((y + phase * 5) & 0xFF);
                const uint8_t b = static_cast<uint8_t>((x + y + phase * 7) & 0xFF);
                const uint16_t pixel = static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
                const size_t offset = (static_cast<size_t>(y) * sink_info.width + x) * 2;
                frame[offset] = static_cast<uint8_t>(pixel & 0xFF);
                frame[offset + 1] = static_cast<uint8_t>(pixel >> 8);
            }
        }
        return frame;
    }
    if (sink_info.format == video::EncoderSinkFormat::RGB888) {
        frame.resize(pixel_count * 3);
        for (uint16_t y = 0; y < sink_info.height; ++y) {
            for (uint16_t x = 0; x < sink_info.width; ++x) {
                const size_t offset = (static_cast<size_t>(y) * sink_info.width + x) * 3;
                frame[offset] = static_cast<uint8_t>((x + phase * 3) & 0xFF);
                frame[offset + 1] = static_cast<uint8_t>((y + phase * 5) & 0xFF);
                frame[offset + 2] = static_cast<uint8_t>((x + y + phase * 7) & 0xFF);
            }
        }
        return frame;
    }

    frame.resize(16);
    std::fill(frame.begin(), frame.end(), static_cast<uint8_t>(0x60 + sink_index));
    return frame;
}

void emit_stub_decoder_frame(const video::DecoderConfig &config, const video::DecoderIface::FrameCallback &callback)
{
    if (!callback) {
        return;
    }
    std::array<uint8_t, 16> decoded_frame = {};
    std::fill(decoded_frame.begin(), decoded_frame.end(), 0xD0);
    callback(config.width, config.height, decoded_frame.data(), decoded_frame.size());
}

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
void ensure_avdevice_registered()
{
    static std::once_flag once;

    std::call_once(once, []() {
        avdevice_register_all();
    });
}

std::string av_error_to_string(int error)
{
    std::array<char, AV_ERROR_MAX_STRING_SIZE> buffer = {};
    av_strerror(error, buffer.data(), buffer.size());
    return std::string(buffer.data());
}

std::string av_frame_rate_to_string(AVRational frame_rate)
{
    if ((frame_rate.num <= 0) || (frame_rate.den <= 0)) {
        return "unknown";
    }
    if ((frame_rate.num % frame_rate.den) == 0) {
        return std::to_string(frame_rate.num / frame_rate.den);
    }
    return std::to_string(frame_rate.num) + "/" + std::to_string(frame_rate.den);
}

bool is_ffmpeg_again(int error)
{
    return error == AVERROR(EAGAIN);
}

size_t bytes_per_encoder_pixel(video::EncoderSinkFormat format)
{
    switch (format) {
    case video::EncoderSinkFormat::RGB565:
    case video::EncoderSinkFormat::YUV422:
    case video::EncoderSinkFormat::O_UYY_E_VYY:
        return 2;
    case video::EncoderSinkFormat::RGB888:
    case video::EncoderSinkFormat::BGR888:
        return 3;
    case video::EncoderSinkFormat::YUV420:
        return 2;
    default:
        return 4;
    }
}

uint8_t get_v4l2_buffer_count(const std::optional<video::EncoderSourceConfig> &source)
{
    if (!source || !source->v4l2_buffer_count) {
        return V4L2_BUFFER_COUNT_DEFAULT;
    }
    return std::max<uint8_t>(1, source->v4l2_buffer_count.value());
}

size_t estimate_rtbufsize_bytes(const video::EncoderSinkInfo &sink_info, uint8_t buffer_count)
{
    const size_t frame_bytes = static_cast<size_t>(sink_info.width) * sink_info.height *
                               bytes_per_encoder_pixel(sink_info.format);
    return std::max(V4L2_RT_BUFFER_MIN_BYTES, frame_bytes * buffer_count);
}

uint32_t frame_interval_ms(uint8_t fps)
{
    return std::max<uint32_t>(1, 1000U / std::max<uint32_t>(1, fps));
}

AVPixelFormat to_av_pixel_format(video::EncoderSinkFormat format)
{
    switch (format) {
    case video::EncoderSinkFormat::RGB565:
        return AV_PIX_FMT_RGB565LE;
    case video::EncoderSinkFormat::RGB888:
        return AV_PIX_FMT_RGB24;
    case video::EncoderSinkFormat::BGR888:
        return AV_PIX_FMT_BGR24;
    case video::EncoderSinkFormat::YUV420:
        return AV_PIX_FMT_YUV420P;
    case video::EncoderSinkFormat::YUV422:
        return AV_PIX_FMT_YUYV422;
    case video::EncoderSinkFormat::O_UYY_E_VYY:
        return AV_PIX_FMT_UYVY422;
    default:
        return AV_PIX_FMT_NONE;
    }
}

AVPixelFormat to_av_pixel_format(video::DecoderSinkFormat format)
{
    switch (format) {
    case video::DecoderSinkFormat::RGB565_LE:
        return AV_PIX_FMT_RGB565LE;
    case video::DecoderSinkFormat::RGB565_BE:
        return AV_PIX_FMT_RGB565BE;
    case video::DecoderSinkFormat::RGB888:
        return AV_PIX_FMT_RGB24;
    case video::DecoderSinkFormat::BGR888:
        return AV_PIX_FMT_BGR24;
    case video::DecoderSinkFormat::YUV420P:
        return AV_PIX_FMT_YUV420P;
    case video::DecoderSinkFormat::YUV422P:
        return AV_PIX_FMT_YUV422P;
    case video::DecoderSinkFormat::YUV422:
        return AV_PIX_FMT_YUYV422;
    case video::DecoderSinkFormat::UYVY422:
    case video::DecoderSinkFormat::O_UYY_E_VYY:
        return AV_PIX_FMT_UYVY422;
    default:
        return AV_PIX_FMT_NONE;
    }
}

AVCodecID to_av_codec_id(video::DecoderSourceFormat format)
{
    switch (format) {
    case video::DecoderSourceFormat::MJPEG:
        return AV_CODEC_ID_MJPEG;
    case video::DecoderSourceFormat::H264:
        return AV_CODEC_ID_H264;
    default:
        return AV_CODEC_ID_NONE;
    }
}

bool convert_frame(
    AVFrame *source_frame, AVPixelFormat destination_format, uint16_t width, uint16_t height,
    std::vector<uint8_t> &output
)
{
    if ((source_frame == nullptr) || (destination_format == AV_PIX_FMT_NONE) || (width == 0) || (height == 0)) {
        return false;
    }

    const auto source_format = static_cast<AVPixelFormat>(source_frame->format);
    const int output_size = av_image_get_buffer_size(destination_format, width, height, 1);
    if (output_size <= 0) {
        BROOKESIA_LOGE("Failed to get converted video frame buffer size");
        return false;
    }
    output.resize(static_cast<size_t>(output_size));

    uint8_t *destination_data[4] = {};
    int destination_linesize[4] = {};
    int error = av_image_fill_arrays(
                    destination_data, destination_linesize, output.data(), destination_format, width, height, 1
                );
    if (error < 0) {
        BROOKESIA_LOGE("Failed to prepare converted video frame: %1%", av_error_to_string(error));
        return false;
    }

    SwsContext *sws = sws_getContext(
                          source_frame->width, source_frame->height, source_format, width, height, destination_format,
                          SWS_BILINEAR, nullptr, nullptr, nullptr
                      );
    if (sws == nullptr) {
        BROOKESIA_LOGE("Failed to create video scaler");
        return false;
    }
    sws_scale(sws, source_frame->data, source_frame->linesize, 0, source_frame->height, destination_data,
              destination_linesize);
    sws_freeContext(sws);
    return true;
}

class VideoFrameConverter {
public:
    VideoFrameConverter() = default;

    ~VideoFrameConverter()
    {
        reset();
    }

    VideoFrameConverter(const VideoFrameConverter &) = delete;
    VideoFrameConverter &operator=(const VideoFrameConverter &) = delete;

    VideoFrameConverter(VideoFrameConverter &&other) noexcept
    {
        move_from(other);
    }

    VideoFrameConverter &operator=(VideoFrameConverter &&other) noexcept
    {
        if (this != &other) {
            reset();
            move_from(other);
        }
        return *this;
    }

    bool convert(
        AVFrame *source_frame, AVPixelFormat destination_format, uint16_t width, uint16_t height,
        std::string *error_message
    )
    {
        if ((source_frame == nullptr) || (destination_format == AV_PIX_FMT_NONE) || (width == 0) || (height == 0)) {
            set_error(error_message, "Invalid video frame conversion parameters");
            return false;
        }

        const auto source_format = static_cast<AVPixelFormat>(source_frame->format);
        const int output_size = av_image_get_buffer_size(destination_format, width, height, 1);
        if (output_size <= 0) {
            set_error(error_message, "Failed to get converted video frame buffer size");
            return false;
        }
        output_.resize(static_cast<size_t>(output_size));

        uint8_t *destination_data[4] = {};
        int destination_linesize[4] = {};
        int error = av_image_fill_arrays(
                        destination_data, destination_linesize, output_.data(), destination_format, width, height, 1
                    );
        if (error < 0) {
            set_error(error_message, "Failed to prepare converted video frame: " + av_error_to_string(error));
            return false;
        }

        SwsContext *next_sws = sws_getCachedContext(
                                   sws_,
                                   source_frame->width,
                                   source_frame->height,
                                   source_format,
                                   width,
                                   height,
                                   destination_format,
                                   SWS_FAST_BILINEAR,
                                   nullptr,
                                   nullptr,
                                   nullptr
                               );
        if (next_sws == nullptr) {
            set_error(error_message, "Failed to create video scaler");
            return false;
        }
        sws_ = next_sws;
        sws_scale(sws_, source_frame->data, source_frame->linesize, 0, source_frame->height, destination_data,
                  destination_linesize);
        return true;
    }

    const uint8_t *data() const
    {
        return output_.data();
    }

    size_t size() const
    {
        return output_.size();
    }

    void reset()
    {
        if (sws_ != nullptr) {
            sws_freeContext(sws_);
            sws_ = nullptr;
        }
        output_.clear();
    }

private:
    void move_from(VideoFrameConverter &other)
    {
        sws_ = other.sws_;
        output_ = std::move(other.output_);
        other.sws_ = nullptr;
    }

    SwsContext *sws_ = nullptr;
    std::vector<uint8_t> output_;
};

std::string get_default_video_device_path()
{
    constexpr const char *fallback_device = "/dev/video0";
    std::error_code error;
    if (std::filesystem::exists(fallback_device, error)) {
        return fallback_device;
    }
    for (const auto &entry : std::filesystem::directory_iterator("/dev", error)) {
        if (error) {
            break;
        }
        const auto name = entry.path().filename().string();
        if (name.starts_with("video")) {
            return entry.path().string();
        }
    }
    return fallback_device;
}
#endif

} // namespace

class VideoEncoderLinuxImpl: public video::EncoderIface {
public:
    ~VideoEncoderLinuxImpl() override
    {
        close();
    }

    bool open(const video::EncoderConfig &config, FrameCallback callback, std::string *error_message) override
    {
        if (!validate_encoder_config(config, error_message)) {
            return false;
        }

        stop_stream_worker();
        std::lock_guard frame_lock(frame_mutex_);
        std::lock_guard lock(mutex_);
        close_real_locked();
        config_ = config;
        callback_ = std::move(callback);
        is_opened_ = true;
        is_started_ = false;
        real_started_ = false;
        reset_frame_buffers_locked();
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
        converters_.clear();
        converters_.resize(config_.sinks.size());
#endif
        return true;
    }

    void close() override
    {
        if (!stream_thread_.joinable()) {
            std::lock_guard lock(mutex_);
            if (is_closed_locked()) {
                return;
            }
        }

        stop_stream_worker();
        std::lock_guard frame_lock(frame_mutex_);
        std::lock_guard lock(mutex_);
        if (is_closed_locked()) {
            return;
        }
        close_real_locked();
        is_opened_ = false;
        is_started_ = false;
        callback_ = nullptr;
        config_ = {};
        reset_frame_buffers_locked();
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
        converters_.clear();
#endif
    }

    bool start(std::string *error_message) override
    {
        bool should_start_stream = false;
        {
            std::lock_guard frame_lock(frame_mutex_);
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                set_error(error_message, "Encoder is not opened");
                return false;
            }
            if (is_started_) {
                if (error_message != nullptr) {
                    error_message->clear();
                }
                return true;
            }

            close_real_locked();
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
            real_frame_warning_count_ = 0;
            real_frame_drain_warning_count_ = 0;
#endif
            real_started_ = start_real_locked();
            if (!real_started_) {
                BROOKESIA_LOGW("Using video encoder stub fallback");
            }
            is_started_ = true;
            stream_stop_requested_.store(false);
            should_start_stream = config_.enable_stream_mode;
        }

        if (should_start_stream && !start_stream_worker(error_message)) {
            std::lock_guard frame_lock(frame_mutex_);
            std::lock_guard lock(mutex_);
            close_real_locked();
            is_started_ = false;
            return false;
        }

        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool stop(std::string *error_message) override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_) {
                set_error(error_message, "Encoder is not opened");
                return false;
            }
            if (!is_started_) {
                if (error_message != nullptr) {
                    error_message->clear();
                }
                return true;
            }
        }

        stop_stream_worker();
        {
            std::lock_guard frame_lock(frame_mutex_);
            std::lock_guard lock(mutex_);
            close_real_locked();
            is_started_ = false;
        }

        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message) override
    {
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || !is_started_) {
                set_error(error_message, "Encoder is not started");
                return false;
            }
            if (config_.enable_stream_mode) {
                set_error(error_message, "This function is not available in stream mode");
                return false;
            }
            if (sink_index >= config_.sinks.size()) {
                set_error(error_message, "Encoder sink index is out of range");
                return false;
            }
        }

        return emit_encoder_frame(sink_index, std::move(callback), error_message);
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

private:
    bool emit_encoder_frame(
        size_t sink_index, FrameCallback callback, std::string *error_message,
        video::EncoderSinkInfo *emitted_sink_info = nullptr
    )
    {
        std::lock_guard frame_lock(frame_mutex_);

        FrameCallback active_callback;
        video::EncoderSinkInfo sink_info = {};
        const uint8_t *data = nullptr;
        size_t size = 0;
        bool use_stub = false;
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || !is_started_) {
                set_error(error_message, "Encoder is not started");
                return false;
            }
            if (sink_index >= config_.sinks.size()) {
                set_error(error_message, "Encoder sink index is out of range");
                return false;
            }

            active_callback = callback ? std::move(callback) : callback_;
            if (!active_callback) {
                set_error(error_message, "Encoder frame callback is empty");
                return false;
            }
            sink_info = config_.sinks[sink_index];
            use_stub = !real_started_;
        }

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
        if (!use_stub) {
            std::string real_error_message;
            if (read_real_frame_locked(sink_index, sink_info, data, size, &real_error_message)) {
                if (error_message != nullptr) {
                    error_message->clear();
                }
            } else {
                log_real_frame_warning_locked(real_error_message);
                use_stub = true;
            }
        }
#else
        use_stub = true;
#endif

        if (use_stub) {
            stub_frame_buffer_ = make_stub_encoder_frame(sink_index, sink_info);
            data = stub_frame_buffer_.data();
            size = stub_frame_buffer_.size();
            if (error_message != nullptr) {
                error_message->clear();
            }
        }

        if ((data == nullptr) || (size == 0)) {
            set_error(error_message, "Encoder frame is empty");
            return false;
        }
        if (emitted_sink_info != nullptr) {
            *emitted_sink_info = sink_info;
        }
        active_callback(sink_index, sink_info, data, size);
        return true;
    }

    bool start_stream_worker(std::string *error_message)
    {
        if (stream_thread_.joinable()) {
            return true;
        }

        try {
            stream_thread_ = std::thread([this]() {
                stream_loop();
            });
        } catch (const std::exception &e) {
            set_error(error_message, std::string("Failed to start video encoder stream worker: ") + e.what());
            return false;
        }
        return true;
    }

    void stop_stream_worker()
    {
        stream_stop_requested_.store(true);
        stream_cv_.notify_all();
        if (stream_thread_.joinable()) {
            stream_thread_.join();
        }
        stream_stop_requested_.store(false);
    }

    void stream_loop()
    {
        while (!stream_stop_requested_.load()) {
            video::EncoderSinkInfo sink_info = {};
            std::string error_message;
            if (!emit_encoder_frame(0, nullptr, &error_message, &sink_info)) {
                if (!stream_stop_requested_.load()) {
                    BROOKESIA_LOGW("Video encoder stream loop stopped: %1%", error_message);
                }
                break;
            }

            const auto interval_ms = std::max<uint32_t>(1, 1000U / std::max<uint32_t>(1, sink_info.fps));
            std::unique_lock lock(stream_wait_mutex_);
            stream_cv_.wait_for(lock, std::chrono::milliseconds(interval_ms), [this]() {
                return stream_stop_requested_.load();
            });
        }
    }

    void reset_frame_buffers_locked()
    {
        stub_frame_buffer_.clear();
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
        encoded_frame_buffer_.clear();
#endif
    }

    bool is_closed_locked() const
    {
        if (is_opened_ || is_started_ || real_started_ || callback_ || !config_.sinks.empty() ||
                !stub_frame_buffer_.empty()) {
            return false;
        }
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
        return (format_context_ == nullptr) && (codec_context_ == nullptr) && (packet_ == nullptr) &&
               (frame_ == nullptr) && (latest_frame_ == nullptr) && converters_.empty() &&
               encoded_frame_buffer_.empty();
#else
        return true;
#endif
    }

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
    bool start_real_locked()
    {
        const auto &sink = config_.sinks.front();
        const std::string device_path = config_.source && config_.source->device_path ?
                                        config_.source->device_path.value() : get_default_video_device_path();
        std::error_code fs_error;
        if (!std::filesystem::exists(device_path, fs_error)) {
            BROOKESIA_LOGW("V4L2 device '%1%' is not available", device_path);
            return false;
        }

        ensure_avdevice_registered();
        const AVInputFormat *input_format = av_find_input_format("video4linux2");
        if (input_format == nullptr) {
            BROOKESIA_LOGW("FFmpeg video4linux2 input format is unavailable after device registration");
            return false;
        }

        AVDictionary *options = nullptr;
        const auto fps = sink.fps > 0 ? sink.fps : 15;
        real_v4l2_buffer_count_ = get_v4l2_buffer_count(config_.source);
        real_rtbufsize_bytes_ = estimate_rtbufsize_bytes(sink, real_v4l2_buffer_count_);
        if (config_.source && config_.source->fixed_width && config_.source->fixed_height) {
            const std::string video_size = std::to_string(config_.source->fixed_width.value()) + "x" +
                                           std::to_string(config_.source->fixed_height.value());
            av_dict_set(&options, "video_size", video_size.c_str(), 0);
        }
        const std::string rtbufsize = std::to_string(real_rtbufsize_bytes_);
        av_dict_set(&options, "fflags", "nobuffer", 0);
        av_dict_set(&options, "rtbufsize", rtbufsize.c_str(), 0);
        av_dict_set(&options, "max_delay", "0", 0);
        av_dict_set(&options, "framerate", std::to_string(fps).c_str(), 0);

        format_context_ = avformat_alloc_context();
        if (format_context_ == nullptr) {
            av_dict_free(&options);
            BROOKESIA_LOGW("Failed to allocate V4L2 format context");
            return false;
        }
        format_context_->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_NONBLOCK;
        int error = avformat_open_input(&format_context_, device_path.c_str(), input_format, &options);
        av_dict_free(&options);
        if (error < 0) {
            BROOKESIA_LOGW("Failed to open V4L2 device '%1%': %2%", device_path, av_error_to_string(error));
            close_real_locked();
            return false;
        }
        format_context_->flags |= AVFMT_FLAG_NOBUFFER | AVFMT_FLAG_NONBLOCK;

        error = avformat_find_stream_info(format_context_, nullptr);
        if (error < 0) {
            BROOKESIA_LOGW("Failed to read V4L2 stream info: %1%", av_error_to_string(error));
            close_real_locked();
            return false;
        }

        video_stream_index_ = av_find_best_stream(format_context_, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
        if (video_stream_index_ < 0) {
            BROOKESIA_LOGW("No video stream found in V4L2 device '%1%'", device_path);
            close_real_locked();
            return false;
        }

        AVStream *stream = format_context_->streams[video_stream_index_];
        video_stream_time_base_ = stream->time_base;
        const auto frame_rate = stream->avg_frame_rate.num > 0 ? stream->avg_frame_rate : stream->r_frame_rate;
        const char *pixel_format = av_get_pix_fmt_name(static_cast<AVPixelFormat>(stream->codecpar->format));
        source_codec_id_ = stream->codecpar->codec_id;
        const AVCodec *codec = avcodec_find_decoder(source_codec_id_);
        if (codec == nullptr) {
            BROOKESIA_LOGW("No FFmpeg decoder found for V4L2 codec id %1%", static_cast<int>(source_codec_id_));
            close_real_locked();
            return false;
        }

        codec_context_ = avcodec_alloc_context3(codec);
        if (codec_context_ == nullptr) {
            close_real_locked();
            return false;
        }
        error = avcodec_parameters_to_context(codec_context_, stream->codecpar);
        if (error < 0) {
            BROOKESIA_LOGW("Failed to copy V4L2 codec parameters: %1%", av_error_to_string(error));
            close_real_locked();
            return false;
        }
        error = avcodec_open2(codec_context_, codec, nullptr);
        if (error < 0) {
            BROOKESIA_LOGW("Failed to open V4L2 decoder: %1%", av_error_to_string(error));
            close_real_locked();
            return false;
        }
        packet_ = av_packet_alloc();
        frame_ = av_frame_alloc();
        latest_frame_ = av_frame_alloc();
        if ((packet_ == nullptr) || (frame_ == nullptr) || (latest_frame_ == nullptr)) {
            close_real_locked();
            return false;
        }

        BROOKESIA_LOGI(
            "V4L2 video backend opened: device='%1%', stream=%2%x%3%, pixel_format=%4%, codec=%5%, fps=%6%, "
            "low_latency_buffers=%7%, rtbufsize=%8%",
            device_path,
            stream->codecpar->width,
            stream->codecpar->height,
            pixel_format != nullptr ? pixel_format : "unknown",
            avcodec_get_name(source_codec_id_),
            av_frame_rate_to_string(frame_rate),
            real_v4l2_buffer_count_,
            real_rtbufsize_bytes_
        );
        return true;
    }

    bool read_real_frame_locked(
        size_t sink_index, const video::EncoderSinkInfo &sink_info, const uint8_t *&data, size_t &size,
        std::string *error_message
    )
    {
        bool has_frame = false;
        bool drain_limit_reached = false;
        uint32_t drained_packets = 0;
        const bool passthrough_encoded =
            ((sink_info.format == video::EncoderSinkFormat::MJPEG) && (source_codec_id_ == AV_CODEC_ID_MJPEG)) ||
            ((sink_info.format == video::EncoderSinkFormat::H264) && (source_codec_id_ == AV_CODEC_ID_H264));
        const auto wait_deadline = std::chrono::steady_clock::now() +
                                   std::chrono::milliseconds(frame_interval_ms(sink_info.fps));

        encoded_frame_buffer_.clear();
        av_frame_unref(latest_frame_);

        while (true) {
            av_packet_unref(packet_);
            const int read_error = av_read_frame(format_context_, packet_);
            if (read_error < 0) {
                if (is_ffmpeg_again(read_error)) {
                    if (has_frame) {
                        break;
                    }
                    if (std::chrono::steady_clock::now() >= wait_deadline) {
                        set_error(error_message, "No V4L2 frame was available before timeout");
                        return false;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }
                if (has_frame) {
                    break;
                }
                set_error(error_message, "Failed to read V4L2 frame: " + av_error_to_string(read_error));
                return false;
            }
            if (packet_->stream_index != video_stream_index_) {
                continue;
            }
            drained_packets++;

            if (passthrough_encoded) {
                if (packet_->size <= 0) {
                    set_error(error_message, "V4L2 packet is empty");
                    return false;
                }
                encoded_frame_buffer_.assign(packet_->data, packet_->data + packet_->size);
                has_frame = true;
            } else if (!decode_real_packet_locked(has_frame, error_message)) {
                return false;
            }
            const bool packet_is_stale = is_real_packet_stale_locked(packet_, sink_info.fps);

            if (drained_packets >= REAL_FRAME_DRAIN_PACKET_LIMIT) {
                drain_limit_reached = true;
                break;
            }
            if (has_frame && !packet_is_stale) {
                break;
            }
        }

        if (!has_frame) {
            set_error(error_message, "No V4L2 frame was decoded");
            return false;
        }
        if (drain_limit_reached) {
            log_real_frame_drain_limit_locked(drained_packets);
        }

        if (passthrough_encoded) {
            data = encoded_frame_buffer_.data();
            size = encoded_frame_buffer_.size();
            return true;
        }

        const AVPixelFormat format = to_av_pixel_format(sink_info.format);
        if (format == AV_PIX_FMT_NONE) {
            set_error(error_message, "Requested video encoder sink format requires hardware encoding");
            return false;
        }
        if (sink_index >= converters_.size()) {
            set_error(error_message, "Video encoder conversion cache is not available");
            return false;
        }
        if (!converters_[sink_index].convert(
                    latest_frame_, format, sink_info.width, sink_info.height, error_message
                )) {
            av_frame_unref(latest_frame_);
            return false;
        }
        data = converters_[sink_index].data();
        size = converters_[sink_index].size();
        av_frame_unref(latest_frame_);
        return true;
    }

    bool decode_real_packet_locked(bool &has_frame, std::string *error_message)
    {
        int send_error = avcodec_send_packet(codec_context_, packet_);
        if (is_ffmpeg_again(send_error)) {
            if (!receive_latest_real_frame_locked(has_frame, error_message)) {
                return false;
            }
            send_error = avcodec_send_packet(codec_context_, packet_);
        }
        if (send_error < 0) {
            set_error(error_message, "Failed to send V4L2 packet: " + av_error_to_string(send_error));
            return false;
        }

        return receive_latest_real_frame_locked(has_frame, error_message);
    }

    bool is_real_packet_stale_locked(const AVPacket *packet, uint8_t fps) const
    {
        if ((packet == nullptr) || (packet->pts == AV_NOPTS_VALUE) || (video_stream_time_base_.num <= 0) ||
                (video_stream_time_base_.den <= 0)) {
            return false;
        }

        const int64_t packet_time_us = av_rescale_q(packet->pts, video_stream_time_base_, AVRational{1, AV_TIME_BASE});
        const int64_t now_us = av_gettime_relative();
        if (packet_time_us >= now_us) {
            return false;
        }

        const int64_t stale_threshold_us = static_cast<int64_t>(frame_interval_ms(fps)) * 2000;
        return (now_us - packet_time_us) > stale_threshold_us;
    }

    bool receive_latest_real_frame_locked(bool &has_frame, std::string *error_message)
    {
        while (true) {
            const int receive_error = avcodec_receive_frame(codec_context_, frame_);
            if (is_ffmpeg_again(receive_error) || (receive_error == AVERROR_EOF)) {
                return true;
            }
            if (receive_error < 0) {
                set_error(error_message, "Failed to decode V4L2 frame: " + av_error_to_string(receive_error));
                return false;
            }

            av_frame_unref(latest_frame_);
            av_frame_move_ref(latest_frame_, frame_);
            has_frame = true;
        }
    }

    void log_real_frame_drain_limit_locked(uint32_t drained_packets)
    {
        if ((real_frame_drain_warning_count_++ % 30) != 0) {
            return;
        }
        BROOKESIA_LOGW(
            "V4L2 frame drain limit reached while catching up to latest frame: drained_packets=%1%",
            drained_packets
        );
    }

    void log_real_frame_warning_locked(const std::string &message)
    {
        if ((real_frame_warning_count_++ % 30) != 0) {
            return;
        }
        BROOKESIA_LOGW(
            "Failed to read V4L2 video frame, using encoder stub fallback: %1%",
            message.empty() ? "unknown error" : message
        );
    }

    void close_real_locked()
    {
        if (packet_ != nullptr) {
            av_packet_free(&packet_);
        }
        if (frame_ != nullptr) {
            av_frame_free(&frame_);
        }
        if (latest_frame_ != nullptr) {
            av_frame_free(&latest_frame_);
        }
        if (codec_context_ != nullptr) {
            avcodec_free_context(&codec_context_);
        }
        if (format_context_ != nullptr) {
            avformat_close_input(&format_context_);
        }
        source_codec_id_ = AV_CODEC_ID_NONE;
        video_stream_index_ = -1;
        video_stream_time_base_ = AVRational{0, 1};
        real_started_ = false;
        real_v4l2_buffer_count_ = V4L2_BUFFER_COUNT_DEFAULT;
        real_rtbufsize_bytes_ = 0;
        for (auto &converter : converters_) {
            converter.reset();
        }
        encoded_frame_buffer_.clear();
    }
#else
    void close_real_locked() {}
    bool start_real_locked()
    {
        return false;
    }
#endif

    mutable std::mutex mutex_;
    std::mutex frame_mutex_;
    std::mutex stream_wait_mutex_;
    std::condition_variable stream_cv_;
    std::thread stream_thread_;
    std::atomic<bool> stream_stop_requested_ {false};
    video::EncoderConfig config_{};
    FrameCallback callback_;
    bool is_opened_ = false;
    bool is_started_ = false;
    bool real_started_ = false;
    std::vector<uint8_t> stub_frame_buffer_;

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
    AVFormatContext *format_context_ = nullptr;
    AVCodecContext *codec_context_ = nullptr;
    AVPacket *packet_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVFrame *latest_frame_ = nullptr;
    AVCodecID source_codec_id_ = AV_CODEC_ID_NONE;
    int video_stream_index_ = -1;
    AVRational video_stream_time_base_ {0, 1};
    std::vector<VideoFrameConverter> converters_;
    std::vector<uint8_t> encoded_frame_buffer_;
    uint32_t real_frame_warning_count_ = 0;
    uint32_t real_frame_drain_warning_count_ = 0;
    uint8_t real_v4l2_buffer_count_ = V4L2_BUFFER_COUNT_DEFAULT;
    size_t real_rtbufsize_bytes_ = 0;
#endif
};

class VideoDecoderLinuxImpl: public video::DecoderIface {
public:
    ~VideoDecoderLinuxImpl() override
    {
        close();
    }

    bool open(const video::DecoderConfig &config, FrameCallback callback, std::string *error_message) override
    {
        if (!validate_decoder_config(config, error_message)) {
            return false;
        }

        std::lock_guard lock(mutex_);
        close_real_locked();
        config_ = config;
        callback_ = std::move(callback);
        is_opened_ = true;
        is_started_ = false;
        real_opened_ = open_real_locked();
        if (!real_opened_) {
            BROOKESIA_LOGW("Using video decoder stub fallback");
        }
        return true;
    }

    void close() override
    {
        std::lock_guard lock(mutex_);
        close_real_locked();
        is_opened_ = false;
        is_started_ = false;
        callback_ = nullptr;
        config_ = {};
    }

    bool start(std::string *error_message) override
    {
        std::lock_guard lock(mutex_);
        if (!is_opened_) {
            set_error(error_message, "Decoder is not opened");
            return false;
        }

        is_started_ = true;
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool stop(std::string *error_message) override
    {
        std::lock_guard lock(mutex_);
        if (!is_opened_) {
            set_error(error_message, "Decoder is not opened");
            return false;
        }

        is_started_ = false;
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool feed_frame(const uint8_t *data, size_t size, std::string *error_message) override
    {
        FrameCallback callback;
        video::DecoderConfig config = {};
        {
            std::lock_guard lock(mutex_);
            if (!is_opened_ || !is_started_) {
                set_error(error_message, "Decoder is not started");
                return false;
            }
            if ((data == nullptr) || (size == 0)) {
                set_error(error_message, "Decoder source frame is empty");
                return false;
            }

            callback = callback_;
            config = config_;
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
            if (real_opened_ && feed_real_frame_locked(data, size, error_message)) {
                if (error_message != nullptr) {
                    error_message->clear();
                }
                return true;
            }
#endif
        }

        emit_stub_decoder_frame(config, callback);
        if (error_message != nullptr) {
            error_message->clear();
        }
        return true;
    }

    bool is_opened() const override
    {
        std::lock_guard lock(mutex_);
        return is_opened_;
    }

    bool is_started() const override
    {
        std::lock_guard lock(mutex_);
        return is_started_;
    }

private:
#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
    bool open_real_locked()
    {
        const AVCodec *codec = avcodec_find_decoder(to_av_codec_id(config_.source_format));
        if (codec == nullptr) {
            return false;
        }
        codec_context_ = avcodec_alloc_context3(codec);
        if (codec_context_ == nullptr) {
            return false;
        }
        codec_context_->width = config_.width;
        codec_context_->height = config_.height;
        int error = avcodec_open2(codec_context_, codec, nullptr);
        if (error < 0) {
            BROOKESIA_LOGW("Failed to open FFmpeg video decoder: %1%", av_error_to_string(error));
            close_real_locked();
            return false;
        }
        packet_ = av_packet_alloc();
        frame_ = av_frame_alloc();
        if ((packet_ == nullptr) || (frame_ == nullptr)) {
            close_real_locked();
            return false;
        }
        return true;
    }

    bool feed_real_frame_locked(const uint8_t *data, size_t size, std::string *error_message)
    {
        av_packet_unref(packet_);
        packet_->data = const_cast<uint8_t *>(data);
        packet_->size = static_cast<int>(size);

        int error = avcodec_send_packet(codec_context_, packet_);
        if (error < 0) {
            set_error(error_message, "Failed to send video packet: " + av_error_to_string(error));
            return false;
        }

        bool emitted_frame = false;
        while (true) {
            error = avcodec_receive_frame(codec_context_, frame_);
            if (error == AVERROR(EAGAIN) || error == AVERROR_EOF) {
                break;
            }
            if (error < 0) {
                set_error(error_message, "Failed to decode video packet: " + av_error_to_string(error));
                return false;
            }

            const AVPixelFormat format = to_av_pixel_format(config_.sink_format);
            std::vector<uint8_t> converted_frame;
            if (!convert_frame(frame_, format, config_.width, config_.height, converted_frame)) {
                set_error(error_message, "Failed to convert decoded video frame");
                return false;
            }
            if (callback_) {
                callback_(config_.width, config_.height, converted_frame.data(), converted_frame.size());
            }
            emitted_frame = true;
            av_frame_unref(frame_);
        }

        return emitted_frame;
    }

    void close_real_locked()
    {
        if (packet_ != nullptr) {
            av_packet_free(&packet_);
        }
        if (frame_ != nullptr) {
            av_frame_free(&frame_);
        }
        if (codec_context_ != nullptr) {
            avcodec_free_context(&codec_context_);
        }
        real_opened_ = false;
    }
#else
    void close_real_locked() {}
    bool open_real_locked()
    {
        return false;
    }
#endif

    mutable std::mutex mutex_;
    video::DecoderConfig config_{};
    FrameCallback callback_;
    bool is_opened_ = false;
    bool is_started_ = false;
    bool real_opened_ = false;

#if !BROOKESIA_HAL_LINUX_VIDEO_BACKEND_STUB
    AVCodecContext *codec_context_ = nullptr;
    AVPacket *packet_ = nullptr;
    AVFrame *frame_ = nullptr;
#endif
};

std::string VideoLinuxDevice::get_encoder_iface_name(size_t id)
{
    return video::EncoderIface::get_default_instance_name(id);
}

std::string VideoLinuxDevice::get_decoder_iface_name(size_t id)
{
    return video::DecoderIface::get_default_instance_name(id);
}

bool VideoLinuxDevice::probe()
{
    return true;
}

std::vector<InterfaceSpec> VideoLinuxDevice::get_interface_specs() const
{
    return {
        {video::EncoderIface::NAME, get_encoder_iface_name(0)},
        {video::DecoderIface::NAME, get_decoder_iface_name(0)},
    };
}

bool VideoLinuxDevice::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::shared_ptr<VideoEncoderLinuxImpl> encoder_iface;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        encoder_iface = std::make_shared<VideoEncoderLinuxImpl>(), false,
        "Failed to create video encoder linux"
    );
    std::shared_ptr<VideoDecoderLinuxImpl> decoder_iface;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        decoder_iface = std::make_shared<VideoDecoderLinuxImpl>(), false,
        "Failed to create video decoder linux"
    );

    interfaces_.emplace(get_encoder_iface_name(0), encoder_iface);
    interfaces_.emplace(get_decoder_iface_name(0), decoder_iface);
    encoder_ifaces_.push_back(std::move(encoder_iface));
    decoder_ifaces_.push_back(std::move(decoder_iface));

    return true;
}

void VideoLinuxDevice::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    interfaces_.erase(get_decoder_iface_name(0));
    interfaces_.erase(get_encoder_iface_name(0));
    decoder_ifaces_.clear();
    encoder_ifaces_.clear();
}

#if BROOKESIA_HAL_LINUX_ENABLE_VIDEO_DEVICE
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    Device, VideoLinuxDevice, VideoLinuxDevice::DEVICE_NAME, VideoLinuxDevice::get_instance(),
    BROOKESIA_HAL_LINUX_VIDEO_DEVICE_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::hal
