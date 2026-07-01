/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_interface/validator.hpp"
#include "private/binding.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#if !BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <cctype>
#include <cstddef>
#include <functional>
#include <optional>

#include "boost/unordered/unordered_flat_set.hpp"
#include "boost/unordered/unordered_flat_map.hpp"

namespace esp_brookesia::gui {

namespace {

static constexpr std::string_view SUPPORTED_VERSION = CURRENT_DOCUMENT_VERSION;

struct SemanticVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

static std::optional<SemanticVersion> parse_semantic_version(std::string_view version)
{
    SemanticVersion parsed_version;
    int *components[] = {
        &parsed_version.major,
        &parsed_version.minor,
        &parsed_version.patch,
    };
    std::size_t component_start = 0;

    for (std::size_t component_index = 0; component_index < 3; ++component_index) {
        if (component_start >= version.size()) {
            return std::nullopt;
        }

        const auto separator = version.find('.', component_start);
        const auto component_end = separator == std::string_view::npos ? version.size() : separator;
        if (component_end == component_start) {
            return std::nullopt;
        }

        int component_value = 0;
        for (std::size_t index = component_start; index < component_end; ++index) {
            if (!std::isdigit(static_cast<unsigned char>(version[index]))) {
                return std::nullopt;
            }
            component_value = component_value * 10 + (version[index] - '0');
        }
        *components[component_index] = component_value;

        if (component_index < 2) {
            if (separator == std::string_view::npos) {
                return std::nullopt;
            }
            component_start = separator + 1;
            continue;
        }
        if (separator != std::string_view::npos) {
            return std::nullopt;
        }
    }

    return parsed_version;
}

static void validate_document_version(const std::string &version, ValidationResult &result)
{
    const auto document_version = parse_semantic_version(version);
    const auto supported_version = parse_semantic_version(SUPPORTED_VERSION);
    if (!document_version || !supported_version) {
        result.errors.push_back(
            "Invalid GUI document version: " + version + " (expected MAJOR.MINOR.PATCH, current: " +
            std::string(SUPPORTED_VERSION) + ")"
        );
        return;
    }

    if (document_version->major != supported_version->major) {
        result.errors.push_back(
            "Incompatible GUI document major version: " + version + " (current: " +
            std::string(SUPPORTED_VERSION) + ")"
        );
        return;
    }
    if (document_version->minor != supported_version->minor) {
        result.warnings.push_back(
            "GUI document minor version differs: " + version + " (current: " +
            std::string(SUPPORTED_VERSION) + ")"
        );
        return;
    }
    if (document_version->patch != supported_version->patch) {
        result.infos.push_back(
            "GUI document patch version differs: " + version + " (current: " +
            std::string(SUPPORTED_VERSION) + ")"
        );
    }
}

static bool is_supported_keyboard_key_role(std::string_view role)
{
    return role.empty() || role == "backspace" || role == "left" || role == "right" || role == "space" ||
           role == "ok" || role == "cancel" || role == "mode";
}

static bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

static bool is_hex_digit(char value)
{
    return std::isxdigit(static_cast<unsigned char>(value)) != 0;
}

static bool is_valid_color(std::string_view color)
{
    if (color.empty()) {
        return true;
    }

    constexpr std::string_view COLOR_REFERENCE_PREFIX = "${color.";
    if (color.starts_with(COLOR_REFERENCE_PREFIX)) {
        return color.size() > COLOR_REFERENCE_PREFIX.size() + 1 && color.back() == '}';
    }

    if (color.size() != 7 || color.front() != '#') {
        return false;
    }

    for (std::size_t i = 1; i < color.size(); ++i) {
        if (!is_hex_digit(color[i])) {
            return false;
        }
    }
    return true;
}

static bool is_valid_gradient_direction(std::string_view direction)
{
    return direction == "none" || direction == "horizontal" || direction == "vertical";
}

static void validate_style(const Style &style, std::string_view owner, std::vector<std::string> &errors)
{
    auto validate_optional_color = [&](const std::optional<std::string> &value, std::string_view field_name) {
        if (value.has_value() && !is_valid_color(*value)) {
            errors.push_back(
                std::string(owner) + " has invalid " + std::string(field_name) + ": " + *value
            );
        }
    };

    validate_optional_color(style.bg_color, "bg_color");
    validate_optional_color(style.bg_gradient_color, "bg_gradient_color");
    validate_optional_color(style.text_color, "text_color");
    validate_optional_color(style.border_color, "border_color");
    validate_optional_color(style.line_color, "line_color");
    validate_optional_color(style.arc_color, "arc_color");
    validate_optional_color(style.arc_gradient_color, "arc_gradient_color");
    validate_optional_color(style.shadow_color, "shadow_color");
    validate_optional_color(style.image_recolor, "image_recolor");

    if (style.bg_gradient_direction.has_value() && !is_valid_gradient_direction(*style.bg_gradient_direction)) {
        errors.push_back(std::string(owner) + " has invalid bg_gradient_direction: " + *style.bg_gradient_direction);
    }
    if (style.bg_main_stop.has_value() && (*style.bg_main_stop < 0 || *style.bg_main_stop > 255)) {
        errors.push_back(std::string(owner) + " has invalid bg_main_stop");
    }
    if (style.bg_gradient_stop.has_value() && (*style.bg_gradient_stop < 0 || *style.bg_gradient_stop > 255)) {
        errors.push_back(std::string(owner) + " has invalid bg_gradient_stop");
    }
    if (style.bg_gradient_opacity.has_value() && (*style.bg_gradient_opacity < 0 || *style.bg_gradient_opacity > 255)) {
        errors.push_back(std::string(owner) + " has invalid bg_gradient_opacity");
    }
    if (style.opacity.has_value() && (*style.opacity < 0 || *style.opacity > 255)) {
        errors.push_back(std::string(owner) + " has invalid opacity");
    }
    if (style.image_opacity.has_value() && (*style.image_opacity < 0 || *style.image_opacity > 255)) {
        errors.push_back(std::string(owner) + " has invalid image_opacity");
    }
    if (style.image_recolor_opacity.has_value() &&
            (*style.image_recolor_opacity < 0 || *style.image_recolor_opacity > 255)) {
        errors.push_back(std::string(owner) + " has invalid image_recolor_opacity");
    }
    if (style.image_font_size.has_value() && *style.image_font_size <= 0) {
        errors.push_back(std::string(owner) + " has invalid image_font_size");
    }
    if (style.arc_opacity.has_value() && (*style.arc_opacity < 0 || *style.arc_opacity > 255)) {
        errors.push_back(std::string(owner) + " has invalid arc_opacity");
    }
    if (style.arc_gradient_segments.has_value() &&
            (*style.arc_gradient_segments < 2 || *style.arc_gradient_segments > 128)) {
        errors.push_back(std::string(owner) + " has invalid arc_gradient_segments");
    }
}

static void validate_state_styles(
    const StateStyleMap &state_styles, std::string_view owner, std::vector<std::string> &errors)
{
    for (const auto &[state_name, style] : state_styles) {
        if (!is_supported_style_state_name(state_name)) {
            errors.push_back(std::string(owner) + " has unsupported stateStyles state: " + state_name);
            continue;
        }
        if (style.font.has_value()) {
            errors.push_back(std::string(owner) + " stateStyles." + state_name + " must not set font");
        }
        if (style.font_size.has_value()) {
            errors.push_back(std::string(owner) + " stateStyles." + state_name + " must not set fontSize");
        }
        if (style.image_font_size.has_value()) {
            errors.push_back(std::string(owner) + " stateStyles." + state_name + " must not set imageFontSize");
        }
        validate_style(style, std::string(owner) + " stateStyles." + state_name, errors);
    }
}

static void validate_style_set(const StyleSet &style_set, std::string_view owner, std::vector<std::string> &errors)
{
    validate_style(style_set.style, owner, errors);
    validate_state_styles(style_set.state_styles, owner, errors);
    for (const auto &[part_name, part_style] : style_set.part_styles) {
        if (!is_supported_style_part_name(part_name)) {
            errors.push_back(std::string(owner) + " has unsupported partStyles part: " + part_name);
            continue;
        }
        if (part_style.style.font.has_value()) {
            errors.push_back(std::string(owner) + " partStyles." + part_name + " must not set font");
        }
        if (part_style.style.font_size.has_value()) {
            errors.push_back(std::string(owner) + " partStyles." + part_name + " must not set fontSize");
        }
        if (part_style.style.image_font_size.has_value()) {
            errors.push_back(std::string(owner) + " partStyles." + part_name + " must not set imageFontSize");
        }
        validate_style(part_style.style, std::string(owner) + " partStyles." + part_name, errors);
        validate_state_styles(part_style.state_styles, std::string(owner) + " partStyles." + part_name, errors);
    }
}

static bool is_outside_align(PlacementAlign align)
{
    return align == PlacementAlign::OutTopLeft || align == PlacementAlign::OutTopMid ||
           align == PlacementAlign::OutTopRight || align == PlacementAlign::OutBottomLeft ||
           align == PlacementAlign::OutBottomMid || align == PlacementAlign::OutBottomRight ||
           align == PlacementAlign::OutLeftTop || align == PlacementAlign::OutLeftMid ||
           align == PlacementAlign::OutLeftBottom || align == PlacementAlign::OutRightTop ||
           align == PlacementAlign::OutRightMid || align == PlacementAlign::OutRightBottom;
}

static std::string join_view_path(const std::vector<std::string> &segments)
{
    std::string path;
    for (const auto &segment : segments) {
        if (!path.empty()) {
            path.push_back('.');
        }
        path += segment;
    }
    return path;
}

static bool is_descendant_path(const std::vector<std::string> &candidate, const std::vector<std::string> &ancestor)
{
    if (candidate.size() <= ancestor.size()) {
        return false;
    }
    return std::equal(ancestor.begin(), ancestor.end(), candidate.begin());
}

static void build_view_path_index(
    const Node &node,
    std::vector<std::string> &current_path,
    boost::unordered_flat_map<std::string, std::vector<std::string>> &view_paths,
    std::vector<std::string> &errors)
{
    BROOKESIA_LOG_TRACE_GUARD();

    if (node.id.find('.') != std::string::npos) {
        errors.push_back("Node '" + node.id + "' must not contain '.' because placement.relativeTo uses dot-separated view paths");
    }

    if (!node.id.empty()) {
        current_path.push_back(node.id);
        const auto path_string = join_view_path(current_path);
        if (!view_paths.emplace(path_string, current_path).second) {
            errors.push_back("Duplicate view path in the same file: " + path_string);
        }
    }

    for (const auto &child : node.children) {
        build_view_path_index(child, current_path, view_paths, errors);
    }

    if (!node.id.empty()) {
        current_path.pop_back();
    }
}

static void validate_node(
    const Node &node,
    const boost::unordered_flat_set<std::string> &font_ids,
    const boost::unordered_flat_map<std::string, std::vector<std::string>> &view_paths,
    boost::unordered_flat_map<std::string, std::string> &action_owners,
    std::vector<std::string> current_view_path,
    std::vector<std::string> &errors)
{
    BROOKESIA_LOG_TRACE_GUARD();

    // BROOKESIA_LOGD("Params: node(%1%)", node);

    if (node.id.empty()) {
        errors.push_back("Node id must not be empty");
    } else if (node.id.find('.') != std::string::npos) {
        errors.push_back("Node '" + node.id + "' must not contain '.' because placement.relativeTo uses dot-separated view paths");
    }

    if (node.type == NodeType::Max) {
        errors.push_back("Node '" + node.id + "' has invalid type");
    }

    if (node.mount_mode == MountMode::Max) {
        errors.push_back("Node '" + node.id + "' has invalid mount_mode");
    }

    if (node.type != NodeType::Screen && node.mount_mode != MountMode::Eager) {
        errors.push_back("Node '" + node.id + "' must not use mount_mode unless it is a top-level screen");
    }

    if (node.layout.type == LayoutType::Max) {
        errors.push_back("Node '" + node.id + "' has invalid layout type");
    }

    if (node.placement.mode == PlacementMode::Max) {
        errors.push_back("Node '" + node.id + "' has invalid placement mode");
    }

    if (node.placement.width.mode == SizeMode::Max || node.placement.height.mode == SizeMode::Max) {
        errors.push_back("Node '" + node.id + "' has invalid size mode");
    }

    if (node.placement.x.mode == PlacementOffsetMode::Max || node.placement.y.mode == PlacementOffsetMode::Max) {
        errors.push_back("Node '" + node.id + "' has invalid placement offset mode");
    }
    if (node.placement.flex_grow < 0) {
        errors.push_back("Node '" + node.id + "' has invalid placement flexGrow");
    }
    if ((node.placement.x.mode == PlacementOffsetMode::Percent && node.placement.x.value < 0) ||
            (node.placement.y.mode == PlacementOffsetMode::Percent && node.placement.y.value < 0)) {
        errors.push_back("Node '" + node.id + "' has invalid placement percent offset");
    }

    if (node.placement.aspect_ratio.has_value()) {
        if (*node.placement.aspect_ratio <= 0.0F) {
            errors.push_back("Node '" + node.id + "' has invalid placement aspect ratio");
        }
        if (node.placement.width.mode == SizeMode::Wrap || node.placement.height.mode == SizeMode::Wrap) {
            errors.push_back(
                "Node '" + node.id + "' uses placement.aspectRatio with wrap width or height"
            );
        }
    }

    if (node.layout.flex_flow == FlexFlow::Max) {
        errors.push_back("Node '" + node.id + "' has invalid flex flow");
    }

    if (node.layout.main_align == Align::Max || node.layout.cross_align == Align::Max ||
            node.placement.align_self == Align::Max || node.placement.align == PlacementAlign::Max) {
        errors.push_back("Node '" + node.id + "' has invalid alignment");
    }

    if (node.placement.mode == PlacementMode::Relative) {
        if (node.placement.relative_to.empty() && is_outside_align(node.placement.align)) {
            errors.push_back("Node '" + node.id + "' must use relative_to when placement align is outside");
        }
        if (!node.placement.relative_to.empty()) {
            auto target_it = view_paths.find(node.placement.relative_to);
            if (target_it == view_paths.end()) {
                errors.push_back(
                    "Node '" + node.id + "' has invalid relative_to target: " + node.placement.relative_to
                );
            } else if (target_it->second == current_view_path) {
                errors.push_back("Node '" + node.id + "' must not use itself as relativeTo target");
            } else if (is_descendant_path(target_it->second, current_view_path)) {
                errors.push_back(
                    "Node '" + node.id + "' must not use a descendant as relativeTo target: " + node.placement.relative_to
                );
            }
        }
    } else if (!node.placement.relative_to.empty()) {
        errors.push_back("Node '" + node.id + "' must use placement mode 'relative' when relative_to is set");
    }

    validate_style(node.style, "Node '" + node.id + "'", errors);
    validate_state_styles(node.state_styles, "Node '" + node.id + "'", errors);
    if (!is_valid_color(node.image_props.recolor)) {
        errors.push_back("Node '" + node.id + "' has invalid image recolor: " + node.image_props.recolor);
    }
    if (node.image_props.recolor_opacity < 0 || node.image_props.recolor_opacity > 255) {
        errors.push_back("Node '" + node.id + "' has invalid image recolor opacity");
    }
    if (node.type == NodeType::FrameView) {
        if (node.frame_view_props.color_format == FrameColorFormat::Max) {
            errors.push_back("Node '" + node.id + "' has invalid frameView color format");
        }
        if (!node.frame_view_props.auto_register_output && node.frame_view_props.output_name.empty()) {
            errors.push_back("Node '" + node.id + "' frameView outputName is required when autoRegisterOutput is false");
        }
    }
    for (const auto &[part_name, part_style] : node.part_styles) {
        if (!is_supported_style_part_name(part_name)) {
            errors.push_back("Node '" + node.id + "' has unsupported partStyles part: " + part_name);
            continue;
        }
        if (part_style.style.font.has_value()) {
            errors.push_back("Node '" + node.id + "' partStyles." + part_name + " must not set font");
        }
        if (part_style.style.font_size.has_value()) {
            errors.push_back("Node '" + node.id + "' partStyles." + part_name + " must not set fontSize");
        }
        if (part_style.style.image_font_size.has_value()) {
            errors.push_back("Node '" + node.id + "' partStyles." + part_name + " must not set imageFontSize");
        }
        validate_style(part_style.style, "Node '" + node.id + "' partStyles." + part_name, errors);
        validate_state_styles(part_style.state_styles, "Node '" + node.id + "' partStyles." + part_name, errors);
    }

    if (node.range_props.max < node.range_props.min) {
        errors.push_back("Node '" + node.id + "' has range max smaller than min");
    }

    if (node.range_props.step <= 0) {
        errors.push_back("Node '" + node.id + "' has non-positive step");
    }

    if (node.dropdown_props.selected_index < 0 ||
            (!node.dropdown_props.options.empty() &&
             node.dropdown_props.selected_index >= static_cast<int32_t>(node.dropdown_props.options.size()))) {
        errors.push_back("Node '" + node.id + "' has invalid selected_index");
    }

    if (node.table_props.rows < 0 || node.table_props.columns < 0) {
        errors.push_back("Node '" + node.id + "' has invalid table dimensions");
    }

    for (const auto &cell : node.table_props.cells) {
        if (cell.row < 0 || cell.column < 0) {
            errors.push_back("Node '" + node.id + "' has invalid table cell coordinate");
        }
    }

    if (node.type == NodeType::Keyboard) {
        if (!is_supported_keyboard_mode(node.keyboard_props.mode)) {
            errors.push_back("Node '" + node.id + "' has unsupported keyboard mode: " + node.keyboard_props.mode);
        }
        boost::unordered_flat_set<std::string> allowed_modes;
        for (const auto &mode : node.keyboard_props.allowed_modes) {
            if (!is_supported_keyboard_mode(mode)) {
                errors.push_back("Node '" + node.id + "' has unsupported keyboard allowed mode: " + mode);
            }
            if (!allowed_modes.insert(mode).second) {
                errors.push_back("Node '" + node.id + "' has duplicate keyboard allowed mode: " + mode);
            }
        }
        if (node.keyboard_props.icon_size.mode != SizeMode::Fixed) {
            errors.push_back("Node '" + node.id + "' keyboard iconSize must be a fixed dimension");
        } else if (node.keyboard_props.icon_size.value < 0) {
            errors.push_back("Node '" + node.id + "' keyboard iconSize must be >= 0");
        }
        for (const auto &[style_class, style_ref] : node.keyboard_props.key_style_refs) {
            if (style_class != "default" && style_class != "special" && style_class != "mode" &&
                    style_class != "action" && style_class != "disabled") {
                errors.push_back("Node '" + node.id + "' has unsupported keyboard key style ref class: " + style_class);
            }
            if (style_ref.empty()) {
                errors.push_back("Node '" + node.id + "' has empty keyboard key style ref for class: " + style_class);
            }
        }
        for (const auto &[mode, layout] : node.keyboard_props.layouts) {
            if (!is_supported_keyboard_mode(mode)) {
                errors.push_back("Node '" + node.id + "' has unsupported keyboard layout mode: " + mode);
            }
            for (const auto &row : layout.rows) {
                if (row.empty()) {
                    errors.push_back("Node '" + node.id + "' has empty keyboard layout row");
                }
                for (const auto &key : row) {
                    if (key.text.empty() && key.role.empty()) {
                        errors.push_back("Node '" + node.id + "' has keyboard key without text or role");
                    }
                    if (key.width <= 0 || key.width > 15) {
                        errors.push_back("Node '" + node.id + "' has keyboard key width out of range");
                    }
                    if (!is_supported_keyboard_key_role(key.role)) {
                        errors.push_back("Node '" + node.id + "' has unsupported keyboard key role: " + key.role);
                    }
                    if (key.role == "mode" && !is_supported_keyboard_mode(key.mode)) {
                        errors.push_back("Node '" + node.id + "' has unsupported keyboard mode key target: " + key.mode);
                    }
                    if (key.role != "mode" && !key.mode.empty()) {
                        errors.push_back("Node '" + node.id + "' has keyboard mode target on a non-mode key");
                    }
                    if (!key.image.empty() && key.image.find('/') != std::string::npos) {
                        errors.push_back("Node '" + node.id + "' has invalid keyboard key image id: " + key.image);
                    }
                }
            }
        }
        for (const auto &[style_class, key_style] : node.keyboard_props.key_styles) {
            if (style_class != "default" && style_class != "special" && style_class != "mode" &&
                    style_class != "action" && style_class != "disabled") {
                errors.push_back("Node '" + node.id + "' has unsupported keyboard key style class: " + style_class);
            }
            if (!is_valid_color(key_style.bg_color) || !is_valid_color(key_style.text_color) ||
                    !is_valid_color(key_style.pressed_bg_color) || !is_valid_color(key_style.pressed_text_color)) {
                errors.push_back("Node '" + node.id + "' has invalid keyboard key style color");
            }
            if (key_style.radius < 0) {
                errors.push_back("Node '" + node.id + "' has invalid keyboard key style radius");
            }
        }
    } else if (!node.keyboard_props.layouts.empty() || !node.keyboard_props.target_text_input.empty() ||
               !node.keyboard_props.allowed_modes.empty() || !node.keyboard_props.key_styles.empty()) {
        errors.push_back("Node '" + node.id + "' has keyboard props on a non-keyboard node");
    }

    for (const auto &event : node.events) {
        if (event.type == EventType::Max) {
            errors.push_back("Node '" + node.id + "' has invalid event type");
        }
        if (!event.action.empty()) {
            const auto owner = current_view_path.empty() ? node.id : join_view_path(current_view_path);
            auto [it, inserted] = action_owners.emplace(event.action, owner);
            if (!inserted && it->second != owner) {
                errors.push_back(
                    "Document action must be unique: '" + event.action + "' is used by both '" + it->second +
                    "' and '" + owner + "'"
                );
            }
        }
    }

    boost::unordered_flat_set<std::string> animation_ids;
    for (const auto &animation : node.animations) {
        if (!animation.id.empty() && !animation_ids.insert(animation.id).second) {
            errors.push_back("Node '" + node.id + "' has duplicate animation id: " + animation.id);
        }
        if (animation.trigger == AnimationTrigger::Max) {
            errors.push_back("Node '" + node.id + "' has invalid animation trigger");
        }
        if (animation.property == AnimationProperty::Max) {
            errors.push_back("Node '" + node.id + "' has invalid animation property");
        }
        if (animation.easing == AnimationEasing::Max) {
            errors.push_back("Node '" + node.id + "' has invalid animation easing");
        }
        if (animation.from_mode == AnimationValueMode::Max || animation.from_mode == AnimationValueMode::Relative) {
            errors.push_back("Node '" + node.id + "' has invalid animation fromMode");
        }
        if (animation.to_mode == AnimationValueMode::Max || animation.to_mode == AnimationValueMode::Current) {
            errors.push_back("Node '" + node.id + "' has invalid animation toMode");
        }
        if (animation.duration < 0 || animation.delay < 0 || animation.repeat < 0) {
            errors.push_back("Node '" + node.id + "' has invalid animation timing");
        }
    }

    for (const auto &[property, expression] : node.bindings) {
        auto binding_target = resolve_binding_target(node.type, property);
        if (!binding_target) {
            errors.push_back("Node '" + node.id + "' has invalid binding path '" + property + "': " + binding_target.error());
        }

        auto store_key = normalize_binding_store_key(expression);
        if (!store_key) {
            errors.push_back("Node '" + node.id + "' has invalid binding value '" + expression + "': " + store_key.error());
        }
    }

    boost::unordered_flat_set<std::string> sibling_ids;
    for (const auto &child : node.children) {
        if (child.type == NodeType::Screen) {
            errors.push_back("Node '" + node.id + "' must not contain nested node type 'Screen'");
        }
        if (!child.id.empty() && !sibling_ids.insert(child.id).second) {
            errors.push_back(
                "Node '" + node.id + "' has duplicate child id in the same parent: " + child.id
            );
        }
        auto child_view_path = current_view_path;
        if (!child.id.empty()) {
            child_view_path.push_back(child.id);
        }
        validate_node(child, font_ids, view_paths, action_owners, std::move(child_view_path), errors);
    }
}

} // namespace

ValidationResult validate_document(const Document &document)
{
    BROOKESIA_LOG_TRACE_GUARD();

    // BROOKESIA_LOGD("Params: document(%1%)", document);

    ValidationResult result;

    validate_document_version(document.version, result);

    if (document.screens.empty()) {
        result.errors.push_back("GUI document requires at least one top-level screen");
    }

    boost::unordered_flat_set<std::string> font_ids;
    boost::unordered_flat_map<std::string, const FontAsset *> font_map;
    boost::unordered_flat_set<std::string> image_ids;
    boost::unordered_flat_set<std::string> theme_ids;
    for (const auto &font : document.fonts) {
        if (font.id.empty()) {
            result.errors.push_back("Font asset id must not be empty");
            continue;
        }
        if (!font_ids.insert(font.id).second) {
            result.errors.push_back("Duplicate font asset id: " + font.id);
            continue;
        }
        if (font.src.empty()) {
            result.errors.push_back("Font asset '" + font.id + "' must not use an empty src");
        }
        font_map.emplace(font.id, &font);
    }

    for (const auto &image : document.images) {
        if (image.id.empty()) {
            result.errors.push_back("Image asset id must not be empty");
            continue;
        }
        if (!image_ids.insert(image.id).second) {
            result.errors.push_back("Duplicate image asset id: " + image.id);
            continue;
        }
        if (image.src.empty()) {
            result.errors.push_back("Image asset '" + image.id + "' must not use an empty src");
        }
        if (image.width <= 0) {
            result.errors.push_back("Image asset '" + image.id + "' must use a positive width");
        }
        if (image.height <= 0) {
            result.errors.push_back("Image asset '" + image.id + "' must use a positive height");
        }
    }

    for (const auto &theme : document.themes) {
        if (theme.id.empty()) {
            result.errors.push_back("Theme asset id must not be empty");
            continue;
        }
        if (!theme_ids.insert(theme.id).second) {
            result.errors.push_back("Duplicate theme asset id: " + theme.id);
        }
        for (const auto &[color_name, color] : theme.colors) {
            if (color_name.empty()) {
                result.errors.push_back("Theme asset color key must not be empty");
                continue;
            }
            if (!is_valid_color(color)) {
                result.errors.push_back("Theme asset color '" + color_name + "' is invalid: " + color);
            }
        }
        for (const auto &[style_key, style_set] : theme.styles) {
            validate_style_set(
                style_set,
                "Theme '" + theme.id + "' style '" + style_key + "'",
                result.errors
            );
        }
    }

    for (const auto &[style_key, style_set] : document.styles) {
        if (style_key.empty()) {
            result.errors.push_back("Local style key must not be empty");
            continue;
        }
        if (style_key.find('.') == std::string::npos) {
            result.errors.push_back("Local style key must contain '.': " + style_key);
            continue;
        }
        validate_style_set(style_set, "Local style '" + style_key + "'", result.errors);
    }

    for (const auto &font : document.fonts) {
        for (const auto &fallback : font.fallbacks) {
            if (!font_ids.contains(fallback)) {
                result.errors.push_back("Font asset '" + font.id + "' references missing fallback: " + fallback);
            }
        }
    }

    boost::unordered_flat_set<std::string> visiting_fonts;
    boost::unordered_flat_set<std::string> visited_fonts;
    std::function<void(const std::string &)> validate_font_cycles = [&](const std::string & font_id) {
        if (visited_fonts.contains(font_id)) {
            return;
        }
        if (!visiting_fonts.insert(font_id).second) {
            result.errors.push_back("Font fallback graph contains a cycle at: " + font_id);
            return;
        }

        auto font_it = font_map.find(font_id);
        if (font_it != font_map.end()) {
            for (const auto &fallback : font_it->second->fallbacks) {
                validate_font_cycles(fallback);
            }
        }

        visiting_fonts.erase(font_id);
        visited_fonts.insert(font_id);
    };
    for (const auto &[font_id, unused_font] : font_map) {
        (void)unused_font;
        validate_font_cycles(font_id);
    }

    boost::unordered_flat_set<std::string> top_level_ids;
    boost::unordered_flat_set<std::string> screen_ids;
    boost::unordered_flat_map<std::string, std::string> action_owners;
    for (const auto &screen : document.screens) {
        if (screen.type != NodeType::Screen) {
            result.errors.push_back("Top-level screen definitions must use node type 'Screen'");
        }
        if (!screen.id.empty() && !top_level_ids.insert(screen.id).second) {
            result.errors.push_back("Duplicate top-level node id: " + screen.id);
        }
        if (!screen.id.empty()) {
            screen_ids.insert(screen.id);
        }
        boost::unordered_flat_map<std::string, std::vector<std::string>> view_paths;
        std::vector<std::string> current_path;
        for (const auto &child : screen.children) {
            build_view_path_index(child, current_path, view_paths, result.errors);
        }
        validate_node(screen, font_ids, view_paths, action_owners, {}, result.errors);
    }

    for (const auto &node : document.templates) {
        if (node.type == NodeType::Screen) {
            result.errors.push_back("Top-level template definitions must not use node type 'Screen'");
        }
        if (!node.id.empty() && !top_level_ids.insert(node.id).second) {
            result.errors.push_back("Duplicate top-level node id: " + node.id);
        }
        boost::unordered_flat_map<std::string, std::vector<std::string>> view_paths;
        std::vector<std::string> current_path;
        for (const auto &child : node.children) {
            build_view_path_index(child, current_path, view_paths, result.errors);
        }
        validate_node(node, font_ids, view_paths, action_owners, {}, result.errors);
    }

    boost::unordered_flat_set<std::string> screen_flow_ids;
    boost::unordered_flat_set<std::string> screen_flow_owned_screens;
    for (const auto &flow : document.screen_flows) {
        if (flow.id.empty()) {
            result.errors.push_back("ScreenFlow id must not be empty");
        } else if (!screen_flow_ids.insert(flow.id).second) {
            result.errors.push_back("Duplicate screenFlow id: " + flow.id);
        }
        if (flow.screens.empty()) {
            result.errors.push_back("ScreenFlow screens must not be empty: " + flow.id);
        }
        boost::unordered_flat_set<std::string> local_screens;
        for (const auto &screen : flow.screens) {
            if (screen.empty()) {
                result.errors.push_back("ScreenFlow screens must not contain empty screen id");
                continue;
            }
            if (!screen_ids.contains(screen)) {
                result.errors.push_back("ScreenFlow references missing screen id: " + screen);
                continue;
            }
            if (!local_screens.insert(screen).second) {
                result.errors.push_back("ScreenFlow contains duplicate screen id: " + screen);
                continue;
            }
            if (!screen_flow_owned_screens.insert(screen).second) {
                result.errors.push_back("Screen is owned by multiple screenFlow assets: " + screen);
            }
        }
        if (flow.initial.empty()) {
            result.errors.push_back("ScreenFlow initial state must not be empty");
        }

        if (!flow.initial.empty() && !local_screens.contains(flow.initial)) {
            result.errors.push_back(
                "ScreenFlow initial state references screen outside this flow: " + flow.initial
            );
        }

        boost::unordered_flat_set<std::string> transition_keys;
        for (const auto &transition : flow.transitions) {
            if (transition.action.empty()) {
                result.errors.push_back("ScreenFlow transition action must not be empty");
            }
            if (transition.to.empty() || !local_screens.contains(transition.to)) {
                result.errors.push_back(
                    "ScreenFlow transition references to screen outside this flow: " + transition.to
                );
            }

            if (transition.from.empty()) {
                const auto key = std::string("\x1f") + transition.action;
                if (!transition_keys.insert(key).second) {
                    result.errors.push_back(
                        "ScreenFlow contains duplicate wildcard transition for action: " + transition.action
                    );
                }
                continue;
            }

            boost::unordered_flat_set<std::string> from_ids;
            for (const auto &from : transition.from) {
                if (from.empty()) {
                    result.errors.push_back("ScreenFlow transition from list contains an empty state id");
                    continue;
                }
                if (!local_screens.contains(from)) {
                    result.errors.push_back(
                        "ScreenFlow transition references from screen outside this flow: " + from
                    );
                    continue;
                }
                if (!from_ids.insert(from).second) {
                    result.errors.push_back(
                        "ScreenFlow transition contains duplicate from state: " + from
                    );
                    continue;
                }

                const auto key = from + "\x1f" + transition.action;
                if (!transition_keys.insert(key).second) {
                    result.errors.push_back(
                        "ScreenFlow contains duplicate transition for source/action: " +
                        from + "/" + transition.action
                    );
                }
            }
        }
    }

    result.success = result.errors.empty();
#if BROOKESIA_GUI_INTERFACE_VALIDATOR_ENABLE_DEBUG_LOG
    for (const auto &error : result.errors) {
        BROOKESIA_LOGD("Validation error: %1%", error);
    }
    for (const auto &warning : result.warnings) {
        BROOKESIA_LOGD("Validation warning: %1%", warning);
    }
    for (const auto &info : result.infos) {
        BROOKESIA_LOGD("Validation info: %1%", info);
    }
#endif
    return result;
}

} // namespace esp_brookesia::gui
