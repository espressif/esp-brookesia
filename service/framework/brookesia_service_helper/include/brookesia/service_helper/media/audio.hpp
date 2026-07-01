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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class PlaybackFunctionPlayParam : uint8_t {
        Url,
        Config,
    };

    enum class PlaybackFunctionPlayUrlsParam : uint8_t {
        Urls,
        Config,
    };

    enum class PlaybackFunctionSetVolumeParam : uint8_t {
        Volume,
    };

    enum class PlaybackFunctionSetMuteParam : uint8_t {
        Enable,
    };

    enum class EncoderFunctionStartParam : uint8_t {
        Config,
    };

    enum class DecoderFunctionRegisterSourceParam : uint8_t {
        Source,
    };

    enum class DecoderFunctionUnregisterSourceParam : uint8_t {
        SourceName,
    };

    enum class DecoderFunctionRequestOutputParam : uint8_t {
        SourceName,
        OutputName,
    };

    enum class DecoderFunctionReleaseOutputParam : uint8_t {
        SourceName,
        OutputName,
    };

    enum class DecoderFunctionSetActiveSourceParam : uint8_t {
        OutputName,
        SourceName,
    };

    enum class DecoderFunctionGetActiveSourceParam : uint8_t {
        OutputName,
    };

    enum class DecoderFunctionOpenStreamParam : uint8_t {
        SourceName,
        OutputName,
        Config,
    };

    enum class DecoderFunctionCloseStreamParam : uint8_t {
        SourceName,
        OutputName,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class PlaybackEventPlayStateChangedParam : uint8_t {
        State,
    };

    enum class PlaybackEventVolumeChangedParam : uint8_t {
        Volume,
    };

    enum class PlaybackEventMuteChangedParam : uint8_t {
        IsMuted,
    };

    enum class EncoderEventAFEEventHappenedParam : uint8_t {
        Event,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema playback_function_schema_play()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::Play),
            .description = "Play audio from a URL. Supports loop and interrupt playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionPlayParam::Url),
                    .description = "Audio URL, for example: \"file://littlefs/example.mp3\".",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionPlayParam::Config),
                    .description = (boost::format("Playback config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(PlayUrlConfig{})).str(),
                    .type = FunctionValueType::Object,
                    .default_value = FunctionValue(BROOKESIA_DESCRIBE_TO_JSON(PlayUrlConfig{}).as_object()),
                }
            },
        };
    }

    static FunctionSchema playback_function_schema_play_list()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::PlayUrls),
            .description = "Play audio from multiple URLs. Supports loop and interrupt playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionPlayUrlsParam::Urls),
                    .description = (boost::format("Audio URL list. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                        "file://littlefs/example1.mp3",
                        "file://littlefs/example2.mp3"
                    }))).str(),
                    .type = FunctionValueType::Array,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionPlayUrlsParam::Config),
                    .description = (boost::format("Playback config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(PlayUrlConfig{})).str(),
                    .type = FunctionValueType::Object,
                    .default_value = FunctionValue(BROOKESIA_DESCRIBE_TO_JSON(PlayUrlConfig{}).as_object()),
                }
            },
        };
    }

    static FunctionSchema playback_function_schema_pause()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::Pause),
            .description = "Pause playback.",
        };
    }

    static FunctionSchema playback_function_schema_resume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::Resume),
            .description = "Resume playback.",
        };
    }

    static FunctionSchema playback_function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::Stop),
            .description = "Stop playback.",
        };
    }

    static FunctionSchema playback_function_schema_set_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::SetVolume),
            .description = "Set target playback volume percentage.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionSetVolumeParam::Volume),
                    .description = "Playback volume percentage in range [0, 100].",
                    .type = FunctionValueType::Number,
                }
            },
        };
    }

    static FunctionSchema playback_function_schema_get_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::GetVolume),
            .description = "Get target playback volume percentage [0, 100].",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Number,
                .description = "Playback volume percentage in range [0, 100].",
            },
        };
    }

    static FunctionSchema playback_function_schema_set_mute()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::SetMute),
            .description = "Set whether audio playback is muted.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionSetMuteParam::Enable),
                    .description = "True to mute playback, false to unmute it.",
                    .type = FunctionValueType::Boolean,
                }
            },
        };
    }

    static FunctionSchema playback_function_schema_get_mute()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::GetMute),
            .description = "Get whether audio playback is muted.",
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Boolean,
                .description = "True when muted, false when unmuted.",
            },
        };
    }

    static FunctionSchema playback_function_schema_load_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::LoadData),
            .description = "Load persisted audio playback state, including volume and mute.",
        };
    }

    static FunctionSchema playback_function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackFunctionId::ResetData),
            .description = "Reset persisted audio playback state, including volume and mute.",
        };
    }

    static FunctionSchema encoder_function_schema_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Start),
            .description = "Start audio encoder.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionStartParam::Config),
                    .description = (boost::format("Audio encoder config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(EncoderDynamicConfig{})).str(),
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema encoder_function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Stop),
            .description = "Stop audio encoder.",
        };
    }

    static FunctionSchema encoder_function_schema_pause()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Pause),
            .description = "Pause audio encoder.",
        };
    }

    static FunctionSchema encoder_function_schema_resume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::Resume),
            .description = "Resume audio encoder.",
        };
    }

    static FunctionSchema encoder_function_schema_pause_wake_end()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::PauseWakeEnd),
            .description = "Pause pending AFE WakeEnd event without pausing audio capture.",
            .require_scheduler = false,
        };
    }

    static FunctionSchema encoder_function_schema_resume_wake_end()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::ResumeWakeEnd),
            .description = "Resume pending AFE WakeEnd event without resuming audio capture.",
            .require_scheduler = false,
        };
    }

    static FunctionSchema encoder_function_schema_get_afe_wake_words()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderFunctionId::GetAFEWakeWords),
            .description = "Get AFE wake words.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                    "ni hao xiao zhi",
                    "hello brookesia"
                }))).str(),
            },
        };
    }

    static FunctionSchema decoder_function_schema_get_outputs()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::GetOutputs),
            .description = "Get audio output list.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<OutputInfo>({
                    OutputInfo{
                        .id = 0,
                        .name = "Speaker0",
                        .role = "speaker",
                        .sample_rates = {16000, 22050, 44100, 48000},
                        .channels = {1, 2},
                        .sample_bits = {16},
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema decoder_function_schema_get_sources()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::GetSources),
            .description = "Get registered audio sources.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<SourceInfo>({
                    SourceInfo{
                        .id = 1,
                        .name = "TTS",
                        .role = "speech",
                        .preferred_outputs = {"Speaker0"},
                        .priority = 10,
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema decoder_function_schema_register_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::RegisterSource),
            .description = "Register an audio source.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionRegisterSourceParam::Source),
                    .description = (boost::format("Source info. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((SourceInfo{
                        .name = "TTS",
                        .role = "speech",
                        .preferred_outputs = {"Speaker0"},
                    }))).str(),
                    .type = FunctionValueType::Object,
                },
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Number,
                .description = "Registered source id.",
            },
        };
    }

    static FunctionSchema decoder_function_schema_unregister_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::UnregisterSource),
            .description = "Unregister an audio source by name.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionUnregisterSourceParam::SourceName),
                    .description = "Source name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema decoder_function_schema_request_output()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::RequestOutput),
            .description = "Request an output for an audio source.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionRequestOutputParam::SourceName),
                    .description = "Source name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionRequestOutputParam::OutputName),
                    .description = "Output name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema decoder_function_schema_release_output()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::ReleaseOutput),
            .description = "Release an output from an audio source.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionReleaseOutputParam::SourceName),
                    .description = "Source name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionReleaseOutputParam::OutputName),
                    .description = "Output name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema decoder_function_schema_set_active_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::SetActiveSource),
            .description = "Set the active audio source for an output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionSetActiveSourceParam::OutputName),
                    .description = "Output name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionSetActiveSourceParam::SourceName),
                    .description = "Source name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema decoder_function_schema_get_active_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::GetActiveSource),
            .description = "Get the active audio source name for an output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionGetActiveSourceParam::OutputName),
                    .description = "Output name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = "Active source name, or an empty string when no source is active.",
            },
        };
    }

    static FunctionSchema decoder_function_schema_open_stream()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::OpenStream),
            .description = "Open a direct audio stream for a source and output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionOpenStreamParam::SourceName),
                    .description = "Source name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionOpenStreamParam::OutputName),
                    .description = "Output name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionOpenStreamParam::Config),
                    .description = (boost::format("Stream config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(StreamConfig{})).str(),
                    .type = FunctionValueType::Object,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema decoder_function_schema_close_stream()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionId::CloseStream),
            .description = "Close a direct audio stream for a source and output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionCloseStreamParam::SourceName),
                    .description = "Source name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(DecoderFunctionCloseStreamParam::OutputName),
                    .description = "Output name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema playback_event_schema_play_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackEventId::PlayStateChanged),
            .description = "Emitted when playback state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackEventPlayStateChangedParam::State),
                    .description = (boost::format("Playback state. Allowed values: %1%") %
                    BROOKESIA_DESCRIBE_TO_STR(std::vector<PlayState>({
                        PlayState::Idle, PlayState::Playing, PlayState::Paused
                    }))).str(),
                    .type = EventItemType::String
                }
            },
        };
    }

    static EventSchema playback_event_schema_volume_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackEventId::VolumeChanged),
            .description = "Emitted when the target audio playback volume changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackEventVolumeChangedParam::Volume),
                    .description = "Current target playback volume percentage [0, 100].",
                    .type = EventItemType::Number,
                }
            },
        };
    }

    static EventSchema playback_event_schema_mute_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackEventId::MuteChanged),
            .description = "Emitted when the target audio playback mute state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(PlaybackEventMuteChangedParam::IsMuted),
                    .description = "Whether audio playback is currently muted. True if muted, false if unmuted.",
                    .type = EventItemType::Boolean,
                }
            },
        };
    }

    static EventSchema encoder_event_schema_afe_event_happened()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventId::AFEEventHappened),
            .description = "Emitted when an AFE event occurs.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EncoderEventAFEEventHappenedParam::Event),
                    .description = (boost::format("AFE event. Allowed values: %1%") %
                    BROOKESIA_DESCRIBE_TO_STR(std::vector<AFE_Event>({
                        AFE_Event::VAD_Start, AFE_Event::VAD_End, AFE_Event::WakeStart, AFE_Event::WakeEnd
                    }))).str(),
                    .type = EventItemType::String
                }
            },
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by helpers ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static std::span<const FunctionSchema> get_playback_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(PlaybackFunctionId::Max)>
        FUNCTION_SCHEMAS = {{
                playback_function_schema_play(),
                playback_function_schema_play_list(),
                playback_function_schema_pause(),
                playback_function_schema_resume(),
                playback_function_schema_stop(),
                playback_function_schema_set_volume(),
                playback_function_schema_get_volume(),
                playback_function_schema_set_mute(),
                playback_function_schema_get_mute(),
                playback_function_schema_load_data(),
                playback_function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_playback_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(PlaybackEventId::Max)> EVENT_SCHEMAS = {{
                playback_event_schema_play_state_changed(),
                playback_event_schema_volume_changed(),
                playback_event_schema_mute_changed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }

    static std::span<const FunctionSchema> get_encoder_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EncoderFunctionId::Max)>
        FUNCTION_SCHEMAS = {{
                encoder_function_schema_start(),
                encoder_function_schema_stop(),
                encoder_function_schema_pause(),
                encoder_function_schema_resume(),
                encoder_function_schema_pause_wake_end(),
                encoder_function_schema_resume_wake_end(),
                encoder_function_schema_get_afe_wake_words(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_encoder_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EncoderEventId::Max)> EVENT_SCHEMAS = {{
                encoder_event_schema_afe_event_happened(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }

    static std::span<const FunctionSchema> get_decoder_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(DecoderFunctionId::Max)>
        FUNCTION_SCHEMAS = {{
                decoder_function_schema_get_outputs(),
                decoder_function_schema_get_sources(),
                decoder_function_schema_register_source(),
                decoder_function_schema_unregister_source(),
                decoder_function_schema_request_output(),
                decoder_function_schema_release_output(),
                decoder_function_schema_set_active_source(),
                decoder_function_schema_get_active_source(),
                decoder_function_schema_open_stream(),
                decoder_function_schema_close_stream(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
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
BROOKESIA_DESCRIBE_STRUCT(AudioPlayUrlConfig, (), (interrupt, delay_ms, loop_count, loop_interval_ms, timeout_ms));
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
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackFunctionPlayParam, Url, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackFunctionPlayUrlsParam, Urls, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackFunctionSetVolumeParam, Volume);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackFunctionSetMuteParam, Enable);
BROOKESIA_DESCRIBE_ENUM(Audio::EncoderFunctionStartParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionRegisterSourceParam, Source);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionUnregisterSourceParam, SourceName);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionRequestOutputParam, SourceName, OutputName);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionReleaseOutputParam, SourceName, OutputName);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionSetActiveSourceParam, OutputName, SourceName);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionGetActiveSourceParam, OutputName);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionOpenStreamParam, SourceName, OutputName, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::DecoderFunctionCloseStreamParam, SourceName, OutputName);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackEventPlayStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackEventVolumeChangedParam, Volume);
BROOKESIA_DESCRIBE_ENUM(Audio::PlaybackEventMuteChangedParam, IsMuted);
BROOKESIA_DESCRIBE_ENUM(Audio::EncoderEventAFEEventHappenedParam, Event);

} // namespace esp_brookesia::service::helper
