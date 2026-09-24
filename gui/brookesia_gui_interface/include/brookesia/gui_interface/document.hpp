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

inline constexpr std::string_view CURRENT_DOCUMENT_VERSION = "0.1.1";

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
    enum class PropId : uint8_t {
        BgColor = 0,
        BgGradientColor,
        BgGradientDirection,
        TextColor,
        BorderColor,
        LineColor,
        ArcColor,
        ArcGradientColor,
        Font,
        BgMainStop,
        BgGradientStop,
        BgGradientOpacity,
        BorderWidth,
        Radius,
        Padding,
        PaddingLeft,
        PaddingRight,
        PaddingTop,
        PaddingBottom,
        Margin,
        MarginLeft,
        MarginRight,
        MarginTop,
        MarginBottom,
        ShadowWidth,
        ShadowOffsetX,
        ShadowOffsetY,
        ShadowColor,
        Opacity,
        LineWidth,
        ImageOpacity,
        ImageRecolor,
        ImageRecolorOpacity,
        FontSize,
        ImageFontSize,
        TextAlignValue,
        ArcWidth,
        ArcOpacity,
        ArcGradientSegments,
        ArcRounded,
        ClipCorner,
    };

    using PropValue = std::variant<std::string, int32_t, bool, TextAlign>;

    struct Prop {
        PropId id = PropId::BgColor;
        PropValue value;
    };

    bool empty() const
    {
        return props_.empty();
    }

    size_t size() const
    {
        return props_.size();
    }

    const std::vector<Prop> &props() const
    {
        return props_;
    }

    void clear()
    {
        props_.clear();
    }

    void merge_from(const Style &source)
    {
        for (const auto &prop : source.props_) {
            set_prop(prop.id, prop.value);
        }
    }

    template <typename T>
    std::optional<T> get(PropId id) const
    {
        for (const auto &prop : props_) {
            if (prop.id != id) {
                continue;
            }
            if (const auto *value = std::get_if<T>(&prop.value)) {
                return *value;
            }
            return std::nullopt;
        }
        return std::nullopt;
    }

    template <typename T>
    void set(PropId id, std::optional<T> value)
    {
        if (!value.has_value()) {
            erase(id);
            return;
        }
        set_prop(id, PropValue(std::move(*value)));
    }

    std::optional<std::string> bg_color() const
    {
        return get<std::string>(PropId::BgColor);
    }
    void set_bg_color(std::optional<std::string> value)
    {
        set(PropId::BgColor, std::move(value));
    }
    std::optional<std::string> bg_gradient_color() const
    {
        return get<std::string>(PropId::BgGradientColor);
    }
    void set_bg_gradient_color(std::optional<std::string> value)
    {
        set(PropId::BgGradientColor, std::move(value));
    }
    std::optional<std::string> bg_gradient_direction() const
    {
        return get<std::string>(PropId::BgGradientDirection);
    }
    void set_bg_gradient_direction(std::optional<std::string> value)
    {
        set(PropId::BgGradientDirection, std::move(value));
    }
    std::optional<std::string> text_color() const
    {
        return get<std::string>(PropId::TextColor);
    }
    void set_text_color(std::optional<std::string> value)
    {
        set(PropId::TextColor, std::move(value));
    }
    std::optional<std::string> border_color() const
    {
        return get<std::string>(PropId::BorderColor);
    }
    void set_border_color(std::optional<std::string> value)
    {
        set(PropId::BorderColor, std::move(value));
    }
    std::optional<std::string> line_color() const
    {
        return get<std::string>(PropId::LineColor);
    }
    void set_line_color(std::optional<std::string> value)
    {
        set(PropId::LineColor, std::move(value));
    }
    std::optional<std::string> arc_color() const
    {
        return get<std::string>(PropId::ArcColor);
    }
    void set_arc_color(std::optional<std::string> value)
    {
        set(PropId::ArcColor, std::move(value));
    }
    std::optional<std::string> arc_gradient_color() const
    {
        return get<std::string>(PropId::ArcGradientColor);
    }
    void set_arc_gradient_color(std::optional<std::string> value)
    {
        set(PropId::ArcGradientColor, std::move(value));
    }
    std::optional<std::string> font() const
    {
        return get<std::string>(PropId::Font);
    }
    void set_font(std::optional<std::string> value)
    {
        set(PropId::Font, std::move(value));
    }
    std::optional<int32_t> bg_main_stop() const
    {
        return get<int32_t>(PropId::BgMainStop);
    }
    void set_bg_main_stop(std::optional<int32_t> value)
    {
        set(PropId::BgMainStop, value);
    }
    std::optional<int32_t> bg_gradient_stop() const
    {
        return get<int32_t>(PropId::BgGradientStop);
    }
    void set_bg_gradient_stop(std::optional<int32_t> value)
    {
        set(PropId::BgGradientStop, value);
    }
    std::optional<int32_t> bg_gradient_opacity() const
    {
        return get<int32_t>(PropId::BgGradientOpacity);
    }
    void set_bg_gradient_opacity(std::optional<int32_t> value)
    {
        set(PropId::BgGradientOpacity, value);
    }
    std::optional<int32_t> border_width() const
    {
        return get<int32_t>(PropId::BorderWidth);
    }
    void set_border_width(std::optional<int32_t> value)
    {
        set(PropId::BorderWidth, value);
    }
    std::optional<int32_t> radius() const
    {
        return get<int32_t>(PropId::Radius);
    }
    void set_radius(std::optional<int32_t> value)
    {
        set(PropId::Radius, value);
    }
    std::optional<int32_t> padding() const
    {
        return get<int32_t>(PropId::Padding);
    }
    void set_padding(std::optional<int32_t> value)
    {
        set(PropId::Padding, value);
    }
    std::optional<int32_t> padding_left() const
    {
        return get<int32_t>(PropId::PaddingLeft);
    }
    void set_padding_left(std::optional<int32_t> value)
    {
        set(PropId::PaddingLeft, value);
    }
    std::optional<int32_t> padding_right() const
    {
        return get<int32_t>(PropId::PaddingRight);
    }
    void set_padding_right(std::optional<int32_t> value)
    {
        set(PropId::PaddingRight, value);
    }
    std::optional<int32_t> padding_top() const
    {
        return get<int32_t>(PropId::PaddingTop);
    }
    void set_padding_top(std::optional<int32_t> value)
    {
        set(PropId::PaddingTop, value);
    }
    std::optional<int32_t> padding_bottom() const
    {
        return get<int32_t>(PropId::PaddingBottom);
    }
    void set_padding_bottom(std::optional<int32_t> value)
    {
        set(PropId::PaddingBottom, value);
    }
    std::optional<int32_t> margin() const
    {
        return get<int32_t>(PropId::Margin);
    }
    void set_margin(std::optional<int32_t> value)
    {
        set(PropId::Margin, value);
    }
    std::optional<int32_t> margin_left() const
    {
        return get<int32_t>(PropId::MarginLeft);
    }
    void set_margin_left(std::optional<int32_t> value)
    {
        set(PropId::MarginLeft, value);
    }
    std::optional<int32_t> margin_right() const
    {
        return get<int32_t>(PropId::MarginRight);
    }
    void set_margin_right(std::optional<int32_t> value)
    {
        set(PropId::MarginRight, value);
    }
    std::optional<int32_t> margin_top() const
    {
        return get<int32_t>(PropId::MarginTop);
    }
    void set_margin_top(std::optional<int32_t> value)
    {
        set(PropId::MarginTop, value);
    }
    std::optional<int32_t> margin_bottom() const
    {
        return get<int32_t>(PropId::MarginBottom);
    }
    void set_margin_bottom(std::optional<int32_t> value)
    {
        set(PropId::MarginBottom, value);
    }
    std::optional<int32_t> shadow_width() const
    {
        return get<int32_t>(PropId::ShadowWidth);
    }
    void set_shadow_width(std::optional<int32_t> value)
    {
        set(PropId::ShadowWidth, value);
    }
    std::optional<int32_t> shadow_offset_x() const
    {
        return get<int32_t>(PropId::ShadowOffsetX);
    }
    void set_shadow_offset_x(std::optional<int32_t> value)
    {
        set(PropId::ShadowOffsetX, value);
    }
    std::optional<int32_t> shadow_offset_y() const
    {
        return get<int32_t>(PropId::ShadowOffsetY);
    }
    void set_shadow_offset_y(std::optional<int32_t> value)
    {
        set(PropId::ShadowOffsetY, value);
    }
    std::optional<std::string> shadow_color() const
    {
        return get<std::string>(PropId::ShadowColor);
    }
    void set_shadow_color(std::optional<std::string> value)
    {
        set(PropId::ShadowColor, std::move(value));
    }
    std::optional<int32_t> opacity() const
    {
        return get<int32_t>(PropId::Opacity);
    }
    void set_opacity(std::optional<int32_t> value)
    {
        set(PropId::Opacity, value);
    }
    std::optional<int32_t> line_width() const
    {
        return get<int32_t>(PropId::LineWidth);
    }
    void set_line_width(std::optional<int32_t> value)
    {
        set(PropId::LineWidth, value);
    }
    std::optional<int32_t> image_opacity() const
    {
        return get<int32_t>(PropId::ImageOpacity);
    }
    void set_image_opacity(std::optional<int32_t> value)
    {
        set(PropId::ImageOpacity, value);
    }
    std::optional<std::string> image_recolor() const
    {
        return get<std::string>(PropId::ImageRecolor);
    }
    void set_image_recolor(std::optional<std::string> value)
    {
        set(PropId::ImageRecolor, std::move(value));
    }
    std::optional<int32_t> image_recolor_opacity() const
    {
        return get<int32_t>(PropId::ImageRecolorOpacity);
    }
    void set_image_recolor_opacity(std::optional<int32_t> value)
    {
        set(PropId::ImageRecolorOpacity, value);
    }
    std::optional<int32_t> font_size() const
    {
        return get<int32_t>(PropId::FontSize);
    }
    void set_font_size(std::optional<int32_t> value)
    {
        set(PropId::FontSize, value);
    }
    std::optional<int32_t> image_font_size() const
    {
        return get<int32_t>(PropId::ImageFontSize);
    }
    void set_image_font_size(std::optional<int32_t> value)
    {
        set(PropId::ImageFontSize, value);
    }
    std::optional<TextAlign> text_align() const
    {
        return get<TextAlign>(PropId::TextAlignValue);
    }
    void set_text_align(std::optional<TextAlign> value)
    {
        set(PropId::TextAlignValue, value);
    }
    std::optional<int32_t> arc_width() const
    {
        return get<int32_t>(PropId::ArcWidth);
    }
    void set_arc_width(std::optional<int32_t> value)
    {
        set(PropId::ArcWidth, value);
    }
    std::optional<int32_t> arc_opacity() const
    {
        return get<int32_t>(PropId::ArcOpacity);
    }
    void set_arc_opacity(std::optional<int32_t> value)
    {
        set(PropId::ArcOpacity, value);
    }
    std::optional<int32_t> arc_gradient_segments() const
    {
        return get<int32_t>(PropId::ArcGradientSegments);
    }
    void set_arc_gradient_segments(std::optional<int32_t> value)
    {
        set(PropId::ArcGradientSegments, value);
    }
    std::optional<bool> arc_rounded() const
    {
        return get<bool>(PropId::ArcRounded);
    }
    void set_arc_rounded(std::optional<bool> value)
    {
        set(PropId::ArcRounded, value);
    }
    std::optional<bool> clip_corner() const
    {
        return get<bool>(PropId::ClipCorner);
    }
    void set_clip_corner(std::optional<bool> value)
    {
        set(PropId::ClipCorner, value);
    }

    bool operator==(const Style &other) const
    {
        if (props_.size() != other.props_.size()) {
            return false;
        }
        for (const auto &prop : props_) {
            bool matched = false;
            for (const auto &other_prop : other.props_) {
                if (other_prop.id == prop.id && other_prop.value == prop.value) {
                    matched = true;
                    break;
                }
            }
            if (!matched) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const Style &other) const
    {
        return !(*this == other);
    }

private:
    std::vector<Prop> props_;

    void erase(PropId id)
    {
        for (auto it = props_.begin(); it != props_.end(); ++it) {
            if (it->id == id) {
                props_.erase(it);
                return;
            }
        }
    }

    void set_prop(PropId id, PropValue value)
    {
        for (auto &prop : props_) {
            if (prop.id == id) {
                prop.value = std::move(value);
                return;
            }
        }
        Prop prop;
        prop.id = id;
        prop.value = std::move(value);
        props_.push_back(std::move(prop));
    }
};

using StateStyleMap = std::map<std::string, Style>;

struct PartStyleSet {
    Style style;
    StateStyleMap state_styles;

    bool operator==(const PartStyleSet &) const = default;
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

    bool operator==(const ImageFontGlyph &) const = default;
};

struct ImageFontSize {
    int32_t height = 0;
    std::vector<ImageFontGlyph> glyphs;

    bool operator==(const ImageFontSize &) const = default;
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
    bool preload = false;
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

    bool operator==(const NativeFontVariant &) const = default;
};

struct RuntimeFontResource {
    std::string id = {};
    std::string kind = "file";
    std::string primary_src = {};
    std::vector<std::string> languages = {};
    std::vector<std::string> fallback_ids = {};
    std::vector<NativeFontVariant> native_fonts = {};
    int32_t image_font_height = 0;
    std::vector<ImageFontGlyph> image_font_glyphs = {};
    std::vector<ImageFontSize> image_font_sizes = {};
};

struct RuntimeImageResource {
    std::string id;
    std::string primary_src;
    uintptr_t native_src = 0;
    int32_t width = 0;
    int32_t height = 0;
    bool preload = false;
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

    bool operator==(const ResolvedFontSpec &) const = default;
};

struct ResolvedStyle {
    Style style;
    ResolvedFontSpec resolved_font;
    StateStyleMap state_styles;
    PartStyleMap part_styles;

    bool operator==(const ResolvedStyle &) const = default;
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
} // namespace esp_brookesia::gui

namespace esp_brookesia::lib_utils {

template <>
inline boost::json::value describe_to_json<esp_brookesia::gui::Style>(const esp_brookesia::gui::Style &style)
{
    const auto prop_name = [](esp_brookesia::gui::Style::PropId id) {
        using PropId = esp_brookesia::gui::Style::PropId;
        switch (id) {
        case PropId::BgColor: return "bg_color";
        case PropId::BgGradientColor: return "bg_gradient_color";
        case PropId::BgGradientDirection: return "bg_gradient_direction";
        case PropId::TextColor: return "text_color";
        case PropId::BorderColor: return "border_color";
        case PropId::LineColor: return "line_color";
        case PropId::ArcColor: return "arc_color";
        case PropId::ArcGradientColor: return "arc_gradient_color";
        case PropId::Font: return "font";
        case PropId::BgMainStop: return "bg_main_stop";
        case PropId::BgGradientStop: return "bg_gradient_stop";
        case PropId::BgGradientOpacity: return "bg_gradient_opacity";
        case PropId::BorderWidth: return "border_width";
        case PropId::Radius: return "radius";
        case PropId::Padding: return "padding";
        case PropId::PaddingLeft: return "padding_left";
        case PropId::PaddingRight: return "padding_right";
        case PropId::PaddingTop: return "padding_top";
        case PropId::PaddingBottom: return "padding_bottom";
        case PropId::Margin: return "margin";
        case PropId::MarginLeft: return "margin_left";
        case PropId::MarginRight: return "margin_right";
        case PropId::MarginTop: return "margin_top";
        case PropId::MarginBottom: return "margin_bottom";
        case PropId::ShadowWidth: return "shadow_width";
        case PropId::ShadowOffsetX: return "shadow_offset_x";
        case PropId::ShadowOffsetY: return "shadow_offset_y";
        case PropId::ShadowColor: return "shadow_color";
        case PropId::Opacity: return "opacity";
        case PropId::LineWidth: return "line_width";
        case PropId::ImageOpacity: return "image_opacity";
        case PropId::ImageRecolor: return "image_recolor";
        case PropId::ImageRecolorOpacity: return "image_recolor_opacity";
        case PropId::FontSize: return "font_size";
        case PropId::ImageFontSize: return "image_font_size";
        case PropId::TextAlignValue: return "text_align";
        case PropId::ArcWidth: return "arc_width";
        case PropId::ArcOpacity: return "arc_opacity";
        case PropId::ArcGradientSegments: return "arc_gradient_segments";
        case PropId::ArcRounded: return "arc_rounded";
        case PropId::ClipCorner: return "clip_corner";
        }
        return "Unknown";
    };

    boost::json::object object;
    for (const auto &prop : style.props()) {
        const auto value = std::visit([](const auto & item) {
            return describe_to_json(item);
        }, prop.value);
        object.emplace(prop_name(prop.id), value);
    }
    return object;
}

template <>
inline void describe_output_member<esp_brookesia::gui::Style>(
    std::ostringstream &oss, const char *name, const esp_brookesia::gui::Style &style, const DescribeOutputFormat &fmt)
{
    if (fmt.quote_field_names) {
        oss << "\"" << fmt.field_prefix << name << "\"";
    } else {
        oss << fmt.field_prefix << name;
    }
    oss << fmt.name_value_separator << describe_json_value_to_string(describe_to_json(style), fmt);
}

} // namespace esp_brookesia::lib_utils

namespace esp_brookesia::gui {

BROOKESIA_DESCRIBE_STRUCT(PartStyleSet, (), (style, state_styles))
BROOKESIA_DESCRIBE_STRUCT(StyleSet, (), (style, state_styles, part_styles))
BROOKESIA_DESCRIBE_STRUCT(ImageFontGlyph, (), (codepoint, src))
BROOKESIA_DESCRIBE_STRUCT(ImageFontSize, (), (height, glyphs))
BROOKESIA_DESCRIBE_STRUCT(FontAsset, (), (id, kind, src, languages, fallbacks, height, glyphs, sizes))
BROOKESIA_DESCRIBE_STRUCT(ImageAsset, (), (id, src, width, height, preload))
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
BROOKESIA_DESCRIBE_STRUCT(RuntimeImageResource, (), (id, primary_src, native_src, width, height, preload))
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
