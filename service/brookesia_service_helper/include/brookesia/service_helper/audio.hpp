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
 * @brief Helper schema definitions for the audio service.
 */
class Audio: public Base<Audio> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief  Mixer related configurations
     */
    struct MixerGainConfig {
        float initial_gain;   /*!< Initial gain value */
        float target_gain;    /*!< Target gain value */
        int transition_time;  /*!< Transition duration in milliseconds */
    };

    /**
     * @brief  Playback related configurations
     */
    struct PlaybackConfig {
        lib_utils::ThreadConfig player_task{
            .core_id = 0,
            .priority = 5,
            .stack_size = 4 * 1024,
            .stack_in_ext = true,
        };
        MixerGainConfig mixer_gain{
            .initial_gain = 0.6,
            .target_gain = 1.0,
            .transition_time = 1500,
        };
    };

    /**
     * @brief Playback control actions.
     */
    enum class PlayControlAction : uint8_t {
        Pause,
        Resume,
        Stop,
    };

    /**
     * @brief Playback states.
     */
    enum class PlayState : uint8_t {
        Idle,
        Playing,
        Paused,
    };

    /**
     * @brief Runtime options for `PlayUrl` and `PlayUrls`.
     */
    struct PlayUrlConfig {
        bool interrupt = true;          /*!< Whether current playback can be interrupted */
        uint32_t delay_ms = 0;          /*!< Delay before playback starts */
        uint32_t loop_count = 0;        /*!< Number of extra loops */
        uint32_t loop_interval_ms = 0;  /*!< Interval between loops */
        uint32_t timeout_ms = 0;        /*!< Timeout for finishing playback */
    };

    /**
     * @brief  Codec related configurations
     */
    enum class CodecFormat : uint8_t {
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

    /**
     * @brief  Encoder related configurations
     */
    struct EncoderStaticConfig {
        lib_utils::ThreadConfig recorder_task{
            .core_id = 0,
            .priority = 10,
            .stack_size = 4 * 1024,
            .stack_in_ext = true,
        };
        lib_utils::ThreadConfig fetcher_task{
            .name = "EncoderFetcher",
            .core_id = 1,
            .priority = 12,
            .stack_size = 6 * 1024,
            .stack_in_ext = true,
        };
    };
    struct EncoderExtraConfigOpus {
        bool enable_vbr;      /*!< Enable Variable Bit Rate (VBR) */
        uint32_t bitrate;     /*!< Bitrate in bps */
    };
    struct EncoderDynamicConfig {
        CodecFormat type;                                                     /*!< Encoder codec type */
        CodecGeneralConfig general;                                           /*!< Encoder common codec settings */
        std::variant<std::monostate, EncoderExtraConfigOpus> extra = std::monostate{}; /*!< Optional codec-specific settings */
        uint32_t fetch_interval_ms = 10;                                      /*!< Encoder fetch interval in milliseconds */
        uint32_t fetch_data_size = 4096;                                      /*!< Encoder fetch size in bytes */
    };

    /**
     * @brief  Decoder related configurations
     */
    struct DecoderStaticConfig {
        lib_utils::ThreadConfig feeder_task{
            .core_id = 1,
            .priority = 5,
            .stack_size = 4 * 1024,
            .stack_in_ext = true,
        };
        MixerGainConfig mixer_gain{
            .initial_gain = 0.9,
            .target_gain = 1.0,
            .transition_time = 1500,
        };
    };
    struct DecoderDynamicConfig {
        CodecFormat type;           /*!< Decoder codec type */
        CodecGeneralConfig general; /*!< Decoder common codec settings */
    };

    /**
     * @brief  AFE related configurations
     */
    struct AFE_VAD_Config {
        uint8_t mode = 4;             /*!< VAD mode */
        uint32_t min_speech_ms = 64;  /*!< Minimum speech duration */
        uint32_t min_noise_ms = 1000; /*!< Minimum noise duration */
    };
    struct AFE_WakeNetConfig {
        std::string model_partition_label = "model"; /*!< Wake model partition label */
        std::string mn_language = "cn";              /*!< Wake model language */
        uint32_t start_timeout_ms = 30000;           /*!< Timeout before wake start */
        uint32_t end_timeout_ms = 10000;             /*!< Timeout before wake end */
    };
    struct AFE_Config {
        lib_utils::ThreadConfig feeder_task{
            .core_id = 1,
            .priority = 5,
            .stack_size = 40 * 1024,
            .stack_in_ext = true,
        };
        lib_utils::ThreadConfig fetcher_task{
            .core_id = 0,
            .priority = 5,
            .stack_size = 6 * 1024,
            .stack_in_ext = true,
        };
        std::optional<AFE_VAD_Config> vad = std::nullopt;
        std::optional<AFE_WakeNetConfig> wakenet = std::nullopt;
    };
    enum class AFE_Event : uint8_t {
        VAD_Start,
        VAD_End,
        WakeStart,
        WakeEnd,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Audio service function identifiers.
     */
    enum class FunctionId : uint8_t {
        SetPlaybackConfig,
        SetEncoderStaticConfig,
        SetDecoderStaticConfig,
        SetAFE_Config,
        GetAFE_WakeWords,
        PauseAFE_WakeupEnd,
        ResumeAFE_WakeupEnd,
        PlayUrl,
        PlayUrls,
        PlayControl,
        SetVolume,
        GetVolume,
        SetMute,
        StartEncoder,
        StopEncoder,
        PauseEncoder,
        ResumeEncoder,
        StartDecoder,
        StopDecoder,
        FeedDecoderData,
        ResetData,
        Max,
    };

    /**
     * @brief Audio service event identifiers.
     */
    enum class EventId : uint8_t {
        PlayStateChanged,
        AFE_EventHappened,
        EncoderDataReady,
        RecorderDataReady,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Parameter keys for `FunctionId::SetPlaybackConfig`.
     */
    enum class FunctionSetPlaybackConfigParam : uint8_t {
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetEncoderStaticConfig`.
     */
    enum class FunctionSetEncoderStaticConfigParam : uint8_t {
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetDecoderStaticConfig`.
     */
    enum class FunctionSetDecoderStaticConfigParam : uint8_t {
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetAFE_Config`.
     */
    enum class FunctionSetAFE_ConfigParam : uint8_t {
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::PlayUrl`.
     */
    enum class FunctionPlayUrlParam : uint8_t {
        Url,
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::PlayUrls`.
     */
    enum class FunctionPlayUrlsParam : uint8_t {
        Urls,
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::PlayControl`.
     */
    enum class FunctionPlayControlParam : uint8_t {
        Action,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetVolume`.
     */
    enum class FunctionSetVolumeParam : uint8_t {
        Volume,
    };

    /**
     * @brief Parameter keys for `FunctionId::SetMute`.
     */
    enum class FunctionSetMuteParam : uint8_t {
        Enable,
    };

    /**
     * @brief Parameter keys for `FunctionId::StartEncoder`.
     */
    enum class FunctionStartEncoderParam : uint8_t {
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::StartDecoder`.
     */
    enum class FunctionStartDecoderParam : uint8_t {
        Config,
    };

    /**
     * @brief Parameter keys for `FunctionId::FeedDecoderData`.
     */
    enum class FunctionFeedDecoderDataParam : uint8_t {
        Data,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Item keys for `EventId::PlayStateChanged`.
     */
    enum class EventPlayStateChangedParam : uint8_t {
        State,
    };

    /**
     * @brief Item keys for `EventId::AFE_EventHappened`.
     */
    enum class EventAFE_EventHappenedParam : uint8_t {
        Event,
    };

    /**
     * @brief Item keys for `EventId::EncoderDataReady`.
     */
    enum class EventEncoderDataReadyParam : uint8_t {
        Data,
    };

    /**
     * @brief Item keys for `EventId::RecorderDataReady`.
     */
    enum class EventRecorderDataReadyParam : uint8_t {
        Data,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_set_playback_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetPlaybackConfig),
            .description = "Set playback config. Call before starting the service.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetPlaybackConfigParam::Config),
                    .description = (boost::format("Playback config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(PlaybackConfig{})).str(),
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_set_encoder_static_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetEncoderStaticConfig),
            .description = "Set encoder static config. Call before starting the service.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEncoderStaticConfigParam::Config),
                    .description = (boost::format("Encoder static config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(EncoderStaticConfig{})).str(),
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_set_decoder_static_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetDecoderStaticConfig),
            .description = "Set decoder static config. Call before starting the service.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetDecoderStaticConfigParam::Config),
                    .description = (boost::format("Decoder static config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(DecoderStaticConfig{})).str(),
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_set_afe_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAFE_Config),
            .description = "Set AFE config. Call before starting the service.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAFE_ConfigParam::Config),
                    .description = (boost::format("AFE config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(AFE_Config{})).str(),
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_get_afe_wake_words()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetAFE_WakeWords),
            .description = (boost::format("Get AFE wake words. Return type: JSON array<string>. "
                                          "Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                "ni hao xiao zhi",
                "hello brookesia"
            }))).str(),
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_pause_afe_wakeup_end()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::PauseAFE_WakeupEnd),
            .description = "Pause AFE wakeup-end task.",
        };
    }

    static FunctionSchema function_schema_resume_afe_wakeup_end()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResumeAFE_WakeupEnd),
            .description = "Resume AFE wakeup-end task.",
        };
    }

    static FunctionSchema function_schema_play_url()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::PlayUrl),
            .description = "Play audio from a URL. Supports loop and interrupt playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlParam::Url),
                    .description = "Audio URL, for example: \"file://spiffs/example.mp3\".",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlParam::Config),
                    .description = (boost::format("Playback config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(PlayUrlConfig{})).str(),
                    .type = FunctionValueType::Object,
                    .default_value = FunctionValue(BROOKESIA_DESCRIBE_TO_JSON(PlayUrlConfig{}).as_object()),
                }
            },
        };
    }

    static FunctionSchema function_schema_play_urls()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::PlayUrls),
            .description = "Play audio from multiple URLs. Supports loop and interrupt playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlsParam::Urls),
                    .description = (boost::format("Audio URL list. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({
                        "file://spiffs/example1.mp3",
                        "file://spiffs/example2.mp3"
                    }))).str(),
                    .type = FunctionValueType::Array,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlsParam::Config),
                    .description = (boost::format("Playback config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(PlayUrlConfig{})).str(),
                    .type = FunctionValueType::Object,
                    .default_value = FunctionValue(BROOKESIA_DESCRIBE_TO_JSON(PlayUrlConfig{}).as_object()),
                }
            },
        };
    }

    static FunctionSchema function_schema_play_control()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::PlayControl),
            .description = "Control audio playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayControlParam::Action),
                    .description = (boost::format("Playback action. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<PlayControlAction>({
                        PlayControlAction::Pause, PlayControlAction::Resume, PlayControlAction::Stop
                    }))).str(),
                    .type = FunctionValueType::String
                }
            },
        };
    }

    static FunctionSchema function_schema_set_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetVolume),
            .description = "Set playback volume.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetVolumeParam::Volume),
                    .description = "Volume in range [0, 100].",
                    .type = FunctionValueType::Number
                }
            },
        };
    }

    static FunctionSchema function_schema_get_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetVolume),
            .description = "Get playback volume. Return type: number. Example: 70",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_mute()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetMute),
            .description = "Set playback mute.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetMuteParam::Enable),
                    .description = "Enable mute.",
                    .type = FunctionValueType::Boolean
                }
            },
        };
    }

    static FunctionSchema function_schema_start_encoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartEncoder),
            .description = "Start audio encoder.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionStartEncoderParam::Config),
                    .description = (boost::format("Audio encoder config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(EncoderDynamicConfig{})).str(),
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema function_schema_stop_encoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopEncoder),
            .description = "Stop audio encoder.",
        };
    }

    static FunctionSchema function_schema_pause_encoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::PauseEncoder),
            .description = "Pause audio encoder.",
        };
    }

    static FunctionSchema function_schema_resume_encoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResumeEncoder),
            .description = "Resume audio encoder.",
        };
    }

    static FunctionSchema function_schema_start_decoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartDecoder),
            .description = "Start audio decoder.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionStartDecoderParam::Config),
                    .description = (boost::format("Audio decoder config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE(DecoderDynamicConfig{})).str(),
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema function_schema_stop_decoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopDecoder),
            .description = "Stop audio decoder.",
        };
    }

    static FunctionSchema function_schema_feed_decoder_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::FeedDecoderData),
            .description = "Feed audio data to the decoder.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionFeedDecoderDataParam::Data),
                    .description = "Audio data to decode.",
                    .type = FunctionValueType::RawBuffer
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetData),
            .description = "Reset audio data. Includes player volume.",
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_play_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::PlayStateChanged),
            .description = "Emitted when playback state changes.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventPlayStateChangedParam::State),
                    .description = (boost::format("Playback state. Allowed values: %1%") %
                    BROOKESIA_DESCRIBE_TO_STR(std::vector<PlayState>({
                        PlayState::Idle, PlayState::Playing, PlayState::Paused
                    }))).str(),
                    .type = EventItemType::String
                }
            },
        };
    }

    static EventSchema event_schema_afe_event_happened()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::AFE_EventHappened),
            .description = "Emitted when an AFE event occurs.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAFE_EventHappenedParam::Event),
                    .description = (boost::format("AFE event. Allowed values: %1%")
                                    %
                    BROOKESIA_DESCRIBE_TO_STR(std::vector<AFE_Event>({
                        AFE_Event::VAD_Start, AFE_Event::VAD_End, AFE_Event::WakeStart, AFE_Event::WakeEnd
                    }))).str(),
                    .type = EventItemType::String
                }
            },
        };
    }

    static EventSchema event_schema_encoder_data_ready()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::EncoderDataReady),
            .description = "Emitted when encoder data is ready.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventEncoderDataReadyParam::Data),
                    .description = "Encoded audio data.",
                    .type = EventItemType::RawBuffer
                }
            },
            .require_scheduler = false,
        };
    }

    static EventSchema event_schema_recorder_data_ready()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::RecorderDataReady),
            .description = "Emitted when recorder data is ready.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventRecorderDataReadyParam::Data),
                    .description = "Recorded raw audio data.",
                    .type = EventItemType::RawBuffer
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
     * @brief Service name used by `ServiceManager`.
     *
     * @return std::string_view Stable service name.
     */
    static constexpr std::string_view get_name()
    {
        return "Audio";
    }

    /**
     * @brief Get function schemas exported by audio service.
     *
     * @return std::span<const FunctionSchema> Static function schema span.
     */
    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_set_playback_config(),
                function_schema_set_encoder_static_config(),
                function_schema_set_decoder_static_config(),
                function_schema_set_afe_config(),
                function_schema_get_afe_wake_words(),
                function_schema_pause_afe_wakeup_end(),
                function_schema_resume_afe_wakeup_end(),
                function_schema_play_url(),
                function_schema_play_urls(),
                function_schema_play_control(),
                function_schema_set_volume(),
                function_schema_get_volume(),
                function_schema_set_mute(),
                function_schema_start_encoder(),
                function_schema_stop_encoder(),
                function_schema_pause_encoder(),
                function_schema_resume_encoder(),
                function_schema_start_decoder(),
                function_schema_stop_decoder(),
                function_schema_feed_decoder_data(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    /**
     * @brief Get event schemas exported by audio service.
     *
     * @return std::span<const EventSchema> Static event schema span.
     */
    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_play_state_changed(),
                event_schema_afe_event_happened(),
                event_schema_encoder_data_ready(),
                event_schema_recorder_data_ready(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/**
 * @brief  Mixer related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::MixerGainConfig, (), (initial_gain, target_gain, transition_time));

/**
 * @brief  Playback related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::PlaybackConfig, (), (player_task, mixer_gain));
BROOKESIA_DESCRIBE_ENUM(Audio::PlayControlAction, Pause, Resume, Stop);
BROOKESIA_DESCRIBE_ENUM(Audio::PlayState, Idle, Playing, Paused);

/**
 * @brief  Codec related configurations
 */
BROOKESIA_DESCRIBE_ENUM(Audio::CodecFormat, PCM, OPUS, G711A, Max);
BROOKESIA_DESCRIBE_STRUCT(Audio::CodecGeneralConfig, (), (channels, sample_bits, sample_rate, frame_duration));

/**
 * @brief  Encoder related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::EncoderStaticConfig, (), (recorder_task, fetcher_task));
BROOKESIA_DESCRIBE_STRUCT(Audio::EncoderExtraConfigOpus, (), (enable_vbr, bitrate));
BROOKESIA_DESCRIBE_STRUCT(Audio::EncoderDynamicConfig, (), (type, general, extra, fetch_interval_ms, fetch_data_size));

/**
 * @brief  Decoder related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::DecoderStaticConfig, (), (feeder_task, mixer_gain));
BROOKESIA_DESCRIBE_STRUCT(Audio::DecoderDynamicConfig, (), (type, general));

/**
 * @brief  AFE related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::AFE_VAD_Config, (), (mode, min_speech_ms, min_noise_ms));
BROOKESIA_DESCRIBE_STRUCT(Audio::AFE_WakeNetConfig, (), (
                              model_partition_label, mn_language, start_timeout_ms, end_timeout_ms
                          ));
BROOKESIA_DESCRIBE_STRUCT(Audio::AFE_Config, (), (feeder_task, fetcher_task, vad, wakenet));
BROOKESIA_DESCRIBE_STRUCT(Audio::PlayUrlConfig, (), (interrupt, delay_ms, loop_count, loop_interval_ms, timeout_ms));
BROOKESIA_DESCRIBE_ENUM(Audio::AFE_Event, VAD_Start, VAD_End, WakeStart, WakeEnd, Max);

/**
 * @brief  Function related
 */
BROOKESIA_DESCRIBE_ENUM(
    Audio::FunctionId, SetPlaybackConfig, SetEncoderStaticConfig, SetDecoderStaticConfig,
    SetAFE_Config, GetAFE_WakeWords, PauseAFE_WakeupEnd, ResumeAFE_WakeupEnd,
    PlayUrl, PlayUrls, PlayControl, SetVolume, GetVolume, SetMute, StartEncoder,
    StopEncoder, PauseEncoder, ResumeEncoder, StartDecoder, StopDecoder, FeedDecoderData, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetPlaybackConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetEncoderStaticConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetDecoderStaticConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetAFE_ConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayUrlParam, Url, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayUrlsParam, Urls, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayControlParam, Action);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetVolumeParam, Volume);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetMuteParam, Enable);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionStartEncoderParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionStartDecoderParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionFeedDecoderDataParam, Data);

/**
 * @brief  Event related
 */
BROOKESIA_DESCRIBE_ENUM(
    Audio::EventId, PlayStateChanged, AFE_EventHappened, EncoderDataReady, RecorderDataReady, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::EventPlayStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(Audio::EventAFE_EventHappenedParam, Event);
BROOKESIA_DESCRIBE_ENUM(Audio::EventEncoderDataReadyParam, Data);
BROOKESIA_DESCRIBE_ENUM(Audio::EventRecorderDataReadyParam, Data);

} // namespace esp_brookesia::service::helper
