/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/runtime_impl.hpp"

namespace esp_brookesia::gui {

std::expected<void, std::string> Runtime::Impl::apply_binding_value(
    TreeRecord &tree,
    NodeRecord &record,
    const BindingTargetInfo &target_info,
    std::string_view value)
{
    const auto target = target_info.target;
    switch (target) {
    case BindingTarget::CommonPropsHidden: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().hidden = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsDisabled: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().disabled = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsClickable: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().clickable = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsScrollable: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().scrollable = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsPressLock: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().press_lock = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsAngle: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().angle = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsZoom: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().zoom = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsPivotX: {
        auto parsed = parse_pivot_from_store_string(value, "commonProps.pivotX");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().pivot_x = *parsed;
        return {};
    }
    case BindingTarget::CommonPropsPivotY: {
        auto parsed = parse_pivot_from_store_string(value, "commonProps.pivotY");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_common_props().pivot_y = *parsed;
        return {};
    }
    case BindingTarget::LabelPropsText:
        record.mutable_label_props().text = std::string(value);
        return {};
    case BindingTarget::ImagePropsSrc:
        if (!value.empty() && !has_image_resource(tree, std::string(value))) {
            return std::unexpected("unknown image resource id: " + std::string(value));
        }
        if (record.handle.is_valid()) {
            return update_image_source(tree, record, value);
        }
        record.mutable_image_props().src = std::string(value);
        return {};
    case BindingTarget::ImagePropsRecolor:
        if (!is_valid_color(value)) {
            return std::unexpected("expected empty string or color in '#RRGGBB' format");
        }
        record.mutable_image_props().recolor = std::string(value);
        return {};
    case BindingTarget::ImagePropsRecolorOpacity: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (*parsed < 0 || *parsed > 255) {
            return std::unexpected("expected integer in range 0..255");
        }
        record.mutable_image_props().recolor_opacity = *parsed;
        return {};
    }
    case BindingTarget::ImagePropsAngle: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_image_props().angle = *parsed;
        return {};
    }
    case BindingTarget::ImagePropsOffsetX: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_image_props().offset_x = *parsed;
        return {};
    }
    case BindingTarget::ImagePropsOffsetY: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_image_props().offset_y = *parsed;
        return {};
    }
    case BindingTarget::ImagePropsZoom: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_image_props().zoom = *parsed;
        return {};
    }
    case BindingTarget::ImagePropsPivotX: {
        auto parsed = parse_pivot_from_store_string(value, "imageProps.pivotX");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_image_props().pivot_x = *parsed;
        return {};
    }
    case BindingTarget::ImagePropsPivotY: {
        auto parsed = parse_pivot_from_store_string(value, "imageProps.pivotY");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_image_props().pivot_y = *parsed;
        return {};
    }
    default:
        break;
    }

    Style *target_style = nullptr;
    const bool stores_base_style_override = target_info.domain == BindingApplyDomain::Style &&
                                            target_info.style_part.empty() &&
                                            target_info.style_state.empty() &&
                                            !record.transient_mutable_definition;
    if (target_info.domain == BindingApplyDomain::Style) {
        if (target_info.style_part.empty()) {
            if (target_info.style_state.empty()) {
                if (record.transient_mutable_definition) {
                    target_style = &const_cast<Node *>(record.definition)->style;
                }
            } else {
                target_style = &record.mutable_state_styles()[target_info.style_state];
            }
        } else {
            auto &part_style = record.mutable_part_styles()[target_info.style_part];
            target_style = target_info.style_state.empty() ?
                           &part_style.style :
                           &part_style.state_styles[target_info.style_state];
        }
    }
    auto set_style_string = [&](void (Style::*setter)(std::optional<std::string>), std::string field_value) {
        if (stores_base_style_override) {
            record.set_style_override(target, std::move(field_value));
        } else {
            (target_style->*setter)(std::move(field_value));
        }
    };
    auto set_style_number = [&](void (Style::*setter)(std::optional<int32_t>), int32_t field_value) {
        if (stores_base_style_override) {
            record.set_style_override(target, field_value);
        } else {
            (target_style->*setter)(field_value);
        }
    };
    auto set_style_bool = [&](void (Style::*setter)(std::optional<bool>), bool field_value) {
        if (stores_base_style_override) {
            record.set_style_override(target, static_cast<int32_t>(field_value));
        } else {
            (target_style->*setter)(field_value);
        }
    };
    switch (target) {
    case BindingTarget::FrameViewPropsAutoRegisterOutput: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_frame_view_props().auto_register_output = *parsed;
        break;
    }
    case BindingTarget::FrameViewPropsOutputName:
        record.mutable_frame_view_props().output_name = std::string(value);
        break;
    case BindingTarget::FrameViewPropsColorFormat: {
        FrameColorFormat parsed = FrameColorFormat::Max;
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed) ||
                parsed == FrameColorFormat::Max) {
            return std::unexpected("unsupported frameViewProps.colorFormat: " + std::string(value));
        }
        record.mutable_frame_view_props().color_format = parsed;
        break;
    }
    case BindingTarget::TextInputPropsText:
        record.mutable_text_input_props().text = std::string(value);
        break;
    case BindingTarget::TextInputPropsPlaceholder:
        record.mutable_text_input_props().placeholder = std::string(value);
        break;
    case BindingTarget::TextInputPropsPassword: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_text_input_props().password = *parsed;
        break;
    }
    case BindingTarget::TextInputPropsMultiline: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_text_input_props().multiline = *parsed;
        break;
    }
    case BindingTarget::TextInputPropsMaxLength: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_text_input_props().max_length = *parsed;
        break;
    }
    case BindingTarget::RangePropsValue:
    case BindingTarget::RangePropsMin:
    case BindingTarget::RangePropsMax:
    case BindingTarget::RangePropsStep: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::RangePropsValue) {
            record.mutable_range_props().value = *parsed;
        } else if (target == BindingTarget::RangePropsMin) {
            record.mutable_range_props().min = *parsed;
        } else if (target == BindingTarget::RangePropsMax) {
            record.mutable_range_props().max = *parsed;
        } else {
            record.mutable_range_props().step = *parsed;
        }
        break;
    }
    case BindingTarget::TogglePropsChecked: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_toggle_props().checked = *parsed;
        break;
    }
    case BindingTarget::DropdownPropsOptions: {
        auto parsed = parse_string_array_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_dropdown_props().options = std::move(*parsed);
        break;
    }
    case BindingTarget::DropdownPropsSelectedIndex: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_dropdown_props().selected_index = *parsed;
        break;
    }
    case BindingTarget::TablePropsRows:
    case BindingTarget::TablePropsColumns: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::TablePropsRows) {
            record.mutable_table_props().rows = *parsed;
        } else {
            record.mutable_table_props().columns = *parsed;
        }
        break;
    }
    case BindingTarget::TablePropsCells: {
        auto parsed = parse_table_cells_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_table_props().cells = std::move(*parsed);
        break;
    }
    case BindingTarget::LinePropsPoints: {
        auto parsed = parse_points_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_line_props().points = std::move(*parsed);
        break;
    }
    case BindingTarget::KeyboardPropsMode:
        record.mutable_keyboard_props().mode = std::string(value);
        break;
    case BindingTarget::KeyboardPropsPopovers: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_keyboard_props().popovers = *parsed;
        break;
    }
    case BindingTarget::KeyboardPropsAllowedModes: {
        auto parsed = parse_keyboard_modes_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_keyboard_props().allowed_modes = std::move(*parsed);
        break;
    }
    case BindingTarget::KeyboardPropsIconSize: {
        auto parsed = parse_dimension_from_store_string(value, tree.environment);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (parsed->mode != SizeMode::Fixed || parsed->value < 0) {
            return std::unexpected("keyboardProps.iconSize must be a fixed dimension >= 0");
        }
        record.mutable_keyboard_props().icon_size = *parsed;
        break;
    }
    case BindingTarget::CanvasPropsCommands: {
        auto parsed = parse_canvas_commands_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_canvas_props().commands = std::move(*parsed);
        break;
    }
    case BindingTarget::StyleBgColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_bg_color, *color);
        break;
    }
    case BindingTarget::StyleBgGradientColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_bg_gradient_color, *color);
        break;
    }
    case BindingTarget::StyleBgGradientDirection:
        if (value != "none" && value != "horizontal" && value != "vertical") {
            return std::unexpected("invalid style.bgGradientDirection");
        }
        set_style_string(&Style::set_bg_gradient_direction, std::string(value));
        break;
    case BindingTarget::StyleTextColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_text_color, *color);
        break;
    }
    case BindingTarget::StyleBorderColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_border_color, *color);
        break;
    }
    case BindingTarget::StyleLineColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_line_color, *color);
        break;
    }
    case BindingTarget::StyleArcColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_arc_color, *color);
        break;
    }
    case BindingTarget::StyleArcGradientColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_arc_gradient_color, *color);
        break;
    }
    case BindingTarget::StyleImageRecolor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_image_recolor, *color);
        break;
    }
    case BindingTarget::StyleFont:
        if (!target_info.style_state.empty() || !target_info.style_part.empty()) {
            return std::unexpected("stateStyles and partStyles do not support font");
        }
        if (!has_font_resource(tree, std::string(value))) {
            return std::unexpected("unknown font resource id: " + std::string(value));
        }
        set_style_string(&Style::set_font, std::string(value));
        break;
    case BindingTarget::StyleShadowColor: {
        auto color = resolve_color_binding_value(tree, value);
        if (!color) {
            return std::unexpected(color.error());
        }
        set_style_string(&Style::set_shadow_color, *color);
        break;
    }
    case BindingTarget::StyleBgMainStop:
    case BindingTarget::StyleBgGradientStop:
    case BindingTarget::StyleBgGradientOpacity:
    case BindingTarget::StyleOpacity:
    case BindingTarget::StyleImageOpacity:
    case BindingTarget::StyleImageRecolorOpacity:
    case BindingTarget::StyleArcOpacity:
    case BindingTarget::StyleArcGradientSegments: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::StyleBgMainStop) {
            set_style_number(&Style::set_bg_main_stop, *parsed);
        } else if (target == BindingTarget::StyleBgGradientStop) {
            set_style_number(&Style::set_bg_gradient_stop, *parsed);
        } else if (target == BindingTarget::StyleBgGradientOpacity) {
            set_style_number(&Style::set_bg_gradient_opacity, *parsed);
        } else if (target == BindingTarget::StyleOpacity) {
            set_style_number(&Style::set_opacity, *parsed);
        } else if (target == BindingTarget::StyleImageOpacity) {
            set_style_number(&Style::set_image_opacity, *parsed);
        } else if (target == BindingTarget::StyleImageRecolorOpacity) {
            set_style_number(&Style::set_image_recolor_opacity, *parsed);
        } else if (target == BindingTarget::StyleArcOpacity) {
            set_style_number(&Style::set_arc_opacity, *parsed);
        } else {
            set_style_number(&Style::set_arc_gradient_segments, *parsed);
        }
        break;
    }
    case BindingTarget::StyleBorderWidth:
    case BindingTarget::StyleRadius:
    case BindingTarget::StylePadding:
    case BindingTarget::StylePaddingLeft:
    case BindingTarget::StylePaddingRight:
    case BindingTarget::StylePaddingTop:
    case BindingTarget::StylePaddingBottom:
    case BindingTarget::StyleMargin:
    case BindingTarget::StyleMarginLeft:
    case BindingTarget::StyleMarginRight:
    case BindingTarget::StyleMarginTop:
    case BindingTarget::StyleMarginBottom:
    case BindingTarget::StyleShadowWidth:
    case BindingTarget::StyleShadowOffsetX:
    case BindingTarget::StyleShadowOffsetY:
    case BindingTarget::StyleLineWidth:
    case BindingTarget::StyleArcWidth: {
        std::string_view field_name = "style";
        switch (target) {
        case BindingTarget::StyleBorderWidth: field_name = "style.borderWidth"; break;
        case BindingTarget::StyleRadius: field_name = "style.radius"; break;
        case BindingTarget::StylePadding: field_name = "style.padding"; break;
        case BindingTarget::StylePaddingLeft: field_name = "style.paddingLeft"; break;
        case BindingTarget::StylePaddingRight: field_name = "style.paddingRight"; break;
        case BindingTarget::StylePaddingTop: field_name = "style.paddingTop"; break;
        case BindingTarget::StylePaddingBottom: field_name = "style.paddingBottom"; break;
        case BindingTarget::StyleMargin: field_name = "style.margin"; break;
        case BindingTarget::StyleMarginLeft: field_name = "style.marginLeft"; break;
        case BindingTarget::StyleMarginRight: field_name = "style.marginRight"; break;
        case BindingTarget::StyleMarginTop: field_name = "style.marginTop"; break;
        case BindingTarget::StyleMarginBottom: field_name = "style.marginBottom"; break;
        case BindingTarget::StyleShadowWidth: field_name = "style.shadowWidth"; break;
        case BindingTarget::StyleShadowOffsetX: field_name = "style.shadowOffsetX"; break;
        case BindingTarget::StyleShadowOffsetY: field_name = "style.shadowOffsetY"; break;
        case BindingTarget::StyleLineWidth: field_name = "style.lineWidth"; break;
        case BindingTarget::StyleArcWidth: field_name = "style.arcWidth"; break;
        default: break;
        }
        auto parsed = parse_scaled_from_store_string(value, field_name, "dp", tree.environment.density);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        switch (target) {
        case BindingTarget::StyleBorderWidth:
            set_style_number(&Style::set_border_width, *parsed);
            break;
        case BindingTarget::StyleRadius: set_style_number(&Style::set_radius, *parsed); break;
        case BindingTarget::StylePadding:
            set_style_number(&Style::set_padding, *parsed);
            break;
        case BindingTarget::StylePaddingLeft:
            set_style_number(&Style::set_padding_left, *parsed);
            break;
        case BindingTarget::StylePaddingRight:
            set_style_number(&Style::set_padding_right, *parsed);
            break;
        case BindingTarget::StylePaddingTop:
            set_style_number(&Style::set_padding_top, *parsed);
            break;
        case BindingTarget::StylePaddingBottom:
            set_style_number(&Style::set_padding_bottom, *parsed);
            break;
        case BindingTarget::StyleMargin: set_style_number(&Style::set_margin, *parsed); break;
        case BindingTarget::StyleMarginLeft: set_style_number(&Style::set_margin_left, *parsed); break;
        case BindingTarget::StyleMarginRight: set_style_number(&Style::set_margin_right, *parsed); break;
        case BindingTarget::StyleMarginTop: set_style_number(&Style::set_margin_top, *parsed); break;
        case BindingTarget::StyleMarginBottom: set_style_number(&Style::set_margin_bottom, *parsed); break;
        case BindingTarget::StyleShadowWidth: set_style_number(&Style::set_shadow_width, *parsed); break;
        case BindingTarget::StyleShadowOffsetX: set_style_number(&Style::set_shadow_offset_x, *parsed); break;
        case BindingTarget::StyleShadowOffsetY: set_style_number(&Style::set_shadow_offset_y, *parsed); break;
        case BindingTarget::StyleLineWidth: set_style_number(&Style::set_line_width, *parsed); break;
        case BindingTarget::StyleArcWidth: set_style_number(&Style::set_arc_width, *parsed); break;
        default: break;
        }
        break;
    }
    case BindingTarget::StyleFontSize: {
        if (!target_info.style_state.empty() || !target_info.style_part.empty()) {
            return std::unexpected("stateStyles and partStyles do not support fontSize");
        }
        auto parsed = parse_scaled_from_store_string(
                          value,
                          "style.fontSize",
                          "sp",
                          tree.environment.density * tree.environment.font_scale
                      );
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        set_style_number(&Style::set_font_size, *parsed);
        break;
    }
    case BindingTarget::StyleImageFontSize: {
        if (!target_info.style_state.empty() || !target_info.style_part.empty()) {
            return std::unexpected("stateStyles and partStyles do not support imageFontSize");
        }
        auto parsed = parse_scaled_from_store_string(
                          value,
                          "style.imageFontSize",
                          "sp",
                          tree.environment.density * tree.environment.font_scale
                      );
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        set_style_number(&Style::set_image_font_size, *parsed);
        break;
    }
    case BindingTarget::StyleArcRounded: {
        auto parsed = parse_bool_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        set_style_bool(&Style::set_arc_rounded, *parsed);
        break;
    }
    case BindingTarget::LayoutType: {
        LayoutType parsed {};
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
            return std::unexpected("invalid layout.type enum");
        }
        record.mutable_layout().type = parsed;
        break;
    }
    case BindingTarget::LayoutFlexFlow: {
        FlexFlow parsed {};
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
            return std::unexpected("invalid layout.flexFlow enum");
        }
        record.mutable_layout().flex_flow = parsed;
        break;
    }
    case BindingTarget::LayoutMainAlign:
    case BindingTarget::LayoutCrossAlign:
    case BindingTarget::PlacementAlignSelf: {
        Align parsed {};
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
            return std::unexpected("invalid align enum");
        }
        if (target == BindingTarget::LayoutMainAlign) {
            record.mutable_layout().main_align = parsed;
        } else if (target == BindingTarget::LayoutCrossAlign) {
            record.mutable_layout().cross_align = parsed;
        } else {
            record.mutable_placement().align_self = parsed;
        }
        break;
    }
    case BindingTarget::LayoutGap: {
        auto parsed = parse_scaled_from_store_string(value, "layout.gap", "dp", tree.environment.density);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_layout().gap = *parsed;
        break;
    }
    case BindingTarget::LayoutGridTemplateColumns:
    case BindingTarget::LayoutGridTemplateRows: {
        auto parsed = parse_dimension_array_from_store_string(value, tree.environment);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::LayoutGridTemplateColumns) {
            record.mutable_layout().grid_template_columns = std::move(*parsed);
        } else {
            record.mutable_layout().grid_template_rows = std::move(*parsed);
        }
        break;
    }
    case BindingTarget::PlacementMode: {
        PlacementMode parsed {};
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
            return std::unexpected("invalid placement.mode enum");
        }
        record.mutable_placement().mode = parsed;
        break;
    }
    case BindingTarget::PlacementWidth:
    case BindingTarget::PlacementHeight: {
        auto parsed = parse_dimension_from_store_string(value, tree.environment);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::PlacementWidth) {
            record.mutable_placement().width = *parsed;
        } else {
            record.mutable_placement().height = *parsed;
        }
        break;
    }
    case BindingTarget::PlacementAspectRatio: {
        auto parsed = parse_aspect_ratio_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        record.mutable_placement().aspect_ratio = *parsed;
        break;
    }
    case BindingTarget::PlacementX:
    case BindingTarget::PlacementY: {
        auto parsed = parse_placement_offset_from_store_string(
                          value,
                          target == BindingTarget::PlacementX ? "placement.x" : "placement.y",
                          tree.environment
                      );
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::PlacementX) {
            record.mutable_placement().x = *parsed;
        } else {
            record.mutable_placement().y = *parsed;
        }
        break;
    }
    case BindingTarget::PlacementAlign: {
        PlacementAlign parsed {};
        if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
            return std::unexpected("invalid placement.align enum");
        }
        record.mutable_placement().align = parsed;
        break;
    }
    case BindingTarget::PlacementRelativeTo:
        record.mutable_placement().relative_to = std::string(value);
        break;
    case BindingTarget::PlacementGridColumn:
    case BindingTarget::PlacementGridRow:
    case BindingTarget::PlacementGridColumnSpan:
    case BindingTarget::PlacementGridRowSpan:
    case BindingTarget::PlacementFlexGrow: {
        auto parsed = parse_int_from_store_string(value);
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (target == BindingTarget::PlacementFlexGrow && *parsed < 0) {
            return std::unexpected("placement.flexGrow must be >= 0");
        }
        if (target == BindingTarget::PlacementGridColumn) {
            record.mutable_placement().grid_column = *parsed;
        } else if (target == BindingTarget::PlacementGridRow) {
            record.mutable_placement().grid_row = *parsed;
        } else if (target == BindingTarget::PlacementGridColumnSpan) {
            record.mutable_placement().grid_column_span = *parsed;
        } else if (target == BindingTarget::PlacementGridRowSpan) {
            record.mutable_placement().grid_row_span = *parsed;
        } else {
            record.mutable_placement().flex_grow = *parsed;
        }
        break;
    }
    default:
        break;
    }
    return {};
}
void Runtime::Impl::reapply_binding_domain(TreeRecord &tree, NodeRecord &record, const BindingTargetInfo &target_info)
{
    if (backend == nullptr) {
        return;
    }
    switch (target_info.domain) {
    case BindingApplyDomain::Props:
        apply_record_props(record, target_info.props_mask);
        if (record.node().type == NodeType::Image &&
                has_mask(target_info.props_mask, PropsApplyMask::ImageSource)) {
            backend->apply_placement(record.handle, record.placement(), PlacementApplyMask::Size);
        }
        break;
    case BindingApplyDomain::Style: {
        const auto style_snapshot = make_style_snapshot(record);
        record.resolved_style = resolve_style_shared(tree, style_snapshot);
        backend->apply_style(record.handle, *record.resolved_style, target_info.style_mask);
        backend->apply_debug_visual(record.handle, view_debug_enabled_);
        break;
    }
    case BindingApplyDomain::Layout:
        backend->apply_layout(record.handle, record.layout(), target_info.layout_mask);
        break;
    case BindingApplyDomain::Placement:
        backend->apply_placement(record.handle, record.placement(), target_info.placement_mask);
        if (has_mask(target_info.placement_mask, PlacementApplyMask::Size)) {
            apply_record_props(record, PropsApplyMask::CommonTransform);
        }
        break;
    }
}
void Runtime::Impl::reapply_binding_masks(TreeRecord &tree, NodeRecord &record, const BindingApplyMasks &masks)
{
    if (backend == nullptr) {
        return;
    }
    if (masks.props != PropsApplyMask::None) {
        apply_record_props(record, masks.props);
        if (record.node().type == NodeType::Image && has_mask(masks.props, PropsApplyMask::ImageSource)) {
            backend->apply_placement(record.handle, record.placement(), PlacementApplyMask::Size);
        }
    }
    if (masks.style != StyleApplyMask::None) {
        const auto style_snapshot = make_style_snapshot(record);
        record.resolved_style = resolve_style_shared(tree, style_snapshot);
        backend->apply_style(record.handle, *record.resolved_style, masks.style);
        backend->apply_debug_visual(record.handle, view_debug_enabled_);
    }
    if (masks.layout != LayoutApplyMask::None) {
        backend->apply_layout(record.handle, record.layout(), masks.layout);
    }
    if (masks.placement != PlacementApplyMask::None) {
        backend->apply_placement(record.handle, record.placement(), masks.placement);
        if (has_mask(masks.placement, PlacementApplyMask::Size)) {
            apply_record_props(record, PropsApplyMask::CommonTransform);
        }
    }
}
void Runtime::Impl::merge_binding_mask(BindingApplyMasks &masks, const BindingTargetInfo &target_info)
{
    switch (target_info.domain) {
    case BindingApplyDomain::Props:
        masks.props = masks.props | target_info.props_mask;
        break;
    case BindingApplyDomain::Style:
        masks.style = masks.style | target_info.style_mask;
        break;
    case BindingApplyDomain::Layout:
        masks.layout = masks.layout | target_info.layout_mask;
        break;
    case BindingApplyDomain::Placement:
        masks.placement = masks.placement | target_info.placement_mask;
        break;
    }
}
bool Runtime::Impl::node_has_binding_key(const Node &node, std::string_view key)
{
    for (const auto &[unused_binding_path, expression] : node.bindings) {
        (void)unused_binding_path;
        auto store_key = normalize_binding_store_key(expression);
        if (store_key && *store_key == key) {
            return true;
        }
    }
    return false;
}
const Node *Runtime::Impl::find_child_node_by_path(
    const Node &node,
    const std::vector<std::string> &segments,
    size_t index
)
{
    if (index >= segments.size()) {
        return &node;
    }
    for (const auto &child : node.children) {
        if (child.id == segments[index]) {
            return find_child_node_by_path(child, segments, index + 1);
        }
    }
    return nullptr;
}
std::vector<std::string> Runtime::Impl::split_absolute_path_segments(std::string_view absolute_path)
{
    std::vector<std::string> segments;
    const auto trimmed = trim_slashes(absolute_path);
    size_t begin = 0;
    while (begin < trimmed.size()) {
        const auto end = trimmed.find('/', begin);
        const auto count = end == std::string::npos ? std::string::npos : end - begin;
        segments.emplace_back(trimmed.substr(begin, count));
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return segments;
}
bool Runtime::Impl::tree_has_binding_declaration_for_update(
    const TreeRecord &tree,
    std::string_view absolute_path,
    std::string_view key
)
{
    const auto segments = split_absolute_path_segments(absolute_path);
    if (segments.empty()) {
        return false;
    }
    auto screen_it = tree.screens.find(segments.front());
    if (screen_it == tree.screens.end()) {
        return false;
    }
    const auto *node = find_child_node_by_path(screen_it->second, segments, 1);
    return node != nullptr && node_has_binding_key(*node, key);
}
void Runtime::Impl::set_binding_values(DocumentId document_id, std::span<const BindingValueUpdate> updates)
{
    if (store == nullptr || updates.empty()) {
        return;
    }
    auto *tree = resolve_tree(document_id);
    if (tree == nullptr) {
        return;
    }

    // A batch represents a final snapshot. Coalesce duplicate path/key writes to their last
    // value so explicit subscribers never receive an earlier value after the backend has
    // already advanced to a later value from the same batch.
    auto update_is_shadowed = [&updates](size_t index) {
        for (size_t later = index + 1; later < updates.size(); ++later) {
            if (updates[index].absolute_path == updates[later].absolute_path &&
                    updates[index].key == updates[later].key) {
                return true;
            }
        }
        return false;
    };

    for (size_t update_index = 0; update_index < updates.size(); ++update_index) {
        if (update_is_shadowed(update_index)) {
            continue;
        }
        const auto &update = updates[update_index];
        store->set_string_silent(document_id, update.absolute_path, update.key, update.value);
    }

    if (tree != nullptr) {
        // Binding batches are small (the system GUI normally sends 1-30 updates). A compact
        // vector avoids constructing a hash table for every flush and keeps all scratch storage
        // in one allocation. Linear lookup also coalesces multiple fields of the same node.
        std::vector<std::pair<NodeUid, BindingApplyMasks>> dirty_nodes;
        dirty_nodes.reserve(std::min(updates.size(), tree->nodes.size()));
        for (size_t update_index = 0; update_index < updates.size(); ++update_index) {
            if (update_is_shadowed(update_index)) {
                continue;
            }
            const auto &update = updates[update_index];
            std::string normalized_path_storage;
            std::string_view query = update.absolute_path;
            if (!is_normalized_absolute_path(query)) {
                normalized_path_storage = normalize_absolute_path(query);
                query = normalized_path_storage;
            }
            auto uid = resolve_normalized_uid(*tree, query);
            if (!uid.has_value()) {
                if (!tree_has_binding_declaration_for_update(*tree, query, update.key)) {
                    BROOKESIA_LOGW(
                        "Binding update target not found and no matching binding declaration exists: "
                        "document_id=%1%, path='%2%', key='%3%', value='%4%'",
                        document_id.value(), query, update.key, update.value
                    );
                }
                continue;
            }
            auto *record = find_node_record(*tree, *uid);
            if (record == nullptr) {
                continue;
            }
            bool matched_binding = false;
            for (const auto &[binding_path, expression] : record->node().bindings) {
                auto store_key = normalize_binding_store_key(expression);
                if (!store_key || *store_key != update.key) {
                    continue;
                }
                matched_binding = true;
                auto binding_target = resolve_binding_target(record->node().type, binding_path);
                if (!binding_target) {
                    BROOKESIA_LOGW(
                        "Skipping invalid batched binding path '%1%' on '%2%': %3%",
                        binding_path,
                        record->absolute_path,
                        binding_target.error()
                    );
                    continue;
                }
                auto apply_result = apply_binding_value(*tree, *record, *binding_target, update.value);
                if (!apply_result) {
                    BROOKESIA_LOGW(
                        "Failed to apply batched binding update: node='%1%', path='%2%', value='%3%', reason='%4%'",
                        record->absolute_path,
                        binding_path,
                        update.value,
                        apply_result.error()
                    );
                    continue;
                }
                auto dirty_it = std::find_if(dirty_nodes.begin(), dirty_nodes.end(), [uid = *uid](const auto & entry) {
                    return entry.first == uid;
                });
                if (dirty_it == dirty_nodes.end()) {
                    dirty_nodes.emplace_back(*uid, BindingApplyMasks{});
                    dirty_it = std::prev(dirty_nodes.end());
                }
                merge_binding_mask(dirty_it->second, *binding_target);
            }
            if (!matched_binding) {
                BROOKESIA_LOGW(
                    "Binding update path exists but no binding declaration matched key: "
                    "document_id=%1%, path='%2%', key='%3%', value='%4%', node_type=%5%",
                    document_id.value(), record->absolute_path, update.key, update.value,
                    get_theme_style_key(record->node().type)
                );
            }
        }

        for (const auto &[uid, masks] : dirty_nodes) {
            auto *record = find_node_record(*tree, uid);
            if (record == nullptr) {
                continue;
            }
            reapply_binding_masks(*tree, *record, masks);
        }
    }

    // Values were written silently before backend application so synchronous backend events
    // observe the new value. Notify explicit public subscribers only after backend state is
    // coherent; a reentrant write with a different value suppresses this stale notification.
    for (size_t update_index = 0; update_index < updates.size(); ++update_index) {
        if (update_is_shadowed(update_index)) {
            continue;
        }
        const auto &update = updates[update_index];
        store->notify_string_if_value(
            document_id,
            update.absolute_path,
            update.key,
            update.value
        );
    }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    // A public listener may unload the document. Resolve it again after notification instead of
    // dereferencing the pre-callback TreeRecord pointer.
    auto *trace_tree = resolve_tree(document_id);
    if (trace_tree != nullptr) {
        auto hp = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        size_t action_route_count = 0;
        size_t action_listener_count = 0;
        {
            std::lock_guard lock(action_registry_->mutex);
            action_route_count = action_registry_->routes.size();
            for (const auto &route : action_registry_->routes) {
                action_listener_count += route.bucket.size();
            }
        }
        BROOKESIA_LOGI(
            "[HeapTrace][gui.binding.signals] doc(%1%) updates(%2%) nodes(%3%) action_signals(%4%) "
            "store_connections(%5%) store_signals(%6%) create_view(%7%) create_subtree(%8%) "
            "destroy_subtree(%9%) psram_free(%10%) action_listeners(%11%)",
            document_id.value(), updates.size(), trace_tree->nodes.size(), action_route_count,
            store ? store->debug_connection_count() : 0,
            store ? store->debug_signal_count() : 0,
            dbg_create_view_count_, dbg_create_subtree_count_, dbg_destroy_subtree_count_,
            hp.external_free, action_listener_count
        );
    }
#endif
}
Runtime::Impl::BindingApplyMasks Runtime::Impl::apply_initial_bindings(
    TreeRecord &tree,
    Node &node,
    std::string_view absolute_path,
    std::vector<InitialStyleBinding> &style_bindings)
{
    if (store == nullptr) {
        return {};
    }

    BindingApplyMasks applied_masks;
    NodeRecord temporary_record;
    temporary_record.definition = &node;
    // This record lives only for the duration of initial binding application, so its definition
    // points at the mutable construction-local Node. Persistent records never enable this flag.
    temporary_record.transient_mutable_definition = true;
    for (const auto &[path, expression] : node.bindings) {
        auto binding_target = resolve_binding_target(node.type, path);
        if (!binding_target) {
            BROOKESIA_LOGW("Skipping invalid initial binding path '%1%': %2%", path, binding_target.error());
            continue;
        }

        auto store_key = normalize_binding_store_key(expression);
        if (!store_key) {
            BROOKESIA_LOGW("Skipping invalid initial binding value '%1%': %2%", expression, store_key.error());
            continue;
        }

        auto value = store->get_string(tree.document_id, absolute_path, *store_key);
        if (!value.has_value()) {
            continue;
        }
        if (binding_target->target == BindingTarget::ImagePropsSrc && !value->empty() &&
                !has_image_resource(tree, *value)) {
            BROOKESIA_LOGD(
                "Skip unavailable initial image binding: node='%1%', path='%2%', value='%3%'",
                absolute_path,
                path,
                *value
            );
            continue;
        }

        auto apply_result = apply_binding_value(tree, temporary_record, *binding_target, *value);
        if (!apply_result) {
            BROOKESIA_LOGW(
                "Failed to apply initial binding: node='%1%', path='%2%', value='%3%', reason='%4%'",
                absolute_path,
                path,
                *value,
                apply_result.error()
            );
            continue;
        }
        if (binding_target->domain == BindingApplyDomain::Style) {
            style_bindings.push_back(InitialStyleBinding{
                .target = *binding_target,
                .value = *value,
            });
        }
        merge_binding_mask(applied_masks, *binding_target);
    }

    return applied_masks;
}
std::string Runtime::Impl::resolve_event_effect_target_path(const NodeRecord &source_record, std::string_view target) const
{
    if (target.empty() || target == "self") {
        return source_record.absolute_path;
    }
    if (target.starts_with("/")) {
        return normalize_absolute_path(target);
    }
    auto relative_target = target;
    if (relative_target.starts_with("./")) {
        relative_target.remove_prefix(2);
    }
    if (relative_target.empty()) {
        return source_record.absolute_path;
    }
    return normalize_absolute_path(source_record.absolute_path + "/" + std::string(relative_target));
}
Runtime::Impl::NodeRecord *Runtime::Impl::resolve_event_effect_target(TreeRecord &tree, const NodeRecord &source_record, std::string_view target)
{
    auto uid = resolve_any_uid(tree, resolve_event_effect_target_path(source_record, target));
    if (!uid) {
        return nullptr;
    }
    return find_node_record(tree, *uid);
}
std::string Runtime::Impl::build_event_animation_key(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view animation_id)
{
    return std::to_string(document_id.value()) + '\x1f' + std::string(absolute_path) + '\x1f' +
           std::string(animation_id);
}
std::expected<void, std::string> Runtime::Impl::apply_event_property_update(
    TreeRecord &tree,
    const NodeRecord &source_record,
    const EventPropertyUpdate &update)
{
    auto *target_record = resolve_event_effect_target(tree, source_record, update.target);
    if (target_record == nullptr) {
        return std::unexpected("Event effect target not found: " + update.target);
    }
    auto target_info = resolve_binding_target(target_record->node().type, update.field);
    if (!target_info) {
        return std::unexpected(target_info.error());
    }
    auto apply_result = apply_binding_value(tree, *target_record, *target_info, update.value);
    if (!apply_result) {
        return std::unexpected(apply_result.error());
    }
    reapply_binding_domain(tree, *target_record, *target_info);
    return {};
}
std::expected<Animation, std::string> Runtime::Impl::resolve_event_animation(
    const NodeRecord &record,
    const EventEffect &effect) const
{
    if (!effect.animation.id.empty() || effect.animation_id.empty()) {
        return effect.animation;
    }
    auto animation_it = std::find_if(
                            record.node().animations.begin(),
                            record.node().animations.end(),
    [&effect](const Animation & animation) {
        return animation.id == effect.animation_id;
    }
                        );
    if (animation_it == record.node().animations.end()) {
        return std::unexpected("Animation not found: " + effect.animation_id);
    }
    return *animation_it;
}
std::expected<void, std::string> Runtime::Impl::start_event_animation(
    TreeRecord &tree,
    const NodeRecord &source_record,
    const EventEffect &effect)
{
    auto *target_record = resolve_event_effect_target(tree, source_record, effect.target);
    if (target_record == nullptr) {
        return std::unexpected("Event effect animation target not found: " + effect.target);
    }
    auto animation = resolve_event_animation(*target_record, effect);
    if (!animation) {
        return std::unexpected(animation.error());
    }
    if (backend == nullptr) {
        return std::unexpected("GUI backend is null");
    }
    const auto animation_id = effect.animation_id.empty() ? animation->id : effect.animation_id;
    std::optional<std::string> animation_key;
    if (!animation_id.empty()) {
        animation_key = build_event_animation_key(
                            tree.document_id, target_record->absolute_path, animation_id
                        );
        if (auto old_it = event_animation_ids_.find(*animation_key); old_it != event_animation_ids_.end()) {
            const auto old_subscription_id = old_it->second;
            event_animation_ids_.erase(old_it);
            (void)unsubscribe_subscription(old_subscription_id);
        }
    }
    auto backend_result = backend->start_animation(target_record->handle, *animation, {});
    if (!backend_result || !backend_result->connection.connected()) {
        return std::unexpected("Failed to start event animation");
    }
    auto connection = std::make_shared<ScopedConnection>(std::move(backend_result->connection));
    const auto subscription_id = next_subscription_id_++;
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [registry = std::weak_ptr<SubscriptionRegistry>(subscription_registry_),
                                    subscription_id,
             connection = std::move(connection)]() mutable {
        connection->disconnect();
        if (auto locked_registry = registry.lock(); locked_registry != nullptr)
        {
            locked_registry->disconnect_handlers.erase(subscription_id);
        }
    };
    subscription_registry_->disconnect_handlers[subscription_id] = disconnect_handler;
    register_document_subscription(subscription_id, tree.document_id);
    if (animation_key) {
        event_animation_ids_[*animation_key] = subscription_id;
    }
    return {};
}
std::expected<void, std::string> Runtime::Impl::stop_event_animation(
    TreeRecord &tree,
    const NodeRecord &source_record,
    const EventEffect &effect)
{
    auto *target_record = resolve_event_effect_target(tree, source_record, effect.target);
    if (target_record == nullptr) {
        return std::unexpected("Event effect animation target not found: " + effect.target);
    }
    const auto key = build_event_animation_key(
                         tree.document_id, target_record->absolute_path, effect.animation_id
                     );
    auto animation_it = event_animation_ids_.find(key);
    if (animation_it == event_animation_ids_.end()) {
        return {};
    }
    (void)unsubscribe_subscription(animation_it->second);
    event_animation_ids_.erase(animation_it);
    return {};
}
void Runtime::Impl::execute_event_effects(
    TreeRecord &tree,
    NodeRecord &source_record,
    const Event &event,
    std::vector<Event> &deferred_actions)
{
    for (const auto &binding : source_record.node().events) {
        if (binding.type != event.type) {
            continue;
        }
        for (const auto &effect : binding.effects) {
            std::expected<void, std::string> result {};
            switch (effect.type) {
            case EventEffectType::EmitAction: {
                if (effect.require_valid_press && source_record.press_lost_since_pressed) {
                    continue;
                }
                auto emitted_event = event;
                emitted_event.action = effect.action;
                deferred_actions.push_back(std::move(emitted_event));
                break;
            }
            case EventEffectType::SetProperty:
            case EventEffectType::SetProperties:
                for (const auto &update : effect.updates) {
                    result = apply_event_property_update(tree, source_record, update);
                    if (!result) {
                        break;
                    }
                }
                break;
            case EventEffectType::StartAnimation:
                result = start_event_animation(tree, source_record, effect);
                break;
            case EventEffectType::StopAnimation:
                result = stop_event_animation(tree, source_record, effect);
                break;
            case EventEffectType::Max:
            default:
                result = std::unexpected("Invalid event effect type");
                break;
            }
            if (!result) {
                BROOKESIA_LOGW(
                    "Failed to execute event effect: node='%1%', event=%2%, effect=%3%, error=%4%",
                    source_record.absolute_path,
                    BROOKESIA_DESCRIBE_ENUM_TO_STR(event.type),
                    BROOKESIA_DESCRIBE_ENUM_TO_STR(effect.type),
                    result.error()
                );
            }
        }
    }
}
KeyboardKeyImageSpec Runtime::Impl::make_keyboard_key_image_spec(const ImageAsset &image)
{
    return KeyboardKeyImageSpec{
        .image_id = image.id,
        .primary_src = image.src,
        .native_src = 0,
        .width = image.width,
        .height = image.height,
    };
}
KeyboardKeyImageSpec Runtime::Impl::make_keyboard_key_image_spec(const RuntimeImageResource &image)
{
    return KeyboardKeyImageSpec{
        .image_id = image.id,
        .primary_src = image.primary_src,
        .native_src = image.native_src,
        .width = image.width,
        .height = image.height,
    };
}
std::optional<KeyboardKeyImageSpec> Runtime::Impl::resolve_keyboard_key_image(
    const TreeRecord &tree,
    std::string_view image_id) const
{
    auto image_it = tree.images.find(std::string(image_id));
    if (image_it != tree.images.end()) {
        return make_keyboard_key_image_spec(image_it->second);
    }

    auto global_image_it = global_images.find(std::string(image_id));
    if (global_image_it != global_images.end()) {
        return make_keyboard_key_image_spec(global_image_it->second);
    }
    return std::nullopt;
}
void Runtime::Impl::resolve_keyboard_key_images(const TreeRecord &tree, KeyboardProps &props) const
{
    for (auto &[unused_mode, layout] : props.layouts) {
        (void)unused_mode;
        for (auto &row : layout.rows) {
            for (auto &key : row) {
                key.resolved_image = {};
                if (key.image.empty()) {
                    continue;
                }
                auto resolved = resolve_keyboard_key_image(tree, key.image);
                if (resolved.has_value()) {
                    key.resolved_image = std::move(*resolved);
                }
            }
        }
    }
}
void Runtime::Impl::resolve_keyboard_key_images(const TreeRecord &tree, Node &node) const
{
    if (node.type == NodeType::Keyboard) {
        resolve_keyboard_key_images(tree, node.keyboard_props);
    }
}
bool Runtime::Impl::node_references_image(const Node &node, std::string_view image_id) const
{
    if (node.type == NodeType::Image && node.image_props.src == image_id) {
        return true;
    }
    if (node.type != NodeType::Keyboard) {
        return false;
    }
    for (const auto &[unused_mode, layout] : node.keyboard_props.layouts) {
        (void)unused_mode;
        for (const auto &row : layout.rows) {
            for (const auto &key : row) {
                if (key.image == image_id) {
                    return true;
                }
            }
        }
    }
    return false;
}
bool Runtime::Impl::node_references_image(const NodeRecord &record, std::string_view image_id) const
{
    if (record.node().type == NodeType::Image) {
        return record.image_props().src == image_id;
    }
    if (record.node().type != NodeType::Keyboard) {
        return false;
    }
    for (const auto &[unused_mode, layout] : record.keyboard_props().layouts) {
        (void)unused_mode;
        for (const auto &row : layout.rows) {
            for (const auto &key : row) {
                if (key.image == image_id) {
                    return true;
                }
            }
        }
    }
    return false;
}
ResolvedImageSpec Runtime::Impl::resolve_image_spec(const TreeRecord &tree, std::string_view image_id) const
{
    if (image_id.empty()) {
        return {};
    }

    auto image_it = tree.images.find(std::string(image_id));
    if (image_it != tree.images.end()) {
        BROOKESIA_LOGD("Resolved image from JSON asset: id='%1%', src='%2%', width=%3%, height=%4%",
                       image_it->second.id,
                       image_it->second.src,
                       image_it->second.width,
                       image_it->second.height);
        return {
            .image_id = image_it->second.id,
            .primary_src = image_it->second.src,
            .native_src = 0,
            .width = image_it->second.width,
            .height = image_it->second.height,
        };
    }

    auto global_image_it = global_images.find(std::string(image_id));
    if (global_image_it == global_images.end()) {
        return {};
    }
    BROOKESIA_LOGD(
        "Resolved image from runtime resource: id='%1%', primary_src='%2%', native_src=%3%, width=%4%, height=%5%",
        global_image_it->second.id,
        global_image_it->second.primary_src,
        global_image_it->second.native_src,
        global_image_it->second.width,
        global_image_it->second.height);
    return {
        .image_id = global_image_it->second.id,
        .primary_src = global_image_it->second.primary_src,
        .native_src = global_image_it->second.native_src,
        .width = global_image_it->second.width,
        .height = global_image_it->second.height,
    };
}
void Runtime::Impl::resolve_image_source(const TreeRecord &tree, Node &node) const
{
    resolve_keyboard_key_style_refs(tree, node);
    resolve_keyboard_key_images(tree, node);

    if (node.type != NodeType::Image) {
        node.resolved_image = {};
        return;
    }
    node.resolved_image = resolve_image_spec(tree, node.image_props.src);
}
bool Runtime::Impl::has_font_resource(const TreeRecord &tree, const std::string &font_id) const
{
    (void)tree;
    return global_fonts.contains(font_id);
}
bool Runtime::Impl::has_image_resource(const TreeRecord &tree, const std::string &image_id) const
{
    return tree.images.contains(image_id) || global_images.contains(image_id);
}

} // namespace esp_brookesia::gui
