/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/style_impl.hpp"

namespace esp_brookesia::gui::lvgl::style_detail {



void apply_style_colors(Record &record, const ResolvedStyle &style)
{
    lv_style_set_bg_opa(&record.style, LV_OPA_TRANSP);
    auto bg_color = parse_color(style.style.bg_color.value_or(""));
    if (bg_color.has_value()) {
        lv_style_set_bg_color(&record.style, lv_color_hex(*bg_color));
        lv_style_set_bg_opa(&record.style, LV_OPA_COVER);
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_COLOR);
    }

    auto bg_gradient_color = parse_color(style.style.bg_gradient_color.value_or(""));
    if (bg_gradient_color.has_value()) {
        lv_style_set_bg_grad_color(&record.style, lv_color_hex(*bg_gradient_color));
        lv_style_set_bg_grad_opa(
            &record.style,
            static_cast<lv_opa_t>(style.style.bg_gradient_opacity.value_or(255))
        );
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_COLOR);
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_OPA);
    }
    if (style.style.bg_gradient_direction.has_value()) {
        auto direction = parse_gradient_direction(*style.style.bg_gradient_direction);
        if (direction.has_value()) {
            lv_style_set_bg_grad_dir(&record.style, *direction);
        }
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_DIR);
    }
    if (style.style.bg_main_stop.has_value()) {
        lv_style_set_bg_main_stop(&record.style, *style.style.bg_main_stop);
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_MAIN_STOP);
    }
    if (style.style.bg_gradient_stop.has_value()) {
        lv_style_set_bg_grad_stop(&record.style, *style.style.bg_gradient_stop);
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BG_GRAD_STOP);
    }

    auto text_color = parse_color(style.style.text_color.value_or(""));
    if (text_color.has_value()) {
        lv_style_set_text_color(&record.style, lv_color_hex(*text_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_TEXT_COLOR);
    }

    auto border_color = parse_color(style.style.border_color.value_or(""));
    if (border_color.has_value()) {
        lv_style_set_border_color(&record.style, lv_color_hex(*border_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_BORDER_COLOR);
    }

    auto line_color = parse_color(style.style.line_color.value_or(""));
    if (line_color.has_value()) {
        lv_style_set_line_color(&record.style, lv_color_hex(*line_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_LINE_COLOR);
    }

    auto arc_color = parse_color(style.style.arc_color.value_or(""));
    if (arc_color.has_value()) {
        lv_style_set_arc_color(&record.style, lv_color_hex(*arc_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_ARC_COLOR);
    }

    auto shadow_color = parse_color(style.style.shadow_color.value_or(""));
    if (shadow_color.has_value()) {
        lv_style_set_shadow_color(&record.style, lv_color_hex(*shadow_color));
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_SHADOW_COLOR);
    }
}

void apply_style_border(Record &record, const ResolvedStyle &style)
{
    lv_style_set_border_width(&record.style, style.style.border_width.value_or(0));
}

void apply_style_radius(Record &record, const ResolvedStyle &style)
{
    lv_style_set_radius(&record.style, style.style.radius.value_or(0));
    lv_style_set_clip_corner(&record.style, style.style.clip_corner.value_or(false));
}

void apply_style_padding(Record &record, const ResolvedStyle &style)
{
    const auto padding_default = style.style.padding.value_or(0);
    lv_style_set_pad_left(&record.style, style.style.padding_left.value_or(padding_default));
    lv_style_set_pad_right(&record.style, style.style.padding_right.value_or(padding_default));
    lv_style_set_pad_top(&record.style, style.style.padding_top.value_or(padding_default));
    lv_style_set_pad_bottom(&record.style, style.style.padding_bottom.value_or(padding_default));
}

void apply_style_margin(Record &record, const ResolvedStyle &style)
{
    const auto margin_default = style.style.margin.value_or(0);
    lv_style_set_margin_left(&record.style, style.style.margin_left.value_or(margin_default));
    lv_style_set_margin_right(&record.style, style.style.margin_right.value_or(margin_default));
    lv_style_set_margin_top(&record.style, style.style.margin_top.value_or(margin_default));
    lv_style_set_margin_bottom(&record.style, style.style.margin_bottom.value_or(margin_default));
}

void apply_style_shadow(Record &record, const ResolvedStyle &style)
{
    lv_style_set_shadow_width(&record.style, style.style.shadow_width.value_or(0));
    lv_style_set_shadow_offset_x(&record.style, style.style.shadow_offset_x.value_or(0));
    lv_style_set_shadow_offset_y(&record.style, style.style.shadow_offset_y.value_or(0));
}

void apply_style_opacity(Record &record, const ResolvedStyle &style)
{
    lv_style_set_opa(&record.style, static_cast<lv_opa_t>(style.style.opacity.value_or(255)));
}

void apply_style_line(Record &record, const ResolvedStyle &style)
{
    lv_style_set_line_width(&record.style, style.style.line_width.value_or(0));
    if (style.style.arc_width.has_value()) {
        lv_style_set_arc_width(&record.style, *style.style.arc_width);
    }
    if (style.style.arc_opacity.has_value()) {
        lv_style_set_arc_opa(&record.style, static_cast<lv_opa_t>(*style.style.arc_opacity));
    }
    if (style.style.arc_rounded.has_value()) {
        lv_style_set_arc_rounded(&record.style, *style.style.arc_rounded);
    }
}

void apply_style_image_opacity(Record &record, const ResolvedStyle &style)
{
    lv_style_set_image_opa(&record.style, static_cast<lv_opa_t>(style.style.image_opacity.value_or(255)));
}

void apply_style_image_recolor(Record &record, const ResolvedStyle &style)
{
    const auto recolor = parse_color(style.style.image_recolor.value_or(""));
    if (recolor.has_value()) {
        lv_style_set_image_recolor(&record.style, lv_color_hex(*recolor));
        lv_style_set_image_recolor_opa(
            &record.style,
            static_cast<lv_opa_t>(style.style.image_recolor_opacity.value_or(255))
        );
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_IMAGE_RECOLOR);
        lv_style_remove_prop(&record.style, LV_STYLE_IMAGE_RECOLOR_OPA);
    }
}

void apply_style_font(BackendImpl &impl, Record &record, const ResolvedStyle &style)
{
    if (style.style.text_align.has_value()) {
        switch (*style.style.text_align) {
        case TextAlign::Auto:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_AUTO);
            break;
        case TextAlign::Left:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_LEFT);
            break;
        case TextAlign::Center:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_CENTER);
            break;
        case TextAlign::Right:
            lv_style_set_text_align(&record.style, LV_TEXT_ALIGN_RIGHT);
            break;
        case TextAlign::Max:
            break;
        }
    } else {
        lv_style_remove_prop(&record.style, LV_STYLE_TEXT_ALIGN);
    }

    if (node_type_uses_text_font(record.type) || style.style.font_size.has_value() ||
            style.style.image_font_size.has_value() ||
            (style.style.font.has_value() && !style.style.font->empty()) ||
            !style.resolved_font.primary_src.empty() || style.resolved_font.kind == "imageFont") {
        auto *font = get_font(impl, record, style);
        lv_style_set_text_font(&record.style, font);
        BROOKESIA_LOGD(
            "Applied text font: node='%1%', requested_font_id='%2%', resolved_font_id='%3%', primary_src='%4%', "
            "font_size=%5%, cache_key='%6%', lv_font=%7%",
            impl.build_absolute_path(record),
            style.style.font.value_or(""),
            style.resolved_font.font_id,
            style.resolved_font.primary_src,
            style.style.font_size.value_or(16),
            record.font_cache_entry != nullptr ? record.font_cache_entry->cache_key.c_str() : "",
            static_cast<const void *>(font)
        );
    } else {
        release_font(impl, record);
    }
}

void apply_optional_color(lv_style_t &lv_style, const std::optional<std::string> &color, lv_style_prop_t prop)
{
    if (!color.has_value()) {
        return;
    }
    auto parsed = parse_color(*color);
    if (!parsed.has_value()) {
        lv_style_remove_prop(&lv_style, prop);
        return;
    }
    const auto lv_color = lv_color_hex(*parsed);
    switch (prop) {
    case LV_STYLE_BG_COLOR:
        lv_style_set_bg_color(&lv_style, lv_color);
        lv_style_set_bg_opa(&lv_style, LV_OPA_COVER);
        break;
    case LV_STYLE_BG_GRAD_COLOR:
        lv_style_set_bg_grad_color(&lv_style, lv_color);
        break;
    case LV_STYLE_TEXT_COLOR:
        lv_style_set_text_color(&lv_style, lv_color);
        break;
    case LV_STYLE_BORDER_COLOR:
        lv_style_set_border_color(&lv_style, lv_color);
        break;
    case LV_STYLE_LINE_COLOR:
        lv_style_set_line_color(&lv_style, lv_color);
        break;
    case LV_STYLE_ARC_COLOR:
        lv_style_set_arc_color(&lv_style, lv_color);
        break;
    case LV_STYLE_SHADOW_COLOR:
        lv_style_set_shadow_color(&lv_style, lv_color);
        break;
    default:
        break;
    }
}

std::optional<lv_grad_dir_t> parse_gradient_direction(std::string_view direction)
{
    if (direction == "none") {
        return LV_GRAD_DIR_NONE;
    }
    if (direction == "horizontal") {
        return LV_GRAD_DIR_HOR;
    }
    if (direction == "vertical") {
        return LV_GRAD_DIR_VER;
    }
    return std::nullopt;
}

void apply_state_style_fields(lv_style_t &lv_style, const Style &style)
{
    apply_optional_color(lv_style, style.bg_color, LV_STYLE_BG_COLOR);
    apply_optional_color(lv_style, style.bg_gradient_color, LV_STYLE_BG_GRAD_COLOR);
    apply_optional_color(lv_style, style.text_color, LV_STYLE_TEXT_COLOR);
    apply_optional_color(lv_style, style.border_color, LV_STYLE_BORDER_COLOR);
    apply_optional_color(lv_style, style.line_color, LV_STYLE_LINE_COLOR);
    apply_optional_color(lv_style, style.arc_color, LV_STYLE_ARC_COLOR);
    apply_optional_color(lv_style, style.shadow_color, LV_STYLE_SHADOW_COLOR);

    if (style.bg_gradient_direction.has_value()) {
        auto direction = parse_gradient_direction(*style.bg_gradient_direction);
        if (direction.has_value()) {
            lv_style_set_bg_grad_dir(&lv_style, *direction);
        }
    }
    if (style.bg_main_stop.has_value()) {
        lv_style_set_bg_main_stop(&lv_style, *style.bg_main_stop);
    }
    if (style.bg_gradient_stop.has_value()) {
        lv_style_set_bg_grad_stop(&lv_style, *style.bg_gradient_stop);
    }
    if (style.bg_gradient_opacity.has_value()) {
        lv_style_set_bg_grad_opa(&lv_style, static_cast<lv_opa_t>(*style.bg_gradient_opacity));
    }

    if (style.border_width.has_value()) {
        lv_style_set_border_width(&lv_style, *style.border_width);
    }
    if (style.radius.has_value()) {
        lv_style_set_radius(&lv_style, *style.radius);
    }
    if (style.padding.has_value()) {
        lv_style_set_pad_all(&lv_style, *style.padding);
    }
    if (style.padding_left.has_value()) {
        lv_style_set_pad_left(&lv_style, *style.padding_left);
    }
    if (style.padding_right.has_value()) {
        lv_style_set_pad_right(&lv_style, *style.padding_right);
    }
    if (style.padding_top.has_value()) {
        lv_style_set_pad_top(&lv_style, *style.padding_top);
    }
    if (style.padding_bottom.has_value()) {
        lv_style_set_pad_bottom(&lv_style, *style.padding_bottom);
    }
    if (style.margin.has_value()) {
        lv_style_set_margin_all(&lv_style, *style.margin);
    }
    if (style.margin_left.has_value()) {
        lv_style_set_margin_left(&lv_style, *style.margin_left);
    }
    if (style.margin_right.has_value()) {
        lv_style_set_margin_right(&lv_style, *style.margin_right);
    }
    if (style.margin_top.has_value()) {
        lv_style_set_margin_top(&lv_style, *style.margin_top);
    }
    if (style.margin_bottom.has_value()) {
        lv_style_set_margin_bottom(&lv_style, *style.margin_bottom);
    }
    if (style.shadow_width.has_value()) {
        lv_style_set_shadow_width(&lv_style, *style.shadow_width);
    }
    if (style.shadow_offset_x.has_value()) {
        lv_style_set_shadow_offset_x(&lv_style, *style.shadow_offset_x);
    }
    if (style.shadow_offset_y.has_value()) {
        lv_style_set_shadow_offset_y(&lv_style, *style.shadow_offset_y);
    }
    if (style.opacity.has_value()) {
        lv_style_set_opa(&lv_style, static_cast<lv_opa_t>(*style.opacity));
    }
    if (style.line_width.has_value()) {
        lv_style_set_line_width(&lv_style, *style.line_width);
    }
    if (style.image_opacity.has_value()) {
        lv_style_set_image_opa(&lv_style, static_cast<lv_opa_t>(*style.image_opacity));
    }
    if (style.image_recolor.has_value()) {
        auto recolor = parse_color(*style.image_recolor);
        if (recolor.has_value()) {
            lv_style_set_image_recolor(&lv_style, lv_color_hex(*recolor));
            lv_style_set_image_recolor_opa(
                &lv_style,
                static_cast<lv_opa_t>(style.image_recolor_opacity.value_or(255))
            );
        }
    }
    if (style.text_align.has_value()) {
        switch (*style.text_align) {
        case TextAlign::Auto:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_AUTO);
            break;
        case TextAlign::Left:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_LEFT);
            break;
        case TextAlign::Center:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_CENTER);
            break;
        case TextAlign::Right:
            lv_style_set_text_align(&lv_style, LV_TEXT_ALIGN_RIGHT);
            break;
        case TextAlign::Max:
            break;
        }
    }
    if (style.arc_width.has_value()) {
        lv_style_set_arc_width(&lv_style, *style.arc_width);
    }
    if (style.arc_opacity.has_value()) {
        lv_style_set_arc_opa(&lv_style, static_cast<lv_opa_t>(*style.arc_opacity));
    }
    if (style.arc_rounded.has_value()) {
        lv_style_set_arc_rounded(&lv_style, *style.arc_rounded);
    }
    if (style.clip_corner.has_value()) {
        lv_style_set_clip_corner(&lv_style, *style.clip_corner);
    }
}
}
