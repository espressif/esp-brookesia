/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "boost/format.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

class Nes: public Base<Nes> {
public:
    enum class State {
        Idle,
        Loaded,
        Running,
        Paused,
        Stopped,
        Error,
    };

    enum class VideoMode {
        Native,
        Fit,
        Fill,
    };

    enum class AudioMode {
        Disabled,
        Auto,
        Required,
    };

    struct GamepadState {
        bool up = false;
        bool down = false;
        bool left = false;
        bool right = false;
        bool a = false;
        bool b = false;
        bool select = false;
        bool start = false;
        bool x = false;
    };

    struct VideoArea {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct Config {
        std::string rom_path;
        std::string save_path;
        std::string display_output_name;
        std::string display_source_name = "NES";
        VideoArea video_area;
        VideoMode video_mode = VideoMode::Fit;
        AudioMode audio_mode = AudioMode::Auto;
        bool auto_activate_display = false;
    };

    enum class FunctionId {
        Load,
        Start,
        Pause,
        Resume,
        Stop,
        Reset,
        Save,
        SetGamepadState,
        GetState,
        Max,
    };

    enum class EventId {
        StateChanged,
        Error,
        SaveCompleted,
        Max,
    };

    enum class FunctionLoadParam {
        Config,
    };

    enum class FunctionSetGamepadStateParam {
        State,
    };

    enum class EventStateChangedParam {
        State,
    };

    enum class EventErrorParam {
        Message,
    };

    enum class EventSaveCompletedParam {
        SavePath,
    };

    static constexpr std::string_view get_name()
    {
        return "NES";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_load(),
                function_schema_start(),
                function_schema_pause(),
                function_schema_resume(),
                function_schema_stop(),
                function_schema_reset(),
                function_schema_save(),
                function_schema_set_gamepad_state(),
                function_schema_get_state(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_state_changed(),
                event_schema_error(),
                event_schema_save_completed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }

private:
    static FunctionSchema function_schema_load()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Load),
            .description = (boost::format("Load a NES ROM. Example config: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((Config{
                .rom_path = "/sdcard/roms/demo.nes",
                .save_path = "/sdcard/roms/demo_nes.save",
                .display_output_name = "Output0",
                .video_area = VideoArea{},
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionLoadParam::Config),
                    .description = "NES runtime configuration. video_area uses the full output when width/height are 0.",
                    .type = FunctionValueType::Object,
                },
            },
        };
    }

    static FunctionSchema function_schema_start()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Start),
            .description = "Start or continue the loaded NES runtime.",
        };
    }

    static FunctionSchema function_schema_pause()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Pause),
            .description = "Pause the NES runtime.",
        };
    }

    static FunctionSchema function_schema_resume()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Resume),
            .description = "Resume the NES runtime.",
        };
    }

    static FunctionSchema function_schema_stop()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Stop),
            .description = "Stop the NES runtime.",
        };
    }

    static FunctionSchema function_schema_reset()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Reset),
            .description = "Soft reset the loaded NES runtime.",
        };
    }

    static FunctionSchema function_schema_save()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::Save),
            .description = "Save SRAM to the configured save path.",
        };
    }

    static FunctionSchema function_schema_set_gamepad_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetGamepadState),
            .description = (boost::format("Set current NES gamepad state. Example: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(GamepadState{})).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetGamepadStateParam::State),
                    .description = "Gamepad state object.",
                    .type = FunctionValueType::Object,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_get_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetState),
            .description = "Get NES runtime state.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = (boost::format("Example: \"%1%\"")
                                % BROOKESIA_DESCRIBE_ENUM_TO_STR(State::Running)).str(),
            },
        };
    }

    static EventSchema event_schema_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::StateChanged),
            .description = "NES runtime state changed.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventStateChangedParam::State),
                    .description = "Runtime state.",
                    .type = EventItemType::String,
                },
            },
        };
    }

    static EventSchema event_schema_error()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::Error),
            .description = "NES runtime error.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventErrorParam::Message),
                    .description = "Error message.",
                    .type = EventItemType::String,
                },
            },
        };
    }

    static EventSchema event_schema_save_completed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::SaveCompleted),
            .description = "NES SRAM save completed.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSaveCompletedParam::SavePath),
                    .description = "Save path.",
                    .type = EventItemType::String,
                },
            },
        };
    }
};

BROOKESIA_DESCRIBE_ENUM(Nes::State, Idle, Loaded, Running, Paused, Stopped, Error);
BROOKESIA_DESCRIBE_ENUM(Nes::VideoMode, Native, Fit, Fill);
BROOKESIA_DESCRIBE_ENUM(Nes::AudioMode, Disabled, Auto, Required);
BROOKESIA_DESCRIBE_STRUCT(
    Nes::GamepadState, (), (up, down, left, right, a, b, select, start, x)
);
BROOKESIA_DESCRIBE_STRUCT(
    Nes::VideoArea, (), (x, y, width, height)
);
BROOKESIA_DESCRIBE_STRUCT(
    Nes::Config, (),
    (
        rom_path, save_path, display_output_name, display_source_name, video_area, video_mode, audio_mode,
        auto_activate_display
    )
);
BROOKESIA_DESCRIBE_ENUM(
    Nes::FunctionId, Load, Start, Pause, Resume, Stop, Reset, Save, SetGamepadState, GetState, Max
);
BROOKESIA_DESCRIBE_ENUM(Nes::EventId, StateChanged, Error, SaveCompleted, Max);
BROOKESIA_DESCRIBE_ENUM(Nes::FunctionLoadParam, Config);
BROOKESIA_DESCRIBE_ENUM(Nes::FunctionSetGamepadStateParam, State);
BROOKESIA_DESCRIBE_ENUM(Nes::EventStateChangedParam, State);
BROOKESIA_DESCRIBE_ENUM(Nes::EventErrorParam, Message);
BROOKESIA_DESCRIBE_ENUM(Nes::EventSaveCompletedParam, SavePath);

} // namespace esp_brookesia::service::helper
