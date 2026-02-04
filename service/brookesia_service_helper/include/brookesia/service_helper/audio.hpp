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

class Audio: public Base<Audio> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief  Peripheral related configurations
     */
    struct PeripheralConfig {
        void *play_dev;
        void *rec_dev;
        uint8_t board_bits;
        uint8_t board_channels;
        uint32_t board_sample_rate;
        std::string mic_layout;
        float recorder_gain;
        std::map<uint8_t, float> recorder_channel_gains;
    };

    /**
     * @brief  Mixer related configurations
     */
    struct MixerGainConfig {
        float initial_gain;
        float target_gain;
        int transition_time;
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
        uint8_t volume_default = 70;
        uint8_t volume_min = 0;
        uint8_t volume_max = 100;
    };
    enum class PlayControlAction : uint8_t {
        Pause,
        Resume,
        Stop,
    };
    enum class PlayState : uint8_t {
        Idle,
        Playing,
        Paused,
    };
    struct PlayUrlConfig {
        bool interrupt = true;
        uint32_t delay_ms = 0;
        uint32_t loop_count = 0;
        uint32_t loop_interval_ms = 0;
        uint32_t timeout_ms = 0;
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
        CodecFormat type;
        CodecGeneralConfig general;
        std::variant<std::monostate, EncoderExtraConfigOpus> extra;
        uint32_t fetch_interval_ms = 10;
        uint32_t fetch_data_size = 4096;
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
        CodecFormat type;
        CodecGeneralConfig general;
    };

    /**
     * @brief  AFE related configurations
     */
    struct AFE_VAD_Config {
        uint8_t mode = 4;
        uint32_t min_speech_ms = 64;
        uint32_t min_noise_ms = 1000;
    };
    struct AFE_WakeNetConfig {
        std::string model_partition_label = "model";
        std::string mn_language = "cn";
        uint32_t start_timeout_ms = 3000;
        uint32_t end_timeout_ms = 10000;
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
    enum class FunctionId : uint8_t {
        SetPeripheralConfig,
        SetPlaybackConfig,
        SetEncoderStaticConfig,
        SetDecoderStaticConfig,
        SetAFE_Config,
        PlayUrl,
        PlayUrls,
        PlayControl,
        SetVolume,
        GetVolume,
        StartEncoder,
        StopEncoder,
        StartDecoder,
        StopDecoder,
        FeedDecoderData,
        Max,
    };

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
    enum class FunctionSetPeripheralConfigParam : uint8_t {
        Config,
    };

    enum class FunctionSetPlaybackConfigParam : uint8_t {
        Config,
    };

    enum class FunctionSetEncoderStaticConfigParam : uint8_t {
        Config,
    };

    enum class FunctionSetDecoderStaticConfigParam : uint8_t {
        Config,
    };

    enum class FunctionSetAFE_ConfigParam : uint8_t {
        Config,
    };

    enum class FunctionPlayUrlParam : uint8_t {
        Url,
        Config,
    };

    enum class FunctionPlayUrlsParam : uint8_t {
        Urls,
        Config,
    };

    enum class FunctionPlayControlParam : uint8_t {
        Action,
    };

    enum class FunctionSetVolumeParam : uint8_t {
        Volume,
    };

    enum class FunctionStartEncoderParam : uint8_t {
        Config,
    };

    enum class FunctionStartDecoderParam : uint8_t {
        Config,
    };

    enum class FunctionFeedDecoderDataParam : uint8_t {
        Data,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventPlayStateChangedParam : uint8_t {
        State,
    };

    enum class EventAFE_EventHappenedParam : uint8_t {
        Event,
    };

    enum class EventEncoderDataReadyParam : uint8_t {
        Data,
    };

    enum class EventRecorderDataReadyParam : uint8_t {
        Data,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_set_peripheral_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetPeripheralConfig),
            .description = "Set the peripheral config. This function should be called before the service start",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetPeripheralConfigParam::Config),
                    .description = "The peripheral config",
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_set_playback_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetPlaybackConfig),
            .description = "Set the playback config. This function should be called before the service start",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetPlaybackConfigParam::Config),
                    .description = "The playback config",
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
            .description = "Set the encoder static config. This function should be called before the service start",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEncoderStaticConfigParam::Config),
                    .description = "The encoder static config",
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
            .description = "Set the decoder static config. This function should be called before the service start",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetDecoderStaticConfigParam::Config),
                    .description = "The decoder static config",
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
            .description = "Set the AFE config. This function should be called before the service start",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAFE_ConfigParam::Config),
                    .description = "The AFE static config",
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_play_url()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::PlayUrl),
            .description = "Play an audio file from the specified URL. Also support loop playback and interrupt playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlParam::Url),
                    .description = "URL of the audio file to play, eg:'file://spiffs/example.mp3'",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlParam::Config),
                    .description = (boost::format("The configuration of the audio playback. Example: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(PlayUrlConfig{})).str(),
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
            .description = "Play audio files from the specified URLs. Also support loop playback and interrupt playback.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlsParam::Urls),
                    .description = (boost::format("URLs of the audio files to play. Example: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<std::string>({
                        "file://spiffs/example1.mp3",
                        "file://spiffs/example2.mp3"
                    }))).str(),
                    .type = FunctionValueType::Array,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayUrlsParam::Config),
                    .description = (boost::format("The configuration of the audio playback. Example: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(PlayUrlConfig{})).str(),
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
            .description = "Control the audio playback",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionPlayControlParam::Action),
                    .description = (boost::format("The action to control the audio playback, "
                                                  "should be one of the following: %1%")
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
            .description = "Set the volume of the audio playback",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetVolumeParam::Volume),
                    .description = "Volume value, range from 0 to 100",
                    .type = FunctionValueType::Number
                }
            },
        };
    }

    static FunctionSchema function_schema_get_volume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetVolume),
            .description = "Get the volume of the audio playback",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_start_encoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartEncoder),
            .description = "Start the audio encoder",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionStartEncoderParam::Config),
                    .description = "The configuration of the audio encoder",
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema function_schema_stop_encoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopEncoder),
            .description = "Stop the audio encoder",
        };
    }

    static FunctionSchema function_schema_start_decoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StartDecoder),
            .description = "Start the audio decoder",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionStartDecoderParam::Config),
                    .description = "The configuration of the audio decoder",
                    .type = FunctionValueType::Object
                }
            },
        };
    }

    static FunctionSchema function_schema_stop_decoder()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopDecoder),
            .description = "Stop the audio decoder",
        };
    }

    static FunctionSchema function_schema_feed_decoder_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::FeedDecoderData),
            .description = "Feed the audio data to the decoder",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionFeedDecoderDataParam::Data),
                    .description = "The audio data to feed to the decoder",
                    .type = FunctionValueType::RawBuffer
                }
            },
            .require_scheduler = false,
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_play_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::PlayStateChanged),
            .description = "Play state changed event",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventPlayStateChangedParam::State),
                    .description = (boost::format("Play state, should be one of the following: %1%") %
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
            .description = "AFE event happened event",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventAFE_EventHappenedParam::Event),
                    .description = (boost::format("The event that happened, should be one of the following: %1%") %
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
            .description = "Encoder data ready event",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventEncoderDataReadyParam::Data),
                    .description = "The audio data that is being encoded",
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
            .description = "Recorder data ready event",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventRecorderDataReadyParam::Data),
                    .description = "The raw audio data that is being recorded",
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
    static constexpr std::string_view get_name()
    {
        return "Audio";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_set_peripheral_config(),
                function_schema_set_playback_config(),
                function_schema_set_encoder_static_config(),
                function_schema_set_decoder_static_config(),
                function_schema_set_afe_config(),
                function_schema_play_url(),
                function_schema_play_urls(),
                function_schema_play_control(),
                function_schema_set_volume(),
                function_schema_get_volume(),
                function_schema_start_encoder(),
                function_schema_stop_encoder(),
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
 * @brief  Peripheral related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::PeripheralConfig, (), (
                              play_dev, rec_dev, board_bits, board_channels, board_sample_rate, mic_layout,
                              recorder_gain, recorder_channel_gains
                          ));

/**
 * @brief  Mixer related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::MixerGainConfig, (), (initial_gain, target_gain, transition_time));

/**
 * @brief  Playback related configurations
 */
BROOKESIA_DESCRIBE_STRUCT(Audio::PlaybackConfig, (), (player_task, mixer_gain, volume_default, volume_min, volume_max));
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
    Audio::FunctionId, SetPeripheralConfig, SetPlaybackConfig, SetEncoderStaticConfig, SetDecoderStaticConfig,
    SetAFE_Config, PlayUrl, PlayUrls, PlayControl, SetVolume, GetVolume, StartEncoder, StopEncoder,
    StartDecoder, StopDecoder, FeedDecoderData, Max
);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetPeripheralConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetPlaybackConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetEncoderStaticConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetDecoderStaticConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetAFE_ConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayUrlParam, Url, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayUrlsParam, Urls, Config);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionPlayControlParam, Action);
BROOKESIA_DESCRIBE_ENUM(Audio::FunctionSetVolumeParam, Volume);
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
