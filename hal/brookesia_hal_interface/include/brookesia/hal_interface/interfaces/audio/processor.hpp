/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file processor.hpp
 * @brief Declares high-level audio processing HAL interfaces.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

namespace esp_brookesia::hal::audio {

/**
 * @brief Playback states.
 */
enum class PlayState : uint8_t {
    Idle,
    Playing,
    Paused,
};

/**
 * @brief Audio codec formats supported by the processor abstraction.
 */
enum class CodecFormat : uint8_t {
    PCM,
    OPUS,
    G711A,
    Max,
};

/**
 * @brief Shared audio codec information.
 */
struct CodecGeneralConfig {
    uint8_t channels;        /*!< Number of audio channels */
    uint8_t sample_bits;     /*!< Bit depth in bits */
    uint32_t sample_rate;    /*!< Sample rate in Hz */
    uint8_t frame_duration;  /*!< Frame duration in milliseconds */
};

/**
 * @brief OPUS encoder-specific options.
 */
struct EncoderExtraConfigOpus {
    bool enable_vbr;   /*!< Enable Variable Bit Rate */
    uint32_t bitrate;  /*!< Bitrate in bps */
};

/**
 * @brief Runtime audio encoder configuration.
 */
struct EncoderDynamicConfig {
    CodecFormat type;  /*!< Encoder codec type */
    CodecGeneralConfig general;
    std::variant<std::monostate, EncoderExtraConfigOpus> extra = std::monostate{};
    uint32_t fetch_interval_ms = 10;
    uint32_t fetch_data_size = 4096;
    bool enable_afe = false; /*!< Enable AFE processing for this encoder run */
    uint32_t afe_wake_start_timeout_ms = 30000;
    uint32_t afe_wake_end_timeout_ms = 10000;
};

/**
 * @brief Runtime audio decoder configuration.
 */
struct DecoderDynamicConfig {
    CodecFormat type;  /*!< Decoder codec type */
    CodecGeneralConfig general;
};

/**
 * @brief Audio front-end event identifiers.
 */
enum class AfeEvent : uint8_t {
    VAD_Start,
    VAD_End,
    WakeStart,
    WakeEnd,
    Max,
};

/**
 * @brief URL playback interface exposed by audio-capable devices.
 */
class PlaybackIface: public Interface {
public:
    static constexpr const char *NAME = "AudioPlayback";

    static std::string get_default_instance_name()
    {
        return "Audio:Playback";
    }

    using EventCallback = std::function<void(PlayState state)>;

    PlaybackIface()
        : Interface(NAME)
    {
    }

    ~PlaybackIface() override = default;

    virtual bool open(EventCallback callback) = 0;
    virtual void close() = 0;
    virtual bool play(const std::string &url) = 0;
    virtual bool pause() = 0;
    virtual bool resume() = 0;
    virtual bool stop() = 0;
    virtual bool is_opened() const = 0;
};

/**
 * @brief Audio encoder and AFE interface exposed by audio-capable devices.
 */
class EncoderIface: public Interface {
public:
    static constexpr const char *NAME = "AudioEncoder";

    static std::string get_default_instance_name(size_t id)
    {
        return std::string("Audio:Encoder:") + std::to_string(id);
    }

    using AfeEventCallback = std::function<void(AfeEvent event)>;
    using DataCallback = std::function<void(const uint8_t *data, size_t size)>;

    struct Callbacks {
        AfeEventCallback afe_event;
        DataCallback recorder_data;
    };

    EncoderIface()
        : Interface(NAME)
    {
    }

    ~EncoderIface() override = default;

    virtual std::vector<std::string> get_afe_wake_words() = 0;
    virtual bool start(const EncoderDynamicConfig &config, Callbacks callbacks) = 0;
    virtual int read_encoded_data(uint8_t *data, size_t size) = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual bool is_started() const = 0;
    virtual bool is_paused() const = 0;
};

/**
 * @brief Audio decoder feeding interface exposed by audio-capable devices.
 */
class DecoderIface: public Interface {
public:
    static constexpr const char *NAME = "AudioDecoder";

    static std::string get_default_instance_name(size_t id)
    {
        return std::string("Audio:Decoder:") + std::to_string(id);
    }

    DecoderIface()
        : Interface(NAME)
    {
    }

    ~DecoderIface() override = default;

    virtual bool start(const DecoderDynamicConfig &config) = 0;
    virtual void stop() = 0;
    virtual bool is_started() const = 0;
    virtual bool feed_data(const uint8_t *data, size_t size) = 0;
};

BROOKESIA_DESCRIBE_ENUM(PlayState, Idle, Playing, Paused);
BROOKESIA_DESCRIBE_ENUM(CodecFormat, PCM, OPUS, G711A, Max);
BROOKESIA_DESCRIBE_STRUCT(CodecGeneralConfig, (), (channels, sample_bits, sample_rate, frame_duration));
BROOKESIA_DESCRIBE_STRUCT(EncoderExtraConfigOpus, (), (enable_vbr, bitrate));
BROOKESIA_DESCRIBE_STRUCT(
    EncoderDynamicConfig, (),
    (type, general, extra, fetch_interval_ms, fetch_data_size, enable_afe, afe_wake_start_timeout_ms,
     afe_wake_end_timeout_ms)
);
BROOKESIA_DESCRIBE_STRUCT(DecoderDynamicConfig, (), (type, general));
BROOKESIA_DESCRIBE_ENUM(AfeEvent, VAD_Start, VAD_End, WakeStart, WakeEnd, Max);

} // namespace esp_brookesia::hal::audio
