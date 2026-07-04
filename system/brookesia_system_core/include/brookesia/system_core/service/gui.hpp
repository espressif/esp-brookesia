/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core/app/types.hpp"

namespace esp_brookesia::system::core {

class System;

class SystemGuiHelper {
public:
    enum class FunctionId : uint8_t {
        SetBinding,
        SetBindings,
        GetBinding,
        GetConstant,
        SetText,
        SetValue,
        SetChecked,
        CreateView,
        DestroyView,
        SubscribeAction,
        TriggerScreenFlow,
        GetScreenFlowState,
        GetViewFrame,
        SetViewSrc,
        StartViewAnimation,
        StopAnimation,
        ScrollToView,
        ExecuteBatch,
        Max,
    };
    enum class EventId : uint8_t {
        Action,
        Max,
    };
    enum class BindingParam : uint8_t {
        Path,
        Key,
        Value,
    };
    enum class BindingsParam : uint8_t {
        Updates,
    };
    enum class PathParam : uint8_t {
        Path,
    };
    enum class TextParam : uint8_t {
        Path,
        Text,
    };
    enum class ValueParam : uint8_t {
        Path,
        Value,
    };
    enum class CheckedParam : uint8_t {
        Path,
        Checked,
    };
    enum class CreateViewParam : uint8_t {
        TemplateId,
        ParentPath,
        InstanceId,
    };
    enum class ActionParam : uint8_t {
        Action,
    };
    enum class SourceParam : uint8_t {
        Path,
        Src,
    };
    enum class AnimationParam : uint8_t {
        Path,
        Animation,
    };
    enum class AnimationIdParam : uint8_t {
        AnimationId,
    };
    enum class ScrollParam : uint8_t {
        Path,
        Animated,
    };
    enum class BatchParam : uint8_t {
        Commands,
    };
    enum class ScreenFlowParam : uint8_t {
        ScreenFlow,
        Action,
    };

    static constexpr std::string_view get_name()
    {
        return BROOKESIA_SYSTEM_CORE_GUI_SERVICE_NAME;
    }
    static std::span<const service::FunctionSchema> get_function_schemas();
    static std::span<const service::EventSchema> get_event_schemas();
};
BROOKESIA_DESCRIBE_ENUM(
    SystemGuiHelper::FunctionId,
    SetBinding,
    SetBindings,
    GetBinding,
    GetConstant,
    SetText,
    SetValue,
    SetChecked,
    CreateView,
    DestroyView,
    SubscribeAction,
    TriggerScreenFlow,
    GetScreenFlowState,
    GetViewFrame,
    SetViewSrc,
    StartViewAnimation,
    StopAnimation,
    ScrollToView,
    ExecuteBatch,
    Max
)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::EventId, Action, Max)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::BindingParam, Path, Key, Value)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::BindingsParam, Updates)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::PathParam, Path)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::TextParam, Path, Text)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::ValueParam, Path, Value)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::CheckedParam, Path, Checked)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::CreateViewParam, TemplateId, ParentPath, InstanceId)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::ActionParam, Action)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::SourceParam, Path, Src)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::AnimationParam, Path, Animation)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::AnimationIdParam, AnimationId)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::ScrollParam, Path, Animated)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::BatchParam, Commands)
BROOKESIA_DESCRIBE_ENUM(SystemGuiHelper::ScreenFlowParam, ScreenFlow, Action)

class GuiService final: public service::ServiceBase {
public:
    explicit GuiService(System &system);

private:
    std::vector<service::FunctionSchema> get_function_schemas() override;
    std::vector<service::EventSchema> get_event_schemas() override;
    FunctionHandlerMap get_function_handlers() override;

    service::FunctionResult set_binding(service::FunctionParameterMap &&params);
    service::FunctionResult set_bindings(service::FunctionParameterMap &&params);
    service::FunctionResult get_binding(service::FunctionParameterMap &&params);
    service::FunctionResult get_constant(service::FunctionParameterMap &&params);
    service::FunctionResult set_text(service::FunctionParameterMap &&params);
    service::FunctionResult set_value(service::FunctionParameterMap &&params);
    service::FunctionResult set_checked(service::FunctionParameterMap &&params);
    service::FunctionResult create_view(service::FunctionParameterMap &&params);
    service::FunctionResult destroy_view(service::FunctionParameterMap &&params);
    service::FunctionResult subscribe_action(service::FunctionParameterMap &&params);
    service::FunctionResult trigger_screen_flow(service::FunctionParameterMap &&params);
    service::FunctionResult get_screen_flow_state(service::FunctionParameterMap &&params);
    service::FunctionResult get_view_frame(service::FunctionParameterMap &&params);
    service::FunctionResult set_view_src(service::FunctionParameterMap &&params);
    service::FunctionResult start_view_animation(service::FunctionParameterMap &&params);
    service::FunctionResult stop_animation(service::FunctionParameterMap &&params);
    service::FunctionResult scroll_to_view(service::FunctionParameterMap &&params);
    service::FunctionResult execute_batch(service::FunctionParameterMap &&params);

    System &system_;
};

} // namespace esp_brookesia::system::core
