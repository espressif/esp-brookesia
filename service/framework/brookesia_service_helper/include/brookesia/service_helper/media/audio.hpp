/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interfaces/audio/processor.hpp"
#include "brookesia/service_manager/detail/static_schema.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Runtime playback options owned by the service layer.
 */
struct AudioPlayUrlConfig {
    bool interrupt = true;          /*!< Whether current playback can be interrupted */
    uint32_t delay_ms = 0;          /*!< Delay before playback starts */
    uint32_t loop_count = 0;        /*!< Number of loops; 0 means play once */
    uint32_t loop_interval_ms = 0;  /*!< Interval between loops */
    uint32_t timeout_ms = 0;        /*!< Timeout for finishing playback */
    /**
     * @brief Admit playback only when the service is idle, without queueing or interrupting another request.
     *
     * Set `interrupt` to false when enabling this option. The check and request admission occur in the
     * AudioPlayback service call. The default false preserves the existing interrupt/queue behavior.
     */
    bool only_if_idle = false;
};

/**
 * @brief Shared schema/type definitions for audio playback, encoder, and decoder helper services.
 */
class Audio {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using PlayState = hal::audio::PlayState;
    using PlayUrlConfig = AudioPlayUrlConfig;
    using CodecFormat = hal::audio::CodecFormat;
    using CodecGeneralConfig = hal::audio::CodecGeneralConfig;
    using EncoderExtraConfigOpus = hal::audio::EncoderExtraConfigOpus;
    using EncoderDynamicConfig = hal::audio::EncoderDynamicConfig;
    using DecoderDynamicConfig = hal::audio::DecoderDynamicConfig;
    using AFE_Event = hal::audio::AfeEvent;

    enum class SourceState : uint8_t {
        Idle,
        Requested,
        Granted,
        Released,
    };

    enum class StreamQueuePolicy : uint8_t {
        DropNewest,
        Max,
    };

    enum class WriteResult : uint8_t {
        Written,
        DroppedNotActive,
        DroppedQueueFull,
        DroppedInvalidData,
        Error,
    };

    struct OutputInfo {
        uint32_t id = 0;
        std::string name;
        std::string role;
        std::vector<uint32_t> sample_rates;
        std::vector<uint8_t> channels;
        std::vector<uint8_t> sample_bits;
    };

    struct SourceInfo {
        uint32_t id = 0;
        std::string name;
        std::string role;
        std::vector<std::string> preferred_outputs;
        int32_t priority = 0;
    };

    struct StreamConfig {
        CodecFormat type = CodecFormat::PCM;
        CodecGeneralConfig general = {};
        uint32_t queue_size_bytes = 32 * 1024;
        StreamQueuePolicy queue_policy = StreamQueuePolicy::DropNewest;
    };

    /**
     * @brief Service name used by the playback helper.
     */
    static constexpr std::string_view PLAYBACK_NAME = "AudioPlayback";
    /**
     * @brief Prefix used to build encoder helper service names.
     */
    static constexpr std::string_view ENCODER_NAME_PREFIX = "AudioEncoder";
    /**
     * @brief Prefix used to build decoder helper service names.
     */
    static constexpr std::string_view DECODER_NAME_PREFIX = "AudioDecoder";

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class PlaybackFunctionId : uint8_t {
        Play,
        PlayUrls,
        Pause,
        Resume,
        Stop,
        SetVolume,
        GetVolume,
        SetMute,
        GetMute,
        LoadData,
        ResetData,
        Max,
    };

    enum class PlaybackEventId : uint8_t {
        PlayStateChanged,
        VolumeChanged,
        MuteChanged,
        Max,
    };

    enum class EncoderFunctionId : uint8_t {
        Start,
        Stop,
        Pause,
        Resume,
        PauseWakeEnd,
        ResumeWakeEnd,
        GetAFEWakeWords,
        Max,
    };

    enum class EncoderEventId : uint8_t {
        AFEEventHappened,
        Max,
    };

    enum class DecoderFunctionId : uint8_t {
        GetOutputs,
        GetSources,
        RegisterSource,
        UnregisterSource,
        RequestOutput,
        ReleaseOutput,
        SetActiveSource,
        GetActiveSource,
        OpenStream,
        CloseStream,
        Max,
    };

    enum class DecoderEventId : uint8_t {
        Max,
    };


















private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the static schema specifications ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using DefaultValueKind = detail::static_schema::DefaultValueKind;
    using DefaultValueSpec = detail::static_schema::DefaultValueSpec;
    using EventItemSpec = detail::static_schema::EventItemSpec;
    using EventSpec = detail::static_schema::EventSpec;
    using FunctionParameterSpec = detail::static_schema::FunctionParameterSpec;
    using FunctionReturnSpec = detail::static_schema::FunctionReturnSpec;
    using FunctionSpec = detail::static_schema::FunctionSpec;

    inline static constexpr std::span<const FunctionParameterSpec> EMPTY_PARAMETERS = {};

    inline static constexpr DefaultValueSpec PLAY_URL_CONFIG_DEFAULT = {
        .kind = DefaultValueKind::JsonObject,
        .string = R"({"interrupt":true,"delay_ms":0,"loop_count":0,"loop_interval_ms":0,"timeout_ms":0,"only_if_idle":false})",
    };

    inline static constexpr std::array<FunctionParameterSpec, 2> PLAYBACK_PLAY_PARAMETERS = {{
            {
                .name = "Url",
                .description = "Audio URL, for example: \"file://littlefs/example.mp3\".",
                .type = FunctionValueType::String,
            },
            {
                .name = "Config",
                .description =
                R"(Playback config. Example: {"interrupt":true,"delay_ms":0,"loop_count":0,)"
                R"("loop_interval_ms":0,"timeout_ms":0,"only_if_idle":false})",
                .type = FunctionValueType::Object,
                .default_value = PLAY_URL_CONFIG_DEFAULT,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 2> PLAYBACK_PLAY_URLS_PARAMETERS = {{
            {
                .name = "Urls",
                .description =
                R"(Audio URL list. Example: ["file://littlefs/example1.mp3","file://littlefs/example2.mp3"])",
                .type = FunctionValueType::Array,
            },
            {
                .name = "Config",
                .description =
                R"(Playback config. Example: {"interrupt":true,"delay_ms":0,"loop_count":0,)"
                R"("loop_interval_ms":0,"timeout_ms":0,"only_if_idle":false})",
                .type = FunctionValueType::Object,
                .default_value = PLAY_URL_CONFIG_DEFAULT,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> PLAYBACK_SET_VOLUME_PARAMETERS = {{
            {
                .name = "Volume",
                .description = "Playback volume percentage in range [0, 100].",
                .type = FunctionValueType::Number,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> PLAYBACK_SET_MUTE_PARAMETERS = {{
            {
                .name = "Enable",
                .description = "True to mute playback, false to unmute it.",
                .type = FunctionValueType::Boolean,
            },
        }
    };

    inline static constexpr std::array<FunctionSpec, static_cast<size_t>(PlaybackFunctionId::Max)>
    PLAYBACK_FUNCTION_SPECS = {{
            {
                .name = "Play",
                .description = "Play audio from a URL. Supports loop and interrupt playback.",
                .parameters = PLAYBACK_PLAY_PARAMETERS,
            },
            {
                .name = "PlayUrls",
                .description = "Play audio from multiple URLs. Supports loop and interrupt playback.",
                .parameters = PLAYBACK_PLAY_URLS_PARAMETERS,
            },
            {
                .name = "Pause",
                .description = "Pause playback.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "Resume",
                .description = "Resume playback.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "Stop",
                .description = "Stop playback.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "SetVolume",
                .description = "Set target playback volume percentage.",
                .parameters = PLAYBACK_SET_VOLUME_PARAMETERS,
            },
            {
                .name = "GetVolume",
                .description = "Get target playback volume percentage [0, 100].",
                .parameters = EMPTY_PARAMETERS,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Number,
                    .description = "Playback volume percentage in range [0, 100].",
                },
            },
            {
                .name = "SetMute",
                .description = "Set whether audio playback is muted.",
                .parameters = PLAYBACK_SET_MUTE_PARAMETERS,
            },
            {
                .name = "GetMute",
                .description = "Get whether audio playback is muted.",
                .parameters = EMPTY_PARAMETERS,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Boolean,
                    .description = "True when muted, false when unmuted.",
                },
            },
            {
                .name = "LoadData",
                .description = "Load persisted audio playback state, including volume and mute.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "ResetData",
                .description = "Reset persisted audio playback state, including volume and mute.",
                .parameters = EMPTY_PARAMETERS,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> PLAYBACK_PLAY_STATE_CHANGED_ITEMS = {{
            {
                .name = "State",
                .description = "Playback state. Allowed values: [Idle, Playing, Paused]",
                .type = EventItemType::String,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> PLAYBACK_VOLUME_CHANGED_ITEMS = {{
            {
                .name = "Volume",
                .description = "Current target playback volume percentage [0, 100].",
                .type = EventItemType::Number,
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> PLAYBACK_MUTE_CHANGED_ITEMS = {{
            {
                .name = "IsMuted",
                .description = "Whether audio playback is currently muted. True if muted, false if unmuted.",
                .type = EventItemType::Boolean,
            },
        }
    };

    inline static constexpr std::array<EventSpec, static_cast<size_t>(PlaybackEventId::Max)> PLAYBACK_EVENT_SPECS = {{
            {
                .name = "PlayStateChanged",
                .description = "Emitted when playback state changes.",
                .items = PLAYBACK_PLAY_STATE_CHANGED_ITEMS,
            },
            {
                .name = "VolumeChanged",
                .description = "Emitted when the target audio playback volume changes.",
                .items = PLAYBACK_VOLUME_CHANGED_ITEMS,
            },
            {
                .name = "MuteChanged",
                .description = "Emitted when the target audio playback mute state changes.",
                .items = PLAYBACK_MUTE_CHANGED_ITEMS,
            },
        }
    };

    static_assert(PLAYBACK_FUNCTION_SPECS.size() == static_cast<size_t>(PlaybackFunctionId::Max));
    static_assert(PLAYBACK_EVENT_SPECS.size() == static_cast<size_t>(PlaybackEventId::Max));

    inline static constexpr std::array<FunctionParameterSpec, 1> ENCODER_START_PARAMETERS = {{
            {
                .name = "Config",
                .description =
                R"(Audio encoder config. Example: {"type":"PCM","general":{"channels":0,"sample_bits":0,)"
                R"("sample_rate":0,"frame_duration":0},"extra":null,"fetch_interval_ms":10,"fetch_data_size":4096,)"
                R"("enable_afe":false,"afe_wake_start_timeout_ms":30000,"afe_wake_end_timeout_ms":10000})",
                .type = FunctionValueType::Object,
            },
        }
    };

    inline static constexpr std::array<FunctionSpec, static_cast<size_t>(EncoderFunctionId::Max)>
    ENCODER_FUNCTION_SPECS = {{
            {
                .name = "Start",
                .description = "Start audio encoder.",
                .parameters = ENCODER_START_PARAMETERS,
            },
            {
                .name = "Stop",
                .description = "Stop audio encoder.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "Pause",
                .description = "Pause audio encoder.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "Resume",
                .description = "Resume audio encoder.",
                .parameters = EMPTY_PARAMETERS,
            },
            {
                .name = "PauseWakeEnd",
                .description = "Pause pending AFE WakeEnd event without pausing audio capture.",
                .parameters = EMPTY_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "ResumeWakeEnd",
                .description = "Resume pending AFE WakeEnd event without resuming audio capture.",
                .parameters = EMPTY_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "GetAFEWakeWords",
                .description = "Get AFE wake words.",
                .parameters = EMPTY_PARAMETERS,
                .require_scheduler = false,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description = R"(Example: ["ni hao xiao zhi","hello brookesia"])",
                },
            },
        }
    };

    inline static constexpr std::array<EventItemSpec, 1> ENCODER_AFE_EVENT_HAPPENED_ITEMS = {{
            {
                .name = "Event",
                .description = "AFE event. Allowed values: [VAD_Start, VAD_End, WakeStart, WakeEnd]",
                .type = EventItemType::String,
            },
        }
    };

    inline static constexpr std::array<EventSpec, static_cast<size_t>(EncoderEventId::Max)> ENCODER_EVENT_SPECS = {{
            {
                .name = "AFEEventHappened",
                .description = "Emitted when an AFE event occurs.",
                .items = ENCODER_AFE_EVENT_HAPPENED_ITEMS,
            },
        }
    };

    static_assert(ENCODER_FUNCTION_SPECS.size() == static_cast<size_t>(EncoderFunctionId::Max));
    static_assert(ENCODER_EVENT_SPECS.size() == static_cast<size_t>(EncoderEventId::Max));

    inline static constexpr std::array<FunctionParameterSpec, 1> DECODER_REGISTER_SOURCE_PARAMETERS = {{
            {
                .name = "Source",
                .description =
                R"(Source info. Example: {"id":0,"name":"TTS","role":"speech","preferred_outputs":["Speaker0"],)"
                R"("priority":0})",
                .type = FunctionValueType::Object,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> DECODER_SOURCE_NAME_PARAMETERS = {{
            {
                .name = "SourceName",
                .description = "Source name.",
                .type = FunctionValueType::String,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 2> DECODER_SOURCE_OUTPUT_PARAMETERS = {{
            {
                .name = "SourceName",
                .description = "Source name.",
                .type = FunctionValueType::String,
            },
            {
                .name = "OutputName",
                .description = "Output name.",
                .type = FunctionValueType::String,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 2> DECODER_OUTPUT_SOURCE_PARAMETERS = {{
            {
                .name = "OutputName",
                .description = "Output name.",
                .type = FunctionValueType::String,
            },
            {
                .name = "SourceName",
                .description = "Source name.",
                .type = FunctionValueType::String,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 1> DECODER_OUTPUT_NAME_PARAMETERS = {{
            {
                .name = "OutputName",
                .description = "Output name.",
                .type = FunctionValueType::String,
            },
        }
    };

    inline static constexpr std::array<FunctionParameterSpec, 3> DECODER_OPEN_STREAM_PARAMETERS = {{
            {
                .name = "SourceName",
                .description = "Source name.",
                .type = FunctionValueType::String,
            },
            {
                .name = "OutputName",
                .description = "Output name.",
                .type = FunctionValueType::String,
            },
            {
                .name = "Config",
                .description =
                R"(Stream config. Example: {"type":"PCM","general":{"channels":0,"sample_bits":0,)"
                R"("sample_rate":0,"frame_duration":0},"queue_size_bytes":32768,"queue_policy":"DropNewest"})",
                .type = FunctionValueType::Object,
            },
        }
    };

    inline static constexpr std::array<FunctionSpec, static_cast<size_t>(DecoderFunctionId::Max)>
    DECODER_FUNCTION_SPECS = {{
            {
                .name = "GetOutputs",
                .description = "Get audio output list.",
                .parameters = EMPTY_PARAMETERS,
                .require_scheduler = false,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description =
                    R"(Example: [{"id":0,"name":"Speaker0","role":"speaker",)"
                    R"("sample_rates":[16000,22050,44100,48000],"channels":[1,2],"sample_bits":[16]}])",
                },
            },
            {
                .name = "GetSources",
                .description = "Get registered audio sources.",
                .parameters = EMPTY_PARAMETERS,
                .require_scheduler = false,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Array,
                    .description =
                    R"(Example: [{"id":1,"name":"TTS","role":"speech","preferred_outputs":["Speaker0"],)"
                    R"("priority":10}])",
                },
            },
            {
                .name = "RegisterSource",
                .description = "Register an audio source.",
                .parameters = DECODER_REGISTER_SOURCE_PARAMETERS,
                .require_scheduler = false,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::Number,
                    .description = "Registered source id.",
                },
            },
            {
                .name = "UnregisterSource",
                .description = "Unregister an audio source by name.",
                .parameters = DECODER_SOURCE_NAME_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "RequestOutput",
                .description = "Request an output for an audio source.",
                .parameters = DECODER_SOURCE_OUTPUT_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "ReleaseOutput",
                .description = "Release an output from an audio source.",
                .parameters = DECODER_SOURCE_OUTPUT_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "SetActiveSource",
                .description = "Set the active audio source for an output.",
                .parameters = DECODER_OUTPUT_SOURCE_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "GetActiveSource",
                .description = "Get the active audio source name for an output.",
                .parameters = DECODER_OUTPUT_NAME_PARAMETERS,
                .require_scheduler = false,
                .return_value = FunctionReturnSpec{
                    .type = FunctionValueType::String,
                    .description = "Active source name, or an empty string when no source is active.",
                },
            },
            {
                .name = "OpenStream",
                .description = "Open a direct audio stream for a source and output.",
                .parameters = DECODER_OPEN_STREAM_PARAMETERS,
                .require_scheduler = false,
            },
            {
                .name = "CloseStream",
                .description = "Close a direct audio stream for a source and output.",
                .parameters = DECODER_SOURCE_OUTPUT_PARAMETERS,
                .require_scheduler = false,
            },
        }
    };

    static_assert(DECODER_FUNCTION_SPECS.size() == static_cast<size_t>(DecoderFunctionId::Max));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by helpers ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static std::span<const FunctionSchema> get_playback_function_schemas()
    {
        static std::array<FunctionSchema, PLAYBACK_FUNCTION_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_function_schemas(PLAYBACK_FUNCTION_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    static std::span<const EventSchema> get_playback_event_schemas()
    {
        static std::array<EventSchema, PLAYBACK_EVENT_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_event_schemas(PLAYBACK_EVENT_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    static std::span<const FunctionSchema> get_encoder_function_schemas()
    {
        static std::array<FunctionSchema, ENCODER_FUNCTION_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_function_schemas(ENCODER_FUNCTION_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    static std::span<const EventSchema> get_encoder_event_schemas()
    {
        static std::array<EventSchema, ENCODER_EVENT_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_event_schemas(ENCODER_EVENT_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    static std::span<const FunctionSchema> get_decoder_function_schemas()
    {
        static std::array<FunctionSchema, DECODER_FUNCTION_SPECS.size()> schemas;
        static const bool initialized = [] {
            detail::static_schema::materialize_function_schemas(DECODER_FUNCTION_SPECS, schemas);
            return true;
        }();
        static_cast<void>(initialized);
        return schemas;
    }

    static std::span<const EventSchema> get_decoder_event_schemas()
    {
        return {};
    }
};

class AudioPlayback: public Base<AudioPlayback> {
public:
    using FunctionId = Audio::PlaybackFunctionId;
    using EventId = Audio::PlaybackEventId;

    static constexpr std::string_view get_name()
    {
        return Audio::PLAYBACK_NAME;
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        return Audio::get_playback_function_schemas();
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return Audio::get_playback_event_schemas();
    }
};

template <int Id>
class AudioEncoder: public Base<AudioEncoder<Id>> {
public:
    using FunctionId = Audio::EncoderFunctionId;
    using EventId = Audio::EncoderEventId;

    static std::string_view get_name()
    {
        static const std::string name = std::string(Audio::ENCODER_NAME_PREFIX) + std::to_string(Id);
        return name;
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        return Audio::get_encoder_function_schemas();
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return Audio::get_encoder_event_schemas();
    }
};

template <int Id>
class AudioDecoder: public Base<AudioDecoder<Id>> {
public:
    using FunctionId = Audio::DecoderFunctionId;
    using EventId = Audio::DecoderEventId;

    static std::string_view get_name()
    {
        static const std::string name = std::string(Audio::DECODER_NAME_PREFIX) + std::to_string(Id);
        return name;
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        return Audio::get_decoder_function_schemas();
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        return Audio::get_decoder_event_schemas();
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_STRUCT(
    AudioPlayUrlConfig, (), (interrupt, delay_ms, loop_count, loop_interval_ms, timeout_ms, only_if_idle)
);
BROOKESIA_DESCRIBE_ENUM(Audio::SourceState, Idle, Requested, Granted, Released);
BROOKESIA_DESCRIBE_ENUM(Audio::StreamQueuePolicy, DropNewest, Max);
BROOKESIA_DESCRIBE_ENUM(
    Audio::WriteResult, Written, DroppedNotActive, DroppedQueueFull, DroppedInvalidData, Error
);
BROOKESIA_DESCRIBE_STRUCT(Audio::OutputInfo, (), (id, name, role, sample_rates, channels, sample_bits));
BROOKESIA_DESCRIBE_STRUCT(Audio::SourceInfo, (), (id, name, role, preferred_outputs, priority));
BROOKESIA_DESCRIBE_STRUCT(Audio::StreamConfig, (), (type, general, queue_size_bytes, queue_policy));
BROOKESIA_DESCRIBE_ENUM(
    Audio::PlaybackFunctionId, Play, PlayUrls, Pause, Resume, Stop, SetVolume, GetVolume, SetMute, GetMute, LoadData,
    ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackEventId, PlayStateChanged, VolumeChanged, MuteChanged, Max);
BROOKESIA_DESCRIBE_ENUM(
    Audio::EncoderFunctionId, Start, Stop, Pause, Resume, PauseWakeEnd, ResumeWakeEnd, GetAFEWakeWords, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::EncoderEventId, AFEEventHappened, Max);
BROOKESIA_DESCRIBE_ENUM(
    Audio::DecoderFunctionId, GetOutputs, GetSources, RegisterSource, UnregisterSource, RequestOutput, ReleaseOutput,
    SetActiveSource, GetActiveSource, OpenStream, CloseStream, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderEventId, Max);

} // namespace esp_brookesia::service::helper
