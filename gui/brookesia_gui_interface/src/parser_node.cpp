/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/parser_impl.hpp"

namespace esp_brookesia::gui::parser_detail {



std::expected<std::string, std::string> parse_event_effect_value(
    const boost::json::object &object,
    std::string_view key)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return std::unexpected("Field '" + std::string(key) + "' is required");
    }
    if (value->is_string()) {
        return std::string(value->as_string().c_str());
    }
    if (value->is_bool()) {
        return value->as_bool() ? "true" : "false";
    }
    if (value->is_int64()) {
        return std::to_string(value->as_int64());
    }
    if (value->is_uint64()) {
        return std::to_string(value->as_uint64());
    }
    if (value->is_double()) {
        return boost::json::serialize(*value);
    }
    if (value->is_array() || value->is_object()) {
        return boost::json::serialize(*value);
    }
    return std::string();
}

std::expected<EventPropertyUpdate, std::string> parse_event_property_update(
    const boost::json::object &object,
    std::string_view default_target)
{
    EventPropertyUpdate update;
    auto target = parse_string_field(object, "target", std::string(default_target));
    if (!target) {
        return std::unexpected(target.error());
    }
    update.target = *target;

    auto field = parse_string_field(object, "field");
    if (!field || field->empty()) {
        return std::unexpected(!field ? field.error() : "Event property update field must not be empty");
    }
    update.field = *field;

    auto value = parse_event_effect_value(object, "value");
    if (!value) {
        return std::unexpected(value.error());
    }
    update.value = *value;
    return update;
}

std::expected<EventEffect, std::string> parse_event_effect(const boost::json::object &object)
{
    EventEffect effect;
    auto type = parse_enum_field<EventEffectType>(object, "type", EventEffectType::EmitAction);
    if (!type) {
        return std::unexpected(type.error());
    }
    effect.type = *type;

    auto target = parse_string_field(object, "target", effect.target);
    if (!target) {
        return std::unexpected(target.error());
    }
    effect.target = *target;

    auto action = parse_string_field(object, "action");
    if (!action) {
        return std::unexpected(action.error());
    }
    effect.action = *action;

    auto require_valid_press = parse_bool_field(object, "requireValidPress");
    if (!require_valid_press) {
        return std::unexpected(require_valid_press.error());
    }
    effect.require_valid_press = *require_valid_press;

    auto animation_id = parse_string_field(object, "animationId");
    if (!animation_id) {
        return std::unexpected(animation_id.error());
    }
    effect.animation_id = *animation_id;

    switch (effect.type) {
    case EventEffectType::EmitAction:
        if (effect.action.empty()) {
            return std::unexpected("emitAction effect requires non-empty field 'action'");
        }
        break;
    case EventEffectType::SetProperty: {
        auto update = parse_event_property_update(object, effect.target);
        if (!update) {
            return std::unexpected(update.error());
        }
        effect.updates.push_back(std::move(*update));
        break;
    }
    case EventEffectType::SetProperties: {
        const auto *updates_value = find_child_value(object, "updates");
        if (updates_value == nullptr || !updates_value->is_array()) {
            return std::unexpected("setProperties effect requires array field 'updates'");
        }
        for (const auto &entry : updates_value->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("setProperties updates entries must be objects");
            }
            auto update = parse_event_property_update(entry.as_object(), effect.target);
            if (!update) {
                return std::unexpected(update.error());
            }
            effect.updates.push_back(std::move(*update));
        }
        if (effect.updates.empty()) {
            return std::unexpected("setProperties effect updates must not be empty");
        }
        break;
    }
    case EventEffectType::StartAnimation: {
        const auto *animation_value = find_child_value(object, "animation");
        if (animation_value != nullptr) {
            if (!animation_value->is_object()) {
                return std::unexpected("startAnimation field 'animation' must be an object");
            }
            auto animation = parse_animation(animation_value->as_object());
            if (!animation) {
                return std::unexpected(animation.error());
            }
            effect.animation = *animation;
        }
        if (effect.animation_id.empty() && animation_value == nullptr) {
            return std::unexpected("startAnimation effect requires 'animationId' or 'animation'");
        }
        break;
    }
    case EventEffectType::StopAnimation:
        if (effect.animation_id.empty()) {
            return std::unexpected("stopAnimation effect requires non-empty field 'animationId'");
        }
        break;
    case EventEffectType::Max:
    default:
        return std::unexpected("Invalid event effect type");
    }
    return effect;
}

std::expected<EventBinding, std::string> parse_event_binding(const boost::json::object &object)
{
    EventBinding binding;
    auto type = parse_enum_field<EventType>(object, "type", EventType::Clicked);
    if (!type) {
        return std::unexpected(type.error());
    }
    binding.type = *type;

    auto action = parse_string_field(object, "action");
    if (!action) {
        return std::unexpected(action.error());
    }
    binding.action = *action;

    const auto *effects_value = find_child_value(object, "effects");
    if (effects_value != nullptr) {
        if (!effects_value->is_array()) {
            return std::unexpected("Field 'effects' must be an array");
        }
        for (const auto &effect_value : effects_value->as_array()) {
            if (!effect_value.is_object()) {
                return std::unexpected("Event effects entries must be objects");
            }
            auto effect = parse_event_effect(effect_value.as_object());
            if (!effect) {
                return std::unexpected(effect.error());
            }
            binding.effects.push_back(std::move(*effect));
        }
    }
    return binding;
}

std::expected<Animation, std::string> parse_animation(const boost::json::object &object)
{
    Animation animation;
    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    animation.id = *id;

    auto trigger = parse_enum_field<AnimationTrigger>(object, "trigger", AnimationTrigger::Mount);
    if (!trigger) {
        return std::unexpected(trigger.error());
    }
    animation.trigger = *trigger;

    auto property = parse_enum_field<AnimationProperty>(object, "property", AnimationProperty::Opacity);
    if (!property) {
        return std::unexpected(property.error());
    }
    animation.property = *property;

    auto from_mode = parse_enum_field<AnimationValueMode>(object, "fromMode", AnimationValueMode::Absolute);
    if (!from_mode) {
        return std::unexpected(from_mode.error());
    }
    animation.from_mode = *from_mode;

    auto from = parse_int_field(object, "from");
    if (!from) {
        return std::unexpected(from.error());
    }
    animation.from = *from;

    auto to_mode = parse_enum_field<AnimationValueMode>(object, "toMode", AnimationValueMode::Absolute);
    if (!to_mode) {
        return std::unexpected(to_mode.error());
    }
    animation.to_mode = *to_mode;

    auto to = parse_int_field(object, "to");
    if (!to) {
        return std::unexpected(to.error());
    }
    animation.to = *to;

    auto duration = parse_int_field(object, "duration", 150);
    if (!duration) {
        return std::unexpected(duration.error());
    }
    animation.duration = *duration;

    auto delay = parse_int_field(object, "delay");
    if (!delay) {
        return std::unexpected(delay.error());
    }
    animation.delay = *delay;

    auto easing = parse_enum_field<AnimationEasing>(object, "easing", AnimationEasing::Linear);
    if (!easing) {
        return std::unexpected(easing.error());
    }
    animation.easing = *easing;

    auto repeat = parse_int_field(object, "repeat");
    if (!repeat) {
        return std::unexpected(repeat.error());
    }
    animation.repeat = *repeat;

    auto playback = parse_bool_field(object, "playback");
    if (!playback) {
        return std::unexpected(playback.error());
    }
    animation.playback = *playback;
    return animation;
}

std::vector<std::string> split_template_path(std::string_view path)
{
    std::vector<std::string> parts;
    std::string current;
    for (const auto ch : path) {
        if (ch == '/') {
            if (!current.empty()) {
                parts.push_back(std::move(current));
                current.clear();
            }
            continue;
        }
        current.push_back(ch);
    }
    if (!current.empty()) {
        parts.push_back(std::move(current));
    }
    return parts;
}

boost::json::object *find_template_override_target(
    boost::json::object &root,
    std::string_view relative_path)
{
    if (relative_path.empty() || relative_path == ".") {
        return &root;
    }
    if (relative_path.front() == '/') {
        return nullptr;
    }

    auto *current = &root;
    for (const auto &part : split_template_path(relative_path)) {
        auto children_it = current->find("children");
        if (children_it == current->end() || !children_it->value().is_array()) {
            return nullptr;
        }
        boost::json::object *next = nullptr;
        for (auto &child_value : children_it->value().as_array()) {
            if (!child_value.is_object()) {
                continue;
            }
            auto &child_object = child_value.as_object();
            auto id = parse_string_field(child_object, "id");
            if (id && *id == part) {
                next = &child_object;
                break;
            }
        }
        if (next == nullptr) {
            return nullptr;
        }
        current = next;
    }
    return current;
}

std::expected<void, std::string> apply_template_overrides(
    boost::json::object &root,
    const boost::json::object &overrides)
{
    static const boost::unordered_flat_set<std::string> forbidden_fields = {
        "id",
        "type",
        "children",
        "node",
        "templateId",
        "template_id",
        "overrides",
        "slots",
    };

    for (const auto &[path_key, override_value] : overrides) {
        const std::string path(path_key.begin(), path_key.end());
        if (!override_value.is_object()) {
            return std::unexpected("Template override '" + path + "' must be an object");
        }
        auto *target = find_template_override_target(root, path);
        if (target == nullptr) {
            return std::unexpected("Template override target not found: " + path);
        }
        for (const auto &[field_key, field_value] : override_value.as_object()) {
            const std::string field(field_key.begin(), field_key.end());
            if (forbidden_fields.contains(field)) {
                return std::unexpected("Template override cannot replace field: " + field);
            }
            auto it = target->find(field_key);
            if (it != target->end() && it->value().is_object() && field_value.is_object()) {
                merge_json(it->value(), field_value);
            } else {
                target->insert_or_assign(field_key, field_value);
            }
        }
    }

    return {};
}

std::expected<void, std::string> expand_template_slots_in_children(
    boost::json::array &children,
    const boost::json::object *slots,
    boost::unordered_flat_set<std::string> &declared_slots)
{
    boost::json::array expanded_children;
    for (auto &child_value : children) {
        if (!child_value.is_object()) {
            expanded_children.emplace_back(std::move(child_value));
            continue;
        }

        auto &child_object = child_value.as_object();
        auto type = parse_string_field(child_object, "type");
        if (type && *type == "slot") {
            auto id = parse_string_field(child_object, "id");
            if (!id) {
                return std::unexpected("Template slot must contain string field 'id'");
            }
            if (id->empty()) {
                return std::unexpected("Template slot id must not be empty");
            }
            if (!declared_slots.emplace(*id).second) {
                return std::unexpected("Duplicate template slot id: " + *id);
            }

            const auto *replacement = find_child_value(child_object, "children");
            if (slots != nullptr) {
                auto slot_it = slots->find(*id);
                if (slot_it != slots->end()) {
                    replacement = &slot_it->value();
                }
            }
            if (replacement == nullptr) {
                continue;
            }
            if (!replacement->is_array()) {
                return std::unexpected("Template slot '" + *id + "' replacement must be an array");
            }
            for (const auto &replacement_child : replacement->as_array()) {
                expanded_children.emplace_back(replacement_child);
            }
            continue;
        }

        if (auto children_it = child_object.find("children");
                children_it != child_object.end() && children_it->value().is_array()) {
            auto result = expand_template_slots_in_children(children_it->value().as_array(), slots, declared_slots);
            if (!result) {
                return std::unexpected(result.error());
            }
        }
        expanded_children.emplace_back(std::move(child_value));
    }

    children = std::move(expanded_children);
    return {};
}

std::expected<void, std::string> apply_template_slots(
    boost::json::object &root,
    const boost::json::object *slots)
{
    boost::unordered_flat_set<std::string> declared_slots;
    if (auto children_it = root.find("children"); children_it != root.end() && children_it->value().is_array()) {
        auto result = expand_template_slots_in_children(children_it->value().as_array(), slots, declared_slots);
        if (!result) {
            return std::unexpected(result.error());
        }
    }

    if (slots != nullptr) {
        for (const auto &[slot_key, slot_value] : *slots) {
            (void)slot_value;
            const std::string slot_id(slot_key.begin(), slot_key.end());
            if (!declared_slots.contains(slot_id)) {
                return std::unexpected("TemplateRef provides unknown slot: " + slot_id);
            }
        }
    }

    return {};
}

std::expected<SparseNode, std::string> parse_template_ref(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack)
{
    auto id = parse_string_field(object, "id");
    auto template_id = parse_string_field(object, "templateId");
    if (!id || !template_id) {
        return std::unexpected(!id ? id.error() : template_id.error());
    }
    const auto template_it = templates.find(*template_id);
    if (template_it == templates.end()) {
        return std::unexpected("TemplateRef references missing templateId: " + *template_id);
    }
    if (std::find(template_stack.begin(), template_stack.end(), *template_id) != template_stack.end()) {
        return std::unexpected("TemplateRef cycle detected for templateId: " + *template_id);
    }

    auto node_object = template_it->second;
    node_object.insert_or_assign("id", *id);
    const boost::json::object *slots = nullptr;
    if (const auto *slots_value = find_child_value(object, "slots"); slots_value != nullptr) {
        if (!slots_value->is_object()) {
            return std::unexpected("TemplateRef field 'slots' must be an object");
        }
        slots = &slots_value->as_object();
    }
    auto slot_result = apply_template_slots(node_object, slots);
    if (!slot_result) {
        return std::unexpected(slot_result.error());
    }
    if (const auto *overrides_value = find_child_value(object, "overrides"); overrides_value != nullptr) {
        if (!overrides_value->is_object()) {
            return std::unexpected("TemplateRef field 'overrides' must be an object");
        }
        auto override_result = apply_template_overrides(node_object, overrides_value->as_object());
        if (!override_result) {
            return std::unexpected(override_result.error());
        }
    }

    template_stack.push_back(*template_id);
    auto parsed = parse_view_node(node_object, environment, templates, interactions, template_stack);
    template_stack.pop_back();
    return parsed;
}

std::expected<std::vector<SparseNode>, std::string> parse_children(
    const boost::json::object &object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack)
{
    std::vector<SparseNode> children;
    const auto *value = find_child_value(object, "children");
    if (value == nullptr) {
        return children;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'children' must be an array");
    }

    for (const auto &child : value->as_array()) {
        if (!child.is_object()) {
            return std::unexpected("Child nodes must be objects");
        }
        const auto &child_object = child.as_object();
        auto child_type = parse_string_field(child_object, "type");
        if (!child_type) {
            return std::unexpected(child_type.error());
        }
        if (*child_type == "slot") {
            return std::unexpected("Node type 'slot' is only allowed inside viewTemplate assets");
        }
        auto parsed_child = (*child_type == "templateRef") ?
                            parse_template_ref(child_object, environment, templates, interactions, template_stack) :
                            parse_view_node(child_object, environment, templates, interactions, template_stack);
        if (!parsed_child) {
            return std::unexpected(parsed_child.error());
        }
        if (parsed_child->type == NodeType::Screen) {
            return std::unexpected("Node type 'screen' is only allowed at the top level");
        }
        children.push_back(std::move(*parsed_child));
    }

    return children;
}

std::expected<NodeType, std::string> parse_node_type(std::string_view type)
{
    if (type == "screen") {
        return NodeType::Screen;
    }
    if (type == "container") {
        return NodeType::Container;
    }
    if (type == "label") {
        return NodeType::Label;
    }
    if (type == "button") {
        return NodeType::Button;
    }
    if (type == "image") {
        return NodeType::Image;
    }
    if (type == "frameView" || type == "frame_view") {
        return NodeType::FrameView;
    }
    if (type == "textInput" || type == "text_input" || type == "textarea" || type == "textArea") {
        return NodeType::TextInput;
    }
    if (type == "slider") {
        return NodeType::Slider;
    }
    if (type == "switch") {
        return NodeType::Switch;
    }
    if (type == "checkbox") {
        return NodeType::Checkbox;
    }
    if (type == "dropdown") {
        return NodeType::Dropdown;
    }
    if (type == "progressBar" || type == "progress_bar") {
        return NodeType::ProgressBar;
    }
    if (type == "spinner") {
        return NodeType::Spinner;
    }
    if (type == "arc") {
        return NodeType::Arc;
    }
    if (type == "line") {
        return NodeType::Line;
    }
    if (type == "table") {
        return NodeType::Table;
    }
    if (type == "keyboard") {
        return NodeType::Keyboard;
    }
    if (type == "canvas") {
        return NodeType::Canvas;
    }

    return std::unexpected("Unsupported view node type: " + std::string(type));
}

bool default_clickable_for_node_type(NodeType type)
{
    switch (type) {
    case NodeType::Label:
        return false;
    case NodeType::Button:
    case NodeType::TextInput:
    case NodeType::Slider:
    case NodeType::Switch:
    case NodeType::Checkbox:
    case NodeType::Dropdown:
    case NodeType::Arc:
    case NodeType::Keyboard:
    case NodeType::Screen:
    case NodeType::Container:
    case NodeType::Image:
    case NodeType::FrameView:
    case NodeType::ProgressBar:
    case NodeType::Spinner:
    case NodeType::Line:
    case NodeType::Table:
    case NodeType::Canvas:
    case NodeType::Max:
    default:
        return true;
    }
}

bool default_scrollable_for_node_type(NodeType type)
{
    switch (type) {
    case NodeType::Screen:
    case NodeType::Container:
        return true;
    case NodeType::Label:
    case NodeType::Button:
    case NodeType::Image:
    case NodeType::FrameView:
    case NodeType::TextInput:
    case NodeType::Slider:
    case NodeType::Switch:
    case NodeType::Checkbox:
    case NodeType::Dropdown:
    case NodeType::ProgressBar:
    case NodeType::Spinner:
    case NodeType::Arc:
    case NodeType::Line:
    case NodeType::Table:
    case NodeType::Keyboard:
    case NodeType::Canvas:
    case NodeType::Max:
    default:
        return false;
    }
}

CommonProps get_builtin_default_common_props(NodeType type)
{
    CommonProps props;
    props.clickable = default_clickable_for_node_type(type);
    props.scrollable = default_scrollable_for_node_type(type);
    return props;
}

Layout get_builtin_default_layout(NodeType type)
{
    (void)type;
    return Layout{};
}

Placement get_builtin_default_placement(NodeType type)
{
    Placement placement;
    if (type == NodeType::Screen) {
        placement.width = Dimension{.mode = SizeMode::Match, .value = 0};
        placement.height = Dimension{.mode = SizeMode::Match, .value = 0};
    }
    return placement;
}

void merge_object_defaults(boost::json::object &target, const boost::json::object &defaults)
{
    for (const auto &entry : defaults) {
        auto target_it = find_key(target, normalize_object_key(entry.key()));
        if (target_it == target.end()) {
            target.insert_or_assign(std::string(entry.key()), entry.value());
            continue;
        }
        if (target_it->value().is_object() && entry.value().is_object()) {
            merge_object_defaults(target_it->value().as_object(), entry.value().as_object());
        }
    }
}

std::expected<void, std::string> prepend_array_defaults(
    boost::json::object &target,
    const boost::json::object &defaults,
    std::string_view field)
{
    auto defaults_it = find_key(defaults, field);
    if (defaults_it == defaults.end()) {
        return {};
    }
    if (!defaults_it->value().is_array()) {
        return std::unexpected("interactionTemplate field '" + std::string(field) + "' must be an array");
    }

    boost::json::array merged;
    for (const auto &value : defaults_it->value().as_array()) {
        merged.push_back(value);
    }

    auto target_it = find_key(target, field);
    if (target_it != target.end()) {
        if (!target_it->value().is_array()) {
            return std::unexpected("Node field '" + std::string(field) + "' must be an array");
        }
        for (const auto &value : target_it->value().as_array()) {
            merged.push_back(value);
        }
        target.erase(target_it);
    }
    target.insert_or_assign(std::string(field), std::move(merged));
    return {};
}

std::expected<void, std::string> apply_interaction_template_object(
    boost::json::object &target,
    const boost::json::object &interaction,
    std::string_view id)
{
    for (const auto &field : {
                "common_props", "state_styles"
            }) {
        auto interaction_it = find_key(interaction, field);
        if (interaction_it == interaction.end()) {
            continue;
        }
        if (!interaction_it->value().is_object()) {
            return std::unexpected(
                       "interactionTemplate '" + std::string(id) + "' field '" + field + "' must be an object"
                   );
        }

        auto target_it = find_key(target, field);
        if (target_it == target.end()) {
            target.insert_or_assign(std::string(interaction_it->key()), interaction_it->value());
            continue;
        }
        if (!target_it->value().is_object()) {
            return std::unexpected("Node field '" + std::string(field) + "' must be an object");
        }
        merge_object_defaults(target_it->value().as_object(), interaction_it->value().as_object());
    }

    auto events = prepend_array_defaults(target, interaction, "events");
    if (!events) {
        return events;
    }
    return prepend_array_defaults(target, interaction, "animations");
}

std::expected<boost::json::object, std::string> apply_interaction_templates(
    const boost::json::object &source,
    const InteractionTemplateRawMap &interactions)
{
    auto refs = parse_string_array(source, "interactionRefs");
    if (!refs) {
        return std::unexpected(refs.error());
    }
    boost::json::object target = source;
    for (auto it = refs->rbegin(); it != refs->rend(); ++it) {
        auto interaction_it = interactions.find(*it);
        if (interaction_it == interactions.end()) {
            return std::unexpected("Node references missing interactionTemplate: " + *it);
        }
        auto apply_result = apply_interaction_template_object(target, interaction_it->second, *it);
        if (!apply_result) {
            return std::unexpected(apply_result.error());
        }
    }
    return target;
}

std::expected<SparseNode, std::string> parse_view_node(
    const boost::json::object &source_object,
    const Environment &environment,
    const TemplateRawMap &templates,
    const InteractionTemplateRawMap &interactions,
    std::vector<std::string> &template_stack,
    std::optional<NodeType> forced_type)
{
    BROOKESIA_LOG_TRACE_GUARD();

    auto interaction_object = apply_interaction_templates(source_object, interactions);
    if (!interaction_object) {
        return std::unexpected(interaction_object.error());
    }
    const auto &object = *interaction_object;

    BROOKESIA_LOGD("Params: object(keys=%1%), environment(%2%)", object.size(), environment);

    SparseNode node;

    if (find_child_value(object, "subtype") != nullptr) {
        return std::unexpected("Field 'subtype' is no longer supported; use node field 'type'");
    }
    if (find_child_value(object, "type") != nullptr &&
            find_child_value(object, "type")->is_string() &&
            find_child_value(object, "type")->as_string() == "view") {
        return std::unexpected("View asset type 'view' is no longer supported; use viewScreen/viewTemplate assets");
    }
    if (forced_type.has_value()) {
        node.type = *forced_type;
    } else {
        auto type_name = parse_string_field(object, "type");
        if (!type_name) {
            return std::unexpected(type_name.error());
        }
        auto type = parse_node_type(*type_name);
        if (!type) {
            return std::unexpected(type.error());
        }
        node.type = *type;
    }
    node.common_props = get_builtin_default_common_props(node.type);
    node.layout = get_builtin_default_layout(node.type);
    node.placement = get_builtin_default_placement(node.type);

    const auto *mount_mode_value = find_child_value(object, "mount_mode");
    if (mount_mode_value != nullptr) {
        if (node.type != NodeType::Screen) {
            return std::unexpected("Field 'mount_mode' is only allowed on top-level screen nodes");
        }

        auto mount_mode = parse_enum_field<MountMode>(object, "mount_mode", MountMode::Eager);
        if (!mount_mode) {
            return std::unexpected(mount_mode.error());
        }
        node.mount_mode = *mount_mode;
    }

    auto id = parse_string_field(object, "id");
    if (!id) {
        return std::unexpected(id.error());
    }
    node.id = *id;

    if (find_child_value(object, "props") != nullptr) {
        return std::unexpected(
                   "Field 'props' is no longer supported; move fields into 'commonProps' or the corresponding "
                   "typed props object"
               );
    }

    const auto *common_props_value = find_child_value(object, "common_props");
    if (common_props_value != nullptr) {
        if (!common_props_value->is_object()) {
            return std::unexpected("Field 'commonProps' must be an object");
        }
        const auto &common_props_object = common_props_value->as_object();
        auto hidden = parse_bool_field(common_props_object, "hidden");
        if (!hidden) {
            return std::unexpected(hidden.error());
        }
        node.common_props.hidden = *hidden;

        auto disabled = parse_bool_field(common_props_object, "disabled");
        if (!disabled) {
            return std::unexpected(disabled.error());
        }
        node.common_props.disabled = *disabled;

        auto clickable = parse_bool_field(common_props_object, "clickable", node.common_props.clickable);
        if (!clickable) {
            return std::unexpected(clickable.error());
        }
        node.common_props.clickable = *clickable;

        auto scrollable = parse_bool_field(common_props_object, "scrollable", node.common_props.scrollable);
        if (!scrollable) {
            return std::unexpected(scrollable.error());
        }
        node.common_props.scrollable = *scrollable;

        auto press_lock = parse_bool_field(common_props_object, "pressLock", node.common_props.press_lock);
        if (!press_lock) {
            return std::unexpected(press_lock.error());
        }
        node.common_props.press_lock = *press_lock;

        auto angle = parse_int_field(common_props_object, "angle", node.common_props.angle);
        if (!angle) {
            return std::unexpected(angle.error());
        }
        node.common_props.angle = *angle;

        auto zoom = parse_int_field(common_props_object, "zoom", node.common_props.zoom);
        if (!zoom) {
            return std::unexpected(zoom.error());
        }
        node.common_props.zoom = *zoom;

        auto pivot_x = parse_pivot_field(common_props_object, "pivotX", node.common_props.pivot_x);
        if (!pivot_x) {
            return std::unexpected(pivot_x.error());
        }
        node.common_props.pivot_x = *pivot_x;

        auto pivot_y = parse_pivot_field(common_props_object, "pivotY", node.common_props.pivot_y);
        if (!pivot_y) {
            return std::unexpected(pivot_y.error());
        }
        node.common_props.pivot_y = *pivot_y;
    }

    const auto *label_props_value = find_child_value(object, "label_props");
    if (label_props_value != nullptr) {
        if (node.type != NodeType::Label && node.type != NodeType::Checkbox) {
            return std::unexpected("Field 'labelProps' is only allowed on label and checkbox nodes");
        }
        if (!label_props_value->is_object()) {
            return std::unexpected("Field 'labelProps' must be an object");
        }
        const auto &label_props_object = label_props_value->as_object();
        auto text = parse_string_field(label_props_object, "text");
        if (!text) {
            return std::unexpected(text.error());
        }
        node.label_props.text = *text;
    }

    const auto *image_props_value = find_child_value(object, "image_props");
    if (image_props_value != nullptr) {
        if (node.type != NodeType::Image) {
            return std::unexpected("Field 'imageProps' is only allowed on image nodes");
        }
        if (!image_props_value->is_object()) {
            return std::unexpected("Field 'imageProps' must be an object");
        }
        const auto &image_props_object = image_props_value->as_object();
        auto src = parse_resource_reference_field(image_props_object, "src", "image", "'${image.<id>}'");
        if (!src) {
            return std::unexpected(src.error());
        }
        node.image_props.src = *src;
        auto inner_align = parse_string_field(image_props_object, "innerAlign", node.image_props.inner_align);
        if (!inner_align) {
            return std::unexpected(inner_align.error());
        }
        node.image_props.inner_align = *inner_align;
        auto recolor = parse_string_field(image_props_object, "recolor", node.image_props.recolor);
        if (!recolor) {
            return std::unexpected(recolor.error());
        }
        node.image_props.recolor = *recolor;
        auto recolor_opacity = parse_int_field(
                                   image_props_object, "recolorOpacity", node.image_props.recolor_opacity
                               );
        if (!recolor_opacity) {
            return std::unexpected(recolor_opacity.error());
        }
        node.image_props.recolor_opacity = *recolor_opacity;
        auto angle = parse_int_field(image_props_object, "angle", node.image_props.angle);
        if (!angle) {
            return std::unexpected(angle.error());
        }
        node.image_props.angle = *angle;
        auto offset_x = parse_int_field(image_props_object, "offsetX", node.image_props.offset_x);
        if (!offset_x) {
            return std::unexpected(offset_x.error());
        }
        node.image_props.offset_x = *offset_x;
        auto offset_y = parse_int_field(image_props_object, "offsetY", node.image_props.offset_y);
        if (!offset_y) {
            return std::unexpected(offset_y.error());
        }
        node.image_props.offset_y = *offset_y;
        auto zoom = parse_int_field(image_props_object, "zoom", node.image_props.zoom);
        if (!zoom) {
            return std::unexpected(zoom.error());
        }
        node.image_props.zoom = *zoom;
        auto pivot_x = parse_pivot_field(image_props_object, "pivotX", node.image_props.pivot_x);
        if (!pivot_x) {
            return std::unexpected(pivot_x.error());
        }
        node.image_props.pivot_x = *pivot_x;
        auto pivot_y = parse_pivot_field(image_props_object, "pivotY", node.image_props.pivot_y);
        if (!pivot_y) {
            return std::unexpected(pivot_y.error());
        }
        node.image_props.pivot_y = *pivot_y;
    }

    const auto *frame_view_props_value = find_child_value(object, "frame_view_props");
    if (frame_view_props_value != nullptr) {
        if (node.type != NodeType::FrameView) {
            return std::unexpected("Field 'frameViewProps' is only allowed on frameView nodes");
        }
        if (!frame_view_props_value->is_object()) {
            return std::unexpected("Field 'frameViewProps' must be an object");
        }
        const auto &frame_view_props_object = frame_view_props_value->as_object();
        auto auto_register_output = parse_bool_field(
                                        frame_view_props_object, "auto_register_output",
                                        node.frame_view_props.auto_register_output
                                    );
        if (!auto_register_output) {
            return std::unexpected(auto_register_output.error());
        }
        node.frame_view_props.auto_register_output = *auto_register_output;

        auto output_name = parse_string_field(
                               frame_view_props_object, "output_name", node.frame_view_props.output_name
                           );
        if (!output_name) {
            return std::unexpected(output_name.error());
        }
        node.frame_view_props.output_name = *output_name;

        auto color_format = parse_enum_field<FrameColorFormat>(
                                frame_view_props_object, "color_format", node.frame_view_props.color_format
                            );
        if (!color_format) {
            return std::unexpected(color_format.error());
        }
        node.frame_view_props.color_format = *color_format;
    }

    const auto *text_input_props_value = find_child_value(object, "text_input_props");
    if (text_input_props_value != nullptr) {
        if (node.type != NodeType::TextInput) {
            return std::unexpected("Field 'textInputProps' is only allowed on textInput nodes");
        }
        if (!text_input_props_value->is_object()) {
            return std::unexpected("Field 'textInputProps' must be an object");
        }
        const auto &text_input_props_object = text_input_props_value->as_object();
        auto text = parse_string_field(text_input_props_object, "text");
        if (!text) {
            return std::unexpected(text.error());
        }
        node.text_input_props.text = *text;
        auto placeholder = parse_string_field(text_input_props_object, "placeholder");
        if (!placeholder) {
            return std::unexpected(placeholder.error());
        }
        node.text_input_props.placeholder = *placeholder;
        auto password = parse_bool_field(text_input_props_object, "password");
        if (!password) {
            return std::unexpected(password.error());
        }
        node.text_input_props.password = *password;
        auto multiline = parse_bool_field(text_input_props_object, "multiline");
        if (!multiline) {
            return std::unexpected(multiline.error());
        }
        node.text_input_props.multiline = *multiline;
        auto max_length = parse_int_field(text_input_props_object, "max_length");
        if (!max_length) {
            return std::unexpected(max_length.error());
        }
        node.text_input_props.max_length = *max_length;
    }

    const auto *range_props_value = find_child_value(object, "range_props");
    if (range_props_value != nullptr) {
        if (node.type != NodeType::Slider && node.type != NodeType::ProgressBar && node.type != NodeType::Arc) {
            return std::unexpected("Field 'rangeProps' is only allowed on slider, progressBar, and arc nodes");
        }
        if (!range_props_value->is_object()) {
            return std::unexpected("Field 'rangeProps' must be an object");
        }
        const auto &range_props_object = range_props_value->as_object();
        auto value = parse_int_field(range_props_object, "value");
        if (!value) {
            return std::unexpected(value.error());
        }
        auto min = parse_int_field(range_props_object, "min");
        if (!min) {
            return std::unexpected(min.error());
        }
        auto max = parse_int_field(range_props_object, "max", 100);
        if (!max) {
            return std::unexpected(max.error());
        }
        auto step = parse_int_field(range_props_object, "step", 1);
        if (!step) {
            return std::unexpected(step.error());
        }
        node.range_props = RangeProps{.value = *value, .min = *min, .max = *max, .step = *step};
    }

    const auto *toggle_props_value = find_child_value(object, "toggle_props");
    if (toggle_props_value != nullptr) {
        if (node.type != NodeType::Switch && node.type != NodeType::Checkbox) {
            return std::unexpected("Field 'toggleProps' is only allowed on switch and checkbox nodes");
        }
        if (!toggle_props_value->is_object()) {
            return std::unexpected("Field 'toggleProps' must be an object");
        }
        const auto &toggle_props_object = toggle_props_value->as_object();
        auto checked = parse_bool_field(toggle_props_object, "checked");
        if (!checked) {
            return std::unexpected(checked.error());
        }
        node.toggle_props.checked = *checked;
    }

    const auto *dropdown_props_value = find_child_value(object, "dropdown_props");
    if (dropdown_props_value != nullptr) {
        if (node.type != NodeType::Dropdown) {
            return std::unexpected("Field 'dropdownProps' is only allowed on dropdown nodes");
        }
        if (!dropdown_props_value->is_object()) {
            return std::unexpected("Field 'dropdownProps' must be an object");
        }
        const auto &dropdown_props_object = dropdown_props_value->as_object();
        auto options = parse_string_array(dropdown_props_object, "options");
        if (!options) {
            return std::unexpected(options.error());
        }
        auto selected_index = parse_int_field(dropdown_props_object, "selected_index");
        if (!selected_index) {
            return std::unexpected(selected_index.error());
        }
        node.dropdown_props.options = std::move(*options);
        node.dropdown_props.selected_index = *selected_index;
    }

    const auto *table_props_value = find_child_value(object, "table_props");
    if (table_props_value != nullptr) {
        if (node.type != NodeType::Table) {
            return std::unexpected("Field 'tableProps' is only allowed on table nodes");
        }
        if (!table_props_value->is_object()) {
            return std::unexpected("Field 'tableProps' must be an object");
        }
        const auto &table_props_object = table_props_value->as_object();
        auto rows = parse_int_field(table_props_object, "rows");
        if (!rows) {
            return std::unexpected(rows.error());
        }
        auto columns = parse_int_field(table_props_object, "columns");
        if (!columns) {
            return std::unexpected(columns.error());
        }
        auto cells = parse_table_cells_field(table_props_object);
        if (!cells) {
            return std::unexpected(cells.error());
        }
        node.table_props = TableProps{.rows = *rows, .columns = *columns, .cells = std::move(*cells)};
    }

    const auto *line_props_value = find_child_value(object, "line_props");
    if (line_props_value != nullptr) {
        if (node.type != NodeType::Line) {
            return std::unexpected("Field 'lineProps' is only allowed on line nodes");
        }
        if (!line_props_value->is_object()) {
            return std::unexpected("Field 'lineProps' must be an object");
        }
        const auto &line_props_object = line_props_value->as_object();
        auto points = parse_points_field(line_props_object);
        if (!points) {
            return std::unexpected(points.error());
        }
        node.line_props.points = std::move(*points);
    }

    const auto *keyboard_props_value = find_child_value(object, "keyboard_props");
    if (keyboard_props_value != nullptr) {
        if (node.type != NodeType::Keyboard) {
            return std::unexpected("Field 'keyboardProps' is only allowed on keyboard nodes");
        }
        if (!keyboard_props_value->is_object()) {
            return std::unexpected("Field 'keyboardProps' must be an object");
        }
        const auto &keyboard_props_object = keyboard_props_value->as_object();
        auto mode = parse_string_field(keyboard_props_object, "mode", "text");
        if (!mode) {
            return std::unexpected(mode.error());
        }
        auto popovers = parse_bool_field(keyboard_props_object, "popovers");
        if (!popovers) {
            return std::unexpected(popovers.error());
        }
        Dimension icon_size{.mode = SizeMode::Fixed, .value = 0};
        if (const auto *icon_size_value = find_child_value(keyboard_props_object, "iconSize"); icon_size_value != nullptr) {
            auto parsed_icon_size = parse_dimension(icon_size_value, "keyboardProps.iconSize", environment);
            if (!parsed_icon_size) {
                return std::unexpected(parsed_icon_size.error());
            }
            icon_size = *parsed_icon_size;
        }
        auto target_text_input = parse_string_field(keyboard_props_object, "targetTextInput");
        if (!target_text_input) {
            return std::unexpected(target_text_input.error());
        }
        auto allowed_modes = parse_keyboard_allowed_modes_field(keyboard_props_object);
        if (!allowed_modes) {
            return std::unexpected(allowed_modes.error());
        }
        auto layouts = parse_keyboard_layouts_field(keyboard_props_object);
        if (!layouts) {
            return std::unexpected(layouts.error());
        }
        auto key_styles = parse_keyboard_key_styles_field(keyboard_props_object, environment);
        if (!key_styles) {
            return std::unexpected(key_styles.error());
        }
        auto key_style_refs = parse_keyboard_key_style_refs_field(keyboard_props_object);
        if (!key_style_refs) {
            return std::unexpected(key_style_refs.error());
        }
        node.keyboard_props = KeyboardProps{
            .mode = *mode,
            .popovers = *popovers,
            .icon_size = icon_size,
            .target_text_input = std::move(*target_text_input),
            .allowed_modes = std::move(*allowed_modes),
            .layouts = std::move(*layouts),
            .key_styles = std::move(*key_styles),
            .key_style_refs = std::move(*key_style_refs),
            .resolved_key_styles = {},
        };
    }

    const auto *canvas_props_value = find_child_value(object, "canvas_props");
    if (canvas_props_value != nullptr) {
        if (node.type != NodeType::Canvas) {
            return std::unexpected("Field 'canvasProps' is only allowed on canvas nodes");
        }
        if (!canvas_props_value->is_object()) {
            return std::unexpected("Field 'canvasProps' must be an object");
        }
        const auto &canvas_props_object = canvas_props_value->as_object();
        auto commands = parse_canvas_commands_field(canvas_props_object);
        if (!commands) {
            return std::unexpected(commands.error());
        }
        node.canvas_props.commands = std::move(*commands);
    }

    const auto *layout_value = find_child_value(object, "layout");
    if (layout_value != nullptr) {
        if (!layout_value->is_object()) {
            return std::unexpected("Field 'layout' must be an object");
        }
        const auto &layout_object = layout_value->as_object();
        if (const auto *type_value = find_child_value(layout_object, "type");
                type_value != nullptr && type_value->is_string() && type_value->as_string() == "absolute") {
            return std::unexpected(
                       "layout.type='absolute' is no longer supported; use placement.mode='absolute'"
                   );
        }
        auto rejected_layout_fields = reject_fields(
        layout_object, {
            "x", "y", "width", "height", "grid_column", "grid_row",
            "grid_column_span", "grid_row_span", "align_self", "flex_grow"
        },
        "placement"
                                      );
        if (!rejected_layout_fields) {
            return std::unexpected(rejected_layout_fields.error());
        }

        if (find_child_value(layout_object, "type") != nullptr) {
            auto layout_type = parse_enum_field<LayoutType>(layout_object, "type", node.layout.type);
            if (!layout_type) {
                return std::unexpected(layout_type.error());
            }
            node.layout.type = *layout_type;
        }

        if (find_child_value(layout_object, "flex_flow") != nullptr) {
            auto flex_flow = parse_enum_field<FlexFlow>(layout_object, "flex_flow", node.layout.flex_flow);
            if (!flex_flow) {
                return std::unexpected(flex_flow.error());
            }
            node.layout.flex_flow = *flex_flow;
        }

        if (find_child_value(layout_object, "main_align") != nullptr) {
            auto main_align = parse_enum_field<Align>(layout_object, "main_align", node.layout.main_align);
            if (!main_align) {
                return std::unexpected(main_align.error());
            }
            node.layout.main_align = *main_align;
        }

        if (find_child_value(layout_object, "cross_align") != nullptr) {
            auto cross_align = parse_enum_field<Align>(layout_object, "cross_align", node.layout.cross_align);
            if (!cross_align) {
                return std::unexpected(cross_align.error());
            }
            node.layout.cross_align = *cross_align;
        }

        if (find_child_value(layout_object, "gap") != nullptr) {
            auto gap = parse_scaled_value(find_child_value(layout_object, "gap"), "gap", "dp", environment.density);
            if (!gap) {
                return std::unexpected(gap.error());
            }
            node.layout.gap = *gap;
        }

        if (find_child_value(layout_object, "grid_template_columns") != nullptr) {
            auto grid_columns = parse_dimension_array(layout_object, "grid_template_columns", environment);
            if (!grid_columns) {
                return std::unexpected(grid_columns.error());
            }
            node.layout.grid_template_columns = std::move(*grid_columns);
        }

        if (find_child_value(layout_object, "grid_template_rows") != nullptr) {
            auto grid_rows = parse_dimension_array(layout_object, "grid_template_rows", environment);
            if (!grid_rows) {
                return std::unexpected(grid_rows.error());
            }
            node.layout.grid_template_rows = std::move(*grid_rows);
        }
    }

    const auto *placement_value = find_child_value(object, "placement");
    if (placement_value != nullptr) {
        if (!placement_value->is_object()) {
            return std::unexpected("Field 'placement' must be an object");
        }
        const auto &placement_object = placement_value->as_object();

        if (find_child_value(placement_object, "mode") != nullptr) {
            auto placement_mode = parse_enum_field<PlacementMode>(placement_object, "mode", node.placement.mode);
            if (!placement_mode) {
                return std::unexpected(placement_mode.error());
            }
            node.placement.mode = *placement_mode;
        }

        if (find_child_value(placement_object, "x") != nullptr) {
            auto x = parse_placement_offset(find_child_value(placement_object, "x"), "x", environment);
            if (!x) {
                return std::unexpected(x.error());
            }
            node.placement.x = *x;
        }

        if (find_child_value(placement_object, "y") != nullptr) {
            auto y = parse_placement_offset(find_child_value(placement_object, "y"), "y", environment);
            if (!y) {
                return std::unexpected(y.error());
            }
            node.placement.y = *y;
        }

        const auto *width_value = find_child_value(placement_object, "width");
        if (width_value != nullptr) {
            auto width = parse_dimension(width_value, "width", environment);
            if (!width) {
                return std::unexpected(width.error());
            }
            node.placement.width = *width;
        }

        const auto *height_value = find_child_value(placement_object, "height");
        if (height_value != nullptr) {
            auto height = parse_dimension(height_value, "height", environment);
            if (!height) {
                return std::unexpected(height.error());
            }
            node.placement.height = *height;
        }

        const auto *aspect_ratio_value = find_child_value(placement_object, "aspect_ratio");
        if (aspect_ratio_value != nullptr) {
            auto aspect_ratio = parse_aspect_ratio(aspect_ratio_value, "aspectRatio");
            if (!aspect_ratio) {
                return std::unexpected(aspect_ratio.error());
            }
            node.placement.aspect_ratio = *aspect_ratio;
        }

        if (find_child_value(placement_object, "align") != nullptr) {
            auto align = parse_enum_field<PlacementAlign>(placement_object, "align", node.placement.align);
            if (!align) {
                return std::unexpected(align.error());
            }
            node.placement.align = *align;
        }

        if (find_child_value(placement_object, "relative_to") != nullptr) {
            auto relative_to = parse_view_reference_field(placement_object, "relative_to");
            if (!relative_to) {
                return std::unexpected(relative_to.error());
            }
            node.placement.relative_to = *relative_to;
        }

        if (find_child_value(placement_object, "grid_column") != nullptr) {
            auto grid_column = parse_int_field(placement_object, "grid_column", node.placement.grid_column);
            if (!grid_column) {
                return std::unexpected(grid_column.error());
            }
            node.placement.grid_column = *grid_column;
        }

        if (find_child_value(placement_object, "grid_row") != nullptr) {
            auto grid_row = parse_int_field(placement_object, "grid_row", node.placement.grid_row);
            if (!grid_row) {
                return std::unexpected(grid_row.error());
            }
            node.placement.grid_row = *grid_row;
        }

        if (find_child_value(placement_object, "grid_column_span") != nullptr) {
            auto grid_column_span = parse_int_field(placement_object, "grid_column_span", node.placement.grid_column_span);
            if (!grid_column_span) {
                return std::unexpected(grid_column_span.error());
            }
            node.placement.grid_column_span = *grid_column_span;
        }

        if (find_child_value(placement_object, "grid_row_span") != nullptr) {
            auto grid_row_span = parse_int_field(placement_object, "grid_row_span", node.placement.grid_row_span);
            if (!grid_row_span) {
                return std::unexpected(grid_row_span.error());
            }
            node.placement.grid_row_span = *grid_row_span;
        }

        if (find_child_value(placement_object, "align_self") != nullptr) {
            auto align_self = parse_enum_field<Align>(placement_object, "align_self", node.placement.align_self);
            if (!align_self) {
                return std::unexpected(align_self.error());
            }
            node.placement.align_self = *align_self;
        }

        if (find_child_value(placement_object, "flex_grow") != nullptr) {
            auto flex_grow = parse_int_field(placement_object, "flex_grow", node.placement.flex_grow);
            if (!flex_grow) {
                return std::unexpected(flex_grow.error());
            }
            if (*flex_grow < 0) {
                return std::unexpected("Field 'flexGrow' must be >= 0");
            }
            node.placement.flex_grow = *flex_grow;
        }
    }

    const auto *style_value = find_child_value(object, "style");
    if (style_value != nullptr) {
        if (!style_value->is_object()) {
            return std::unexpected("Field 'style' must be an object");
        }
        auto style = parse_style_object(style_value->as_object(), environment);
        if (!style) {
            return std::unexpected(style.error());
        }
        node.style = std::move(*style);
    }

    const auto *state_styles_value = find_child_value(object, "state_styles");
    if (state_styles_value != nullptr) {
        if (!state_styles_value->is_object()) {
            return std::unexpected("Field 'stateStyles' must be an object");
        }
        auto state_styles = parse_state_styles_object(state_styles_value->as_object(), environment);
        if (!state_styles) {
            return std::unexpected(state_styles.error());
        }
        node.state_styles = std::move(*state_styles);
    }

    const auto *part_styles_value = find_child_value(object, "part_styles");
    if (part_styles_value != nullptr) {
        if (!part_styles_value->is_object()) {
            return std::unexpected("Field 'partStyles' must be an object");
        }
        auto part_styles = parse_part_styles_object(part_styles_value->as_object(), environment);
        if (!part_styles) {
            return std::unexpected(part_styles.error());
        }
        node.part_styles = std::move(*part_styles);
    }

    auto style_refs = parse_string_array(object, "style_refs");
    if (!style_refs) {
        return std::unexpected(style_refs.error());
    }
    node.style_refs = std::move(*style_refs);

    const auto *events_value = find_child_value(object, "events");
    if (events_value != nullptr) {
        if (!events_value->is_array()) {
            return std::unexpected("Field 'events' must be an array");
        }
        for (const auto &entry : events_value->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("Event entries must be objects");
            }
            auto binding = parse_event_binding(entry.as_object());
            if (!binding) {
                return std::unexpected(binding.error());
            }
            node.events.push_back(std::move(*binding));
        }
    }

    const auto *animations_value = find_child_value(object, "animations");
    if (animations_value != nullptr) {
        if (!animations_value->is_array()) {
            return std::unexpected("Field 'animations' must be an array");
        }
        for (const auto &entry : animations_value->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("Animation entries must be objects");
            }
            auto animation = parse_animation(entry.as_object());
            if (!animation) {
                return std::unexpected(animation.error());
            }
            node.animations.push_back(*animation);
        }
    }

    const auto *bindings_value = find_child_value(object, "bindings");
    if (bindings_value != nullptr) {
        if (!bindings_value->is_object()) {
            return std::unexpected("Field 'bindings' must be an object");
        }
        for (const auto &[key, value] : bindings_value->as_object()) {
            if (!value.is_string()) {
                return std::unexpected("Binding expressions must be strings");
            }
            node.bindings.emplace(std::string(key), value.as_string().c_str());
        }
    }

    auto children = parse_children(object, environment, templates, interactions, template_stack);
    if (!children) {
        return std::unexpected(children.error());
    }
    node.children = std::move(*children);

    return node;
}

std::expected<uint32_t, std::string> parse_image_font_codepoint(std::string_view text)
{
    std::string hex_text(text);
    if (hex_text.starts_with("U+") || hex_text.starts_with("u+")) {
        hex_text.erase(0, 2);
    } else if (hex_text.starts_with("0x") || hex_text.starts_with("0X")) {
        hex_text.erase(0, 2);
    } else {
        return std::unexpected("imageFont glyph codepoint must use U+XXXX or 0xXXXX format");
    }
    if (hex_text.empty()) {
        return std::unexpected("imageFont glyph codepoint must not be empty");
    }

    char *parse_end = nullptr;
    errno = 0;
    const auto parsed = std::strtoul(hex_text.c_str(), &parse_end, 16);
    if (errno != 0 || parse_end == hex_text.c_str() || *parse_end != '\0' || parsed > 0x10FFFFUL ||
            (parsed >= 0xD800UL && parsed <= 0xDFFFUL)) {
        return std::unexpected("Invalid imageFont glyph codepoint: " + std::string(text));
    }
    return static_cast<uint32_t>(parsed);
}
}
