/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string_view>
#include <string>
#include <variant>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/gui_interface/enums.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::gui {

inline constexpr std::string_view CURRENT_DOCUMENT_VERSION = "0.1.0";

struct Environment {
    int32_t width_px = 320;
    int32_t height_px = 480;
    float density = 1.0F;
    float font_scale = 1.0F;
    std::string language = "en";
    std::string theme_id = "default";
    std::map<std::string, std::string> colors = {};
};

struct EnvironmentDependencies {
    bool theme = false;
    bool language = false;
    bool metrics = false;

    bool any() const
    {
        return theme || language || metrics;
    }
};

struct Dimension {
    SizeMode mode = SizeMode::Wrap;
    int32_t value = 0;

    bool operator==(const Dimension &) const = default;
};

struct PlacementOffset {
    PlacementOffsetMode mode = PlacementOffsetMode::Fixed;
    int32_t value = 0;

    PlacementOffset() = default;
    PlacementOffset(int32_t fixed_value)
        : mode(PlacementOffsetMode::Fixed)
        , value(fixed_value)
    {}

    PlacementOffset &operator=(int32_t fixed_value)
    {
        mode = PlacementOffsetMode::Fixed;
        value = fixed_value;
        return *this;
    }

    bool operator==(const PlacementOffset &) const = default;
};

struct PivotValue {
    bool percent = false;
    int32_t value = 0;

    bool operator==(const PivotValue &) const = default;
};

struct CommonProps {
    bool disabled = false;
    bool clickable = true;
    bool scrollable = false;
    bool press_lock = true;
    bool hidden = false;
    int32_t angle = 0;
    int32_t zoom = 256;
    PivotValue pivot_x;
    PivotValue pivot_y;

    bool operator==(const CommonProps &) const = default;
};

struct LabelProps {
    std::string text;
};

struct ImageProps {
    std::string src;
    std::string inner_align = "default";
    std::string recolor;
    int32_t recolor_opacity = 255;
    int32_t angle = 0;
    int32_t offset_x = 0;
    int32_t offset_y = 0;
    int32_t zoom = 256;
    PivotValue pivot_x;
    PivotValue pivot_y;
};

struct FrameViewProps {
    bool auto_register_output = true;
    std::string output_name;
    FrameColorFormat color_format = FrameColorFormat::RGB565;
};

struct TextInputProps {
    std::string text;
    std::string placeholder;
    bool password = false;
    bool multiline = false;
    int32_t max_length = 0;
};

struct RangeProps {
    int32_t value = 0;
    int32_t min = 0;
    int32_t max = 100;
    int32_t step = 1;
};

struct ToggleProps {
    bool checked = false;
};

struct DropdownProps {
    std::vector<std::string> options;
    int32_t selected_index = 0;
};

struct TableCell {
    int32_t row = 0;
    int32_t column = 0;
    std::string text;
};

struct TableProps {
    int32_t rows = 0;
    int32_t columns = 0;
    std::vector<TableCell> cells;
};

struct Point {
    int32_t x = 0;
    int32_t y = 0;
};

struct LineProps {
    std::vector<Point> points;
};

struct KeyboardKeyImageSpec {
    std::string image_id;
    std::string primary_src;
    uintptr_t native_src = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct KeyboardKey {
    std::string text;
    int32_t width = 1;
    std::string role;
    std::string mode;
    std::string style_class;
    std::string image;
    KeyboardKeyImageSpec resolved_image;
};

struct KeyboardLayout {
    std::vector<std::vector<KeyboardKey>> rows;
};

struct KeyboardKeyStyle {
    std::string bg_color;
    std::string text_color;
    std::string pressed_bg_color;
    std::string pressed_text_color;
    int32_t radius = 0;
};

struct KeyboardProps {
    std::string mode = "text";
    bool popovers = false;
    Dimension icon_size { .mode = SizeMode::Fixed, .value = 0 };
    std::string target_text_input;
    std::vector<std::string> allowed_modes;
    std::map<std::string, KeyboardLayout> layouts;
    std::map<std::string, KeyboardKeyStyle> key_styles;
    std::map<std::string, std::string> key_style_refs;
    std::map<std::string, KeyboardKeyStyle> resolved_key_styles;
};

struct CanvasCommand {
    std::string type;
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
    std::string color;
};

struct CanvasProps {
    std::vector<CanvasCommand> commands;
};

struct Animation {
    std::string id;
    AnimationTrigger trigger = AnimationTrigger::Manual;
    AnimationProperty property = AnimationProperty::Opacity;
    AnimationValueMode from_mode = AnimationValueMode::Absolute;
    int32_t from = 0;
    AnimationValueMode to_mode = AnimationValueMode::Absolute;
    int32_t to = 0;
    int32_t duration = 150;
    int32_t delay = 0;
    AnimationEasing easing = AnimationEasing::Linear;
    int32_t repeat = 0;
    bool playback = false;
};

struct Layout {
    LayoutType type = LayoutType::None;
    FlexFlow flex_flow = FlexFlow::Column;
    Align main_align = Align::Start;
    Align cross_align = Align::Start;
    int32_t gap = 0;
    std::vector<Dimension> grid_template_columns;
    std::vector<Dimension> grid_template_rows;

    bool operator==(const Layout &) const = default;
};

struct Placement {
    PlacementMode mode = PlacementMode::Absolute;
    Dimension width;
    Dimension height;
    std::optional<float> aspect_ratio;
    PlacementOffset x;
    PlacementOffset y;
    PlacementAlign align = PlacementAlign::TopLeft;
    std::string relative_to;
    int32_t grid_column = 0;
    int32_t grid_row = 0;
    int32_t grid_column_span = 1;
    int32_t grid_row_span = 1;
    Align align_self = Align::Start;
    int32_t flex_grow = 0;

    bool operator==(const Placement &) const = default;
};

struct Style {
    std::optional<std::string> bg_color;
    std::optional<std::string> bg_gradient_color;
    std::optional<std::string> bg_gradient_direction;
    std::optional<std::string> text_color;
    std::optional<std::string> border_color;
    std::optional<std::string> line_color;
    std::optional<std::string> arc_color;
    std::optional<std::string> arc_gradient_color;
    std::optional<std::string> font;
    std::optional<int32_t> bg_main_stop;
    std::optional<int32_t> bg_gradient_stop;
    std::optional<int32_t> bg_gradient_opacity;
    std::optional<int32_t> border_width;
    std::optional<int32_t> radius;
    std::optional<int32_t> padding;
    std::optional<int32_t> padding_left;
    std::optional<int32_t> padding_right;
    std::optional<int32_t> padding_top;
    std::optional<int32_t> padding_bottom;
    std::optional<int32_t> margin;
    std::optional<int32_t> margin_left;
    std::optional<int32_t> margin_right;
    std::optional<int32_t> margin_top;
    std::optional<int32_t> margin_bottom;
    std::optional<int32_t> shadow_width;
    std::optional<int32_t> shadow_offset_x;
    std::optional<int32_t> shadow_offset_y;
    std::optional<std::string> shadow_color;
    std::optional<int32_t> opacity;
    std::optional<int32_t> line_width;
    std::optional<int32_t> image_opacity;
    std::optional<std::string> image_recolor;
    std::optional<int32_t> image_recolor_opacity;
    std::optional<int32_t> font_size;
    std::optional<int32_t> image_font_size;
    std::optional<TextAlign> text_align;
    std::optional<int32_t> arc_width;
    std::optional<int32_t> arc_opacity;
    std::optional<int32_t> arc_gradient_segments;
    std::optional<bool> arc_rounded;
    std::optional<bool> clip_corner;
};

using StateStyleMap = std::map<std::string, Style>;

struct PartStyleSet {
    Style style;
    StateStyleMap state_styles;
};

using PartStyleMap = std::map<std::string, PartStyleSet>;

struct StyleSet {
    Style style;
    StateStyleMap state_styles;
    PartStyleMap part_styles;
};

inline bool is_supported_style_state_name(std::string_view state)
{
    return state == "pressed" || state == "checked" || state == "focused" || state == "focusKey" ||
           state == "edited" || state == "hovered" || state == "scrolled" || state == "disabled" ||
           state == "user1" || state == "user2" || state == "user3" || state == "user4";
}

inline bool is_supported_style_part_name(std::string_view part)
{
    return part == "indicator" || part == "knob";
}

struct ImageFontGlyph {
    uint32_t codepoint = 0;
    std::string src;
};

struct ImageFontSize {
    int32_t height = 0;
    std::vector<ImageFontGlyph> glyphs;
};

struct FontAsset {
    std::string id;
    std::string kind = "file";
    std::string src;
    std::vector<std::string> languages;
    std::vector<std::string> fallbacks;
    int32_t height = 0;
    std::vector<ImageFontGlyph> glyphs;
    std::vector<ImageFontSize> sizes;
};

struct ImageAsset {
    std::string id;
    std::string src;
    int32_t width = 0;
    int32_t height = 0;
};

struct ThemeAsset {
    std::string id;
    std::map<std::string, std::string> colors = {};
    std::map<std::string, StyleSet> styles;
};

struct StyleAsset {
    std::map<std::string, StyleSet> styles;
};

struct NativeFontVariant {
    uintptr_t native_src = 0;
    int32_t native_size = 0;
};

struct RuntimeFontResource {
    std::string id;
    std::string kind = "file";
    std::string primary_src;
    std::vector<std::string> languages;
    std::vector<std::string> fallback_ids;
    std::vector<NativeFontVariant> native_fonts;
    int32_t image_font_height = 0;
    std::vector<ImageFontGlyph> image_font_glyphs;
    std::vector<ImageFontSize> image_font_sizes;
};

struct RuntimeImageResource {
    std::string id;
    std::string primary_src;
    uintptr_t native_src = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct GuiDisplayInfo {
    std::string id;
    int32_t width_px = 0;
    int32_t height_px = 0;
    bool is_default = false;
};

struct MountTarget {
    std::string display_id;
    GuiLayer layer = GuiLayer::Default;
    MountStackMode mount_mode = MountStackMode::Replace;
    int32_t z_order = 0;
};

struct ResolvedFontSpec {
    std::string font_id;
    std::string kind = "file";
    std::string primary_src;
    std::vector<std::string> fallback_srcs;
    std::vector<NativeFontVariant> native_fonts;
    int32_t image_font_height = 0;
    std::vector<ImageFontGlyph> image_font_glyphs;
    std::vector<ImageFontSize> image_font_sizes;
};

struct ResolvedStyle {
    Style style;
    ResolvedFontSpec resolved_font;
    StateStyleMap state_styles;
    PartStyleMap part_styles;
};

struct ResolvedImageSpec {
    std::string image_id;
    std::string primary_src;
    uintptr_t native_src = 0;
    int32_t width = 0;
    int32_t height = 0;
};

struct ViewFrame {
    int32_t x = 0;
    int32_t y = 0;
    int32_t width = 0;
    int32_t height = 0;
};

using TypedPropsVariant = std::variant <
                          LabelProps,
                          ImageProps,
                          FrameViewProps,
                          TextInputProps,
                          RangeProps,
                          ToggleProps,
                          DropdownProps,
                          TableProps,
                          LineProps,
                          KeyboardProps,
                          CanvasProps
                          >;

using ViewStateValue = std::variant <
                       CommonProps,
                       TypedPropsVariant,
                       Layout,
                       Placement,
                       Style,
                       ResolvedFontSpec,
                       ResolvedImageSpec,
                       ViewFrame
                       >;

struct EventPropertyUpdate {
    std::string target = "self";
    std::string field;
    std::string value;
};

struct EventEffect {
    EventEffectType type = EventEffectType::EmitAction;
    std::string target = "self";
    std::string action;
    bool require_valid_press = false;
    std::string field;
    std::string value;
    std::vector<EventPropertyUpdate> updates;
    std::string animation_id;
    Animation animation;
};

struct EventBinding {
    EventType type = EventType::Clicked;
    std::string action;
    std::vector<EventEffect> effects;
};

struct ScreenFlowTransition {
    std::vector<std::string> from;
    std::string action;
    std::string to;
};

struct ScreenFlow {
    std::string id;
    std::vector<std::string> screens;
    std::string initial;
    std::vector<ScreenFlowTransition> transitions;
};

using BindingMap = std::map<std::string, std::string>;

struct Node {
    NodeType type = NodeType::Container;
    MountMode mount_mode = MountMode::Eager;
    std::string id;
    CommonProps common_props;
    LabelProps label_props;
    ImageProps image_props;
    FrameViewProps frame_view_props;
    TextInputProps text_input_props;
    RangeProps range_props;
    ToggleProps toggle_props;
    DropdownProps dropdown_props;
    TableProps table_props;
    LineProps line_props;
    KeyboardProps keyboard_props;
    CanvasProps canvas_props;
    Layout layout;
    Placement placement;
    Style style;
    StateStyleMap state_styles;
    PartStyleMap part_styles;
    std::vector<std::string> style_refs;
    ResolvedImageSpec resolved_image;
    std::vector<EventBinding> events;
    std::vector<Animation> animations;
    BindingMap bindings;
    std::vector<Node> children;
};

struct Document {
    std::string version = std::string(CURRENT_DOCUMENT_VERSION);
    EnvironmentDependencies environment_dependencies;
    // Compatibility summary for existing callers. New runtime code should use environment_dependencies
    // so changing one environment field does not force unrelated file-backed documents to reparse.
    bool theme_sensitive = false;
    boost::json::value constants;
    std::vector<FontAsset> fonts;
    std::vector<ImageAsset> images;
    std::vector<ThemeAsset> themes;
    std::map<std::string, StyleSet> styles;
    std::vector<ScreenFlow> screen_flows;
    std::vector<Node> screens;
    std::vector<Node> templates;
};

struct ValidationResult {
    bool success = true;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
    std::vector<std::string> infos;
};

BROOKESIA_DESCRIBE_STRUCT(Environment, (), (width_px, height_px, density, font_scale, language, theme_id, colors))
BROOKESIA_DESCRIBE_STRUCT(EnvironmentDependencies, (), (theme, language, metrics))
BROOKESIA_DESCRIBE_STRUCT(Dimension, (), (mode, value))
BROOKESIA_DESCRIBE_STRUCT(PlacementOffset, (), (mode, value))
BROOKESIA_DESCRIBE_STRUCT(PivotValue, (), (percent, value))
BROOKESIA_DESCRIBE_STRUCT(
    CommonProps, (), (disabled, clickable, scrollable, press_lock, hidden, angle, zoom, pivot_x, pivot_y)
)
BROOKESIA_DESCRIBE_STRUCT(LabelProps, (), (text))
BROOKESIA_DESCRIBE_STRUCT(
    ImageProps, (), (src, inner_align, recolor, recolor_opacity, angle, offset_x, offset_y, zoom, pivot_x, pivot_y)
)
BROOKESIA_DESCRIBE_STRUCT(FrameViewProps, (), (auto_register_output, output_name, color_format))
BROOKESIA_DESCRIBE_STRUCT(TextInputProps, (), (text, placeholder, password, multiline, max_length))
BROOKESIA_DESCRIBE_STRUCT(RangeProps, (), (value, min, max, step))
BROOKESIA_DESCRIBE_STRUCT(ToggleProps, (), (checked))
BROOKESIA_DESCRIBE_STRUCT(DropdownProps, (), (options, selected_index))
BROOKESIA_DESCRIBE_STRUCT(TableCell, (), (row, column, text))
BROOKESIA_DESCRIBE_STRUCT(TableProps, (), (rows, columns, cells))
BROOKESIA_DESCRIBE_STRUCT(Point, (), (x, y))
BROOKESIA_DESCRIBE_STRUCT(LineProps, (), (points))
BROOKESIA_DESCRIBE_STRUCT(KeyboardKeyImageSpec, (), (image_id, primary_src, native_src, width, height))
BROOKESIA_DESCRIBE_STRUCT(KeyboardKey, (), (text, width, role, mode, style_class, image, resolved_image))
BROOKESIA_DESCRIBE_STRUCT(KeyboardLayout, (), (rows))
BROOKESIA_DESCRIBE_STRUCT(KeyboardKeyStyle, (), (bg_color, text_color, pressed_bg_color, pressed_text_color, radius))
BROOKESIA_DESCRIBE_STRUCT(
    KeyboardProps, (),
    (mode, popovers, icon_size, target_text_input, allowed_modes, layouts, key_styles, key_style_refs,
     resolved_key_styles)
)
BROOKESIA_DESCRIBE_STRUCT(CanvasCommand, (), (type, x, y, width, height, color))
BROOKESIA_DESCRIBE_STRUCT(CanvasProps, (), (commands))
BROOKESIA_DESCRIBE_STRUCT(
    Animation, (), (id, trigger, property, from_mode, from, to_mode, to, duration, delay, easing, repeat, playback)
)
BROOKESIA_DESCRIBE_STRUCT(
    Layout, (),
    (type, flex_flow, main_align, cross_align, gap, grid_template_columns, grid_template_rows)
)
BROOKESIA_DESCRIBE_STRUCT(
    Placement, (),
    (mode, width, height, aspect_ratio, x, y, align, relative_to,
     grid_column, grid_row, grid_column_span, grid_row_span, align_self, flex_grow)
)
BROOKESIA_DESCRIBE_STRUCT(
    Style, (),
    (bg_color, bg_gradient_color, bg_gradient_direction, text_color, border_color, line_color, arc_color,
     arc_gradient_color, font, bg_main_stop, bg_gradient_stop, bg_gradient_opacity, border_width, radius, padding,
     padding_left, padding_right, padding_top, padding_bottom, margin, margin_left, margin_right, margin_top,
     margin_bottom, shadow_width, shadow_offset_x, shadow_offset_y, shadow_color, opacity, line_width, image_opacity,
     image_recolor, image_recolor_opacity, font_size, image_font_size, text_align, arc_width, arc_opacity,
     arc_gradient_segments, arc_rounded, clip_corner)
)
BROOKESIA_DESCRIBE_STRUCT(PartStyleSet, (), (style, state_styles))
BROOKESIA_DESCRIBE_STRUCT(StyleSet, (), (style, state_styles, part_styles))
BROOKESIA_DESCRIBE_STRUCT(ImageFontGlyph, (), (codepoint, src))
BROOKESIA_DESCRIBE_STRUCT(ImageFontSize, (), (height, glyphs))
BROOKESIA_DESCRIBE_STRUCT(FontAsset, (), (id, kind, src, languages, fallbacks, height, glyphs, sizes))
BROOKESIA_DESCRIBE_STRUCT(ImageAsset, (), (id, src, width, height))
BROOKESIA_DESCRIBE_STRUCT(ThemeAsset, (), (id, colors, styles))
BROOKESIA_DESCRIBE_STRUCT(StyleAsset, (), (styles))
BROOKESIA_DESCRIBE_STRUCT(ScreenFlowTransition, (), (from, action, to))
BROOKESIA_DESCRIBE_STRUCT(ScreenFlow, (), (id, screens, initial, transitions))
BROOKESIA_DESCRIBE_STRUCT(NativeFontVariant, (), (native_src, native_size))
BROOKESIA_DESCRIBE_STRUCT(
    RuntimeFontResource, (),
    (id, kind, primary_src, languages, fallback_ids, native_fonts, image_font_height, image_font_glyphs,
     image_font_sizes)
)
BROOKESIA_DESCRIBE_STRUCT(RuntimeImageResource, (), (id, primary_src, native_src, width, height))
BROOKESIA_DESCRIBE_STRUCT(GuiDisplayInfo, (), (id, width_px, height_px, is_default))
BROOKESIA_DESCRIBE_STRUCT(MountTarget, (), (display_id, layer, mount_mode, z_order))
BROOKESIA_DESCRIBE_STRUCT(
    ResolvedFontSpec, (),
    (font_id, kind, primary_src, fallback_srcs, native_fonts, image_font_height, image_font_glyphs,
     image_font_sizes)
)
BROOKESIA_DESCRIBE_STRUCT(ResolvedStyle, (), (style, resolved_font, state_styles, part_styles))
BROOKESIA_DESCRIBE_STRUCT(ResolvedImageSpec, (), (image_id, primary_src, native_src, width, height))
BROOKESIA_DESCRIBE_STRUCT(ViewFrame, (), (x, y, width, height))
BROOKESIA_DESCRIBE_STRUCT(EventPropertyUpdate, (), (target, field, value))
BROOKESIA_DESCRIBE_STRUCT(
    EventEffect, (), (type, target, action, require_valid_press, field, value, updates, animation_id, animation)
)
BROOKESIA_DESCRIBE_STRUCT(EventBinding, (), (type, action, effects))
BROOKESIA_DESCRIBE_STRUCT(
    Node, (),
    (type, mount_mode, id, common_props, label_props, image_props, frame_view_props, text_input_props, range_props,
     toggle_props, dropdown_props, table_props, line_props, keyboard_props, canvas_props, layout, placement, style,
     state_styles, part_styles, style_refs, resolved_image, events, animations, bindings, children)
)
BROOKESIA_DESCRIBE_STRUCT(
    Document, (),
    (version, environment_dependencies, theme_sensitive, fonts, images, themes, styles, screen_flows, screens, templates)
)
BROOKESIA_DESCRIBE_STRUCT(ValidationResult, (), (success, errors, warnings, infos))

} // namespace esp_brookesia::gui
