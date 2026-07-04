/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>

#include "brookesia/gui_interface/backend.hpp"
#include "brookesia/gui_interface/document.hpp"

namespace esp_brookesia::gui {

enum class BindingTarget {
    CommonPropsHidden,
    CommonPropsDisabled,
    CommonPropsClickable,
    CommonPropsScrollable,
    CommonPropsPressLock,
    CommonPropsAngle,
    CommonPropsZoom,
    CommonPropsPivotX,
    CommonPropsPivotY,
    LabelPropsText,
    ImagePropsSrc,
    ImagePropsRecolor,
    ImagePropsRecolorOpacity,
    ImagePropsAngle,
    ImagePropsOffsetX,
    ImagePropsOffsetY,
    ImagePropsZoom,
    ImagePropsPivotX,
    ImagePropsPivotY,
    FrameViewPropsAutoRegisterOutput,
    FrameViewPropsOutputName,
    FrameViewPropsColorFormat,
    TextInputPropsText,
    TextInputPropsPlaceholder,
    TextInputPropsPassword,
    TextInputPropsMultiline,
    TextInputPropsMaxLength,
    RangePropsValue,
    RangePropsMin,
    RangePropsMax,
    RangePropsStep,
    TogglePropsChecked,
    DropdownPropsOptions,
    DropdownPropsSelectedIndex,
    TablePropsRows,
    TablePropsColumns,
    TablePropsCells,
    LinePropsPoints,
    KeyboardPropsMode,
    KeyboardPropsPopovers,
    KeyboardPropsAllowedModes,
    KeyboardPropsIconSize,
    CanvasPropsCommands,
    StyleBgColor,
    StyleBgGradientColor,
    StyleBgGradientDirection,
    StyleTextColor,
    StyleBorderColor,
    StyleLineColor,
    StyleArcColor,
    StyleArcGradientColor,
    StyleFont,
    StyleBgMainStop,
    StyleBgGradientStop,
    StyleBgGradientOpacity,
    StyleBorderWidth,
    StyleRadius,
    StylePadding,
    StylePaddingLeft,
    StylePaddingRight,
    StylePaddingTop,
    StylePaddingBottom,
    StyleMargin,
    StyleMarginLeft,
    StyleMarginRight,
    StyleMarginTop,
    StyleMarginBottom,
    StyleShadowWidth,
    StyleShadowOffsetX,
    StyleShadowOffsetY,
    StyleShadowColor,
    StyleOpacity,
    StyleLineWidth,
    StyleImageOpacity,
    StyleImageRecolor,
    StyleImageRecolorOpacity,
    StyleFontSize,
    StyleImageFontSize,
    StyleArcWidth,
    StyleArcOpacity,
    StyleArcGradientSegments,
    StyleArcRounded,
    LayoutType,
    LayoutFlexFlow,
    LayoutMainAlign,
    LayoutCrossAlign,
    LayoutGap,
    LayoutGridTemplateColumns,
    LayoutGridTemplateRows,
    PlacementMode,
    PlacementWidth,
    PlacementHeight,
    PlacementAspectRatio,
    PlacementX,
    PlacementY,
    PlacementAlign,
    PlacementRelativeTo,
    PlacementGridColumn,
    PlacementGridRow,
    PlacementGridColumnSpan,
    PlacementGridRowSpan,
    PlacementAlignSelf,
    PlacementFlexGrow,
};

enum class BindingApplyDomain {
    Props,
    Style,
    Layout,
    Placement,
};

struct BindingTargetInfo {
    BindingTarget target;
    BindingApplyDomain domain;
    PropsApplyMask props_mask = PropsApplyMask::None;
    StyleApplyMask style_mask = StyleApplyMask::None;
    LayoutApplyMask layout_mask = LayoutApplyMask::None;
    PlacementApplyMask placement_mask = PlacementApplyMask::None;
    std::string style_part;
    std::string style_state;
};

inline bool binding_uses_legacy_store_prefix(std::string_view expression)
{
    return expression.starts_with("store.");
}

inline std::expected<std::string, std::string> normalize_binding_store_key(std::string_view expression)
{
    if (expression.empty()) {
        return std::unexpected("Binding store key must not be empty");
    }
    if (binding_uses_legacy_store_prefix(expression)) {
        return std::unexpected(
                   "Binding value '" + std::string(expression) +
                   "' must not use the legacy 'store.' prefix; use the bare store key instead"
               );
    }
    return std::string(expression);
}

inline std::expected<BindingTargetInfo, std::string> resolve_binding_target(
    NodeType node_type,
    std::string_view path)
{
    if (path.empty()) {
        return std::unexpected("Binding path must not be empty");
    }
    if (path.find('_') != std::string_view::npos) {
        return std::unexpected(
                   "Binding path '" + std::string(path) +
                   "' must use public camelCase field names; snake_case is no longer supported"
               );
    }
    if (path == "text") {
        return std::unexpected(
                   "Binding path 'text' is no longer supported; use 'labelProps.text' or "
                   "'textInputProps.text'"
               );
    }
    if (path == "src") {
        return std::unexpected("Binding path 'src' is no longer supported; use 'imageProps.src'");
    }
    if (path.starts_with("props.")) {
        return std::unexpected(
                   "Binding path '" + std::string(path) +
                   "' still uses the legacy 'props' object; move it to the new props domain"
               );
    }

    const auto expect_node_type =
        [&](std::string_view full_path,
    std::initializer_list<NodeType> accepted_types) -> std::expected<void, std::string> {
        for (const auto accepted : accepted_types)
        {
            if (accepted == node_type) {
                return {};
            }
        }
        return std::unexpected(
                   "Binding path '" + std::string(full_path) + "' is not supported for node type '" +
                   BROOKESIA_DESCRIBE_ENUM_TO_STR(node_type) + "'"
               );
    };

    const auto make_props = [&](BindingTarget target, PropsApplyMask mask) {
        BindingTargetInfo info;
        info.target = target;
        info.domain = BindingApplyDomain::Props;
        info.props_mask = mask;
        return std::expected<BindingTargetInfo, std::string>(std::move(info));
    };
    const auto make_style = [&](BindingTarget target, StyleApplyMask mask) {
        BindingTargetInfo info;
        info.target = target;
        info.domain = BindingApplyDomain::Style;
        info.style_mask = mask;
        return std::expected<BindingTargetInfo, std::string>(std::move(info));
    };
    const auto make_layout = [&](BindingTarget target, LayoutApplyMask mask) {
        BindingTargetInfo info;
        info.target = target;
        info.domain = BindingApplyDomain::Layout;
        info.layout_mask = mask;
        return std::expected<BindingTargetInfo, std::string>(std::move(info));
    };
    const auto make_placement = [&](BindingTarget target, PlacementApplyMask mask) {
        BindingTargetInfo info;
        info.target = target;
        info.domain = BindingApplyDomain::Placement;
        info.placement_mask = mask;
        return std::expected<BindingTargetInfo, std::string>(std::move(info));
    };

    const auto resolve_state_style_target =
    [&](std::string_view full_path, std::string_view remaining) -> std::expected<BindingTargetInfo, std::string> {
        const auto separator = remaining.find('.');
        if (separator == std::string_view::npos || separator == 0 || separator + 1 >= remaining.size())
        {
            return std::unexpected(
                "Binding path '" + std::string(full_path) +
                "' must use stateStyles.<state>.<styleField>"
            );
        }
        const std::string state_name(remaining.substr(0, separator));
        if (!is_supported_style_state_name(state_name))
        {
            return std::unexpected("Unsupported stateStyles state in binding path: " + state_name);
        }
        const std::string style_field(remaining.substr(separator + 1));
        auto base_target = resolve_binding_target(node_type, "style." + style_field);
        if (!base_target)
        {
            return std::unexpected(base_target.error());
        }
        if (base_target->domain != BindingApplyDomain::Style ||
                base_target->target == BindingTarget::StyleFont ||
                base_target->target == BindingTarget::StyleFontSize)
        {
            return std::unexpected(
                "Binding path '" + std::string(full_path) +
                "' uses a style field that is not supported by stateStyles"
            );
        }
        base_target->style_state = state_name;
        base_target->style_mask = StyleApplyMask::All;
        return base_target;
    };

    if (path.starts_with("partStyles.")) {
        constexpr std::string_view prefix = "partStyles.";
        auto remaining = path.substr(prefix.size());
        const auto separator = remaining.find('.');
        if (separator == std::string_view::npos || separator == 0 || separator + 1 >= remaining.size()) {
            return std::unexpected(
                       "Binding path '" + std::string(path) +
                       "' must use partStyles.<part>.<styleField> or partStyles.<part>.stateStyles.<state>.<styleField>"
                   );
        }
        const std::string part_name(remaining.substr(0, separator));
        if (!is_supported_style_part_name(part_name)) {
            return std::unexpected("Unsupported partStyles part in binding path: " + part_name);
        }
        auto style_field = remaining.substr(separator + 1);
        std::expected<BindingTargetInfo, std::string> target =
            std::unexpected(std::string("Invalid partStyles binding target"));
        if (style_field.starts_with("stateStyles.")) {
            target = resolve_state_style_target(path, style_field.substr(std::string_view("stateStyles.").size()));
        } else {
            if (style_field.starts_with("style.")) {
                style_field = style_field.substr(std::string_view("style.").size());
            }
            target = resolve_binding_target(node_type, "style." + std::string(style_field));
            if (target && target->domain == BindingApplyDomain::Style &&
                    (target->target == BindingTarget::StyleFont || target->target == BindingTarget::StyleFontSize)) {
                return std::unexpected(
                           "Binding path '" + std::string(path) +
                           "' uses a style field that is not supported by partStyles"
                       );
            }
        }
        if (!target) {
            return std::unexpected(target.error());
        }
        if (target->domain != BindingApplyDomain::Style) {
            return std::unexpected("Binding path '" + std::string(path) + "' must target a style field");
        }
        target->style_part = part_name;
        target->style_mask = StyleApplyMask::All;
        return target;
    }

    if (path.starts_with("stateStyles.")) {
        constexpr std::string_view prefix = "stateStyles.";
        auto remaining = path.substr(prefix.size());
        return resolve_state_style_target(path, remaining);
    }

    if (path == "commonProps.hidden") {
        return make_props(BindingTarget::CommonPropsHidden, PropsApplyMask::CommonHidden);
    }
    if (path == "commonProps.disabled") {
        return make_props(BindingTarget::CommonPropsDisabled, PropsApplyMask::CommonDisabled);
    }
    if (path == "commonProps.clickable") {
        return make_props(BindingTarget::CommonPropsClickable, PropsApplyMask::CommonClickable);
    }
    if (path == "commonProps.scrollable") {
        return make_props(BindingTarget::CommonPropsScrollable, PropsApplyMask::CommonScrollable);
    }
    if (path == "commonProps.pressLock") {
        return make_props(BindingTarget::CommonPropsPressLock, PropsApplyMask::CommonPressLock);
    }
    if (path == "commonProps.angle") {
        return make_props(BindingTarget::CommonPropsAngle, PropsApplyMask::CommonTransform);
    }
    if (path == "commonProps.zoom") {
        return make_props(BindingTarget::CommonPropsZoom, PropsApplyMask::CommonTransform);
    }
    if (path == "commonProps.pivotX") {
        return make_props(BindingTarget::CommonPropsPivotX, PropsApplyMask::CommonTransform);
    }
    if (path == "commonProps.pivotY") {
        return make_props(BindingTarget::CommonPropsPivotY, PropsApplyMask::CommonTransform);
    }

    if (path == "labelProps.text") {
        auto supported = expect_node_type(path, {NodeType::Label, NodeType::Checkbox});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::LabelPropsText, PropsApplyMask::LabelText);
    }
    if (path == "imageProps.src") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsSrc, PropsApplyMask::ImageSource);
    }
    if (path == "imageProps.recolor") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsRecolor, PropsApplyMask::ImageRecolor);
    }
    if (path == "imageProps.recolorOpacity") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsRecolorOpacity, PropsApplyMask::ImageRecolor);
    }
    if (path == "imageProps.angle") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsAngle, PropsApplyMask::ImageAngle);
    }
    if (path == "imageProps.offsetX") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsOffsetX, PropsApplyMask::ImageOffsetX);
    }
    if (path == "imageProps.offsetY") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsOffsetY, PropsApplyMask::ImageOffsetY);
    }
    if (path == "imageProps.zoom") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsZoom, PropsApplyMask::ImageZoom);
    }
    if (path == "imageProps.pivotX") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsPivotX, PropsApplyMask::ImagePivot);
    }
    if (path == "imageProps.pivotY") {
        auto supported = expect_node_type(path, {NodeType::Image});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::ImagePropsPivotY, PropsApplyMask::ImagePivot);
    }
    if (path == "frameViewProps.autoRegisterOutput") {
        auto supported = expect_node_type(path, {NodeType::FrameView});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::FrameViewPropsAutoRegisterOutput, PropsApplyMask::FrameViewConfig);
    }
    if (path == "frameViewProps.outputName") {
        auto supported = expect_node_type(path, {NodeType::FrameView});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::FrameViewPropsOutputName, PropsApplyMask::FrameViewConfig);
    }
    if (path == "frameViewProps.colorFormat") {
        auto supported = expect_node_type(path, {NodeType::FrameView});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::FrameViewPropsColorFormat, PropsApplyMask::FrameViewConfig);
    }
    if (path == "textInputProps.text") {
        auto supported = expect_node_type(path, {NodeType::TextInput});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TextInputPropsText, PropsApplyMask::TextInputText);
    }
    if (path == "textInputProps.placeholder") {
        auto supported = expect_node_type(path, {NodeType::TextInput});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TextInputPropsPlaceholder, PropsApplyMask::TextInputPlaceholder);
    }
    if (path == "textInputProps.password") {
        auto supported = expect_node_type(path, {NodeType::TextInput});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TextInputPropsPassword, PropsApplyMask::TextInputPassword);
    }
    if (path == "textInputProps.multiline") {
        auto supported = expect_node_type(path, {NodeType::TextInput});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TextInputPropsMultiline, PropsApplyMask::TextInputMultiline);
    }
    if (path == "textInputProps.maxLength") {
        auto supported = expect_node_type(path, {NodeType::TextInput});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TextInputPropsMaxLength, PropsApplyMask::TextInputMaxLength);
    }

    if (path == "rangeProps.value") {
        auto supported = expect_node_type(path, {NodeType::Slider, NodeType::ProgressBar, NodeType::Arc});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::RangePropsValue, PropsApplyMask::RangeValue);
    }
    if (path == "rangeProps.min") {
        auto supported = expect_node_type(path, {NodeType::Slider, NodeType::ProgressBar, NodeType::Arc});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::RangePropsMin, PropsApplyMask::RangeRange);
    }
    if (path == "rangeProps.max") {
        auto supported = expect_node_type(path, {NodeType::Slider, NodeType::ProgressBar, NodeType::Arc});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::RangePropsMax, PropsApplyMask::RangeRange);
    }
    if (path == "rangeProps.step") {
        auto supported = expect_node_type(path, {NodeType::Slider, NodeType::ProgressBar, NodeType::Arc});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::RangePropsStep, PropsApplyMask::RangeRange);
    }

    if (path == "toggleProps.checked") {
        auto supported = expect_node_type(path, {NodeType::Switch, NodeType::Checkbox});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TogglePropsChecked, PropsApplyMask::ToggleChecked);
    }

    if (path == "dropdownProps.options") {
        auto supported = expect_node_type(path, {NodeType::Dropdown});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::DropdownPropsOptions, PropsApplyMask::DropdownOptions);
    }
    if (path == "dropdownProps.selectedIndex") {
        auto supported = expect_node_type(path, {NodeType::Dropdown});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::DropdownPropsSelectedIndex, PropsApplyMask::DropdownSelectedIndex);
    }

    if (path == "tableProps.rows") {
        auto supported = expect_node_type(path, {NodeType::Table});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TablePropsRows, PropsApplyMask::TableRows);
    }
    if (path == "tableProps.columns") {
        auto supported = expect_node_type(path, {NodeType::Table});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TablePropsColumns, PropsApplyMask::TableColumns);
    }
    if (path == "tableProps.cells") {
        auto supported = expect_node_type(path, {NodeType::Table});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::TablePropsCells, PropsApplyMask::TableCells);
    }

    if (path == "lineProps.points") {
        auto supported = expect_node_type(path, {NodeType::Line});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::LinePropsPoints, PropsApplyMask::LinePoints);
    }

    if (path == "keyboardProps.mode") {
        auto supported = expect_node_type(path, {NodeType::Keyboard});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::KeyboardPropsMode, PropsApplyMask::KeyboardMode);
    }
    if (path == "keyboardProps.popovers") {
        auto supported = expect_node_type(path, {NodeType::Keyboard});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::KeyboardPropsPopovers, PropsApplyMask::KeyboardPopovers);
    }
    if (path == "keyboardProps.allowedModes") {
        auto supported = expect_node_type(path, {NodeType::Keyboard});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::KeyboardPropsAllowedModes, PropsApplyMask::KeyboardConfig);
    }
    if (path == "keyboardProps.iconSize") {
        auto supported = expect_node_type(path, {NodeType::Keyboard});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::KeyboardPropsIconSize, PropsApplyMask::KeyboardConfig);
    }

    if (path == "canvasProps.commands") {
        auto supported = expect_node_type(path, {NodeType::Canvas});
        if (!supported) {
            return std::unexpected(supported.error());
        }
        return make_props(BindingTarget::CanvasPropsCommands, PropsApplyMask::CanvasCommands);
    }

    if (path == "style.bgColor") {
        return make_style(BindingTarget::StyleBgColor, StyleApplyMask::Color);
    }
    if (path == "style.bgGradientColor") {
        return make_style(BindingTarget::StyleBgGradientColor, StyleApplyMask::All);
    }
    if (path == "style.bgGradientDirection") {
        return make_style(BindingTarget::StyleBgGradientDirection, StyleApplyMask::All);
    }
    if (path == "style.textColor") {
        return make_style(BindingTarget::StyleTextColor, StyleApplyMask::Color);
    }
    if (path == "style.borderColor") {
        return make_style(BindingTarget::StyleBorderColor, StyleApplyMask::Color);
    }
    if (path == "style.lineColor") {
        return make_style(BindingTarget::StyleLineColor, StyleApplyMask::Color);
    }
    if (path == "style.arcColor") {
        return make_style(BindingTarget::StyleArcColor, StyleApplyMask::All);
    }
    if (path == "style.arcGradientColor") {
        return make_style(BindingTarget::StyleArcGradientColor, StyleApplyMask::All);
    }
    if (path == "style.font") {
        return make_style(BindingTarget::StyleFont, StyleApplyMask::Font);
    }
    if (path == "style.bgMainStop") {
        return make_style(BindingTarget::StyleBgMainStop, StyleApplyMask::All);
    }
    if (path == "style.bgGradientStop") {
        return make_style(BindingTarget::StyleBgGradientStop, StyleApplyMask::All);
    }
    if (path == "style.bgGradientOpacity") {
        return make_style(BindingTarget::StyleBgGradientOpacity, StyleApplyMask::All);
    }
    if (path == "style.borderWidth") {
        return make_style(BindingTarget::StyleBorderWidth, StyleApplyMask::Border);
    }
    if (path == "style.radius") {
        return make_style(BindingTarget::StyleRadius, StyleApplyMask::Radius);
    }
    if (path == "style.padding") {
        return make_style(BindingTarget::StylePadding, StyleApplyMask::Padding);
    }
    if (path == "style.paddingLeft") {
        return make_style(BindingTarget::StylePaddingLeft, StyleApplyMask::Padding);
    }
    if (path == "style.paddingRight") {
        return make_style(BindingTarget::StylePaddingRight, StyleApplyMask::Padding);
    }
    if (path == "style.paddingTop") {
        return make_style(BindingTarget::StylePaddingTop, StyleApplyMask::Padding);
    }
    if (path == "style.paddingBottom") {
        return make_style(BindingTarget::StylePaddingBottom, StyleApplyMask::Padding);
    }
    if (path == "style.margin") {
        return make_style(BindingTarget::StyleMargin, StyleApplyMask::Margin);
    }
    if (path == "style.marginLeft") {
        return make_style(BindingTarget::StyleMarginLeft, StyleApplyMask::Margin);
    }
    if (path == "style.marginRight") {
        return make_style(BindingTarget::StyleMarginRight, StyleApplyMask::Margin);
    }
    if (path == "style.marginTop") {
        return make_style(BindingTarget::StyleMarginTop, StyleApplyMask::Margin);
    }
    if (path == "style.marginBottom") {
        return make_style(BindingTarget::StyleMarginBottom, StyleApplyMask::Margin);
    }
    if (path == "style.shadowWidth") {
        return make_style(BindingTarget::StyleShadowWidth, StyleApplyMask::Shadow);
    }
    if (path == "style.shadowOffsetX") {
        return make_style(BindingTarget::StyleShadowOffsetX, StyleApplyMask::Shadow);
    }
    if (path == "style.shadowOffsetY") {
        return make_style(BindingTarget::StyleShadowOffsetY, StyleApplyMask::Shadow);
    }
    if (path == "style.shadowColor") {
        return make_style(BindingTarget::StyleShadowColor, StyleApplyMask::Shadow | StyleApplyMask::Color);
    }
    if (path == "style.opacity") {
        return make_style(BindingTarget::StyleOpacity, StyleApplyMask::Opacity);
    }
    if (path == "style.lineWidth") {
        return make_style(BindingTarget::StyleLineWidth, StyleApplyMask::Line);
    }
    if (path == "style.imageOpacity") {
        return make_style(BindingTarget::StyleImageOpacity, StyleApplyMask::ImageOpacity);
    }
    if (path == "style.imageRecolor") {
        return make_style(BindingTarget::StyleImageRecolor, StyleApplyMask::ImageRecolor);
    }
    if (path == "style.imageRecolorOpacity") {
        return make_style(BindingTarget::StyleImageRecolorOpacity, StyleApplyMask::ImageRecolor);
    }
    if (path == "style.fontSize") {
        return make_style(BindingTarget::StyleFontSize, StyleApplyMask::Font);
    }
    if (path == "style.imageFontSize") {
        return make_style(BindingTarget::StyleImageFontSize, StyleApplyMask::Font);
    }
    if (path == "style.arcWidth") {
        return make_style(BindingTarget::StyleArcWidth, StyleApplyMask::All);
    }
    if (path == "style.arcOpacity") {
        return make_style(BindingTarget::StyleArcOpacity, StyleApplyMask::All);
    }
    if (path == "style.arcGradientSegments") {
        return make_style(BindingTarget::StyleArcGradientSegments, StyleApplyMask::All);
    }
    if (path == "style.arcRounded") {
        return make_style(BindingTarget::StyleArcRounded, StyleApplyMask::All);
    }

    if (path == "layout.type") {
        return make_layout(BindingTarget::LayoutType, LayoutApplyMask::All);
    }
    if (path == "layout.flexFlow") {
        return make_layout(BindingTarget::LayoutFlexFlow, LayoutApplyMask::FlexFlow);
    }
    if (path == "layout.mainAlign") {
        return make_layout(BindingTarget::LayoutMainAlign, LayoutApplyMask::Align);
    }
    if (path == "layout.crossAlign") {
        return make_layout(BindingTarget::LayoutCrossAlign, LayoutApplyMask::Align);
    }
    if (path == "layout.gap") {
        return make_layout(BindingTarget::LayoutGap, LayoutApplyMask::Gap);
    }
    if (path == "layout.gridTemplateColumns") {
        return make_layout(BindingTarget::LayoutGridTemplateColumns, LayoutApplyMask::GridTracks);
    }
    if (path == "layout.gridTemplateRows") {
        return make_layout(BindingTarget::LayoutGridTemplateRows, LayoutApplyMask::GridTracks);
    }

    if (path == "placement.mode") {
        return make_placement(BindingTarget::PlacementMode, PlacementApplyMask::All);
    }
    if (path == "placement.width") {
        return make_placement(BindingTarget::PlacementWidth, PlacementApplyMask::Size);
    }
    if (path == "placement.height") {
        return make_placement(BindingTarget::PlacementHeight, PlacementApplyMask::Size);
    }
    if (path == "placement.aspectRatio") {
        return make_placement(BindingTarget::PlacementAspectRatio, PlacementApplyMask::Size);
    }
    if (path == "placement.x") {
        return make_placement(BindingTarget::PlacementX, PlacementApplyMask::Position);
    }
    if (path == "placement.y") {
        return make_placement(BindingTarget::PlacementY, PlacementApplyMask::Position);
    }
    if (path == "placement.align") {
        return make_placement(BindingTarget::PlacementAlign, PlacementApplyMask::Align);
    }
    if (path == "placement.relativeTo") {
        return make_placement(BindingTarget::PlacementRelativeTo, PlacementApplyMask::RelativeTo);
    }
    if (path == "placement.gridColumn") {
        return make_placement(BindingTarget::PlacementGridColumn, PlacementApplyMask::GridCell);
    }
    if (path == "placement.gridRow") {
        return make_placement(BindingTarget::PlacementGridRow, PlacementApplyMask::GridCell);
    }
    if (path == "placement.gridColumnSpan") {
        return make_placement(BindingTarget::PlacementGridColumnSpan, PlacementApplyMask::GridCell);
    }
    if (path == "placement.gridRowSpan") {
        return make_placement(BindingTarget::PlacementGridRowSpan, PlacementApplyMask::GridCell);
    }
    if (path == "placement.alignSelf") {
        return make_placement(BindingTarget::PlacementAlignSelf, PlacementApplyMask::AlignSelf);
    }
    if (path == "placement.flexGrow") {
        return make_placement(BindingTarget::PlacementFlexGrow, PlacementApplyMask::FlexGrow);
    }

    return std::unexpected("Unsupported binding path: " + std::string(path));
}

} // namespace esp_brookesia::gui
