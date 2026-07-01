/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/gui_interface/document.hpp"
#include "brookesia/gui_interface/event.hpp"
#include "brookesia/gui_interface/handles.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#include "brookesia/gui_interface/scoped_connection.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::gui {

struct BackendEvent {
    BackendHandle handle;
    EventType type = EventType::Clicked;
    std::string action;
    boost::json::object payload;
};

BROOKESIA_DESCRIBE_STRUCT(BackendEvent, (), (handle, type, action, payload))

struct BackendAnimationStartResult {
    ScopedConnection connection;
    int32_t resolved_from = 0;
    int32_t resolved_to = 0;
};

enum class PropsApplyMask : uint64_t {
    None = 0,
    CommonHidden = 1ULL << 0,
    CommonDisabled = 1ULL << 1,
    CommonClickable = 1ULL << 2,
    CommonScrollable = 1ULL << 3,
    CommonTransform = 1ULL << 4,
    LabelText = 1ULL << 5,
    ImageSource = 1ULL << 6,
    ImageInnerAlign = 1ULL << 7,
    ImageAngle = 1ULL << 8,
    ImageOffsetX = 1ULL << 9,
    ImageOffsetY = 1ULL << 10,
    ImageZoom = 1ULL << 11,
    ImagePivot = 1ULL << 12,
    TextInputText = 1ULL << 13,
    TextInputPlaceholder = 1ULL << 14,
    TextInputPassword = 1ULL << 15,
    TextInputMultiline = 1ULL << 16,
    TextInputMaxLength = 1ULL << 17,
    RangeValue = 1ULL << 18,
    RangeRange = 1ULL << 19,
    ToggleChecked = 1ULL << 20,
    DropdownOptions = 1ULL << 21,
    DropdownSelectedIndex = 1ULL << 22,
    TableRows = 1ULL << 23,
    TableColumns = 1ULL << 24,
    TableCells = 1ULL << 25,
    LinePoints = 1ULL << 26,
    KeyboardMode = 1ULL << 27,
    KeyboardPopovers = 1ULL << 28,
    CanvasCommands = 1ULL << 29,
    CommonPressLock = 1ULL << 30,
    KeyboardConfig = 1ULL << 31,
    ImageRecolor = 1ULL << 32,
    FrameViewConfig = 1ULL << 33,
    All = 0xFFFFFFFFFFFFFFFFULL,
};

enum class StyleApplyMask : uint32_t {
    None = 0,
    Color = 1U << 0,
    Font = 1U << 1,
    Border = 1U << 2,
    Radius = 1U << 3,
    Padding = 1U << 4,
    Margin = 1U << 5,
    Shadow = 1U << 6,
    Opacity = 1U << 7,
    Line = 1U << 8,
    ImageOpacity = 1U << 9,
    ImageRecolor = 1U << 10,
    All = 0xFFFFFFFFU,
};

enum class LayoutApplyMask : uint32_t {
    None = 0,
    Type = 1U << 0,
    FlexFlow = 1U << 1,
    Align = 1U << 2,
    Gap = 1U << 3,
    GridTracks = 1U << 4,
    All = 0xFFFFFFFFU,
};

enum class PlacementApplyMask : uint32_t {
    None = 0,
    Mode = 1U << 0,
    Size = 1U << 1,
    Position = 1U << 2,
    Align = 1U << 3,
    RelativeTo = 1U << 4,
    GridCell = 1U << 5,
    AlignSelf = 1U << 6,
    FlexGrow = 1U << 7,
    All = 0xFFFFFFFFU,
};

constexpr PropsApplyMask operator|(PropsApplyMask lhs, PropsApplyMask rhs)
{
    return static_cast<PropsApplyMask>(static_cast<uint64_t>(lhs) | static_cast<uint64_t>(rhs));
}

constexpr StyleApplyMask operator|(StyleApplyMask lhs, StyleApplyMask rhs)
{
    return static_cast<StyleApplyMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr LayoutApplyMask operator|(LayoutApplyMask lhs, LayoutApplyMask rhs)
{
    return static_cast<LayoutApplyMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr PlacementApplyMask operator|(PlacementApplyMask lhs, PlacementApplyMask rhs)
{
    return static_cast<PlacementApplyMask>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

constexpr bool has_mask(PropsApplyMask mask, PropsApplyMask value)
{
    return (static_cast<uint64_t>(mask) & static_cast<uint64_t>(value)) != 0;
}

constexpr bool has_mask(StyleApplyMask mask, StyleApplyMask value)
{
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(value)) != 0;
}

constexpr bool has_mask(LayoutApplyMask mask, LayoutApplyMask value)
{
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(value)) != 0;
}

constexpr bool has_mask(PlacementApplyMask mask, PlacementApplyMask value)
{
    return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(value)) != 0;
}

class IBackend {
public:
    using EventSink = std::function<void(const BackendEvent &)>;
    using ThreadGuardCallback = std::function<void()>;

    struct ThreadGuard {
        ThreadGuardCallback lock;
        ThreadGuardCallback unlock;
    };

    IBackend() = default;
    IBackend(const IBackend &) = delete;
    IBackend &operator=(const IBackend &) = delete;
    virtual ~IBackend() = default;

    virtual void set_event_sink(EventSink sink) = 0;
    virtual std::optional<ThreadGuard> get_thread_guard() const
    {
        return std::nullopt;
    }
    virtual BackendHandle create_node(
        const Node &node,
        BackendHandle parent,
        std::string_view parent_path,
        std::string_view scope_root_absolute_path
    ) = 0;
    virtual void destroy_node(BackendHandle handle) = 0;
    virtual void apply_props(
        BackendHandle handle, const Node &node, PropsApplyMask mask = PropsApplyMask::All
    ) = 0;
    virtual void apply_layout(
        BackendHandle handle, const Layout &layout, LayoutApplyMask mask = LayoutApplyMask::All
    ) = 0;
    virtual void apply_placement(
        BackendHandle handle, const Placement &placement, PlacementApplyMask mask = PlacementApplyMask::All
    ) = 0;
    virtual void apply_style(
        BackendHandle handle, const ResolvedStyle &style, StyleApplyMask mask = StyleApplyMask::All
    ) = 0;
    virtual void apply_debug_visual(BackendHandle handle, bool enabled) = 0;
    virtual void apply_animations(BackendHandle handle, const std::vector<Animation> &animations) = 0;
    virtual std::optional<BackendAnimationStartResult> start_animation(
        BackendHandle handle,
        const Animation &animation,
        std::function<void()> completed_handler = {}) = 0;
    virtual void bind_events(BackendHandle handle, const std::vector<EventBinding> &events) = 0;
    virtual std::vector<GuiDisplayInfo> list_displays() const = 0;
    virtual std::vector<GuiLayer> list_layers() const = 0;
    virtual bool mount_screen(BackendHandle handle, const MountTarget &target) = 0;
    virtual bool unmount_screen(BackendHandle handle) = 0;
    virtual bool register_font_resource(const RuntimeFontResource &resource) = 0;
    virtual std::vector<RuntimeFontResource> list_font_resources() const = 0;
    virtual std::expected<RuntimeImageResource, std::string> resolve_image_resource(
        RuntimeImageResource resource
    ) const
    {
        return resource;
    }
    virtual bool requires_preloaded_image_resource(const RuntimeImageResource &resource) const
    {
        (void)resource;
        return false;
    }
    virtual std::expected<void, std::string> preload_image_resource(const RuntimeImageResource &resource) = 0;
    virtual void release_image_resource(const RuntimeImageResource &resource) = 0;
    virtual void process_timers() {}
    virtual std::optional<ViewFrame> get_node_frame(BackendHandle handle) const = 0;
    virtual bool scroll_node_to_visible(BackendHandle handle, bool animated) = 0;
};

} // namespace esp_brookesia::gui
