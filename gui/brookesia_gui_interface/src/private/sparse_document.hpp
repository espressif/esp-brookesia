/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>

#include "brookesia/gui_interface/document.hpp"

namespace esp_brookesia::gui {

// Public documents keep the 0.8 optional fields; resident runtime data stores only written properties.

struct SparseStyle {
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

    void merge_from(const SparseStyle &source)
    {
        for (const auto &prop : source.props_) {
            set_prop(prop.id, prop.value);
        }
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

    bool operator==(const SparseStyle &other) const
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

    bool operator!=(const SparseStyle &other) const
    {
        return !(*this == other);
    }

private:
    std::vector<Prop> props_;

    template <typename T>
    std::optional<T> get(PropId id) const
    {
        for (const auto &prop : props_) {
            if (prop.id != id) {
                continue;
            }
            // PropId found: type must match the SparseStyle accessor that stores it.
            const auto *value = std::get_if<T>(&prop.value);
            assert(value != nullptr);
            if (value == nullptr) {
                return std::nullopt;
            }
            return *value;
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

using SparseStateStyleMap = std::map<std::string, SparseStyle>;

struct SparsePartStyleSet {
    SparseStyle style;
    SparseStateStyleMap state_styles;

    bool operator==(const SparsePartStyleSet &) const = default;
};

using SparsePartStyleMap = std::map<std::string, SparsePartStyleSet>;


struct SparseStyleSet {
    SparseStyle style;
    SparseStateStyleMap state_styles;
    SparsePartStyleMap part_styles;
};


struct SparseThemeAsset {
    std::string id;
    std::map<std::string, std::string> colors = {};
    std::map<std::string, SparseStyleSet> styles;
};


struct SparseStyleAsset {
    std::map<std::string, SparseStyleSet> styles;
};


struct SparseResolvedStyle {
    SparseStyle style;
    ResolvedFontSpec resolved_font;
    SparseStateStyleMap state_styles;
    SparsePartStyleMap part_styles;

    bool operator==(const SparseResolvedStyle &) const = default;
};


struct SparseNode {
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
    SparseStyle style;
    SparseStateStyleMap state_styles;
    SparsePartStyleMap part_styles;
    std::vector<std::string> style_refs;
    ResolvedImageSpec resolved_image;
    std::vector<EventBinding> events;
    std::vector<Animation> animations;
    BindingMap bindings;
    std::vector<SparseNode> children;
};


struct SparseDocument {
    std::string version = std::string(CURRENT_DOCUMENT_VERSION);
    EnvironmentDependencies environment_dependencies;
    // Compatibility summary for existing callers. New runtime code should use environment_dependencies
    // so changing one environment field does not force unrelated file-backed documents to reparse.
    bool theme_sensitive = false;
    boost::json::value constants;
    std::vector<FontAsset> fonts;
    std::vector<ImageAsset> images;
    std::vector<SparseThemeAsset> themes;
    std::map<std::string, SparseStyleSet> styles;
    std::vector<ScreenFlow> screen_flows;
    std::vector<SparseNode> screens;
    std::vector<SparseNode> templates;
};


struct SparseParsedDocument {
    SparseDocument document;
    std::vector<std::string> dependency_files;
};

SparseStyle to_sparse(const Style &source);
Style to_public(const SparseStyle &source);

SparsePartStyleSet to_sparse(const PartStyleSet &source);
PartStyleSet to_public(const SparsePartStyleSet &source);

SparseStyleSet to_sparse(const StyleSet &source);
StyleSet to_public(const SparseStyleSet &source);

SparseThemeAsset to_sparse(const ThemeAsset &source);
ThemeAsset to_public(const SparseThemeAsset &source);

SparseStyleAsset to_sparse(const StyleAsset &source);
StyleAsset to_public(const SparseStyleAsset &source);

SparseResolvedStyle to_sparse(const ResolvedStyle &source);
ResolvedStyle to_public(const SparseResolvedStyle &source);

SparseNode to_sparse(const Node &source);
Node to_public(const SparseNode &source);

SparseDocument to_sparse(const Document &source);
Document to_public(const SparseDocument &source);

Node to_public_node_snapshot(const SparseNode &source);
std::shared_ptr<const SparseResolvedStyle> intern_sparse_resolved_style(
    std::vector<std::weak_ptr<const SparseResolvedStyle>> &pool,
    SparseResolvedStyle style,
    bool *reused = nullptr);

} // namespace esp_brookesia::gui


namespace esp_brookesia::lib_utils {

template <>
inline boost::json::value describe_to_json<esp_brookesia::gui::SparseStyle>(const esp_brookesia::gui::SparseStyle &style)
{
    const auto prop_name = [](esp_brookesia::gui::SparseStyle::PropId id) {
        using PropId = esp_brookesia::gui::SparseStyle::PropId;
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
    auto props = style.props();
    std::sort(props.begin(), props.end(), [](const auto & lhs, const auto & rhs) {
        return lhs.id < rhs.id;
    });
    for (const auto &prop : props) {
        const auto value = std::visit([](const auto & item) {
            return describe_to_json(item);
        }, prop.value);
        object.emplace(prop_name(prop.id), value);
    }
    return object;
}

template <>
inline void describe_output_member<esp_brookesia::gui::SparseStyle>(
    std::ostringstream &oss, const char *name, const esp_brookesia::gui::SparseStyle &style, const DescribeOutputFormat &fmt)
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

BROOKESIA_DESCRIBE_STRUCT(SparsePartStyleSet, (), (style, state_styles))

BROOKESIA_DESCRIBE_STRUCT(SparseStyleSet, (), (style, state_styles, part_styles))

BROOKESIA_DESCRIBE_STRUCT(SparseThemeAsset, (), (id, colors, styles))

BROOKESIA_DESCRIBE_STRUCT(SparseStyleAsset, (), (styles))

BROOKESIA_DESCRIBE_STRUCT(SparseResolvedStyle, (), (style, resolved_font, state_styles, part_styles))

BROOKESIA_DESCRIBE_STRUCT(
    SparseNode, (),
    (type, mount_mode, id, common_props, label_props, image_props, frame_view_props, text_input_props, range_props,
     toggle_props, dropdown_props, table_props, line_props, keyboard_props, canvas_props, layout, placement, style,
     state_styles, part_styles, style_refs, resolved_image, events, animations, bindings, children)
)

BROOKESIA_DESCRIBE_STRUCT(
    SparseDocument, (),
    (version, environment_dependencies, theme_sensitive, fonts, images, themes, styles, screen_flows, screens, templates)
)

} // namespace esp_brookesia::gui
