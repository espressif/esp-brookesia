/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the emote-expression service.
 */
class ExpressionEmote: public Base<ExpressionEmote> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /**
     * @brief Supported message categories rendered by the expression service.
     */
    enum class EventMessageType {
        Idle,
        Speak,
        Listen,
        System,
        User,
        Battery,
        Max,
    };

    /**
     * @brief Supported asset-source backends used when loading expression assets.
     */
    enum class AssetSourceType {
        Path,
        PartitionLabel,
        Max,
    };

    /**
     * @brief Description of one asset source used by the expression service.
     */
    struct AssetSource {
        std::string source; ///< Source identifier such as a path or partition label.
        AssetSourceType type; ///< How `source` should be interpreted.
        bool flag_enable_mmap = false; ///< Whether mmap-backed loading should be enabled.
    };

    /**
     * @brief Runtime display and task configuration for the expression service.
     */
    struct Config {
        uint32_t h_res = 0; ///< Horizontal resolution in pixels.
        uint32_t v_res = 0; ///< Vertical resolution in pixels.
        size_t buf_pixels = 0; ///< Display buffer size in pixels.
        uint32_t fps = 0; ///< Target render frame rate.
        int task_priority = 0; ///< Render task priority.
        int task_stack = 0; ///< Render task stack size in bytes.
        int task_affinity = 0; ///< Core affinity for the render task.
        bool task_stack_in_ext = false; ///< Whether the task stack should live in external memory.
        bool flag_swap_color_bytes = false; ///< Whether output color bytes must be swapped.
        bool flag_double_buffer = false; ///< Whether double buffering is enabled.
        bool flag_buff_dma = false; ///< Whether display buffers must be DMA-capable.
        bool flag_buff_spiram = false; ///< Whether display buffers may be allocated in SPIRAM.
    };

    /**
     * @brief Parameters delivered with the flush-ready event.
     */
    struct FlushReadyEventParam {
        int x_start = 0; ///< Left edge of the dirty region.
        int y_start = 0; ///< Top edge of the dirty region.
        int x_end = 0; ///< Right edge of the dirty region.
        int y_end = 0; ///< Bottom edge of the dirty region.
        const void *data = nullptr; ///< Pixel buffer for the dirty region.
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
        RefreshAll,
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
            .description = "Set emote config.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetConfigParam::Config),
                    .description = (boost::format("Config. Example: %1%")
                    % BROOKESIA_DESCRIBE_JSON_SERIALIZE((Config{
                        .h_res = 320,
                        .v_res = 240,
                        .buf_pixels = 320 * 24,
                        .fps = 30,
                        .task_priority = 5,
                        .task_stack = 4096,
                        .task_affinity = 0,
                        .task_stack_in_ext = true,
                        .flag_swap_color_bytes = false,
                        .flag_double_buffer = false,
                        .flag_buff_dma = false,
                        .flag_buff_spiram = true,
                    }))).str(),
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
            .description = "Load assets from the specified source.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionLoadAssetsParam::Source),
                    .description = (boost::format("Asset source as a JSON object. Example: %1%")
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
            .description = "Set emoji and hide animation immediately.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEmojiParam::Emoji),
                    .description = "Emoji name.",
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
            .description = "Hide current emoji.",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_animation()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetAnimation),
            .description = "Set animation and hide emoji immediately.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetAnimationParam::Animation),
                    .description = "Animation name.",
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
            .description = "Insert animation; it hides immediately and shows after the duration.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionInsertAnimationParam::Animation),
                    .description = "Animation name.",
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionInsertAnimationParam::DurationMs),
                    .description = "Animation duration in milliseconds. Stops automatically after this duration.",
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
            .description = "Stop current animation and hide it immediately.",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_wait_animation_frame_done()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::WaitAnimationFrameDone),
            .description = "Wait for each animation frame to finish.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionWaitAnimationFrameDoneParam::TimeoutMs),
                    .description = "Timeout in milliseconds. `0` means wait forever.",
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
            .description = "Set message for a specified emote event.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEventMessageParam::Event),
                    .description = (boost::format("Event type. Allowed values: %1%")
                    % BROOKESIA_DESCRIBE_TO_STR(std::vector<EventMessageType>({
                        EventMessageType::Idle, EventMessageType::Speak, EventMessageType::Listen,
                        EventMessageType::System, EventMessageType::User, EventMessageType::Battery
                    }))).str(),
                    .type = FunctionValueType::String
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetEventMessageParam::Message),
                    .description = "Message text.",
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
            .description = "Hide current event message.",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_set_qrcode()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::SetQrcode),
            .description = "Set QR code and hide emoji and animation immediately.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetQrcodeParam::Qrcode),
                    .description = "QR code content.",
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
            .description = "Hide current QR code and show emoji immediately.",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_notify_flush_finished()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::NotifyFlushFinished),
            .description = "Notify emote flush finished.",
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_refresh_all()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::RefreshAll),
            .description = "Refresh the screen.",
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
            .description = "Emitted when emote flush is ready.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventFlushReadyParam::Param),
                    .description = (boost::format("Flush-ready parameter as a JSON object. Example: %1%")
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
                function_schema_refresh_all(),
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
BROOKESIA_DESCRIBE_ENUM(ExpressionEmote::EventMessageType, Idle, Speak, Listen, System, User, Battery, Max);
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
    NotifyFlushFinished, RefreshAll, Max
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
