/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/sparse_document.hpp"

namespace esp_brookesia::gui {

SparseStyle to_sparse(const Style &source)
{
    SparseStyle result;
    result.set_bg_color(source.bg_color);
    result.set_bg_gradient_color(source.bg_gradient_color);
    result.set_bg_gradient_direction(source.bg_gradient_direction);
    result.set_text_color(source.text_color);
    result.set_border_color(source.border_color);
    result.set_line_color(source.line_color);
    result.set_arc_color(source.arc_color);
    result.set_arc_gradient_color(source.arc_gradient_color);
    result.set_font(source.font);
    result.set_bg_main_stop(source.bg_main_stop);
    result.set_bg_gradient_stop(source.bg_gradient_stop);
    result.set_bg_gradient_opacity(source.bg_gradient_opacity);
    result.set_border_width(source.border_width);
    result.set_radius(source.radius);
    result.set_padding(source.padding);
    result.set_padding_left(source.padding_left);
    result.set_padding_right(source.padding_right);
    result.set_padding_top(source.padding_top);
    result.set_padding_bottom(source.padding_bottom);
    result.set_margin(source.margin);
    result.set_margin_left(source.margin_left);
    result.set_margin_right(source.margin_right);
    result.set_margin_top(source.margin_top);
    result.set_margin_bottom(source.margin_bottom);
    result.set_shadow_width(source.shadow_width);
    result.set_shadow_offset_x(source.shadow_offset_x);
    result.set_shadow_offset_y(source.shadow_offset_y);
    result.set_shadow_color(source.shadow_color);
    result.set_opacity(source.opacity);
    result.set_line_width(source.line_width);
    result.set_image_opacity(source.image_opacity);
    result.set_image_recolor(source.image_recolor);
    result.set_image_recolor_opacity(source.image_recolor_opacity);
    result.set_font_size(source.font_size);
    result.set_image_font_size(source.image_font_size);
    result.set_text_align(source.text_align);
    result.set_arc_width(source.arc_width);
    result.set_arc_opacity(source.arc_opacity);
    result.set_arc_gradient_segments(source.arc_gradient_segments);
    result.set_arc_rounded(source.arc_rounded);
    result.set_clip_corner(source.clip_corner);
    return result;
}

Style to_public(const SparseStyle &source)
{
    Style result;
    result.bg_color = source.bg_color();
    result.bg_gradient_color = source.bg_gradient_color();
    result.bg_gradient_direction = source.bg_gradient_direction();
    result.text_color = source.text_color();
    result.border_color = source.border_color();
    result.line_color = source.line_color();
    result.arc_color = source.arc_color();
    result.arc_gradient_color = source.arc_gradient_color();
    result.font = source.font();
    result.bg_main_stop = source.bg_main_stop();
    result.bg_gradient_stop = source.bg_gradient_stop();
    result.bg_gradient_opacity = source.bg_gradient_opacity();
    result.border_width = source.border_width();
    result.radius = source.radius();
    result.padding = source.padding();
    result.padding_left = source.padding_left();
    result.padding_right = source.padding_right();
    result.padding_top = source.padding_top();
    result.padding_bottom = source.padding_bottom();
    result.margin = source.margin();
    result.margin_left = source.margin_left();
    result.margin_right = source.margin_right();
    result.margin_top = source.margin_top();
    result.margin_bottom = source.margin_bottom();
    result.shadow_width = source.shadow_width();
    result.shadow_offset_x = source.shadow_offset_x();
    result.shadow_offset_y = source.shadow_offset_y();
    result.shadow_color = source.shadow_color();
    result.opacity = source.opacity();
    result.line_width = source.line_width();
    result.image_opacity = source.image_opacity();
    result.image_recolor = source.image_recolor();
    result.image_recolor_opacity = source.image_recolor_opacity();
    result.font_size = source.font_size();
    result.image_font_size = source.image_font_size();
    result.text_align = source.text_align();
    result.arc_width = source.arc_width();
    result.arc_opacity = source.arc_opacity();
    result.arc_gradient_segments = source.arc_gradient_segments();
    result.arc_rounded = source.arc_rounded();
    result.clip_corner = source.clip_corner();
    return result;
}

namespace {
template <typename T>
T convert_sparse(const T &value)
{
    return value;
}
template <typename T>
T convert_public(const T &value)
{
    return value;
}

SparseStyle convert_sparse(const Style &value)
{
    return to_sparse(value);
}
Style convert_public(const SparseStyle &value)
{
    return to_public(value);
}

SparsePartStyleSet convert_sparse(const PartStyleSet &value)
{
    return to_sparse(value);
}
PartStyleSet convert_public(const SparsePartStyleSet &value)
{
    return to_public(value);
}

SparseStyleSet convert_sparse(const StyleSet &value)
{
    return to_sparse(value);
}
StyleSet convert_public(const SparseStyleSet &value)
{
    return to_public(value);
}

SparseThemeAsset convert_sparse(const ThemeAsset &value)
{
    return to_sparse(value);
}
ThemeAsset convert_public(const SparseThemeAsset &value)
{
    return to_public(value);
}



SparseNode convert_sparse(const Node &value)
{
    return to_sparse(value);
}
Node convert_public(const SparseNode &value)
{
    return to_public(value);
}


template <typename T>
auto convert_sparse(const std::map<std::string, T> &source)
{
    using Value = decltype(convert_sparse(std::declval<const T &>()));
    std::map<std::string, Value> result;
    for (const auto &[key, value] : source) {
        result.emplace(key, convert_sparse(value));
    }
    return result;
}
template <typename T>
auto convert_sparse(const std::vector<T> &source)
{
    using Value = decltype(convert_sparse(std::declval<const T &>()));
    std::vector<Value> result;
    result.reserve(source.size());
    for (const auto &value : source) {
        result.push_back(convert_sparse(value));
    }
    return result;
}

template <typename T>
auto convert_public(const std::map<std::string, T> &source)
{
    using Value = decltype(convert_public(std::declval<const T &>()));
    std::map<std::string, Value> result;
    for (const auto &[key, value] : source) {
        result.emplace(key, convert_public(value));
    }
    return result;
}
template <typename T>
auto convert_public(const std::vector<T> &source)
{
    using Value = decltype(convert_public(std::declval<const T &>()));
    std::vector<Value> result;
    result.reserve(source.size());
    for (const auto &value : source) {
        result.push_back(convert_public(value));
    }
    return result;
}

} // namespace

SparsePartStyleSet to_sparse(const PartStyleSet &source)
{
    return SparsePartStyleSet{
        .style = convert_sparse(source.style),
        .state_styles = convert_sparse(source.state_styles),
    };
}

PartStyleSet to_public(const SparsePartStyleSet &source)
{
    return PartStyleSet{
        .style = convert_public(source.style),
        .state_styles = convert_public(source.state_styles),
    };
}

SparseStyleSet to_sparse(const StyleSet &source)
{
    return SparseStyleSet{
        .style = convert_sparse(source.style),
        .state_styles = convert_sparse(source.state_styles),
        .part_styles = convert_sparse(source.part_styles),
    };
}

StyleSet to_public(const SparseStyleSet &source)
{
    return StyleSet{
        .style = convert_public(source.style),
        .state_styles = convert_public(source.state_styles),
        .part_styles = convert_public(source.part_styles),
    };
}

SparseThemeAsset to_sparse(const ThemeAsset &source)
{
    return SparseThemeAsset{
        .id = convert_sparse(source.id),
        .colors = convert_sparse(source.colors),
        .styles = convert_sparse(source.styles),
    };
}

ThemeAsset to_public(const SparseThemeAsset &source)
{
    return ThemeAsset{
        .id = convert_public(source.id),
        .colors = convert_public(source.colors),
        .styles = convert_public(source.styles),
    };
}

SparseStyleAsset to_sparse(const StyleAsset &source)
{
    return SparseStyleAsset{
        .styles = convert_sparse(source.styles),
    };
}

StyleAsset to_public(const SparseStyleAsset &source)
{
    return StyleAsset{
        .styles = convert_public(source.styles),
    };
}

SparseResolvedStyle to_sparse(const ResolvedStyle &source)
{
    return SparseResolvedStyle{
        .style = convert_sparse(source.style),
        .resolved_font = convert_sparse(source.resolved_font),
        .state_styles = convert_sparse(source.state_styles),
        .part_styles = convert_sparse(source.part_styles),
    };
}

ResolvedStyle to_public(const SparseResolvedStyle &source)
{
    return ResolvedStyle{
        .style = convert_public(source.style),
        .resolved_font = convert_public(source.resolved_font),
        .state_styles = convert_public(source.state_styles),
        .part_styles = convert_public(source.part_styles),
    };
}

SparseNode to_sparse(const Node &source)
{
    return SparseNode{
        .type = convert_sparse(source.type),
        .mount_mode = convert_sparse(source.mount_mode),
        .id = convert_sparse(source.id),
        .common_props = convert_sparse(source.common_props),
        .label_props = convert_sparse(source.label_props),
        .image_props = convert_sparse(source.image_props),
        .frame_view_props = convert_sparse(source.frame_view_props),
        .text_input_props = convert_sparse(source.text_input_props),
        .range_props = convert_sparse(source.range_props),
        .toggle_props = convert_sparse(source.toggle_props),
        .dropdown_props = convert_sparse(source.dropdown_props),
        .table_props = convert_sparse(source.table_props),
        .line_props = convert_sparse(source.line_props),
        .keyboard_props = convert_sparse(source.keyboard_props),
        .canvas_props = convert_sparse(source.canvas_props),
        .layout = convert_sparse(source.layout),
        .placement = convert_sparse(source.placement),
        .style = convert_sparse(source.style),
        .state_styles = convert_sparse(source.state_styles),
        .part_styles = convert_sparse(source.part_styles),
        .style_refs = convert_sparse(source.style_refs),
        .resolved_image = convert_sparse(source.resolved_image),
        .events = convert_sparse(source.events),
        .animations = convert_sparse(source.animations),
        .bindings = convert_sparse(source.bindings),
        .children = convert_sparse(source.children),
    };
}

Node to_public(const SparseNode &source)
{
    return Node{
        .type = convert_public(source.type),
        .mount_mode = convert_public(source.mount_mode),
        .id = convert_public(source.id),
        .common_props = convert_public(source.common_props),
        .label_props = convert_public(source.label_props),
        .image_props = convert_public(source.image_props),
        .frame_view_props = convert_public(source.frame_view_props),
        .text_input_props = convert_public(source.text_input_props),
        .range_props = convert_public(source.range_props),
        .toggle_props = convert_public(source.toggle_props),
        .dropdown_props = convert_public(source.dropdown_props),
        .table_props = convert_public(source.table_props),
        .line_props = convert_public(source.line_props),
        .keyboard_props = convert_public(source.keyboard_props),
        .canvas_props = convert_public(source.canvas_props),
        .layout = convert_public(source.layout),
        .placement = convert_public(source.placement),
        .style = convert_public(source.style),
        .state_styles = convert_public(source.state_styles),
        .part_styles = convert_public(source.part_styles),
        .style_refs = convert_public(source.style_refs),
        .resolved_image = convert_public(source.resolved_image),
        .events = convert_public(source.events),
        .animations = convert_public(source.animations),
        .bindings = convert_public(source.bindings),
        .children = convert_public(source.children),
    };
}

SparseDocument to_sparse(const Document &source)
{
    return SparseDocument{
        .version = convert_sparse(source.version),
        .environment_dependencies = convert_sparse(source.environment_dependencies),
        .theme_sensitive = convert_sparse(source.theme_sensitive),
        .constants = convert_sparse(source.constants),
        .fonts = convert_sparse(source.fonts),
        .images = convert_sparse(source.images),
        .themes = convert_sparse(source.themes),
        .styles = convert_sparse(source.styles),
        .screen_flows = convert_sparse(source.screen_flows),
        .screens = convert_sparse(source.screens),
        .templates = convert_sparse(source.templates),
    };
}

Document to_public(const SparseDocument &source)
{
    return Document{
        .version = convert_public(source.version),
        .environment_dependencies = convert_public(source.environment_dependencies),
        .theme_sensitive = convert_public(source.theme_sensitive),
        .constants = convert_public(source.constants),
        .fonts = convert_public(source.fonts),
        .images = convert_public(source.images),
        .themes = convert_public(source.themes),
        .styles = convert_public(source.styles),
        .screen_flows = convert_public(source.screen_flows),
        .screens = convert_public(source.screens),
        .templates = convert_public(source.templates),
    };
}

Node to_public_node_snapshot(const SparseNode &source)
{
    return Node{
        .type = convert_public(source.type),
        .mount_mode = convert_public(source.mount_mode),
        .id = convert_public(source.id),
        .common_props = convert_public(source.common_props),
        .label_props = convert_public(source.label_props),
        .image_props = convert_public(source.image_props),
        .frame_view_props = convert_public(source.frame_view_props),
        .text_input_props = convert_public(source.text_input_props),
        .range_props = convert_public(source.range_props),
        .toggle_props = convert_public(source.toggle_props),
        .dropdown_props = convert_public(source.dropdown_props),
        .table_props = convert_public(source.table_props),
        .line_props = convert_public(source.line_props),
        .keyboard_props = convert_public(source.keyboard_props),
        .canvas_props = convert_public(source.canvas_props),
        .layout = convert_public(source.layout),
        .placement = convert_public(source.placement),
        .style = convert_public(source.style),
        .state_styles = convert_public(source.state_styles),
        .part_styles = convert_public(source.part_styles),
        .style_refs = convert_public(source.style_refs),
        .resolved_image = convert_public(source.resolved_image),
        .events = convert_public(source.events),
        .animations = convert_public(source.animations),
        .bindings = convert_public(source.bindings),
        .children = {},
    };
}

std::shared_ptr<const SparseResolvedStyle> intern_sparse_resolved_style(
    std::vector<std::weak_ptr<const SparseResolvedStyle>> &pool,
    SparseResolvedStyle style,
    bool *reused)
{
    if (reused != nullptr) {
        *reused = false;
    }
    for (auto it = pool.begin(); it != pool.end();) {
        auto candidate = it->lock();
        if (candidate == nullptr) {
            it = pool.erase(it);
            continue;
        }
        if (*candidate == style) {
            if (reused != nullptr) {
                *reused = true;
            }
            return candidate;
        }
        ++it;
    }
    auto result = std::make_shared<const SparseResolvedStyle>(std::move(style));
    pool.push_back(result);
    return result;
}

} // namespace esp_brookesia::gui
