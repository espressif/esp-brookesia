/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file processor.hpp
 * @brief Declares video processor HAL interfaces.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

namespace esp_brookesia::hal::video {

/**
 * @brief Encoder sink formats.
 */
enum class EncoderSinkFormat : uint8_t {
    H264,
    MJPEG,
    RGB565,
    RGB888,
    BGR888,
    YUV420,
    YUV422,
    O_UYY_E_VYY,
    Max,
};

/**
 * @brief One encoder sink stream description.
 */
struct EncoderSinkInfo {
    EncoderSinkFormat format;
    uint16_t width;
    uint16_t height;
    uint8_t fps;
};

/**
 * @brief Encoder source camera description.
 */
struct EncoderSourceConfig {
    std::optional<std::string> device_path = std::nullopt;
    std::optional<EncoderSinkFormat> fixed_format = std::nullopt;
    std::optional<uint16_t> fixed_width = std::nullopt;
    std::optional<uint16_t> fixed_height = std::nullopt;
    std::optional<uint8_t> v4l2_buffer_count = std::nullopt;
};

/**
 * @brief Encoder open configuration.
 */
struct EncoderConfig {
    std::vector<EncoderSinkInfo> sinks;
    bool enable_stream_mode = false;
    std::optional<EncoderSourceConfig> source = std::nullopt;
};

/**
 * @brief Decoder input formats.
 */
enum class DecoderSourceFormat : uint8_t {
    H264,
    MJPEG,
    Max,
};

/**
 * @brief Decoder output formats.
 */
enum class DecoderSinkFormat : uint8_t {
    RGB565_LE,
    RGB565_BE,
    RGB888,
    BGR888,
    YUV420P,
    YUV422P,
    YUV422,
    UYVY422,
    O_UYY_E_VYY,
    Max,
};

/**
 * @brief Decoder open configuration.
 */
struct DecoderConfig {
    uint16_t width;
    uint16_t height;
    DecoderSourceFormat source_format;
    DecoderSinkFormat sink_format;
    bool enable_stream_mode;
    bool enable_hw_acceleration;
};

/**
 * @brief High-level video encoder interface.
 */
class EncoderIface: public Interface {
public:
    static constexpr const char *NAME = "VideoEncoder";

    static std::string get_default_instance_name(size_t id)
    {
        return std::string("Video:Encoder:") + std::to_string(id);
    }

    using FrameCallback = std::function<void(
                              size_t sink_index, const EncoderSinkInfo &sink_info, const uint8_t *data, size_t size
                          )>;

    EncoderIface()
        : Interface(NAME)
    {
    }

    virtual ~EncoderIface() = default;

    virtual bool open(const EncoderConfig &config, FrameCallback callback, std::string *error_message) = 0;
    virtual void close() = 0;
    virtual bool start(std::string *error_message) = 0;
    virtual bool stop(std::string *error_message) = 0;
    virtual bool fetch_frame(size_t sink_index, FrameCallback callback, std::string *error_message) = 0;
    virtual bool is_opened() const = 0;
    virtual bool is_started() const = 0;
};

/**
 * @brief High-level video decoder interface.
 */
class DecoderIface: public Interface {
public:
    static constexpr const char *NAME = "VideoDecoder";

    static std::string get_default_instance_name(size_t id)
    {
        return std::string("Video:Decoder:") + std::to_string(id);
    }

    using FrameCallback = std::function<void(uint16_t width, uint16_t height, const uint8_t *data, size_t size)>;

    DecoderIface()
        : Interface(NAME)
    {
    }

    virtual ~DecoderIface() = default;

    virtual bool open(const DecoderConfig &config, FrameCallback callback, std::string *error_message) = 0;
    virtual void close() = 0;
    virtual bool start(std::string *error_message) = 0;
    virtual bool stop(std::string *error_message) = 0;
    virtual bool feed_frame(const uint8_t *data, size_t size, std::string *error_message) = 0;
    virtual bool is_opened() const = 0;
    virtual bool is_started() const = 0;
};

BROOKESIA_DESCRIBE_ENUM(EncoderSinkFormat, H264, MJPEG, RGB565, RGB888, BGR888, YUV420, YUV422, O_UYY_E_VYY, Max);
BROOKESIA_DESCRIBE_STRUCT(EncoderSinkInfo, (), (format, width, height, fps));
BROOKESIA_DESCRIBE_STRUCT(
    EncoderSourceConfig, (), (device_path, fixed_format, fixed_width, fixed_height, v4l2_buffer_count)
);
BROOKESIA_DESCRIBE_STRUCT(EncoderConfig, (), (sinks, enable_stream_mode, source));
BROOKESIA_DESCRIBE_ENUM(DecoderSourceFormat, H264, MJPEG, Max);
BROOKESIA_DESCRIBE_ENUM(
    DecoderSinkFormat, RGB565_LE, RGB565_BE, RGB888, BGR888, YUV420P, YUV422P, YUV422, UYVY422, O_UYY_E_VYY,
    Max
);
BROOKESIA_DESCRIBE_STRUCT(
    DecoderConfig, (), (width, height, source_format, sink_format, enable_stream_mode, enable_hw_acceleration)
);

} // namespace esp_brookesia::hal::video
