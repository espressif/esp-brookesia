/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/parser_impl.hpp"

namespace esp_brookesia::gui::parser_detail {



std::expected<Style, std::string> parse_style_object(
    const boost::json::object &style_object,
    const Environment &environment,
    bool allow_font,
    bool allow_font_size)
{
    Style style;

    auto parse_optional_string = [&](std::string_view key) -> std::expected<std::optional<std::string>, std::string> {
        const auto *value = find_child_value(style_object, key);
        if (value == nullptr)
        {
            return std::optional<std::string> {};
        }
        auto parsed = parse_string_field(style_object, key, "");
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        return std::optional<std::string>(*parsed);
    };

    auto parse_optional_scaled = [&](std::string_view key, std::string_view unit, float scale)
    -> std::expected<std::optional<int32_t>, std::string> {
        const auto *value = find_child_value(style_object, key);
        if (value == nullptr)
        {
            return std::optional<int32_t> {};
        }
        auto parsed = parse_scaled_value(value, key, unit, scale);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        return std::optional<int32_t>(*parsed);
    };

    auto parse_optional_int = [&](std::string_view key) -> std::expected<std::optional<int32_t>, std::string> {
        const auto *value = find_child_value(style_object, key);
        if (value == nullptr)
        {
            return std::optional<int32_t> {};
        }
        auto parsed = parse_int_field(style_object, key, 0);
        if (!parsed)
        {
            return std::unexpected(parsed.error());
        }
        return std::optional<int32_t>(*parsed);
    };

    if (auto bg_color = parse_optional_string("bg_color"); !bg_color) {
        return std::unexpected(bg_color.error());
    } else {
        style.set_bg_color(std::move(*bg_color));
    }
    if (auto bg_gradient_color = parse_optional_string("bg_gradient_color"); !bg_gradient_color) {
        return std::unexpected(bg_gradient_color.error());
    } else {
        style.set_bg_gradient_color(std::move(*bg_gradient_color));
    }
    if (auto bg_gradient_direction = parse_optional_string("bg_gradient_direction"); !bg_gradient_direction) {
        return std::unexpected(bg_gradient_direction.error());
    } else {
        style.set_bg_gradient_direction(std::move(*bg_gradient_direction));
    }
    if (auto text_color = parse_optional_string("text_color"); !text_color) {
        return std::unexpected(text_color.error());
    } else {
        style.set_text_color(std::move(*text_color));
    }
    if (auto border_color = parse_optional_string("border_color"); !border_color) {
        return std::unexpected(border_color.error());
    } else {
        style.set_border_color(std::move(*border_color));
    }
    if (auto line_color = parse_optional_string("line_color"); !line_color) {
        return std::unexpected(line_color.error());
    } else {
        style.set_line_color(std::move(*line_color));
    }
    if (auto arc_color = parse_optional_string("arc_color"); !arc_color) {
        return std::unexpected(arc_color.error());
    } else {
        style.set_arc_color(std::move(*arc_color));
    }
    if (auto arc_gradient_color = parse_optional_string("arc_gradient_color"); !arc_gradient_color) {
        return std::unexpected(arc_gradient_color.error());
    } else {
        style.set_arc_gradient_color(std::move(*arc_gradient_color));
    }
    if (auto shadow_color = parse_optional_string("shadow_color"); !shadow_color) {
        return std::unexpected(shadow_color.error());
    } else {
        style.set_shadow_color(std::move(*shadow_color));
    }

    const auto *font_value = find_child_value(style_object, "font");
    if (font_value != nullptr) {
        if (!allow_font) {
            return std::unexpected("State styles do not support field 'font'");
        }
        auto font = parse_resource_reference_field(style_object, "font", "font", "'${font.<id>}'");
        if (!font) {
            return std::unexpected(font.error());
        }
        style.set_font(*font);
    }

    if (auto bg_main_stop = parse_optional_int("bg_main_stop"); !bg_main_stop) {
        return std::unexpected(bg_main_stop.error());
    } else {
        style.set_bg_main_stop(*bg_main_stop);
    }
    if (auto bg_gradient_stop = parse_optional_int("bg_gradient_stop"); !bg_gradient_stop) {
        return std::unexpected(bg_gradient_stop.error());
    } else {
        style.set_bg_gradient_stop(*bg_gradient_stop);
    }
    if (auto bg_gradient_opacity = parse_optional_int("bg_gradient_opacity"); !bg_gradient_opacity) {
        return std::unexpected(bg_gradient_opacity.error());
    } else {
        style.set_bg_gradient_opacity(*bg_gradient_opacity);
    }
    if (auto border_width = parse_optional_scaled("border_width", "dp", environment.density); !border_width) {
        return std::unexpected(border_width.error());
    } else {
        style.set_border_width(*border_width);
    }
    if (auto radius = parse_optional_scaled("radius", "dp", environment.density); !radius) {
        return std::unexpected(radius.error());
    } else {
        style.set_radius(*radius);
    }
    if (auto padding = parse_optional_scaled("padding", "dp", environment.density); !padding) {
        return std::unexpected(padding.error());
    } else {
        style.set_padding(*padding);
    }
    if (auto padding_left = parse_optional_scaled("padding_left", "dp", environment.density); !padding_left) {
        return std::unexpected(padding_left.error());
    } else {
        style.set_padding_left(*padding_left);
    }
    if (auto padding_right = parse_optional_scaled("padding_right", "dp", environment.density); !padding_right) {
        return std::unexpected(padding_right.error());
    } else {
        style.set_padding_right(*padding_right);
    }
    if (auto padding_top = parse_optional_scaled("padding_top", "dp", environment.density); !padding_top) {
        return std::unexpected(padding_top.error());
    } else {
        style.set_padding_top(*padding_top);
    }
    if (auto padding_bottom = parse_optional_scaled("padding_bottom", "dp", environment.density); !padding_bottom) {
        return std::unexpected(padding_bottom.error());
    } else {
        style.set_padding_bottom(*padding_bottom);
    }
    if (auto margin = parse_optional_scaled("margin", "dp", environment.density); !margin) {
        return std::unexpected(margin.error());
    } else {
        style.set_margin(*margin);
    }
    if (auto margin_left = parse_optional_scaled("margin_left", "dp", environment.density); !margin_left) {
        return std::unexpected(margin_left.error());
    } else {
        style.set_margin_left(*margin_left);
    }
    if (auto margin_right = parse_optional_scaled("margin_right", "dp", environment.density); !margin_right) {
        return std::unexpected(margin_right.error());
    } else {
        style.set_margin_right(*margin_right);
    }
    if (auto margin_top = parse_optional_scaled("margin_top", "dp", environment.density); !margin_top) {
        return std::unexpected(margin_top.error());
    } else {
        style.set_margin_top(*margin_top);
    }
    if (auto margin_bottom = parse_optional_scaled("margin_bottom", "dp", environment.density); !margin_bottom) {
        return std::unexpected(margin_bottom.error());
    } else {
        style.set_margin_bottom(*margin_bottom);
    }
    if (auto shadow_width = parse_optional_scaled("shadow_width", "dp", environment.density); !shadow_width) {
        return std::unexpected(shadow_width.error());
    } else {
        style.set_shadow_width(*shadow_width);
    }
    if (auto shadow_offset_x = parse_optional_scaled("shadow_offset_x", "dp", environment.density); !shadow_offset_x) {
        return std::unexpected(shadow_offset_x.error());
    } else {
        style.set_shadow_offset_x(*shadow_offset_x);
    }
    if (auto shadow_offset_y = parse_optional_scaled("shadow_offset_y", "dp", environment.density); !shadow_offset_y) {
        return std::unexpected(shadow_offset_y.error());
    } else {
        style.set_shadow_offset_y(*shadow_offset_y);
    }
    if (auto opacity = parse_optional_int("opacity"); !opacity) {
        return std::unexpected(opacity.error());
    } else {
        style.set_opacity(*opacity);
    }
    if (auto line_width = parse_optional_scaled("line_width", "dp", environment.density); !line_width) {
        return std::unexpected(line_width.error());
    } else {
        style.set_line_width(*line_width);
    }
    if (auto image_opacity = parse_optional_int("image_opacity"); !image_opacity) {
        return std::unexpected(image_opacity.error());
    } else {
        style.set_image_opacity(*image_opacity);
    }
    if (auto image_recolor = parse_optional_string("image_recolor"); !image_recolor) {
        return std::unexpected(image_recolor.error());
    } else {
        style.set_image_recolor(std::move(*image_recolor));
    }
    if (auto image_recolor_opacity = parse_optional_int("image_recolor_opacity"); !image_recolor_opacity) {
        return std::unexpected(image_recolor_opacity.error());
    } else {
        style.set_image_recolor_opacity(*image_recolor_opacity);
    }
    if (find_child_value(style_object, "font_size") != nullptr && !allow_font_size) {
        return std::unexpected("State styles do not support field 'fontSize'");
    }
    if (allow_font_size) {
        if (auto font_size = parse_optional_scaled("font_size", "sp", environment.density * environment.font_scale);
                !font_size) {
            return std::unexpected(font_size.error());
        } else {
            style.set_font_size(*font_size);
        }
    }
    if (find_child_value(style_object, "image_font_size") != nullptr && !allow_font_size) {
        return std::unexpected("State styles do not support field 'imageFontSize'");
    }
    if (allow_font_size) {
        if (auto image_font_size =
                    parse_optional_scaled("image_font_size", "sp", environment.density * environment.font_scale);
                !image_font_size) {
            return std::unexpected(image_font_size.error());
        } else {
            style.set_image_font_size(*image_font_size);
        }
    }
    const auto *text_align_value = find_child_value(style_object, "text_align");
    if (text_align_value != nullptr) {
        auto text_align_text = parse_string_field(style_object, "text_align", "");
        if (!text_align_text) {
            return std::unexpected(text_align_text.error());
        }
        TextAlign text_align = TextAlign::Auto;
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(*text_align_text, text_align)) {
            return std::unexpected("Field 'textAlign' has invalid enum value");
        }
        style.set_text_align(text_align);
    }

    if (auto arc_width = parse_optional_scaled("arc_width", "dp", environment.density); !arc_width) {
        return std::unexpected(arc_width.error());
    } else {
        style.set_arc_width(*arc_width);
    }
    if (auto arc_opacity = parse_optional_int("arc_opacity"); !arc_opacity) {
        return std::unexpected(arc_opacity.error());
    } else {
        style.set_arc_opacity(*arc_opacity);
    }
    if (auto arc_gradient_segments = parse_optional_int("arc_gradient_segments"); !arc_gradient_segments) {
        return std::unexpected(arc_gradient_segments.error());
    } else {
        style.set_arc_gradient_segments(*arc_gradient_segments);
    }
    const auto *arc_rounded_value = find_child_value(style_object, "arc_rounded");
    if (arc_rounded_value != nullptr) {
        if (!arc_rounded_value->is_bool()) {
            return std::unexpected("Field 'arcRounded' must be boolean");
        }
        style.set_arc_rounded(arc_rounded_value->as_bool());
    }
    const auto *clip_corner_value = find_child_value(style_object, "clip_corner");
    if (clip_corner_value != nullptr) {
        if (!clip_corner_value->is_bool()) {
            return std::unexpected("Field 'clipCorner' must be boolean");
        }
        style.set_clip_corner(clip_corner_value->as_bool());
    }

    return style;
}

std::expected<StateStyleMap, std::string> parse_state_styles_object(
    const boost::json::object &state_styles_object,
    const Environment &environment)
{
    StateStyleMap state_styles;

    for (const auto &[state_key, style_value] : state_styles_object) {
        const std::string state_name(state_key.begin(), state_key.end());
        if (!is_supported_style_state_name(state_name)) {
            return std::unexpected("Unsupported stateStyles state: " + state_name);
        }
        if (!style_value.is_object()) {
            return std::unexpected("stateStyles." + state_name + " must be an object");
        }
        auto style = parse_style_object(style_value.as_object(), environment, false, false);
        if (!style) {
            return std::unexpected("stateStyles." + state_name + " parse failed: " + style.error());
        }
        state_styles.insert_or_assign(state_name, std::move(*style));
    }

    return state_styles;
}

std::expected<PartStyleSet, std::string> parse_part_style_set_object(
    const boost::json::object &part_style_object,
    const Environment &environment)
{
    PartStyleSet part_style_set;

    const auto *style_value = find_child_value(part_style_object, "style");
    if (style_value != nullptr) {
        if (!style_value->is_object()) {
            return std::unexpected("partStyles style field must be an object");
        }
        auto style = parse_style_object(style_value->as_object(), environment, false, false);
        if (!style) {
            return std::unexpected(style.error());
        }
        part_style_set.style = std::move(*style);
    } else {
        auto style = parse_style_object(part_style_object, environment, false, false);
        if (!style) {
            return std::unexpected(style.error());
        }
        part_style_set.style = std::move(*style);
    }

    const auto *state_styles_value = find_child_value(part_style_object, "state_styles");
    if (state_styles_value != nullptr) {
        if (!state_styles_value->is_object()) {
            return std::unexpected("partStyles stateStyles field must be an object");
        }
        auto state_styles = parse_state_styles_object(state_styles_value->as_object(), environment);
        if (!state_styles) {
            return std::unexpected(state_styles.error());
        }
        part_style_set.state_styles = std::move(*state_styles);
    }

    return part_style_set;
}

std::expected<PartStyleMap, std::string> parse_part_styles_object(
    const boost::json::object &part_styles_object,
    const Environment &environment)
{
    PartStyleMap part_styles;

    for (const auto &[part_key, part_style_value] : part_styles_object) {
        const std::string part_name(part_key.begin(), part_key.end());
        if (!is_supported_style_part_name(part_name)) {
            return std::unexpected("Unsupported partStyles part: " + part_name);
        }
        if (!part_style_value.is_object()) {
            return std::unexpected("partStyles." + part_name + " must be an object");
        }
        auto part_style = parse_part_style_set_object(part_style_value.as_object(), environment);
        if (!part_style) {
            return std::unexpected("partStyles." + part_name + " parse failed: " + part_style.error());
        }
        part_styles.insert_or_assign(part_name, std::move(*part_style));
    }

    return part_styles;
}

std::expected<StyleSet, std::string> parse_style_set_object(
    const boost::json::object &style_object,
    const Environment &environment)
{
    StyleSet style_set;

    auto style = parse_style_object(style_object, environment);
    if (!style) {
        return std::unexpected(style.error());
    }
    style_set.style = std::move(*style);

    const auto *state_styles_value = find_child_value(style_object, "state_styles");
    if (state_styles_value != nullptr) {
        if (!state_styles_value->is_object()) {
            return std::unexpected("Field 'stateStyles' must be an object");
        }
        auto state_styles = parse_state_styles_object(state_styles_value->as_object(), environment);
        if (!state_styles) {
            return std::unexpected(state_styles.error());
        }
        style_set.state_styles = std::move(*state_styles);
    }

    const auto *part_styles_value = find_child_value(style_object, "part_styles");
    if (part_styles_value != nullptr) {
        if (!part_styles_value->is_object()) {
            return std::unexpected("Field 'partStyles' must be an object");
        }
        auto part_styles = parse_part_styles_object(part_styles_value->as_object(), environment);
        if (!part_styles) {
            return std::unexpected(part_styles.error());
        }
        style_set.part_styles = std::move(*part_styles);
    }

    return style_set;
}

const boost::unordered_flat_set<std::string> &get_theme_subtype_style_keys()
{
    static const boost::unordered_flat_set<std::string> keys = {
        "all",
        "screen",
        "container",
        "label",
        "button",
        "image",
        "textInput",
        "slider",
        "switch",
        "checkbox",
        "dropdown",
        "progressBar",
        "spinner",
        "arc",
        "line",
        "table",
        "keyboard",
        "canvas",
    };
    return keys;
}

std::expected<std::map<std::string, StyleSet>, std::string> parse_named_style_map(
    const boost::json::object &styles_object,
    const boost::json::value &constants,
    const Environment &environment,
    bool allow_subtype_keys)
{
    std::map<std::string, StyleSet> styles;
    const auto &subtype_keys = get_theme_subtype_style_keys();

    for (const auto &[style_key, style_value] : styles_object) {
        const std::string key(style_key.begin(), style_key.end());
        if (!allow_subtype_keys && key.find('.') == std::string::npos) {
            return std::unexpected(
                       "Unsupported local style key: " + key +
                       "; local named styles must contain '.'"
                   );
        }
        if (allow_subtype_keys && !subtype_keys.contains(key) && key.find('.') == std::string::npos) {
            return std::unexpected(
                       "Unsupported theme style key: " + key +
                       "; custom named styles must contain '.'"
                   );
        }
        if (!style_value.is_object()) {
            return std::unexpected("Style '" + key + "' must be an object");
        }

        boost::json::value resolved_style_value = style_value;
        auto replace_result = substitute_references(
                                  resolved_style_value,
                                  constants,
                                  environment,
        {"styles", key}
                              );
        if (!replace_result) {
            return std::unexpected("Style '" + key + "' resolve failed: " + replace_result.error());
        }

        auto style_set = parse_style_set_object(resolved_style_value.as_object(), environment);
        if (!style_set) {
            return std::unexpected("Style '" + key + "' parse failed: " + style_set.error());
        }
        styles.insert_or_assign(key, std::move(*style_set));
    }

    return styles;
}

std::expected<int32_t, std::string> parse_scaled_value(
    const boost::json::value *value, std::string_view field_name, std::string_view unit, float scale)
{
    if (value == nullptr) {
        return 0;
    }
    if (value->is_int64()) {
        return static_cast<int32_t>(value->as_int64());
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (!text.ends_with(unit)) {
        return std::unexpected(
                   "Field '" + std::string(field_name) + "' must use " + std::string(unit) + " units"
               );
    }

    const std::string number_text = text.substr(0, text.size() - unit.size());
    const float number = std::stof(number_text);
    return static_cast<int32_t>(std::lround(number * scale));
}

std::expected<PlacementOffset, std::string> parse_placement_offset(
    const boost::json::value *value, std::string_view field_name, const Environment &environment)
{
    if (value == nullptr) {
        return PlacementOffset {};
    }
    if (value->is_int64()) {
        return PlacementOffset(static_cast<int32_t>(value->as_int64()));
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (text.ends_with("%")) {
        const std::string number_text = text.substr(0, text.size() - 1);
        const float number = std::stof(number_text);
        if (!std::isfinite(number) || number < 0.0F) {
            return std::unexpected("Field '" + std::string(field_name) + "' percent value must be >= 0");
        }
        PlacementOffset offset;
        offset.mode = PlacementOffsetMode::Percent;
        offset.value = static_cast<int32_t>(std::lround(number));
        return offset;
    }

    auto fixed = parse_scaled_value(value, field_name, "dp", environment.density);
    if (!fixed) {
        return std::unexpected(fixed.error());
    }
    return PlacementOffset(*fixed);
}

std::expected<Dimension, std::string> parse_dimension(
    const boost::json::value *value, std::string_view field_name, const Environment &environment)
{
    Dimension dimension;
    if (value == nullptr) {
        return dimension;
    }

    if (value->is_int64()) {
        dimension.mode = SizeMode::Fixed;
        dimension.value = static_cast<int32_t>(value->as_int64());
        return dimension;
    }

    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string");
    }

    const std::string text = value->as_string().c_str();
    if (text == "match") {
        dimension.mode = SizeMode::Match;
        return dimension;
    }
    if (text == "wrap") {
        dimension.mode = SizeMode::Wrap;
        return dimension;
    }
    if (text.ends_with("%")) {
        const std::string number_text = text.substr(0, text.size() - 1);
        const float number = std::stof(number_text);
        if (!std::isfinite(number) || number < 0.0F) {
            return std::unexpected("Field '" + std::string(field_name) + "' percent value must be >= 0");
        }
        dimension.mode = SizeMode::Percent;
        dimension.value = static_cast<int32_t>(std::lround(number));
        return dimension;
    }
    if (!text.ends_with("dp")) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be dp/%/match/wrap");
    }

    dimension.mode = SizeMode::Fixed;
    dimension.value = static_cast<int32_t>(
                          std::lround(std::stof(text.substr(0, text.size() - 2)) * environment.density)
                      );
    return dimension;
}

std::expected<float, std::string> parse_positive_float_text(
    std::string_view text, std::string_view field_name)
{
    std::string owned(text);
    char *parse_end = nullptr;
    errno = 0;
    const float parsed = std::strtof(owned.c_str(), &parse_end);
    if (errno != 0 || parse_end == owned.c_str() || *parse_end != '\0' || !std::isfinite(parsed) ||
            parsed <= 0.0F) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a positive number");
    }
    return parsed;
}

std::expected<float, std::string> parse_aspect_ratio(
    const boost::json::value *value, std::string_view field_name)
{
    if (value == nullptr) {
        return 0.0F;
    }

    if (value->is_int64()) {
        const auto integer_value = value->as_int64();
        if (integer_value <= 0) {
            return std::unexpected("Field '" + std::string(field_name) + "' must be positive");
        }
        return static_cast<float>(integer_value);
    }
    if (value->is_uint64()) {
        const auto unsigned_value = value->as_uint64();
        if (unsigned_value == 0) {
            return std::unexpected("Field '" + std::string(field_name) + "' must be positive");
        }
        return static_cast<float>(unsigned_value);
    }
    if (value->is_double()) {
        const auto double_value = value->as_double();
        if (!std::isfinite(double_value) || double_value <= 0.0) {
            return std::unexpected("Field '" + std::string(field_name) + "' must be positive");
        }
        return static_cast<float>(double_value);
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be a string or number");
    }

    const std::string text = value->as_string().c_str();
    const auto separator = text.find(':');
    if (separator == std::string::npos) {
        return parse_positive_float_text(text, field_name);
    }

    auto width = parse_positive_float_text(std::string_view(text).substr(0, separator), field_name);
    if (!width) {
        return std::unexpected(width.error());
    }
    auto height = parse_positive_float_text(std::string_view(text).substr(separator + 1), field_name);
    if (!height) {
        return std::unexpected(height.error());
    }
    return *width / *height;
}

std::expected<std::string, std::string> parse_string_field(
    const boost::json::object &object, std::string_view key, std::string default_value)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(key) + "' must be a string");
    }
    return std::string(value->as_string().c_str());
}

std::expected<bool, std::string> parse_bool_field(
    const boost::json::object &object, std::string_view key, bool default_value)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_bool()) {
        return std::unexpected("Field '" + std::string(key) + "' must be a bool");
    }
    return value->as_bool();
}

std::expected<int32_t, std::string> parse_int_field(
    const boost::json::object &object, std::string_view key, int32_t default_value)
{
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return default_value;
    }
    if (!value->is_int64()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an integer");
    }
    return static_cast<int32_t>(value->as_int64());
}

bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

std::expected<KeyboardKey, std::string> parse_keyboard_key(const boost::json::value &value)
{
    KeyboardKey key;
    if (value.is_string()) {
        key.text = value.as_string().c_str();
        return key;
    }
    if (!value.is_object()) {
        return std::unexpected("Keyboard layout key must be a string or object");
    }

    const auto &object = value.as_object();
    auto text = parse_string_field(object, "text");
    if (!text) {
        return std::unexpected(text.error());
    }
    auto width = parse_int_field(object, "width", 1);
    if (!width) {
        return std::unexpected(width.error());
    }
    auto role = parse_string_field(object, "role");
    if (!role) {
        return std::unexpected(role.error());
    }
    auto mode = parse_string_field(object, "mode");
    if (!mode) {
        return std::unexpected(mode.error());
    }
    auto style_class = parse_string_field(object, "styleClass");
    if (!style_class) {
        return std::unexpected(style_class.error());
    }
    auto image = parse_resource_reference_field(
                     object,
                     "image",
                     "image",
                     "'${image.<id>}' syntax"
                 );
    if (!image) {
        return std::unexpected(image.error());
    }
    if (*width <= 0 || *width > 15) {
        return std::unexpected("Keyboard layout key width must be in range 1..15");
    }
    if (*role == "mode") {
        if (mode->empty()) {
            return std::unexpected("Keyboard layout mode key requires field 'mode'");
        }
        if (!is_supported_keyboard_mode(*mode)) {
            return std::unexpected("Unsupported keyboard mode target: " + *mode);
        }
    } else if (!mode->empty()) {
        return std::unexpected("Keyboard layout key field 'mode' is only valid when role is 'mode'");
    }

    key.text = std::move(*text);
    key.width = *width;
    key.role = std::move(*role);
    key.mode = std::move(*mode);
    key.style_class = std::move(*style_class);
    key.image = std::move(*image);
    return key;
}

std::expected<KeyboardLayout, std::string> parse_keyboard_layout(const boost::json::value &value)
{
    if (!value.is_array()) {
        return std::unexpected("Keyboard layout must be an array of rows");
    }

    KeyboardLayout layout;
    for (const auto &row_value : value.as_array()) {
        if (!row_value.is_array()) {
            return std::unexpected("Keyboard layout row must be an array");
        }
        std::vector<KeyboardKey> row;
        for (const auto &key_value : row_value.as_array()) {
            auto key = parse_keyboard_key(key_value);
            if (!key) {
                return std::unexpected(key.error());
            }
            if (key->text.empty() && key->role.empty()) {
                return std::unexpected("Keyboard layout key requires text or role");
            }
            row.push_back(std::move(*key));
        }
        if (row.empty()) {
            return std::unexpected("Keyboard layout row must not be empty");
        }
        layout.rows.push_back(std::move(row));
    }
    if (layout.rows.empty()) {
        return std::unexpected("Keyboard layout must not be empty");
    }
    return layout;
}

std::expected<std::map<std::string, KeyboardLayout>, std::string> parse_keyboard_layouts_field(
    const boost::json::object &object)
{
    std::map<std::string, KeyboardLayout> layouts;
    const auto *value = find_child_value(object, "layouts");
    if (value == nullptr) {
        return layouts;
    }
    if (!value->is_object()) {
        return std::unexpected("Field 'keyboardProps.layouts' must be an object");
    }

    for (const auto &[mode_key, layout_value] : value->as_object()) {
        std::string mode(mode_key.begin(), mode_key.end());
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Unsupported keyboard layout mode: " + mode);
        }
        auto layout = parse_keyboard_layout(layout_value);
        if (!layout) {
            return std::unexpected("keyboardProps.layouts." + mode + " parse failed: " + layout.error());
        }
        layouts.insert_or_assign(std::move(mode), std::move(*layout));
    }

    return layouts;
}

std::expected<std::vector<std::string>, std::string> parse_keyboard_allowed_modes_field(
    const boost::json::object &object)
{
    auto modes = parse_string_array(object, "allowedModes");
    if (!modes) {
        return std::unexpected(modes.error());
    }
    if (modes->empty()) {
        return *modes;
    }

    std::unordered_set<std::string> seen_modes;
    for (const auto &mode : *modes) {
        if (!is_supported_keyboard_mode(mode)) {
            return std::unexpected("Unsupported keyboard allowed mode: " + mode);
        }
        if (!seen_modes.insert(mode).second) {
            return std::unexpected("Duplicate keyboard allowed mode: " + mode);
        }
    }
    return *modes;
}

std::expected<KeyboardKeyStyle, std::string> parse_keyboard_key_style(
    const boost::json::value &value, std::string_view class_name, const Environment &environment)
{
    if (!value.is_object()) {
        return std::unexpected("keyboardProps.keyStyles." + std::string(class_name) + " must be an object");
    }

    const auto &object = value.as_object();
    auto bg_color = parse_string_field(object, "bgColor");
    if (!bg_color) {
        return std::unexpected(bg_color.error());
    }
    auto text_color = parse_string_field(object, "textColor");
    if (!text_color) {
        return std::unexpected(text_color.error());
    }
    auto pressed_bg_color = parse_string_field(object, "pressedBgColor");
    if (!pressed_bg_color) {
        return std::unexpected(pressed_bg_color.error());
    }
    auto pressed_text_color = parse_string_field(object, "pressedTextColor");
    if (!pressed_text_color) {
        return std::unexpected(pressed_text_color.error());
    }
    int32_t radius = 0;
    if (const auto *radius_value = find_child_value(object, "radius"); radius_value != nullptr) {
        auto parsed_radius = parse_scaled_value(
                                 radius_value,
                                 "keyboardProps.keyStyles." + std::string(class_name) + ".radius",
                                 "dp",
                                 environment.density
                             );
        if (!parsed_radius) {
            return std::unexpected(parsed_radius.error());
        }
        radius = std::max<int32_t>(0, *parsed_radius);
    }
    return KeyboardKeyStyle{
        .bg_color = std::move(*bg_color),
        .text_color = std::move(*text_color),
        .pressed_bg_color = std::move(*pressed_bg_color),
        .pressed_text_color = std::move(*pressed_text_color),
        .radius = radius,
    };
}

bool is_supported_keyboard_key_style_class(std::string_view class_name)
{
    return class_name == "default" || class_name == "special" || class_name == "mode" || class_name == "action" ||
           class_name == "disabled";
}

std::expected<std::map<std::string, KeyboardKeyStyle>, std::string> parse_keyboard_key_styles_field(
    const boost::json::object &object, const Environment &environment)
{
    std::map<std::string, KeyboardKeyStyle> styles;
    const auto *value = find_child_value(object, "keyStyles");
    if (value == nullptr) {
        return styles;
    }
    if (!value->is_object()) {
        return std::unexpected("Field 'keyboardProps.keyStyles' must be an object");
    }

    for (const auto &[class_key, style_value] : value->as_object()) {
        std::string class_name(class_key.begin(), class_key.end());
        if (!is_supported_keyboard_key_style_class(class_name)) {
            return std::unexpected("Unsupported keyboard key style class: " + class_name);
        }
        auto style = parse_keyboard_key_style(style_value, class_name, environment);
        if (!style) {
            return std::unexpected(style.error());
        }
        styles.insert_or_assign(std::move(class_name), std::move(*style));
    }
    return styles;
}

std::expected<std::map<std::string, std::string>, std::string> parse_keyboard_key_style_refs_field(
    const boost::json::object &object)
{
    std::map<std::string, std::string> style_refs;
    const auto *value = find_child_value(object, "keyStyleRefs");
    if (value == nullptr) {
        return style_refs;
    }
    if (!value->is_object()) {
        return std::unexpected("Field 'keyboardProps.keyStyleRefs' must be an object");
    }

    for (const auto &[class_key, ref_value] : value->as_object()) {
        std::string class_name(class_key.begin(), class_key.end());
        if (!is_supported_keyboard_key_style_class(class_name)) {
            return std::unexpected("Unsupported keyboard key style ref class: " + class_name);
        }
        if (!ref_value.is_string()) {
            return std::unexpected("keyboardProps.keyStyleRefs." + class_name + " must be a string");
        }
        style_refs.insert_or_assign(std::move(class_name), ref_value.as_string().c_str());
    }
    return style_refs;
}

std::expected<PivotValue, std::string> parse_pivot_value(
    const boost::json::value *value,
    std::string_view field_name,
    PivotValue default_value)
{
    if (value == nullptr) {
        return default_value;
    }
    if (value->is_int64()) {
        return PivotValue{
            .percent = false,
            .value = static_cast<int32_t>(value->as_int64()),
        };
    }
    if (!value->is_string()) {
        return std::unexpected("Field '" + std::string(field_name) + "' must be an integer or percent string");
    }

    std::string text(value->as_string().c_str());
    if (!text.ends_with('%')) {
        return std::unexpected("Field '" + std::string(field_name) + "' percent value must end with '%'");
    }
    text.pop_back();
    char *parse_end = nullptr;
    errno = 0;
    const float parsed = std::strtof(text.c_str(), &parse_end);
    if (errno != 0 || parse_end == text.c_str() || *parse_end != '\0') {
        return std::unexpected("Field '" + std::string(field_name) + "' percent value must be numeric");
    }
    return PivotValue{
        .percent = true,
        .value = static_cast<int32_t>(std::lround(parsed)),
    };
}

std::expected<PivotValue, std::string> parse_pivot_field(
    const boost::json::object &object,
    std::string_view key,
    PivotValue default_value)
{
    return parse_pivot_value(find_child_value(object, key), key, default_value);
}

std::expected<std::vector<Dimension>, std::string> parse_dimension_array(
    const boost::json::object &object,
    std::string_view key,
    const Environment &environment)
{
    std::vector<Dimension> result;
    const auto *value = find_child_value(object, key);
    if (value == nullptr) {
        return result;
    }
    if (!value->is_array()) {
        return std::unexpected("Field '" + std::string(key) + "' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        auto dimension = parse_dimension(&entry, key, environment);
        if (!dimension) {
            return std::unexpected(dimension.error());
        }
        result.push_back(*dimension);
    }
    return result;
}

std::expected<void, std::string> reject_fields(
    const boost::json::object &object, std::initializer_list<std::string_view> fields, std::string_view target)
{
    for (auto field : fields) {
        if (find_child_value(object, field) != nullptr) {
            return std::unexpected(
                       "Field '" + std::string(field) + "' must be moved to '" + std::string(target) + "'"
                   );
        }
    }
    return {};
}

std::expected<std::vector<Point>, std::string> parse_points_field(const boost::json::object &object)
{
    std::vector<Point> points;
    const auto *value = find_child_value(object, "points");
    if (value == nullptr) {
        return points;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'points' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("Field 'points' entries must be objects");
        }
        const auto &point_object = entry.as_object();
        auto x = parse_int_field(point_object, "x");
        if (!x) {
            return std::unexpected(x.error());
        }
        auto y = parse_int_field(point_object, "y");
        if (!y) {
            return std::unexpected(y.error());
        }
        points.push_back(Point{.x = *x, .y = *y});
    }
    return points;
}

std::expected<std::vector<TableCell>, std::string> parse_table_cells_field(const boost::json::object &object)
{
    std::vector<TableCell> cells;
    const auto *value = find_child_value(object, "cells");
    if (value == nullptr) {
        return cells;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'cells' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("Field 'cells' entries must be objects");
        }
        const auto &cell_object = entry.as_object();
        auto row = parse_int_field(cell_object, "row");
        if (!row) {
            return std::unexpected(row.error());
        }
        auto column = parse_int_field(cell_object, "column");
        if (!column) {
            return std::unexpected(column.error());
        }
        auto text = parse_string_field(cell_object, "text");
        if (!text) {
            return std::unexpected(text.error());
        }
        cells.push_back(TableCell{.row = *row, .column = *column, .text = *text});
    }
    return cells;
}

std::expected<std::vector<CanvasCommand>, std::string> parse_canvas_commands_field(
    const boost::json::object &object)
{
    std::vector<CanvasCommand> commands;
    const auto *value = find_child_value(object, "commands");
    if (value == nullptr) {
        return commands;
    }
    if (!value->is_array()) {
        return std::unexpected("Field 'commands' must be an array");
    }
    for (const auto &entry : value->as_array()) {
        if (!entry.is_object()) {
            return std::unexpected("Field 'commands' entries must be objects");
        }
        const auto &command_object = entry.as_object();
        auto type = parse_string_field(command_object, "type");
        if (!type) {
            return std::unexpected(type.error());
        }
        auto x = parse_int_field(command_object, "x");
        if (!x) {
            return std::unexpected(x.error());
        }
        auto y = parse_int_field(command_object, "y");
        if (!y) {
            return std::unexpected(y.error());
        }
        auto width = parse_int_field(command_object, "width");
        if (!width) {
            return std::unexpected(width.error());
        }
        auto height = parse_int_field(command_object, "height");
        if (!height) {
            return std::unexpected(height.error());
        }
        auto color = parse_string_field(command_object, "color");
        if (!color) {
            return std::unexpected(color.error());
        }
        commands.push_back(CanvasCommand{
            .type = *type,
            .x = *x,
            .y = *y,
            .width = *width,
            .height = *height,
            .color = *color,
        });
    }
    return commands;
}
}
