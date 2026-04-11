/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Shared schema/type definitions for video encoder and decoder helper services.
 */
class Video {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief  Encoder related configurations
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
        EncoderSinkFormat format; /*!< Output sink format */
        uint16_t width;           /*!< Output width */
        uint16_t height;          /*!< Output height */
        uint8_t fps;              /*!< Output frames per second */
    };

    /**
     * @brief Encoder open configuration.
     */
    struct EncoderConfig {
        std::vector<EncoderSinkInfo> sinks; /*!< Output sink list */
        bool enable_stream_mode;            /*!< Whether encoder works in stream push mode */
    };

    /**
     * @brief  Decoder related configurations
     */
    enum class DecoderSourceFormat : uint8_t {
        H264,
        MJPEG,
        Max,
    };
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
        uint16_t width;                  /*!< Decode frame width */
        uint16_t height;                 /*!< Decode frame height */
        DecoderSourceFormat source_format; /*!< Decoder input format */
        DecoderSinkFormat sink_format;     /*!< Decoder output format */
        bool enable_stream_mode;           /*!< Whether decoder works in stream mode */
        bool enable_hw_acceleration;       /*!< Whether hardware acceleration is enabled */
    };

    /**
     * @brief Prefix used to build encoder helper service names.
     */
    static constexpr std::string_view ENCODER_NAME_PREFIX = "VideoEncoder";
    /**
     * @brief Prefix used to build decoder helper service names.
     */
    static constexpr std::string_view DECODER_NAME_PREFIX = "VideoDecoder";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Video encoder function identifiers.
     */
    enum class EncoderFunctionId : uint8_t {
        Open,
        Close,
        Start,
        Stop,
        FetchFrame,
        Max,
    };
    /**
     * @brief Video encoder event identifiers.
     */
    enum class EncoderEventId : uint8_t {
        StreamSinkFrameReady,
        FetchSinkFrameReady,
        Max,
    };

    /**
     * @brief Video decoder function identifiers.
     */
    enum class DecoderFunctionId : uint8_t {
        Open,
        Close,
        Start,
        Stop,
        FeedFrame,
        Max,
    };
    /**
     * @brief Video decoder event identifiers.
     */
    enum class DecoderEventId : uint8_t {
        SinkFrameReady,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Parameter keys for `EncoderFunctionId::Open`.
     */
    enum class EncoderFunctionOpenParam : uint8_t {
        Config,
    };
    /**
     * @brief Parameter keys for `EncoderFunctionId::FetchFrame`.
     */
    enum class EncoderFunctionFetchFrameParam : uint8_t {
        SinkIndex,
    };

    /**
     * @brief Parameter keys for `DecoderFunctionId::Open`.
     */
    enum class DecoderFunctionOpenParam : uint8_t {
        Config,
    };
    /**
     * @brief Parameter keys for `DecoderFunctionId::FeedFrame`.
     */
    enum class DecoderFunctionFeedFrameParam : uint8_t {
        Frame,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Item keys for `EncoderEventId::StreamSinkFrameReady`.
     */
    enum class EncoderEventStreamSinkFrameReadyParam : uint8_t {
        SinkIndex,
        SinkInfo,
        Frame,
    };
    /**
     * @brief Item keys for `EncoderEventId::FetchSinkFrameReady`.
     */
    enum class EncoderEventFetchSinkFrameReadyParam : uint8_t {
        SinkIndex,
        SinkInfo,
        Frame,
    };
    /**
     * @brief Item keys for `DecoderEventId::SinkFrameReady`.
     */
    enum class DecoderEventSinkFrameReadyParam : uint8_t {
        Width,
        Height,
        Frame,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema encoder_function_schema_open()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Open),
            .description = "Open the encoder with config.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionOpenParam::Config),
                    .description = (boost::format("Encoder config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((EncoderConfig{
                        .sinks = std::vector<EncoderSinkInfo>({
                            EncoderSinkInfo{EncoderSinkFormat::H264, 320, 240, 30},
                            EncoderSinkInfo{EncoderSinkFormat::MJPEG, 320, 240, 15}
                        }),
                        .enable_stream_mode = true,
                    }))).str(),
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema encoder_function_schema_close()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Close),
            .description = "Close encoder.",
        };
    }

    static FunctionSchema encoder_function_schema_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Start),
            .description = "Start encoder.",
        };
    }

    static FunctionSchema encoder_function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Stop),
            .description = "Stop encoder.",
        };
    }

    static FunctionSchema encoder_function_schema_fetch_frame()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::FetchFrame),
            .description = "Fetch an encoder output frame and emit `FetchSinkFrameReady`. "
            "Only available in non-stream mode.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionFetchFrameParam::SinkIndex),
                    .description = "Sink index.",
                    .type = FunctionValueType::Number,
                    .default_value = 0
                }
            }
        };
    }

    static FunctionSchema decoder_function_schema_open()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::Open),
            .description = "Open the decoder with config.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionOpenParam::Config),
                    .description = (boost::format("Decoder config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((DecoderConfig{
                        .width = 320,
                        .height = 240,
                        .source_format = DecoderSourceFormat::H264,
                        .sink_format = DecoderSinkFormat::RGB565_LE,
                        .enable_stream_mode = true,
                        .enable_hw_acceleration = true,
                    }))).str(),
                    .type = FunctionValueType::Object
                }
            }
        };
    }

    static FunctionSchema decoder_function_schema_close()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::Close),
            .description = "Close decoder.",
        };
    }

    static FunctionSchema decoder_function_schema_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::Start),
            .description = "Start decoder.",
        };
    }

    static FunctionSchema decoder_function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::Stop),
            .description = "Stop decoder.",
        };
    }

    static FunctionSchema decoder_function_schema_feed_frame()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::FeedFrame),
            .description = "Feed a decoder input frame.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionFeedFrameParam::Frame),
                    .description = "Frame data.",
                    .type = FunctionValueType::RawBuffer
                }
            }
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema encoder_event_schema_stream_sink_frame_ready()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventId::StreamSinkFrameReady),
            .description = "Emitted when an encoder stream frame is ready. Stream mode only.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventStreamSinkFrameReadyParam::SinkIndex),
                    .description = "Sink index.",
                    .type = EventItemType::Number
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventStreamSinkFrameReadyParam::SinkInfo),
                    .description = (boost::format("Sink info. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((EncoderSinkInfo{
                        EncoderSinkFormat::H264, 320, 240, 30
                    }))).str(),
                    .type = EventItemType::Object
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventStreamSinkFrameReadyParam::Frame),
                    .description = "Encoded frame data.",
                    .type = EventItemType::RawBuffer
                }
            },
            .require_scheduler = false,
        };
    }
    static EventSchema encoder_event_schema_fetch_sink_frame_ready()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventId::FetchSinkFrameReady),
            .description = "Emitted when an encoder fetched frame is ready. Non-stream mode only.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventFetchSinkFrameReadyParam::SinkIndex),
                    .description = "Sink index.",
                    .type = EventItemType::Number
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventFetchSinkFrameReadyParam::SinkInfo),
                    .description = (boost::format("Sink info. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((EncoderSinkInfo{
                        EncoderSinkFormat::MJPEG, 320, 240, 15
                    }))).str(),
                    .type = EventItemType::Object
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventFetchSinkFrameReadyParam::Frame),
                    .description = "Encoded frame data.",
                    .type = EventItemType::RawBuffer
                }
            },
            .require_scheduler = false,
        };
    }

    static EventSchema decoder_event_schema_sink_frame_ready()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderEventId::SinkFrameReady),
            .description = "Emitted when a decoder output frame is ready.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderEventSinkFrameReadyParam::Width),
                    .description = "Decoded frame width.",
                    .type = EventItemType::Number
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderEventSinkFrameReadyParam::Height),
                    .description = "Decoded frame height.",
                    .type = EventItemType::Number
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderEventSinkFrameReadyParam::Frame),
                    .description = "Decoded frame data.",
                    .type = EventItemType::RawBuffer,
                }
            },
            .require_scheduler = false,
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Get all encoder function schemas.
     *
     * @return std::span<const FunctionSchema> Static schema span.
     */
    static std::span<const FunctionSchema> get_encoder_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EncoderFunctionId::Max)> FUNCTION_SCHEMAS = {{
                encoder_function_schema_open(),
                encoder_function_schema_close(),
                encoder_function_schema_start(),
                encoder_function_schema_stop(),
                encoder_function_schema_fetch_frame(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }
    /**
     * @brief Get all encoder event schemas.
     *
     * @return std::span<const EventSchema> Static schema span.
     */
    static std::span<const EventSchema> get_encoder_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EncoderEventId::Max)> EVENT_SCHEMAS = {{
                encoder_event_schema_stream_sink_frame_ready(),
                encoder_event_schema_fetch_sink_frame_ready(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }

    /**
     * @brief Get all decoder function schemas.
     *
     * @return std::span<const FunctionSchema> Static schema span.
     */
    static std::span<const FunctionSchema> get_decoder_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(DecoderFunctionId::Max)> FUNCTION_SCHEMAS = {{
                decoder_function_schema_open(),
                decoder_function_schema_close(),
                decoder_function_schema_start(),
                decoder_function_schema_stop(),
                decoder_function_schema_feed_frame(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }
    /**
     * @brief Get all decoder event schemas.
     *
     * @return std::span<const EventSchema> Static schema span.
     */
    static std::span<const EventSchema> get_decoder_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(DecoderEventId::Max)> EVENT_SCHEMAS = {{
                decoder_event_schema_sink_frame_ready(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

template <int Id>
class VideoEncoder: public Base<VideoEncoder<Id>> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Re-exported function id enum for encoder service instance.
     */
    using FunctionId = Video::EncoderFunctionId;
    /**
     * @brief Re-exported event id enum for encoder service instance.
     */
    using EventId = Video::EncoderEventId;

    /**
     * @brief Get service name of this encoder instance.
     *
     * @return std::string_view Service name in format `VideoEncoder<Id>`.
     */
    static std::string_view get_name()
    {
        static const std::string name = std::string(Video::ENCODER_NAME_PREFIX) + std::to_string(Id);
        return name;
    }

    /**
     * @brief Get function schemas for this encoder instance.
     *
     * @return std::span<const FunctionSchema> Static schema span.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        return Video::get_encoder_function_schemas();
    }

    /**
     * @brief Get event schemas for this encoder instance.
     *
     * @return std::span<const EventSchema> Static schema span.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        return Video::get_encoder_event_schemas();
    }
};

template <int Id>
class VideoDecoder: public Base<VideoDecoder<Id>> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Re-exported function id enum for decoder service instance.
     */
    using FunctionId = Video::DecoderFunctionId;
    /**
     * @brief Re-exported event id enum for decoder service instance.
     */
    using EventId = Video::DecoderEventId;

    /**
     * @brief Get service name of this decoder instance.
     *
     * @return std::string_view Service name in format `VideoDecoder<Id>`.
     */
    static std::string_view get_name()
    {
        static const std::string name = std::string(Video::DECODER_NAME_PREFIX) + std::to_string(Id);
        return name;
    }

    /**
     * @brief Get function schemas for this decoder instance.
     *
     * @return std::span<const FunctionSchema> Static schema span.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        return Video::get_decoder_function_schemas();
    }

    /**
     * @brief Get event schemas for this decoder instance.
     *
     * @return std::span<const EventSchema> Static schema span.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        return Video::get_decoder_event_schemas();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief  Encoder related configurations
 */
BROOKESIA_DESCRIBE_ENUM(
    Video::EncoderSinkFormat, H264, MJPEG, RGB565, RGB888, BGR888, YUV420, YUV422, O_UYY_E_VYY, Max
);
BROOKESIA_DESCRIBE_STRUCT(Video::EncoderSinkInfo, (), (format, width, height, fps));
BROOKESIA_DESCRIBE_STRUCT(Video::EncoderConfig, (), (sinks, enable_stream_mode));

/**
 * @brief  Decoder related configurations
 */
BROOKESIA_DESCRIBE_ENUM(Video::DecoderSourceFormat, H264, MJPEG, Max);
BROOKESIA_DESCRIBE_ENUM(
    Video::DecoderSinkFormat, RGB565_LE, RGB565_BE, RGB888, BGR888, YUV420P, YUV422P, YUV422, UYVY422, O_UYY_E_VYY,
    Max
);
BROOKESIA_DESCRIBE_STRUCT(
    Video::DecoderConfig, (), (width, height, source_format, sink_format, enable_stream_mode, enable_hw_acceleration)
);

/**
 * @brief  Function related
 */
BROOKESIA_DESCRIBE_ENUM(Video::EncoderFunctionId, Open, Close, Start, Stop, FetchFrame, Max);
BROOKESIA_DESCRIBE_ENUM(Video::DecoderFunctionId, Open, Close, Start, Stop, FeedFrame, Max);
BROOKESIA_DESCRIBE_ENUM(Video::EncoderFunctionOpenParam, Config);
BROOKESIA_DESCRIBE_ENUM(Video::EncoderFunctionFetchFrameParam, SinkIndex);
BROOKESIA_DESCRIBE_ENUM(Video::DecoderFunctionOpenParam, Config);
BROOKESIA_DESCRIBE_ENUM(Video::DecoderFunctionFeedFrameParam, Frame);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(Video::EncoderEventId, StreamSinkFrameReady, FetchSinkFrameReady, Max);
BROOKESIA_DESCRIBE_ENUM(Video::DecoderEventId, SinkFrameReady, Max);
BROOKESIA_DESCRIBE_ENUM(Video::EncoderEventStreamSinkFrameReadyParam, SinkIndex, SinkInfo, Frame);
BROOKESIA_DESCRIBE_ENUM(Video::EncoderEventFetchSinkFrameReadyParam, SinkIndex, SinkInfo, Frame);
BROOKESIA_DESCRIBE_ENUM(Video::DecoderEventSinkFrameReadyParam, Width, Height, Frame);

} // namespace esp_brookesia::service::helper
