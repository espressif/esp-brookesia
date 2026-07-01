/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/gui_interface/macro_configs.h"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::gui {

enum class NodeType {
    Screen,
    Container,
    Label,
    Button,
    Image,
    FrameView,
    TextInput,
    Slider,
    Switch,
    Checkbox,
    Dropdown,
    ProgressBar,
    Spinner,
    Arc,
    Line,
    Table,
    Keyboard,
    Canvas,
    Max,
};

enum class FrameColorFormat {
    RGB565,
    RGB888,
    Max,
};

enum class LayoutType {
    None,
    Flex,
    Grid,
    Max,
};

enum class PlacementMode {
    Flow,
    Absolute,
    Relative,
    Max,
};

enum class SizeMode {
    Fixed,
    Match,
    Wrap,
    Percent,
    Max,
};

enum class PlacementOffsetMode {
    Fixed,
    Percent,
    Max,
};

enum class FlexFlow {
    Row,
    Column,
    RowWrap,
    ColumnWrap,
    Max,
};

enum class Align {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
    SpaceEvenly,
    Stretch,
    Max,
};

enum class TextAlign {
    Auto,
    Left,
    Center,
    Right,
    Max,
};

enum class PlacementAlign {
    TopLeft,
    TopMid,
    TopRight,
    BottomLeft,
    BottomMid,
    BottomRight,
    LeftMid,
    RightMid,
    Center,
    OutTopLeft,
    OutTopMid,
    OutTopRight,
    OutBottomLeft,
    OutBottomMid,
    OutBottomRight,
    OutLeftTop,
    OutLeftMid,
    OutLeftBottom,
    OutRightTop,
    OutRightMid,
    OutRightBottom,
    Max,
};

enum class ViewStateKind {
    CommonProps,
    TypedProps,
    Layout,
    Placement,
    Style,
    ResolvedFont,
    ResolvedImage,
    Frame,
    Max,
};

enum class GuiLayer {
    Default,
    Top,
    System,
    Bottom,
    Max,
};

enum class MountStackMode {
    Replace,
    Stack,
    Max,
};

enum class EventType {
    Pressed,
    Pressing,
    PressLost,
    Released,
    Clicked,
    LongPressed,
    LongPressedRepeat,
    Focused,
    Defocused,
    ValueChanged,
    Ready,
    Cancel,
    Scroll,
    Gesture,
    Max,
};

enum class EventEffectType {
    EmitAction,
    SetProperty,
    SetProperties,
    StartAnimation,
    StopAnimation,
    Max,
};

enum class AnimationTrigger {
    Manual,
    Mount,
    Show,
    Hide,
    Max,
};

enum class AnimationProperty {
    Opacity,
    X,
    Y,
    Width,
    Height,
    Scale,
    Angle,
    OffsetX,
    OffsetY,
    Max,
};

enum class AnimationEasing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut,
    Overshoot,
    Bounce,
    Step,
    Max,
};

enum class AnimationValueMode {
    Absolute,
    Current,
    Relative,
    Max,
};

enum class MountMode {
    Eager,
    Dynamic,
    Max,
};

BROOKESIA_DESCRIBE_ENUM(
    NodeType, Screen, Container, Label, Button, Image, FrameView, TextInput, Slider, Switch, Checkbox, Dropdown,
    ProgressBar, Spinner, Arc, Line, Table, Keyboard, Canvas, Max
)
BROOKESIA_DESCRIBE_ENUM(FrameColorFormat, RGB565, RGB888, Max)
BROOKESIA_DESCRIBE_ENUM(LayoutType, None, Flex, Grid, Max)
BROOKESIA_DESCRIBE_ENUM(PlacementMode, Flow, Absolute, Relative, Max)
BROOKESIA_DESCRIBE_ENUM(SizeMode, Fixed, Match, Wrap, Percent, Max)
BROOKESIA_DESCRIBE_ENUM(PlacementOffsetMode, Fixed, Percent, Max)
BROOKESIA_DESCRIBE_ENUM(FlexFlow, Row, Column, RowWrap, ColumnWrap, Max)
BROOKESIA_DESCRIBE_ENUM(Align, Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly, Stretch, Max)
BROOKESIA_DESCRIBE_ENUM(TextAlign, Auto, Left, Center, Right, Max)
BROOKESIA_DESCRIBE_ENUM(
    PlacementAlign, TopLeft, TopMid, TopRight, BottomLeft, BottomMid, BottomRight, LeftMid, RightMid, Center,
    OutTopLeft, OutTopMid, OutTopRight, OutBottomLeft, OutBottomMid, OutBottomRight, OutLeftTop, OutLeftMid,
    OutLeftBottom, OutRightTop, OutRightMid, OutRightBottom, Max
)
BROOKESIA_DESCRIBE_ENUM(
    ViewStateKind, CommonProps, TypedProps, Layout, Placement, Style, ResolvedFont, ResolvedImage, Frame, Max
)
BROOKESIA_DESCRIBE_ENUM(GuiLayer, Default, Top, System, Bottom, Max)
BROOKESIA_DESCRIBE_ENUM(MountStackMode, Replace, Stack, Max)
BROOKESIA_DESCRIBE_ENUM(
    EventType, Pressed, Pressing, PressLost, Released, Clicked, LongPressed, LongPressedRepeat, Focused, Defocused,
    ValueChanged, Ready, Cancel, Scroll, Gesture, Max
)
BROOKESIA_DESCRIBE_ENUM(EventEffectType, EmitAction, SetProperty, SetProperties, StartAnimation, StopAnimation, Max)
BROOKESIA_DESCRIBE_ENUM(MountMode, Eager, Dynamic, Max)
BROOKESIA_DESCRIBE_ENUM(AnimationTrigger, Manual, Mount, Show, Hide, Max)
BROOKESIA_DESCRIBE_ENUM(AnimationProperty, Opacity, X, Y, Width, Height, Scale, Angle, OffsetX, OffsetY, Max)
BROOKESIA_DESCRIBE_ENUM(AnimationEasing, Linear, EaseIn, EaseOut, EaseInOut, Overshoot, Bounce, Step, Max)
BROOKESIA_DESCRIBE_ENUM(AnimationValueMode, Absolute, Current, Relative, Max)

} // namespace esp_brookesia::gui
