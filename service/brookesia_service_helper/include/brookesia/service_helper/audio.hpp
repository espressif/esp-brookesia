/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>
#include <type_traits>
#include "boost/format.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

class Audio: public Base<Audio> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class CodecFormat {
        PCM,
        OPUS,
        G711A,
        Max,
    };

    struct CodecGeneralConfig {
        uint8_t   channels;        /*!< Number of audio channels (1-4) */
        uint8_t   sample_bits;     /*!< Bit depth in bits (e.g., 8, 16, 24, 32) */
        uint32_t  sample_rate;     /*!< Sample rate in Hz (e.g., 8000, 16000, 24000, 32000, 44100, 48000) */
        uint8_t  frame_duration;   /*!< Frame duration in milliseconds */
    };

    struct EncoderExtraConfigOpus {
        bool enable_vbr;      /*!< Enable Variable Bit Rate (VBR) */
        uint32_t bitrate;     /*!< Bitrate in bps */
    };

    /**
     * @brief  Audio encoder configuration
     */
    struct EncoderConfig {
        CodecFormat type;
        CodecGeneralConfig general;
        std::variant<std::monostate, EncoderExtraConfigOpus> extra;
    };

    /**
     * @brief  Audio decoder configuration
     */
    struct DecoderConfig {
        CodecFormat type;
        CodecGeneralConfig general;
    };

    enum class PlayControlAction {
        Pause,
        Resume,
        Stop,
    };

    enum class PlayState {
        Idle,
        Playing,
        Paused,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        PlayUrl,
        PlayControl,
        SetVolume,
        GetVolume,
        StartEncoder,
        StopEncoder,
        SetEncoderReadDataSize,
        StartDecoder,
        StopDecoder,
        FeedDecoderData,
        Max,
    };

    enum class EventId {
        PlayStateChanged,
        EncoderEventHappened,
        EncoderDataReady,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionPlayUrlParam {
        Url,
    };

    enum class FunctionPlayControlParam {
        Action,
    };

    enum class FunctionSetVolumeParam {
        Volume,
    };

    enum class FunctionStartEncoderParam {
        Config,
    };

    enum class FunctionSetEncoderReadDataSizeParam {
        Size,
    };

    enum class FunctionStartDecoderParam {
        Config,
    };

    enum class FunctionFeedDecoderDataParam {
        Data,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventPlayStateChangedParam {
        State,
    };

    enum class EventEncoderEventHappenedParam {
        Event,
    };

    enum class EventEncoderDataReadyParam {
        Data,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_play_url()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::PlayUrl),
            // description
            "Play an audio file from the specified URL",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlParam::Url),
                    // description
                    "URL of the audio file to play, eg:'file://spiffs/example.mp3'",
                    // type
                    FunctionValueType::String
                }
            }
        };
    }

    static FunctionSchema function_schema_play_control()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::PlayControl),
            // description
            "Control the audio playback",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionPlayControlParam::Action),
                    // description
                    (boost::format("The action to control the audio playback, should be one of the following: %1%") %
                    BROOKESIA_DESCRIBE_TO_STR(std::vector<PlayControlAction>({
                        PlayControlAction::Pause, PlayControlAction::Resume, PlayControlAction::Stop
                    }))).str(),
                    // type
                    FunctionValueType::String
                }
            }
        };
    }

    static FunctionSchema function_schema_set_volume()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetVolume),
            // description
            "Set the volume of the audio playback",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionSetVolumeParam::Volume),
                    // description
                    "Volume value, range from 0 to 100",
                    // type
                    FunctionValueType::Number
                }
            }
        };
    }

    static FunctionSchema function_schema_get_volume()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetVolume),
            // description
            "Get the volume of the audio playback",
            // parameters
            {}
        };
    }

    static FunctionSchema function_schema_start_encoder()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartEncoder),
            // description
            "Start the audio encoder",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionStartEncoderParam::Config),
                    // description
                    "The configuration of the audio encoder",
                    // type
                    FunctionValueType::Object
                }
            }
        };
    }

    static FunctionSchema function_schema_stop_encoder()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopEncoder),
            // description
            "Stop the audio encoder",
            // parameters
            {}
        };
    }

    static FunctionSchema function_schema_set_encoder_read_data_size()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetEncoderReadDataSize),
            // description
            "Set the data size of the encoder read",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionSetEncoderReadDataSizeParam::Size),
                    // description
                    "The data size of the encoder read.",
                    // type
                    FunctionValueType::Number,
                    // default value
                    std::optional<FunctionValue>(4096.0)
                }
            }
        };
    }

    static FunctionSchema function_schema_start_decoder()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartDecoder),
            // description
            "Start the audio decoder",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionStartDecoderParam::Config),
                    // description
                    "The configuration of the audio decoder",
                    // type
                    FunctionValueType::Object
                }
            }
        };
    }

    static FunctionSchema function_schema_stop_decoder()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopDecoder),
            // description
            "Stop the audio decoder",
            // parameters
            {}
        };
    }

    static FunctionSchema function_schema_feed_decoder_data()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(FunctionId::FeedDecoderData),
            // description
            "Feed the audio data to the decoder",
            // parameters
            {
                // parameters[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(FunctionFeedDecoderDataParam::Data),
                    // description
                    "The audio data to feed to the decoder",
                    // type
                    FunctionValueType::RawBuffer
                }
            }
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_play_state_changed()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(EventId::PlayStateChanged),
            // description
            "Play state changed event",
            // items
            {
                // items[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(EventPlayStateChangedParam::State),
                    // description
                    (boost::format("Play state, should be one of the following: %1%") %
                    BROOKESIA_DESCRIBE_TO_STR(std::vector<PlayState>({
                        PlayState::Idle, PlayState::Playing, PlayState::Paused
                    }))).str(),
                    // type
                    EventItemType::String
                }
            }
        };
    }

    static EventSchema event_schema_encoder_event_happened()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(EventId::EncoderEventHappened),
            // description
            "Encoder event happened event",
            // items
            {
                // items[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(EventEncoderEventHappenedParam::Event),
                    // description
                    "The event that happened",
                    // type
                    EventItemType::RawBuffer
                }
            }
        };
    }

    static EventSchema event_schema_encoder_data_ready()
    {
        return {
            // name
            BROOKESIA_DESCRIBE_TO_STR(EventId::EncoderDataReady),
            // description
            "Encoder data ready event",
            // items
            {
                // items[0]
                {
                    // name
                    BROOKESIA_DESCRIBE_TO_STR(EventEncoderDataReadyParam::Data),
                    // description
                    "The audio data that is being encoded",
                    // type
                    EventItemType::RawBuffer
                }
            }
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr std::string_view get_name()
    {
        return "Audio";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_play_url(),
                function_schema_play_control(),
                function_schema_set_volume(),
                function_schema_get_volume(),
                function_schema_start_encoder(),
                function_schema_stop_encoder(),
                function_schema_set_encoder_read_data_size(),
                function_schema_start_decoder(),
                function_schema_stop_decoder(),
                function_schema_feed_decoder_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_play_state_changed(),
                event_schema_encoder_event_happened(),
                event_schema_encoder_data_ready(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_ENUM(Audio::CodecFormat, PCM, OPUS, G711A, Max);
BROOKESIA_DESCRIBE_STRUCT(Audio::CodecGeneralConfig, (), (channels, sample_bits, sample_rate, frame_duration));
BROOKESIA_DESCRIBE_STRUCT(Audio::EncoderExtraConfigOpus, (), (enable_vbr, bitrate));
BROOKESIA_DESCRIBE_STRUCT(Audio::DecoderConfig, (), (type, general));
BROOKESIA_DESCRIBE_STRUCT(Audio::EncoderConfig, (), (type, general, extra));
BROOKESIA_DESCRIBE_ENUM(Audio::PlayControlAction, Pause, Resume, Stop);
BROOKESIA_DESCRIBE_ENUM(Audio::PlayState, Idle, Playing, Paused);
BROOKESIA_DESCRIBE_ENUM(
    Audio::FunctionId, PlayUrl, PlayControl, SetVolume, GetVolume, StartEncoder, StopEncoder, SetEncoderReadDataSize,
    StartDecoder, StopDecoder, FeedDecoderData, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::EventId, PlayStateChanged, EncoderEventHappened, EncoderDataReady, Max);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayUrlParam, Url);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayControlParam, Action);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetVolumeParam, Volume);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionStartEncoderParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetEncoderReadDataSizeParam, Size);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionStartDecoderParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionFeedDecoderDataParam, Data);
BROOKESIA_DESCRIBE_ENUM(Audio::EventPlayStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(Audio::EventEncoderEventHappenedParam, Event);
BROOKESIA_DESCRIBE_ENUM(Audio::EventEncoderDataReadyParam, Data);

} // namespace esp_brookesia::service::helper
