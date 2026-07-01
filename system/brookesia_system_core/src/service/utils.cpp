/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/service/utils.hpp"

#include <array>
#include <utility>

namespace esp_brookesia::system::core {

FunctionResult make_error(std::string error)
{
    return FunctionResult{
        .success = false,
        .error_message = std::move(error),
    };
}

FunctionResult make_success()
{
    return FunctionResult{.success = true};
}

std::expected<std::string, std::string> get_string_param(
    const FunctionParameterMap &params,
    std::string_view name
)
{
    auto it = params.find(std::string(name));
    if (it == params.end()) {
        return std::unexpected("Missing parameter: " + std::string(name));
    }
    const auto *value = std::get_if<std::string>(&it->second);
    if (value == nullptr) {
        return std::unexpected("Parameter must be string: " + std::string(name));
    }
    return *value;
}

std::expected<double, std::string> get_number_param(
    const FunctionParameterMap &params,
    std::string_view name
)
{
    auto it = params.find(std::string(name));
    if (it == params.end()) {
        return std::unexpected("Missing parameter: " + std::string(name));
    }
    const auto *value = std::get_if<double>(&it->second);
    if (value == nullptr) {
        return std::unexpected("Parameter must be number: " + std::string(name));
    }
    return *value;
}

std::expected<bool, std::string> get_bool_param(
    const FunctionParameterMap &params,
    std::string_view name
)
{
    auto it = params.find(std::string(name));
    if (it == params.end()) {
        return std::unexpected("Missing parameter: " + std::string(name));
    }
    const auto *value = std::get_if<bool>(&it->second);
    if (value == nullptr) {
        return std::unexpected("Parameter must be bool: " + std::string(name));
    }
    return *value;
}

std::expected<boost::json::object, std::string> get_object_param(
    const FunctionParameterMap &params,
    std::string_view name
)
{
    auto it = params.find(std::string(name));
    if (it == params.end()) {
        return std::unexpected("Missing parameter: " + std::string(name));
    }
    const auto *value = std::get_if<boost::json::object>(&it->second);
    if (value == nullptr) {
        return std::unexpected("Parameter must be object: " + std::string(name));
    }
    return *value;
}

std::expected<boost::json::array, std::string> get_array_param(
    const FunctionParameterMap &params,
    std::string_view name
)
{
    auto it = params.find(std::string(name));
    if (it == params.end()) {
        return std::unexpected("Missing parameter: " + std::string(name));
    }
    const auto *value = std::get_if<boost::json::array>(&it->second);
    if (value == nullptr) {
        return std::unexpected("Parameter must be array: " + std::string(name));
    }
    return *value;
}

std::expected<std::string, std::string> get_object_string(
    const boost::json::object &object,
    std::string_view name,
    std::string default_value
)
{
    auto it = object.find(name);
    if (it == object.end()) {
        return default_value;
    }
    if (!it->value().is_string()) {
        return std::unexpected("Object field must be string: " + std::string(name));
    }
    return std::string(it->value().as_string());
}

std::expected<int32_t, std::string> get_object_int(
    const boost::json::object &object,
    std::string_view name,
    int32_t default_value
)
{
    auto it = object.find(name);
    if (it == object.end()) {
        return default_value;
    }
    if (!it->value().is_number()) {
        return std::unexpected("Object field must be number: " + std::string(name));
    }
    if (it->value().is_int64()) {
        return static_cast<int32_t>(it->value().as_int64());
    }
    if (it->value().is_uint64()) {
        return static_cast<int32_t>(it->value().as_uint64());
    }
    return static_cast<int32_t>(it->value().as_double());
}

std::expected<bool, std::string> get_object_bool(
    const boost::json::object &object,
    std::string_view name,
    bool default_value
)
{
    auto it = object.find(name);
    if (it == object.end()) {
        return default_value;
    }
    if (!it->value().is_bool()) {
        return std::unexpected("Object field must be bool: " + std::string(name));
    }
    return it->value().as_bool();
}

std::expected<gui::Animation, std::string> animation_from_json(const boost::json::object &object)
{
    gui::Animation animation;
    auto id = get_object_string(object, "id");
    auto property = get_object_string(object, "property", BROOKESIA_DESCRIBE_TO_STR(animation.property));
    auto from_mode = get_object_string(object, "fromMode", BROOKESIA_DESCRIBE_TO_STR(animation.from_mode));
    auto from = get_object_int(object, "from", animation.from);
    auto to_mode = get_object_string(object, "toMode", BROOKESIA_DESCRIBE_TO_STR(animation.to_mode));
    auto to = get_object_int(object, "to", animation.to);
    auto duration = get_object_int(object, "duration", animation.duration);
    auto delay = get_object_int(object, "delay", animation.delay);
    auto easing = get_object_string(object, "easing", BROOKESIA_DESCRIBE_TO_STR(animation.easing));
    auto repeat = get_object_int(object, "repeat", animation.repeat);
    auto playback = get_object_bool(object, "playback", animation.playback);
    if (!id || !property || !from_mode || !from || !to_mode || !to || !duration || !delay || !easing || !repeat || !playback) {
        return std::unexpected(
                   !id ? id.error() :
                   (!property ? property.error() :
                    (!from_mode ? from_mode.error() :
                     (!from ? from.error() :
                      (!to_mode ? to_mode.error() :
                       (!to ? to.error() :
                        (!duration ? duration.error() :
                         (!delay ? delay.error() :
                          (!easing ? easing.error() :
                           (!repeat ? repeat.error() : playback.error())))))))))
               );
    }
    animation.id = *id;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*property, animation.property)) {
        return std::unexpected("Invalid animation property: " + *property);
    }
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*from_mode, animation.from_mode)) {
        return std::unexpected("Invalid animation fromMode: " + *from_mode);
    }
    if (animation.from_mode == gui::AnimationValueMode::Relative || animation.from_mode == gui::AnimationValueMode::Max) {
        return std::unexpected("Invalid animation fromMode: " + *from_mode);
    }
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*to_mode, animation.to_mode)) {
        return std::unexpected("Invalid animation toMode: " + *to_mode);
    }
    if (animation.to_mode == gui::AnimationValueMode::Current || animation.to_mode == gui::AnimationValueMode::Max) {
        return std::unexpected("Invalid animation toMode: " + *to_mode);
    }
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*easing, animation.easing)) {
        return std::unexpected("Invalid animation easing: " + *easing);
    }
    animation.from = *from;
    animation.to = *to;
    animation.duration = *duration;
    animation.delay = *delay;
    animation.repeat = *repeat;
    animation.playback = *playback;
    return animation;
}

const boost::json::value *find_object_field(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback
)
{
    auto it = object.find(primary);
    if (it != object.end()) {
        return &it->value();
    }
    it = object.find(fallback);
    if (it != object.end()) {
        return &it->value();
    }
    return nullptr;
}

std::expected<std::string, std::string> get_flexible_object_string(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback
)
{
    const auto *value = find_object_field(object, primary, fallback);
    if (value == nullptr) {
        return std::unexpected("Missing object field: " + std::string(primary));
    }
    if (!value->is_string()) {
        return std::unexpected("Object field must be string: " + std::string(primary));
    }
    return std::string(value->as_string());
}

std::expected<boost::json::object, std::string> get_flexible_object_object(
    const boost::json::object &object,
    std::string_view primary,
    std::string_view fallback,
    boost::json::object default_value
)
{
    const auto *value = find_object_field(object, primary, fallback);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_object()) {
        return std::unexpected("Object field must be object: " + std::string(primary));
    }
    return value->as_object();
}

std::expected<std::vector<gui::BindingValueUpdate>, std::string> parse_binding_updates(
    const boost::json::array &updates_array
)
{
    std::vector<gui::BindingValueUpdate> updates;
    updates.reserve(updates_array.size());
    for (const auto &value : updates_array) {
        if (!value.is_object()) {
            return std::unexpected("Each binding update must be an object");
        }
        const auto &object = value.as_object();
        auto path = get_object_string(object, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::BindingParam::Path));
        auto key = get_object_string(object, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::BindingParam::Key));
        auto binding_value = get_object_string(object, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::BindingParam::Value));
        if (!path || !key || !binding_value) {
            return std::unexpected(!path ? path.error() : (!key ? key.error() : binding_value.error()));
        }
        updates.push_back({
            .absolute_path = std::move(*path),
            .key = std::move(*key),
            .value = std::move(*binding_value),
        });
    }
    return updates;
}

std::expected<std::vector<GuiBatchCommand>, std::string> parse_gui_batch_commands(
    const boost::json::array &commands_array
)
{
    std::vector<GuiBatchCommand> commands;
    commands.reserve(commands_array.size());

    for (const auto &value : commands_array) {
        if (!value.is_object()) {
            return std::unexpected("Each GUI batch command must be an object");
        }
        const auto &object = value.as_object();
        auto name = get_flexible_object_string(object, "Name", "name");
        auto params = get_flexible_object_object(object, "Params", "params");
        if (!name || !params) {
            return std::unexpected(!name ? name.error() : params.error());
        }

        GuiBatchCommand command;
        if (*name == BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::FunctionId::SetBindings)) {
            auto updates_value = params->find(BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::BindingsParam::Updates));
            if (updates_value == params->end() || !updates_value->value().is_array()) {
                return std::unexpected("SetBindings command requires Updates array");
            }
            auto updates = parse_binding_updates(updates_value->value().as_array());
            if (!updates) {
                return std::unexpected(updates.error());
            }
            command.type = GuiBatchCommand::Type::SetBindings;
            command.binding_updates = std::move(*updates);
        } else if (*name == BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::FunctionId::SetViewSrc)) {
            auto path = get_object_string(*params, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::SourceParam::Path));
            auto src = get_object_string(*params, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::SourceParam::Src));
            if (!path || !src) {
                return std::unexpected(!path ? path.error() : src.error());
            }
            command.type = GuiBatchCommand::Type::SetViewSrc;
            command.path = std::move(*path);
            command.src = std::move(*src);
        } else if (*name == BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::FunctionId::StopAnimation)) {
            auto animation_id = get_object_int(
                                    *params,
                                    BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::AnimationIdParam::AnimationId)
                                );
            if (!animation_id) {
                return std::unexpected(animation_id.error());
            }
            command.type = GuiBatchCommand::Type::StopAnimation;
            command.animation_id = static_cast<gui::SubscriptionId>(*animation_id);
        } else if (*name == BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::FunctionId::StartViewAnimation)) {
            auto path = get_object_string(*params, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::AnimationParam::Path));
            auto animation_it = params->find(BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::AnimationParam::Animation));
            if (!path) {
                return std::unexpected(path.error());
            }
            if (animation_it == params->end() || !animation_it->value().is_object()) {
                return std::unexpected("StartViewAnimation command requires Animation object");
            }
            auto animation = animation_from_json(animation_it->value().as_object());
            if (!animation) {
                return std::unexpected(animation.error());
            }
            command.type = GuiBatchCommand::Type::StartViewAnimation;
            command.path = std::move(*path);
            command.animation = std::move(*animation);
        } else if (*name == BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::FunctionId::ScrollToView)) {
            auto path = get_object_string(*params, BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::ScrollParam::Path));
            auto animated = get_object_bool(
                                *params,
                                BROOKESIA_DESCRIBE_TO_STR(SystemGuiHelper::ScrollParam::Animated),
                                true
                            );
            if (!path || !animated) {
                return std::unexpected(!path ? path.error() : animated.error());
            }
            command.type = GuiBatchCommand::Type::ScrollToView;
            command.path = std::move(*path);
            command.animated = *animated;
        } else {
            return std::unexpected("Unsupported GUI batch command: " + *name);
        }
        commands.push_back(std::move(command));
    }

    return commands;
}

boost::json::object gui_batch_result_to_json(const GuiBatchResult &batch_result)
{
    boost::json::array results;
    for (const auto &result : batch_result.results) {
        boost::json::object object;
        object["Name"] = result.name;
        object["Success"] = result.success;
        if (!result.success) {
            object["ErrorMessage"] = result.error_message;
        }
        object["Data"] = result.data;
        results.push_back(std::move(object));
    }
    boost::json::object root;
    root["Success"] = batch_result.success;
    root["Results"] = std::move(results);
    return root;
}

AppId to_app_id(double value)
{
    return static_cast<AppId>(value);
}

bool is_native_system_app(const AppInfo &app)
{
    return app.manifest.kind == AppKind::Native;
}

std::expected<std::optional<AppInfo>, std::string> get_runtime_caller(System &system)
{
    auto app_id = system.get_current_runtime_app_owner();
    if (!app_id) {
        if (app_id.error() == "No active runtime app context") {
            return std::optional<AppInfo> {};
        }
        return std::unexpected(app_id.error());
    }
    auto app = system.get_app(*app_id);
    if (!app) {
        return std::unexpected("Runtime caller app not found");
    }
    return app;
}

boost::json::object app_info_to_json(const AppInfo &info, std::string_view language)
{
    boost::json::object manifest;
    manifest["id"] = info.manifest.id;
    manifest["name"] = resolve_app_display_name(info.manifest, language);
    boost::json::object names;
    for (const auto &[name_language, name] : info.manifest.localized_names) {
        names[name_language] = name;
    }
    manifest["names"] = std::move(names);
    manifest["version"] = info.manifest.version;
    manifest["kind"] = BROOKESIA_DESCRIBE_TO_STR(info.manifest.kind);
    manifest["visible"] = info.manifest.visible;
    manifest["icon_id"] = info.manifest.icon_id;
    boost::json::array supported_systems;
    for (const auto &system : info.manifest.supported_systems) {
        supported_systems.emplace_back(system);
    }
    manifest["supported_systems"] = std::move(supported_systems);
    manifest["icon_path"] = info.manifest.icon_path;

    boost::json::object root;
    root["app_id"] = info.app_id;
    root["manifest"] = std::move(manifest);
    root["state"] = BROOKESIA_DESCRIBE_TO_STR(info.state);
    root["last_error"] = info.last_error;
    return root;
}

FunctionSchema make_no_param_schema(SystemCoreHelper::FunctionId id, std::string description)
{
    return FunctionSchema{
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .require_scheduler = false,
    };
}

FunctionSchema make_app_id_schema(SystemCoreHelper::FunctionId id, std::string description)
{
    return FunctionSchema{
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemCoreHelper::AppIdParam::AppId),
                .description = "System app id",
                .type = FunctionValueType::Number,
            },
        },
        .require_scheduler = false,
    };
}

FunctionSchema make_timer_id_schema(SystemTimerHelper::FunctionId id, std::string description)
{
    return FunctionSchema{
        .name = BROOKESIA_DESCRIBE_TO_STR(id),
        .description = std::move(description),
        .parameters = {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(SystemTimerHelper::TimerParam::TimerId),
                .description = "App-owned timer id",
                .type = FunctionValueType::Number,
            },
        },
        .require_scheduler = false,
    };
}

} // namespace esp_brookesia::system::core
