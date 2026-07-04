/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "boost/format.hpp"
#include "brookesia/hal_interface/interfaces/display/panel.hpp"
#include "brookesia/hal_interface/interfaces/display/touch.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

class Display: public Base<Display> {
public:
    using PixelFormat = hal::display::PanelIface::PixelFormat;
    using TouchOperationMode = hal::display::TouchIface::OperationMode;

    enum class OutputSlot {
        HalPanel,
        Buffer,
    };

    struct OutputTouchCapability {
        uint32_t id = 0;
        std::string name;
        std::string instance;
        uint32_t max_points = 1;
        TouchOperationMode operation_mode = TouchOperationMode::Max;
    };

    struct OutputBacklightCapability {
        std::string instance;
        bool on_off_supported = false;
    };

    struct OutputInfo {
        uint32_t id = 0;
        std::string name;
        uint32_t width = 0;
        uint32_t height = 0;
        PixelFormat pixel_format = PixelFormat::RGB565;
        OutputSlot slot = OutputSlot::HalPanel;
        std::string panel_instance;
        std::string group_id;
        std::optional<OutputTouchCapability> touch = std::nullopt;
        std::optional<OutputBacklightCapability> backlight = std::nullopt;
    };

    struct SourceInfo {
        uint32_t id = 0;
        std::string name;
        std::string role;
        std::vector<std::string> preferred_outputs;
        int priority = 0;
    };

    struct TouchInfo {
        uint32_t id = 0;
        std::string name;
        uint32_t x_max = 0;
        uint32_t y_max = 0;
        uint32_t max_points = 1;
        TouchOperationMode operation_mode = TouchOperationMode::Max;
        std::string touch_instance;
        std::string group_id;
        std::vector<std::string> bound_outputs;
    };

    enum class TouchGestureEventType {
        Press,
        Pressing,
        Release,
    };

    enum class TouchGestureDirection {
        None,
        Up,
        Down,
        Left,
        Right,
    };

    enum class TouchGestureArea : uint8_t {
        Center     = 0,
        TopEdge    = (1 << 0),
        BottomEdge = (1 << 1),
        LeftEdge   = (1 << 2),
        RightEdge  = (1 << 3),
    };

    struct TouchGestureThreshold {
        uint8_t direction_angle = 45;
        uint16_t direction_vertical = 0;
        uint16_t direction_horizon = 0;
        uint16_t horizontal_edge = 0;
        uint16_t vertical_edge = 0;
        uint16_t duration_short_ms = 220;
        float speed_slow_px_per_ms = 0.6F;
    };

    struct TouchGestureConfig {
        bool enabled = false;
        uint16_t detect_period_ms = 20;
        bool direction_lock_enabled = true;
        uint16_t release_debounce_ms = 40;
        TouchGestureThreshold threshold;
    };

    struct TouchGestureInfo {
        uint32_t output_id = 0;
        std::string output_name;
        std::string touch_name;
        TouchGestureEventType event_type = TouchGestureEventType::Press;
        TouchGestureDirection direction = TouchGestureDirection::None;
        uint8_t start_area = static_cast<uint8_t>(TouchGestureArea::Center);
        uint8_t stop_area = static_cast<uint8_t>(TouchGestureArea::Center);
        int start_x = -1;
        int start_y = -1;
        int stop_x = -1;
        int stop_y = -1;
        uint32_t duration_ms = 0;
        float speed_px_per_ms = 0;
        float distance_px = 0;
        bool flags_slow_speed = false;
        bool flags_short_duration = false;
    };

    enum class SourceState {
        Registered,
        Requested,
        Granted,
        Dummy,
        Revoked,
    };

    enum class FunctionId {
        GetOutputs,
        GetSources,
        RegisterSource,
        UnregisterSource,
        RequestOutput,
        ReleaseOutput,
        SetActiveSource,
        GetActiveSource,
        SetActiveSourceRole,
        GetActiveRole,
        GetSourceRoles,
        SetTouchGestureConfig,
        GetTouchGestureConfig,
        SetBacklightBrightness,
        GetBacklightBrightness,
        SetBacklightOnOff,
        GetBacklightOnOff,
        LoadData,
        ResetData,
        Max,
    };

    enum class EventId {
        SourceStateChanged,
        ActiveSourceChanged,
        OutputRegistered,
        OutputUnregistered,
        TouchGesture,
        BacklightBrightnessChanged,
        BacklightOnOffChanged,
        Max,
    };

    enum class FunctionSetActiveSourceParam {
        OutputName,
        SourceName,
    };

    enum class FunctionRegisterSourceParam {
        Source,
    };

    enum class FunctionUnregisterSourceParam {
        SourceName,
    };

    enum class FunctionRequestOutputParam {
        SourceName,
        OutputName,
    };

    enum class FunctionReleaseOutputParam {
        SourceName,
        OutputName,
    };

    enum class FunctionGetActiveSourceParam {
        OutputName,
    };

    enum class FunctionSetActiveSourceRoleParam {
        OutputName,
        Role,
    };

    enum class FunctionGetActiveRoleParam {
        OutputName,
    };

    enum class FunctionSetTouchGestureConfigParam {
        OutputId,
        Config,
    };

    enum class FunctionGetTouchGestureConfigParam {
        OutputId,
    };

    enum class FunctionSetBacklightBrightnessParam {
        OutputId,
        Brightness,
    };

    enum class FunctionGetBacklightBrightnessParam {
        OutputId,
    };

    enum class FunctionSetBacklightOnOffParam {
        OutputId,
        On,
    };

    enum class FunctionGetBacklightOnOffParam {
        OutputId,
    };

    enum class FunctionResetDataParam {
        OutputId,
    };

    enum class FunctionLoadDataParam {
        OutputId,
    };

    enum class EventSourceStateChangedParam {
        SourceName,
        OutputName,
        State,
    };

    enum class EventActiveSourceChangedParam {
        OutputName,
        SourceName,
    };

    enum class EventOutputRegisteredParam {
        Info,
    };

    enum class EventOutputUnregisteredParam {
        OutputName,
    };

    enum class EventTouchGestureParam {
        Info,
    };

    enum class EventBacklightBrightnessChangedParam {
        OutputId,
        OutputName,
        Brightness,
    };

    enum class EventBacklightOnOffChangedParam {
        OutputId,
        OutputName,
        IsOn,
    };

    static constexpr std::string_view get_name()
    {
        return "Display";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_get_outputs(),
                function_schema_get_sources(),
                function_schema_register_source(),
                function_schema_unregister_source(),
                function_schema_request_output(),
                function_schema_release_output(),
                function_schema_set_active_source(),
                function_schema_get_active_source(),
                function_schema_set_active_source_role(),
                function_schema_get_active_role(),
                function_schema_get_source_roles(),
                function_schema_set_touch_gesture_config(),
                function_schema_get_touch_gesture_config(),
                function_schema_set_backlight_brightness(),
                function_schema_get_backlight_brightness(),
                function_schema_set_backlight_on_off(),
                function_schema_get_backlight_on_off(),
                function_schema_load_data(),
                function_schema_reset_data(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_source_state_changed(),
                event_schema_active_source_changed(),
                event_schema_output_registered(),
                event_schema_output_unregistered(),
                event_schema_touch_gesture(),
                event_schema_backlight_brightness_changed(),
                event_schema_backlight_on_off_changed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }

private:
    static FunctionSchema function_schema_get_outputs()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetOutputs),
            .description = "Get display outputs.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<OutputInfo>({
                    OutputInfo{
                        .id = 1,
                        .name = "Output0",
                        .width = 240,
                        .height = 240,
                        .pixel_format = PixelFormat::RGB565,
                        .slot = OutputSlot::HalPanel,
                        .panel_instance = "Display:Panel0",
                        .group_id = "main_display",
                        .touch = OutputTouchCapability{
                            .id = 1,
                            .name = "Touch0",
                            .instance = "Display:Touch0",
                            .max_points = 5,
                            .operation_mode = TouchOperationMode::Interrupt,
                        },
                        .backlight = OutputBacklightCapability{
                            .instance = "Display:Backlight0",
                            .on_off_supported = true,
                        },
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_get_sources()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetSources),
            .description = "Get registered display sources.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<SourceInfo>({
                    SourceInfo{
                        .id = 1,
                        .name = "LVGL",
                        .role = "gui",
                        .preferred_outputs = {"Output0"},
                        .priority = 0,
                    },
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_register_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::RegisterSource),
            .description = (boost::format("Register a display source. Example source: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE((SourceInfo{
                .name = "LVGL",
                .role = "gui",
                .preferred_outputs = {"Output0"},
                .priority = 0,
            }))).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRegisterSourceParam::Source),
                    .description = "Display source info object. The id field is assigned by Display service.",
                    .type = FunctionValueType::Object,
                },
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Number,
                .description = "Registered display source id.",
            },
        };
    }

    static FunctionSchema function_schema_unregister_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::UnregisterSource),
            .description = "Unregister a named display source.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionUnregisterSourceParam::SourceName),
                    .description = "Display source name.",
                    .type = FunctionValueType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_request_output()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::RequestOutput),
            .description = "Request permission for a source to draw to one output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRequestOutputParam::SourceName),
                    .description = "Registered source name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRequestOutputParam::OutputName),
                    .description = "Display output name. Empty string uses the first output.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string()),
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_release_output()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::ReleaseOutput),
            .description = "Release a source's request for one output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionReleaseOutputParam::SourceName),
                    .description = "Registered source name.",
                    .type = FunctionValueType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionReleaseOutputParam::OutputName),
                    .description = "Display output name. Empty string uses the first output.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string()),
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_set_active_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetActiveSource),
            .description = "Grant one display output to a named source. Empty source name clears the active source.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetActiveSourceParam::OutputName),
                    .description = "Display output name, for example Output0. Empty string uses the first output.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string()),
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetActiveSourceParam::SourceName),
                    .description = "Registered source name, or empty string to clear.",
                    .type = FunctionValueType::String,
                },
            },
        };
    }

    static FunctionSchema function_schema_get_active_source()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetActiveSource),
            .description = "Get the active source name for one display output. Returns empty string when no source "
            "is active.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetActiveSourceParam::OutputName),
                    .description = "Display output name, for example Output0. Empty string uses the first output.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string()),
                },
            },
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = "Active source name, or an empty string when no source is active.",
            },
        };
    }

    static FunctionSchema function_schema_set_active_source_role()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetActiveSourceRole),
            .description = "Grant one display output to the first registered source with the specified role.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetActiveSourceRoleParam::OutputName),
                    .description = "Display output name, for example Output0. Empty string uses the first output.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string()),
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetActiveSourceRoleParam::Role),
                    .description = "Registered source role.",
                    .type = FunctionValueType::String,
                },
            },
        };
    }

    static FunctionSchema function_schema_get_active_role()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetActiveRole),
            .description = "Get the active source role for one display output. Returns empty string when no source "
            "is active.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetActiveRoleParam::OutputName),
                    .description = "Display output name, for example Output0. Empty string uses the first output.",
                    .type = FunctionValueType::String,
                    .default_value = FunctionValue(std::string()),
                },
            },
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::String,
                .description = "Active source role, or an empty string when no source is active.",
            },
        };
    }

    static FunctionSchema function_schema_get_source_roles()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetSourceRoles),
            .description = "Get registered display source roles, de-duplicated in first-registration order.",
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Array,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(std::vector<std::string>({"gui", "video"}))).str(),
            },
        };
    }

    static FunctionSchema function_schema_set_touch_gesture_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetTouchGestureConfig),
            .description = (boost::format("Configure touch gesture detection for one output. Example config: %1%")
            % BROOKESIA_DESCRIBE_JSON_SERIALIZE(TouchGestureConfig{})).str(),
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetTouchGestureConfigParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs().",
                    .type = FunctionValueType::Number,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetTouchGestureConfigParam::Config),
                    .description = "Gesture configuration object. Zero threshold fields use output defaults.",
                    .type = FunctionValueType::Object,
                    .default_value = FunctionValue(BROOKESIA_DESCRIBE_TO_JSON(TouchGestureConfig{}).as_object()),
                },
            },
            .require_scheduler = false,
        };
    }

    static FunctionSchema function_schema_get_touch_gesture_config()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetTouchGestureConfig),
            .description = "Get touch gesture config for one output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetTouchGestureConfigParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs().",
                    .type = FunctionValueType::Number,
                },
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE(TouchGestureConfig{})).str(),
            },
        };
    }

    static FunctionSchema function_schema_set_backlight_brightness()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetBacklightBrightness),
            .description = "Set backlight brightness percentage for one display output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetBacklightBrightnessParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs().",
                    .type = FunctionValueType::Number,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetBacklightBrightnessParam::Brightness),
                    .description = "Backlight brightness percentage in range [0, 100].",
                    .type = FunctionValueType::Number,
                },
            },
        };
    }

    static FunctionSchema function_schema_get_backlight_brightness()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetBacklightBrightness),
            .description = "Get backlight brightness percentage for one display output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetBacklightBrightnessParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs().",
                    .type = FunctionValueType::Number,
                },
            },
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Number,
                .description = "Backlight brightness percentage in range [0, 100].",
            },
        };
    }

    static FunctionSchema function_schema_set_backlight_on_off()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::SetBacklightOnOff),
            .description = "Turn the backlight on or off for one display output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetBacklightOnOffParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs().",
                    .type = FunctionValueType::Number,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionSetBacklightOnOffParam::On),
                    .description = "True to turn on the backlight, false to turn it off.",
                    .type = FunctionValueType::Boolean,
                },
            },
        };
    }

    static FunctionSchema function_schema_get_backlight_on_off()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::GetBacklightOnOff),
            .description = "Get whether the backlight is enabled for one display output.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetBacklightOnOffParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs().",
                    .type = FunctionValueType::Number,
                },
            },
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Boolean,
                .description = "True when backlight is enabled, false otherwise.",
            },
        };
    }

    static FunctionSchema function_schema_reset_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::ResetData),
            .description = "Reset persisted Display state. OutputId 0 resets all backlight-bound outputs.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionResetDataParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs(), or 0 for all outputs.",
                    .type = FunctionValueType::Number,
                    .default_value = FunctionValue(0.0),
                },
            },
        };
    }

    static FunctionSchema function_schema_load_data()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(FunctionId::LoadData),
            .description = "Load persisted Display state. OutputId 0 loads all backlight-bound outputs.",
            .parameters = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionLoadDataParam::OutputId),
                    .description = "Runtime Display output id from GetOutputs(), or 0 for all outputs.",
                    .type = FunctionValueType::Number,
                    .default_value = FunctionValue(0.0),
                },
            },
        };
    }

    static EventSchema event_schema_source_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::SourceStateChanged),
            .description = "A display source state changed relative to an output.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSourceStateChangedParam::SourceName),
                    .description = "Source name.",
                    .type = EventItemType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSourceStateChangedParam::OutputName),
                    .description = "Output name, or empty string for source-global changes.",
                    .type = EventItemType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventSourceStateChangedParam::State),
                    .description = "New source state.",
                    .type = EventItemType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static EventSchema event_schema_active_source_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::ActiveSourceChanged),
            .description = "The active source for an output changed.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventActiveSourceChangedParam::OutputName),
                    .description = "Output name.",
                    .type = EventItemType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventActiveSourceChangedParam::SourceName),
                    .description = "Active source name, or empty string when cleared.",
                    .type = EventItemType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static EventSchema event_schema_output_registered()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::OutputRegistered),
            .description = "A dynamic display output was registered.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventOutputRegisteredParam::Info),
                    .description = "Registered output info.",
                    .type = EventItemType::Object,
                },
            },
            .require_scheduler = false,
        };
    }

    static EventSchema event_schema_output_unregistered()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::OutputUnregistered),
            .description = "A dynamic display output was unregistered.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventOutputUnregisteredParam::OutputName),
                    .description = "Display output name.",
                    .type = EventItemType::String,
                },
            },
            .require_scheduler = false,
        };
    }

    static EventSchema event_schema_touch_gesture()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::TouchGesture),
            .description = "A display output touch gesture changed state.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventTouchGestureParam::Info),
                    .description = "Gesture event payload.",
                    .type = EventItemType::Object,
                },
            },
            .require_scheduler = false,
        };
    }

    static EventSchema event_schema_backlight_brightness_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::BacklightBrightnessChanged),
            .description = "The backlight brightness for an output changed.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventBacklightBrightnessChangedParam::OutputId),
                    .description = "Runtime Display output id.",
                    .type = EventItemType::Number,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventBacklightBrightnessChangedParam::OutputName),
                    .description = "Output name.",
                    .type = EventItemType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventBacklightBrightnessChangedParam::Brightness),
                    .description = "Current target backlight brightness percentage [0, 100].",
                    .type = EventItemType::Number,
                },
            },
        };
    }

    static EventSchema event_schema_backlight_on_off_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_ENUM_TO_STR(EventId::BacklightOnOffChanged),
            .description = "The backlight on/off state for an output changed.",
            .items = {
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventBacklightOnOffChangedParam::OutputId),
                    .description = "Runtime Display output id.",
                    .type = EventItemType::Number,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventBacklightOnOffChangedParam::OutputName),
                    .description = "Output name.",
                    .type = EventItemType::String,
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventBacklightOnOffChangedParam::IsOn),
                    .description = "Whether display backlight is currently on. True if on, false if off.",
                    .type = EventItemType::Boolean,
                },
            },
        };
    }
};

BROOKESIA_DESCRIBE_ENUM(
    Display::FunctionId, GetOutputs, GetSources, RegisterSource, UnregisterSource, RequestOutput, ReleaseOutput,
    SetActiveSource, GetActiveSource, SetActiveSourceRole, GetActiveRole, GetSourceRoles, SetTouchGestureConfig,
    GetTouchGestureConfig, SetBacklightBrightness, GetBacklightBrightness, SetBacklightOnOff, GetBacklightOnOff,
    LoadData, ResetData, Max
);
BROOKESIA_DESCRIBE_ENUM(
    Display::EventId, SourceStateChanged, ActiveSourceChanged, OutputRegistered, OutputUnregistered, TouchGesture,
    BacklightBrightnessChanged, BacklightOnOffChanged, Max
);
BROOKESIA_DESCRIBE_ENUM(Display::OutputSlot, HalPanel, Buffer);
BROOKESIA_DESCRIBE_ENUM(Display::SourceState, Registered, Requested, Granted, Dummy, Revoked);
BROOKESIA_DESCRIBE_ENUM(Display::TouchGestureEventType, Press, Pressing, Release);
BROOKESIA_DESCRIBE_ENUM(Display::TouchGestureDirection, None, Up, Down, Left, Right);
BROOKESIA_DESCRIBE_ENUM(Display::TouchGestureArea, Center, TopEdge, BottomEdge, LeftEdge, RightEdge);
BROOKESIA_DESCRIBE_STRUCT(Display::OutputTouchCapability, (), (id, name, instance, max_points, operation_mode));
BROOKESIA_DESCRIBE_STRUCT(Display::OutputBacklightCapability, (), (instance, on_off_supported));
BROOKESIA_DESCRIBE_STRUCT(
    Display::OutputInfo, (),
    (id, name, width, height, pixel_format, slot, panel_instance, group_id, touch, backlight)
);
BROOKESIA_DESCRIBE_STRUCT(Display::SourceInfo, (), (id, name, role, preferred_outputs, priority));
BROOKESIA_DESCRIBE_STRUCT(
    Display::TouchInfo, (),
    (id, name, x_max, y_max, max_points, operation_mode, touch_instance, group_id, bound_outputs)
);
BROOKESIA_DESCRIBE_STRUCT(
    Display::TouchGestureThreshold, (),
    (
        direction_vertical, direction_horizon, direction_angle, horizontal_edge, vertical_edge, duration_short_ms,
        speed_slow_px_per_ms
    )
);
BROOKESIA_DESCRIBE_STRUCT(
    Display::TouchGestureConfig, (),
    (enabled, detect_period_ms, direction_lock_enabled, release_debounce_ms, threshold)
);
BROOKESIA_DESCRIBE_STRUCT(
    Display::TouchGestureInfo, (),
    (
        output_id, output_name, touch_name, event_type, direction, start_area, stop_area, start_x, start_y, stop_x, stop_y,
        duration_ms, speed_px_per_ms, distance_px, flags_slow_speed, flags_short_duration
    )
);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionSetActiveSourceParam, OutputName, SourceName);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionRegisterSourceParam, Source);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionUnregisterSourceParam, SourceName);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionRequestOutputParam, SourceName, OutputName);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionReleaseOutputParam, SourceName, OutputName);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionGetActiveSourceParam, OutputName);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionSetActiveSourceRoleParam, OutputName, Role);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionGetActiveRoleParam, OutputName);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionSetTouchGestureConfigParam, OutputId, Config);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionGetTouchGestureConfigParam, OutputId);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionSetBacklightBrightnessParam, OutputId, Brightness);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionGetBacklightBrightnessParam, OutputId);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionSetBacklightOnOffParam, OutputId, On);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionGetBacklightOnOffParam, OutputId);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionLoadDataParam, OutputId);
BROOKESIA_DESCRIBE_ENUM(Display::FunctionResetDataParam, OutputId);
BROOKESIA_DESCRIBE_ENUM(Display::EventSourceStateChangedParam, SourceName, OutputName, State);
BROOKESIA_DESCRIBE_ENUM(Display::EventActiveSourceChangedParam, OutputName, SourceName);
BROOKESIA_DESCRIBE_ENUM(Display::EventOutputRegisteredParam, Info);
BROOKESIA_DESCRIBE_ENUM(Display::EventOutputUnregisteredParam, OutputName);
BROOKESIA_DESCRIBE_ENUM(Display::EventTouchGestureParam, Info);
BROOKESIA_DESCRIBE_ENUM(Display::EventBacklightBrightnessChangedParam, OutputId, OutputName, Brightness);
BROOKESIA_DESCRIBE_ENUM(Display::EventBacklightOnOffChangedParam, OutputId, OutputName, IsOn);

} // namespace esp_brookesia::service::helper
