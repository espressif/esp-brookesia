/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

class ExpressionEmote: public Base<ExpressionEmote> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventMessageType {
        Idle,
        Speak,
        Listen,
        System,
        User,
        Battery,
        QRCode,
        Max,
    };

    enum class AssetSourceType {
        Path,
        PartitionLabel,
        Max,
    };

    struct AssetSource {
        std::string source;
        AssetSourceType type;
        bool flag_enable_mmap = false;
    };

    struct Config {
        uint32_t h_res = 0;
        uint32_t v_res = 0;
        size_t buf_pixels = 0;
        uint32_t fps = 0;
        int task_priority = 0;
        int task_stack = 0;
        int task_affinity = 0;
        bool task_stack_in_ext = false;
        bool flag_swap_color_bytes = false;
        bool flag_double_buffer = false;
        bool flag_buff_dma = false;
        bool flag_buff_spiram = false;
    };

    struct FlushReadyEventParam {
        int x_start = 0;
        int y_start = 0;
        int x_end = 0;
        int y_end = 0;
        const void *data = nullptr;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        SetConfig,
        LoadAssetsSource,
        SetEmoji,
        HideEmoji,
        SetAnimation,
        InsertAnimation,
        StopAnimation,
        WaitAnimationFrameDone,
        SetEventMessage,
        HideEventMessage,
        SetQrcode,
        HideQrcode,
        NotifyFlushFinished,
        Max,
    };

    enum class EventId {
        FlushReady,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionSetConfigParam {
        Config,
    };

    enum class FunctionLoadAssetsParam {
        Source,
    };

    enum class FunctionSetEmojiParam {
        Emoji,
    };

    enum class FunctionSetAnimationParam {
        Animation,
    };

    enum class FunctionInsertAnimationParam {
        Animation,
        DurationMs,
    };

    enum class FunctionSetQrcodeParam {
        Qrcode,
    };

    enum class FunctionWaitAnimationFrameDoneParam {
        TimeoutMs,
    };

    enum class FunctionSetEventMessageParam {
        Event,
        Message,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventFlushReadyParam {
        Param,
    };

private:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function schemas /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static FunctionSchema function_schema_set_configs()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetConfig),
            .description = "Set the configurations",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetConfigParam::Config),
                    .description = "Configuration",
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_load_assets()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::LoadAssetsSource),
            .description = "Load the assets from the specified source",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionLoadAssetsParam::Source),
                    .description = (boost::format("Source of the assets, should be a JSON object. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((AssetSource{
                        .source = "anim_icon",
                        .type = AssetSourceType::PartitionLabel,
                        .flag_enable_mmap = false,
                    }))).str(),
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_emoji()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetEmoji),
            .description = "Set the emoji. Animation will be hidden immediately",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEmojiParam::Emoji),
                    .description = "Name of the emoji to set",
                    .type = FunctionValueType::String
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_hide_emoji()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::HideEmoji),
            .description = "Hide the current emoji",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_animation()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAnimation),
            .description = "Set the animation. Emoji will be hidden immediately",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAnimationParam::Animation),
                    .description = "Name of the animation to set",
                    .type = FunctionValueType::String
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_insert_animation()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::InsertAnimation),
            .description = "Insert the animation. Animation will be hidden immediately and will "
            "be shown after the duration",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionInsertAnimationParam::Animation),
                    .description = "Name of the animation to insert",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionInsertAnimationParam::DurationMs),
                    .description = "Duration of the animation in milliseconds, will be auto-stopped after the duration",
                    .type = FunctionValueType::Number
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_stop_animation()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::StopAnimation),
            .description = "Stop the current animation. Animation will be hidden immediately",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_wait_animation_frame_done()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::WaitAnimationFrameDone),
            .description = "Wait for the animation every frame done",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionWaitAnimationFrameDoneParam::TimeoutMs),
                    .description = "Timeout in milliseconds, 0 means wait forever",
                    .type = FunctionValueType::Number,
                    .default_value = 0.0,
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_event_message()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetEventMessage),
            .description = "Set the message for the specified event of the emote system",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEventMessageParam::Event),
                    .description = (boost::format("Event to set, should be one of the following types: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<EventMessageType>({
                        EventMessageType::Idle, EventMessageType::Speak, EventMessageType::Listen,
                        EventMessageType::System, EventMessageType::User, EventMessageType::Battery,
                        EventMessageType::QRCode
                    }))).str(),
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEventMessageParam::Message),
                    .description = "Message to set",
                    .type = FunctionValueType::String,
                    .default_value = "",
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_hide_event_message()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::HideEventMessage),
            .description = "Hide the current event message",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_qrcode()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetQrcode),
            .description = "Set the QR code. Emoji and animation will be hidden immediately",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetQrcodeParam::Qrcode),
                    .description = "QR code to set",
                    .type = FunctionValueType::String
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_hide_qrcode()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::HideQrcode),
            .description = "Hide the current QR code. Emoji will be shown immediately",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_notify_flush_finished()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::NotifyFlushFinished),
            .description = "Notify the flush finished event of the emote system",
            .require_scheduler = false
        };
    }

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event schemas /////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static EventSchema event_schema_flush_ready()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::FlushReady),
            .description = "The flush ready event of the emote system",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventFlushReadyParam::Param),
                    .description = (boost::format("Parameter of the flush ready event, should be a JSON object. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((FlushReadyEventParam{
                        .x_start = 0,
                        .y_start = 0,
                        .x_end = 100,
                        .y_end = 100,
                        .data = reinterpret_cast<const void *>(0x12345678)
                    }))).str(),
                    .type = EventItemType::Object
                }
            },
            .require_scheduler = false
        };
    }

public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the functions required by the Base class /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    static constexpr std::string_view get_name()
    {
        return "Emote";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_set_configs(),
                function_schema_load_assets(),
                function_schema_set_emoji(),
                function_schema_hide_emoji(),
                function_schema_set_animation(),
                function_schema_insert_animation(),
                function_schema_stop_animation(),
                function_schema_wait_animation_frame_done(),
                function_schema_set_event_message(),
                function_schema_hide_event_message(),
                function_schema_set_qrcode(),
                function_schema_hide_qrcode(),
                function_schema_notify_flush_finished(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_flush_ready(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the describe macros //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::EventMessageType, Idle, Speak, Listen, System, User, Battery, QRCode, Max);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::AssetSourceType, Path, PartitionLabel, Max);
BROOKESIA_DESCRIBE_STRUCT(ExpressionEmote::AssetSource, (), (source, type, flag_enable_mmap));
BROOKESIA_DESCRIBE_STRUCT(
    ExpressionEmote::Config, (), (
        h_res, v_res, buf_pixels, fps, task_priority, task_stack, task_affinity, task_stack_in_ext,
        flag_swap_color_bytes, flag_double_buffer, flag_buff_dma, flag_buff_spiram
    )
);
BROOKESIA_DESCRIBE_STRUCT(ExpressionEmote::FlushReadyEventParam, (), (x_start, y_start, x_end, y_end, data));
BROOKESIA_DESCRIBE_ENUM(
    ExpressionEmote::FunctionId, SetConfig, LoadAssetsSource, SetEmoji, HideEmoji, SetAnimation, InsertAnimation,
    StopAnimation, WaitAnimationFrameDone, SetEventMessage, HideEventMessage, SetQrcode, HideQrcode,
    NotifyFlushFinished, Max
);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::EventId, FlushReady, Max);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionSetConfigParam, Config);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionLoadAssetsParam, Source);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionSetEmojiParam, Emoji);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionSetAnimationParam, Animation);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionInsertAnimationParam, Animation, DurationMs);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionWaitAnimationFrameDoneParam, TimeoutMs);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionSetEventMessageParam, Event, Message);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::FunctionSetQrcodeParam, Qrcode);
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::EventFlushReadyParam, Param);

} // namespace esp_brookesia::service::helper
