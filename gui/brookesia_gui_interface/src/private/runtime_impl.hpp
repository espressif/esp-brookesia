#pragma once

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_interface/runtime.hpp"
#include "brookesia/gui_interface/data_store.hpp"
#include "brookesia/gui_interface/parser.hpp"
#include "brookesia/gui_interface/validator.hpp"
#include "private/binding.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
#include "brookesia/lib_utils/memory_profiler.hpp"
#endif
#if !BROOKESIA_GUI_INTERFACE_RUNTIME_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <algorithm>
#include <atomic>
#include <array>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iterator>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <unordered_map>
#include <system_error>
#include <utility>
#include <vector>

#include "brookesia/lib_utils/signal.hpp"
#include "boost/json.hpp"
#include "boost/unordered/unordered_flat_map.hpp"
#include "boost/unordered/unordered_flat_set.hpp"
#include "boost/unordered/unordered_node_map.hpp"
#include "brookesia/service_helper/system/storage.hpp"

#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
#   define GUI_INTERFACE_PROFILE_LOGI(...) BROOKESIA_LOGI(__VA_ARGS__)
#else
#   define GUI_INTERFACE_PROFILE_LOGI(...) do { if (false) { BROOKESIA_LOGI(__VA_ARGS__); } } while (0)
#endif

namespace esp_brookesia::gui {

namespace {

using StorageHelper = service::helper::Storage;
using RuntimeProfileClock = std::chrono::steady_clock;

static int64_t runtime_profile_elapsed_ms(
    const RuntimeProfileClock::time_point &start,
    const RuntimeProfileClock::time_point &end)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
}

[[maybe_unused]] static int64_t runtime_profile_elapsed_us(
    const RuntimeProfileClock::time_point &start,
    const RuntimeProfileClock::time_point &end)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
}

static double runtime_profile_us_to_ms(int64_t value_us)
{
    return static_cast<double>(value_us) / 1000.0;
}

static std::string normalize_file_path(std::string_view file_path)
{
    return std::filesystem::path(file_path).lexically_normal().string();
}

static bool is_fast_action_event_type(EventType type)
{
    switch (type) {
    case EventType::Pressed:
    case EventType::Released:
    case EventType::Clicked:
    case EventType::LongPressed:
    case EventType::LongPressedRepeat:
        return true;
    case EventType::Pressing:
    case EventType::Focused:
    case EventType::Defocused:
    case EventType::ValueChanged:
    case EventType::Ready:
    case EventType::Cancel:
    case EventType::Scroll:
    case EventType::Gesture:
    case EventType::Max:
    default:
        return false;
    }
}

static bool is_fast_action_backend_event(const BackendEvent &event)
{
    return is_fast_action_event_type(event.type) && !event.action.empty() && event.payload.empty();
}

static std::string build_fast_action_route_key(
    BackendHandle handle,
    EventType type,
    std::string_view action
)
{
    std::string key = std::to_string(handle.value());
    key.push_back('\x1f');
    key.append(std::to_string(static_cast<int>(type)));
    key.push_back('\x1f');
    key.append(action);
    return key;
}

static std::string path_to_string(const Path &path)
{
    std::ostringstream oss;
    oss << path;
    return oss.str();
}

static Path append_path(const Path &base, std::string_view segment)
{
    auto segments = base.segments();
    if (!segment.empty()) {
        segments.emplace_back(segment);
    }
    return Path(std::move(segments));
}

static Node clone_node_without_children(const Node &node)
{
    return Node{
        .type = node.type,
        .mount_mode = node.mount_mode,
        .id = node.id,
        .common_props = node.common_props,
        .label_props = node.label_props,
        .image_props = node.image_props,
        .frame_view_props = node.frame_view_props,
        .text_input_props = node.text_input_props,
        .range_props = node.range_props,
        .toggle_props = node.toggle_props,
        .dropdown_props = node.dropdown_props,
        .table_props = node.table_props,
        .line_props = node.line_props,
        .keyboard_props = node.keyboard_props,
        .canvas_props = node.canvas_props,
        .layout = node.layout,
        .placement = node.placement,
        .style = node.style,
        .state_styles = node.state_styles,
        .part_styles = node.part_styles,
        .style_refs = node.style_refs,
        .resolved_image = node.resolved_image,
        .events = node.events,
        .animations = node.animations,
        .bindings = node.bindings,
        .children = {},
    };
}

static bool node_has_style_binding(const Node &node)
{
    for (const auto &[path, unused_expression] : node.bindings) {
        (void)unused_expression;
        if (path.starts_with("style.") || path.starts_with("stateStyles.") ||
                path.starts_with("partStyles.")) {
            return true;
        }
    }
    return false;
}

static std::string trim_slashes(std::string_view path)
{
    size_t begin = 0;
    size_t end = path.size();
    while (begin < end && path[begin] == '/') {
        ++begin;
    }
    while (end > begin && path[end - 1] == '/') {
        --end;
    }
    return std::string(path.substr(begin, end - begin));
}

static std::string normalize_absolute_path(std::string_view path)
{
    const auto trimmed = trim_slashes(path);
    if (trimmed.empty()) {
        return "/";
    }

    return "/" + trimmed;
}

static bool is_normalized_absolute_path(std::string_view path)
{
    return path.size() > 1 && path.front() == '/' && path.back() != '/';
}

static std::string absolute_node_path_to_string(std::string_view root_id, const Path &path)
{
    std::string result = "/";
    result.append(root_id);

    const auto relative_path = path_to_string(path);
    if (!relative_path.empty()) {
        result.push_back('/');
        result.append(relative_path);
    }

    return result;
}

static std::string parent_absolute_path_to_string(std::string_view root_id, const Path &current_path)
{
    if (current_path.empty()) {
        return "/";
    }

    std::string result = "/";
    result.append(root_id);
    result.push_back('/');

    const auto &segments = current_path.segments();
    for (size_t i = 0; i + 1 < segments.size(); ++i) {
        result.append(segments[i]);
        result.push_back('/');
    }

    return result;
}

static std::string build_event_action_route_key(DocumentId document_id, std::string_view action)
{
    std::ostringstream oss;
    oss << document_id.value() << '\x1f' << action;
    return oss.str();
}

static std::string build_event_action_route_prefix(DocumentId document_id)
{
    std::ostringstream oss;
    oss << document_id.value() << '\x1f';
    return oss.str();
}

static std::expected<int32_t, std::string> parse_int_from_store_string(std::string_view value)
{
    int32_t result = 0;
    const auto begin = value.data();
    const auto end = value.data() + value.size();
    const auto [ptr, error_code] = std::from_chars(begin, end, result);
    if (error_code != std::errc() || ptr != end) {
        return std::unexpected("expected integer");
    }
    return result;
}

static std::expected<bool, std::string> parse_bool_from_store_string(std::string_view value)
{
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    return std::unexpected("expected 'true' or 'false'");
}

static bool is_valid_color(std::string_view color)
{
    if (color.empty()) {
        return true;
    }
    if (color.size() != 7 || color.front() != '#') {
        return false;
    }
    return std::all_of(color.begin() + 1, color.end(), [](char value) {
        return std::isxdigit(static_cast<unsigned char>(value)) != 0;
    });
}

static std::string trim_store_token(std::string_view value)
{
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
        value.remove_prefix(1);
    }
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
        value.remove_suffix(1);
    }
    return std::string(value);
}

static bool is_supported_keyboard_mode(std::string_view mode)
{
    return mode == "text" || mode == "upper" || mode == "number" || mode == "special";
}

static std::expected<std::vector<std::string>, std::string> parse_keyboard_modes_from_store_string(
    std::string_view value)
{
    if (value.empty()) {
        return std::unexpected("keyboard allowed modes must not be empty");
    }

    std::vector<std::string> modes;
    std::unordered_map<std::string, bool> seen_modes;
    while (true) {
        const auto separator = value.find(',');
        auto token = trim_store_token(value.substr(0, separator));
        if (token.empty()) {
            return std::unexpected("keyboard allowed modes contains an empty item");
        }
        if (!is_supported_keyboard_mode(token)) {
            return std::unexpected("unsupported keyboard mode: " + token);
        }
        if (seen_modes.contains(token)) {
            return std::unexpected("duplicate keyboard mode: " + token);
        }
        seen_modes.emplace(token, true);
        modes.push_back(std::move(token));
        if (separator == std::string_view::npos) {
            break;
        }
        value.remove_prefix(separator + 1);
    }
    return modes;
}

static std::expected<PivotValue, std::string> parse_pivot_from_store_string(
    std::string_view value,
    std::string_view field_name)
{
    if (value.empty()) {
        return std::unexpected("must not be empty");
    }
    if (value.ends_with("%")) {
        std::string owned(value.substr(0, value.size() - 1));
        char *parse_end = nullptr;
        errno = 0;
        const float parsed = std::strtof(owned.c_str(), &parse_end);
        if (errno != 0 || parse_end == owned.c_str() || *parse_end != '\0') {
            return std::unexpected("expected numeric percent value for field '" + std::string(field_name) + "'");
        }
        return PivotValue{
            .percent = true,
            .value = static_cast<int32_t>(std::lround(parsed)),
        };
    }

    auto integer_value = parse_int_from_store_string(value);
    if (!integer_value) {
        return std::unexpected("field '" + std::string(field_name) + "' must use integer px or percent value");
    }
    return PivotValue{
        .percent = false,
        .value = *integer_value,
    };
}

static std::expected<int32_t, std::string> parse_scaled_from_store_string(
    std::string_view value,
    std::string_view field_name,
    std::string_view unit,
    float scale)
{
    if (value.empty()) {
        return std::unexpected("must not be empty");
    }

    auto parse_numeric = [&](std::string_view text) -> std::expected<int32_t, std::string> {
        std::string owned(text);
        char *parse_end = nullptr;
        errno = 0;
        const float parsed = std::strtof(owned.c_str(), &parse_end);
        if (errno != 0 || parse_end == owned.c_str() || *parse_end != '\0')
        {
            return std::unexpected("expected numeric value for field '" + std::string(field_name) + "'");
        }
        return static_cast<int32_t>(std::lround(parsed * scale));
    };

    if (!unit.empty() && value.ends_with(unit)) {
        return parse_numeric(value.substr(0, value.size() - unit.size()));
    }

    auto integer_value = parse_int_from_store_string(value);
    if (!integer_value) {
        return std::unexpected(
                   "field '" + std::string(field_name) + "' must use " + std::string(unit) +
                   " units or an integer px value"
               );
    }
    return *integer_value;
}

static std::expected<Dimension, std::string> parse_dimension_from_store_string(
    std::string_view value,
    const Environment &environment)
{
    if (value == "match") {
        return Dimension{.mode = SizeMode::Match, .value = 0};
    }
    if (value == "wrap") {
        return Dimension{.mode = SizeMode::Wrap, .value = 0};
    }
    if (value.ends_with("dp")) {
        auto fixed = parse_scaled_from_store_string(value, "dimension", "dp", environment.density);
        if (!fixed) {
            return std::unexpected(fixed.error());
        }
        return Dimension{.mode = SizeMode::Fixed, .value = *fixed};
    }
    if (value.ends_with("%")) {
        auto percent = parse_scaled_from_store_string(value, "dimension", "%", 1.0F);
        if (!percent) {
            return std::unexpected(percent.error());
        }
        if (*percent < 0) {
            return std::unexpected("dimension percent value must be >= 0");
        }
        return Dimension{.mode = SizeMode::Percent, .value = *percent};
    }

    auto integer_value = parse_int_from_store_string(value);
    if (!integer_value) {
        return std::unexpected("dimension must be match/wrap, dp, percent, or integer px");
    }
    return Dimension{.mode = SizeMode::Fixed, .value = *integer_value};
}

static std::expected<PlacementOffset, std::string> parse_placement_offset_from_store_string(
    std::string_view value,
    std::string_view field_name,
    const Environment &environment)
{
    if (value.ends_with("%")) {
        auto percent = parse_scaled_from_store_string(value, field_name, "%", 1.0F);
        if (!percent) {
            return std::unexpected(percent.error());
        }
        if (*percent < 0) {
            return std::unexpected(std::string(field_name) + " percent value must be >= 0");
        }
        PlacementOffset offset;
        offset.mode = PlacementOffsetMode::Percent;
        offset.value = *percent;
        return offset;
    }

    auto fixed = parse_scaled_from_store_string(value, field_name, "dp", environment.density);
    if (!fixed) {
        return std::unexpected(fixed.error());
    }
    return PlacementOffset(*fixed);
}

static std::expected<float, std::string> parse_positive_float_from_store_string(
    std::string_view value,
    std::string_view field_name)
{
    std::string owned(value);
    char *parse_end = nullptr;
    errno = 0;
    const float parsed = std::strtof(owned.c_str(), &parse_end);
    if (errno != 0 || parse_end == owned.c_str() || *parse_end != '\0' || !std::isfinite(parsed) ||
            parsed <= 0.0F) {
        return std::unexpected("field '" + std::string(field_name) + "' must be a positive number");
    }
    return parsed;
}

static std::expected<float, std::string> parse_aspect_ratio_from_store_string(std::string_view value)
{
    const auto separator = value.find(':');
    if (separator == std::string_view::npos) {
        return parse_positive_float_from_store_string(value, "placement.aspectRatio");
    }

    auto width = parse_positive_float_from_store_string(value.substr(0, separator), "placement.aspectRatio");
    if (!width) {
        return std::unexpected(width.error());
    }
    auto height = parse_positive_float_from_store_string(value.substr(separator + 1), "placement.aspectRatio");
    if (!height) {
        return std::unexpected(height.error());
    }
    return *width / *height;
}

static std::expected<boost::json::value, std::string> parse_json_fragment(
    std::string_view value,
    std::string_view field_name)
{
    boost::system::error_code error_code;
    auto parsed = boost::json::parse(value, error_code);
    if (error_code) {
        return std::unexpected(
                   "field '" + std::string(field_name) + "' must use a JSON fragment: " + error_code.message()
               );
    }
    return parsed;
}

static std::expected<boost::json::value, std::string> get_json_value_by_dot_path(
    const boost::json::value &root,
    std::string_view path)
{
    if (path.empty()) {
        return std::unexpected("Constant path must not be empty");
    }

    const boost::json::value *current = &root;
    size_t begin = 0;
    while (begin <= path.size()) {
        const size_t end = path.find('.', begin);
        const auto segment = path.substr(begin, end == std::string_view::npos ? path.size() - begin : end - begin);
        if (segment.empty()) {
            return std::unexpected("Constant path contains an empty segment: " + std::string(path));
        }
        if (!current->is_object()) {
            return std::unexpected("Constant path does not resolve to a value: " + std::string(path));
        }

        auto it = current->as_object().find(segment);
        if (it == current->as_object().end()) {
            return std::unexpected("Constant path not found: " + std::string(path));
        }
        current = &it->value();
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }

    return *current;
}

static std::string build_mount_layer_key(std::string_view display_id, GuiLayer layer)
{
    return std::string(display_id) + '\x1f' + BROOKESIA_DESCRIBE_ENUM_TO_STR(layer);
}

static std::string build_replace_mount_target_key(std::string_view display_id, GuiLayer layer)
{
    return build_mount_layer_key(display_id, layer) + "\x1freplace";
}

static std::string build_stack_mount_target_key(
    std::string_view display_id,
    GuiLayer layer,
    DocumentId document_id,
    std::string_view absolute_path)
{
    return build_mount_layer_key(display_id, layer) + "\x1fstack\x1f" +
           std::to_string(document_id.value()) + '\x1f' + std::string(absolute_path);
}

static bool same_mount_layer(const MountTarget &lhs, const MountTarget &rhs)
{
    return lhs.display_id == rhs.display_id && lhs.layer == rhs.layer;
}

static std::string build_screen_flow_key(DocumentId document_id, std::string_view flow_id)
{
    return std::to_string(document_id.value()) + '\x1f' + std::string(flow_id);
}

static const ScreenFlowTransition *find_screen_flow_transition(
    const ScreenFlow &flow,
    std::string_view current_state,
    std::string_view action)
{
    auto exact = std::find_if(
                     flow.transitions.begin(),
                     flow.transitions.end(),
    [current_state, action](const ScreenFlowTransition & transition) {
        return !transition.from.empty() && transition.action == action &&
               std::find(transition.from.begin(), transition.from.end(), current_state) != transition.from.end();
    }
                 );
    if (exact != flow.transitions.end()) {
        return &(*exact);
    }

    auto wildcard = std::find_if(
                        flow.transitions.begin(),
                        flow.transitions.end(),
    [action](const ScreenFlowTransition & transition) {
        return transition.from.empty() && transition.action == action;
    }
                    );
    return wildcard == flow.transitions.end() ? nullptr : &(*wildcard);
}

static std::string make_screen_flow_screen_path(std::string_view state_id)
{
    return "/" + std::string(state_id);
}

static bool screen_flow_contains_state(const ScreenFlow &flow, std::string_view state_id)
{
    return std::find(flow.screens.begin(), flow.screens.end(), state_id) != flow.screens.end();
}

static std::vector<std::string> normalize_dependency_files(std::vector<std::string> dependency_files)
{
    std::vector<std::string> normalized;
    normalized.reserve(dependency_files.size());
    boost::unordered_flat_set<std::string> seen;
    for (auto &file : dependency_files) {
        auto normalized_file = normalize_file_path(file);
        if (seen.insert(normalized_file).second) {
            normalized.push_back(std::move(normalized_file));
        }
    }
    return normalized;
}

static std::unordered_map<std::string, uint64_t> capture_dependency_mtimes(
    const std::vector<std::string> &dependency_files)
{
    std::unordered_map<std::string, uint64_t> mtimes;
    for (const auto &file : dependency_files) {
        auto file_info = StorageHelper::fs_stat(file);
        if (!file_info || !file_info->exists) {
            continue;
        }
        mtimes.insert_or_assign(file, file_info->mtime_ms);
    }
    return mtimes;
}

static std::string get_theme_style_key(NodeType type)
{
    switch (type) {
    case NodeType::Screen: return "screen";
    case NodeType::Container: return "container";
    case NodeType::Label: return "label";
    case NodeType::Button: return "button";
    case NodeType::Image: return "image";
    case NodeType::FrameView: return "frameView";
    case NodeType::TextInput: return "textInput";
    case NodeType::Slider: return "slider";
    case NodeType::Switch: return "switch";
    case NodeType::Checkbox: return "checkbox";
    case NodeType::Dropdown: return "dropdown";
    case NodeType::ProgressBar: return "progressBar";
    case NodeType::Spinner: return "spinner";
    case NodeType::Arc: return "arc";
    case NodeType::Line: return "line";
    case NodeType::Table: return "table";
    case NodeType::Keyboard: return "keyboard";
    case NodeType::Canvas: return "canvas";
    case NodeType::Max: return {};
    }
    return {};
}

static const ThemeAsset &get_builtin_default_theme()
{
    auto make_builtin_base_style = []() {
        Style style;
        style.set_border_width(0);
        style.set_radius(0);
        style.set_padding(0);
        style.set_margin(0);
        style.set_shadow_width(0);
        style.set_shadow_offset_x(0);
        style.set_shadow_offset_y(0);
        style.set_opacity(255);
        style.set_line_width(0);
        style.set_image_opacity(255);
        // Leave font unset here so Runtime can select the language default font, e.g. zh_CN -> kaiti.
        // Setting "default" in the built-in theme forces every node onto the English font.
        style.set_font_size(16);
        return style;
    };

    static const ThemeAsset theme = {
        .id = "__builtin_default_theme__",
        .colors = {},
        .styles = {
            {
                "all", StyleSet{.style = make_builtin_base_style(), .state_styles = {}, .part_styles = {}}
            },
            {"screen", StyleSet{}},
            {"container", StyleSet{}},
            {"label", StyleSet{}},
            {"button", StyleSet{}},
            {"dropdown", StyleSet{}},
        },
    };
    return theme;
}

static bool is_builtin_default_font_id(std::string_view font_id)
{
    return font_id == "default";
}

static std::optional<std::string> parse_color_reference_path(std::string_view value)
{
    constexpr std::string_view COLOR_PREFIX = "${color.";
    if (!value.starts_with(COLOR_PREFIX)) {
        return std::nullopt;
    }
    if (value.size() <= COLOR_PREFIX.size() + 1 || value.back() != '}') {
        return std::string();
    }
    return std::string(value.substr(COLOR_PREFIX.size(), value.size() - COLOR_PREFIX.size() - 1));
}

} // namespace

class Runtime::Impl {

public:
    friend class Runtime;

    struct SubscriptionRegistry {
        boost::unordered_flat_map<SubscriptionId, std::shared_ptr<std::function<void()>>> disconnect_handlers;
    };

    struct ActionSlot {
        std::atomic<bool> connected {true};
        Runtime::ActionHandler handler;
        std::shared_ptr<std::function<void()>> disconnect_handler;
    };

    struct ActionBucket {
        std::shared_ptr<ActionSlot> first_listener;
        std::vector<std::shared_ptr<ActionSlot>> extra_listeners;

        void add(std::shared_ptr<ActionSlot> listener)
        {
            if (first_listener == nullptr) {
                first_listener = std::move(listener);
                return;
            }
            extra_listeners.push_back(std::move(listener));
        }

        bool remove(const std::shared_ptr<ActionSlot> &listener)
        {
            if (first_listener == listener) {
                if (extra_listeners.empty()) {
                    first_listener.reset();
                    return true;
                }
                first_listener = std::move(extra_listeners.front());
                extra_listeners.erase(extra_listeners.begin());
                return true;
            }
            auto listener_it = std::find(extra_listeners.begin(), extra_listeners.end(), listener);
            if (listener_it == extra_listeners.end()) {
                return false;
            }
            extra_listeners.erase(listener_it);
            return true;
        }

        void disconnect_all()
        {
            if (first_listener != nullptr) {
                first_listener->connected.store(false, std::memory_order_release);
                if (first_listener->disconnect_handler != nullptr) {
                    *first_listener->disconnect_handler = {};
                }
            }
            for (const auto &listener : extra_listeners) {
                listener->connected.store(false, std::memory_order_release);
                if (listener->disconnect_handler != nullptr) {
                    *listener->disconnect_handler = {};
                }
            }
            first_listener.reset();
            extra_listeners.clear();
        }

        bool empty() const
        {
            return first_listener == nullptr && extra_listeners.empty();
        }

        size_t size() const
        {
            return (first_listener == nullptr ? 0 : 1) + extra_listeners.size();
        }
    };

    struct ActionRoute {
        std::string key;
        ActionBucket bucket;
    };

    struct ActionRegistry {
        std::mutex mutex;
        std::vector<ActionRoute> routes;

        ActionRoute *find_route(std::string_view key)
        {
            for (auto &route : routes) {
                if (route.key == key) {
                    return &route;
                }
            }
            return nullptr;
        }
    };

    struct MountedScreenRef {
        DocumentId document_id;
        std::string absolute_path;
        MountTarget target;
        uint64_t sequence = 0;
    };

    struct TransientScreenRef {
        DocumentId document_id;
        std::string absolute_path;
        MountTarget target;
    };

    struct RunningScreenFlow {
        DocumentId document_id;
        std::string flow_id;
        std::string current_state;
        std::string current_screen;
        MountTarget target;
    };

    struct RunningScreenFlowSnapshot {
        DocumentId document_id;
        std::string flow_id;
        std::string current_state;
        std::string current_screen;
        MountTarget target;
        bool was_mounted = false;
    };

    struct InstanceSnapshot {
        std::string template_id;
        std::string parent_absolute_path;
        std::string instance_id;
        size_t sibling_order = 0;
    };

    struct NodeStateSnapshot {
        std::string absolute_path;
        bool hidden = false;
        std::optional<Point> runtime_position;
    };

    struct BindingApplyMasks {
        PropsApplyMask props = PropsApplyMask::None;
        StyleApplyMask style = StyleApplyMask::None;
        LayoutApplyMask layout = LayoutApplyMask::None;
        PlacementApplyMask placement = PlacementApplyMask::None;
    };

    struct InitialStyleBinding {
        BindingTargetInfo target;
        std::string value;
    };

    // Node identities are private to one loaded document. A 32-bit ID keeps the per-node record,
    // parent/child lists, and lookup-index values compact while still allowing over four billion
    // node creations before a document must be reloaded. Public document/backend handles stay 64-bit.
    using NodeUid = uint32_t;

    struct NodeRecord {
        struct InteractionState {
            esp_brookesia::lib_utils::signal<void()> click_signal;
            esp_brookesia::lib_utils::signal<void(const Event &)> event_signal;
        };

        struct CompactPropsState {
            struct ImageState {
                ImageProps props;
                ResolvedImageSpec resolved;
            };

            struct StyleOverride {
                BindingTarget target = BindingTarget::StyleOpacity;
                std::string string_value;
                int32_t numeric_value = 0;
            };

            struct NestedStyleState {
                StateStyleMap state_styles;
                PartStyleMap part_styles;
            };

            struct StyleState {
                std::vector<StyleOverride> overrides;
                std::unique_ptr<NestedStyleState> nested;
            };

            std::optional<CommonProps> common_props;
            std::optional<LabelProps> label_props;
            // Do not inline the comparatively large image state: common/label-only nodes also use
            // CompactPropsState and should pay only one pointer for this optional domain.
            std::unique_ptr<ImageState> image_state;
            // NodeType determines the concrete props type. Store an exact-size allocation instead
            // of a variant whose storage would be sized for KeyboardProps even for a bool toggle.
            void *type_props_state = nullptr;
            NodeType type_props_type = NodeType::Max;
            std::unique_ptr<StyleState> style_state;
            std::unique_ptr<Layout> layout;
            std::unique_ptr<Placement> placement;

            ~CompactPropsState()
            {
                switch (type_props_type) {
                case NodeType::FrameView:
                    delete static_cast<FrameViewProps *>(type_props_state);
                    break;
                case NodeType::TextInput:
                    delete static_cast<TextInputProps *>(type_props_state);
                    break;
                case NodeType::Slider:
                case NodeType::ProgressBar:
                case NodeType::Arc:
                    delete static_cast<RangeProps *>(type_props_state);
                    break;
                case NodeType::Switch:
                case NodeType::Checkbox:
                    delete static_cast<ToggleProps *>(type_props_state);
                    break;
                case NodeType::Dropdown:
                    delete static_cast<DropdownProps *>(type_props_state);
                    break;
                case NodeType::Table:
                    delete static_cast<TableProps *>(type_props_state);
                    break;
                case NodeType::Line:
                    delete static_cast<LineProps *>(type_props_state);
                    break;
                case NodeType::Keyboard:
                    delete static_cast<KeyboardProps *>(type_props_state);
                    break;
                case NodeType::Canvas:
                    delete static_cast<CanvasProps *>(type_props_state);
                    break;
                case NodeType::Screen:
                case NodeType::Container:
                case NodeType::Label:
                case NodeType::Image:
                case NodeType::Button:
                case NodeType::Spinner:
                case NodeType::Max:
                    break;
                }
            }
        };

        std::string absolute_path;
        // Shared across all node instances that resolve to an identical style (same node type +
        // styleRefs + inline style/state/part overrides) within the current style revision. Template
        // list items (e.g. Wi-Fi scan rows) instantiate the same definition N times; storing one
        // ResolvedStyle per instance duplicated its resolved_font/state/part vectors N times. Sharing a
        // single immutable copy via shared_ptr removes that per-item PSRAM cost. ResolvedStyle is only
        // ever reassigned wholesale (never mutated in place), so const sharing is safe.
        std::shared_ptr<const ResolvedStyle> resolved_style;
        BackendHandle handle;
        const Node *definition = nullptr;
        std::unique_ptr<CompactPropsState> compact_props_state;
        uint32_t applied_style_revision = 0;
        NodeUid parent_uid = 0;
        std::unique_ptr<std::vector<NodeUid>> children;
        bool is_dynamic_template_root = false;
        bool press_lost_since_pressed = false;
        // Used only by the stack-local record in apply_initial_bindings(). Persistent records
        // always reference immutable definitions and keep mutations in CompactPropsState.
        bool transient_mutable_definition = false;

        CompactPropsState &ensure_compact_props_state()
        {
            if (compact_props_state == nullptr) {
                compact_props_state = std::make_unique<CompactPropsState>();
            }
            return *compact_props_state;
        }

        template <typename T>
        const T &typed_props(T Node::*member) const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->*member;
            }
            if (compact_props_state != nullptr && compact_props_state->type_props_state != nullptr) {
                return *static_cast<const T *>(compact_props_state->type_props_state);
            }
            return definition->*member;
        }

        template <typename T>
        T &mutable_typed_props(T Node::*member)
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->*member;
            }
            auto &state = ensure_compact_props_state();
            if (state.type_props_state == nullptr) {
                state.type_props_state = new T(definition->*member);
                state.type_props_type = definition->type;
            }
            return *static_cast<T *>(state.type_props_state);
        }

        const Node &node() const
        {
            return *definition;
        }

        std::string_view node_id() const
        {
            if (!is_dynamic_template_root) {
                return node().id;
            }
            const auto separator = absolute_path.find_last_of('/');
            return std::string_view(absolute_path).substr(separator == std::string::npos ? 0 : separator + 1);
        }

        const CommonProps &common_props() const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->common_props;
            }
            if (compact_props_state != nullptr && compact_props_state->common_props.has_value()) {
                return *compact_props_state->common_props;
            }
            return definition->common_props;
        }

        CommonProps &mutable_common_props()
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->common_props;
            }
            auto &state = ensure_compact_props_state();
            if (!state.common_props.has_value()) {
                state.common_props = definition->common_props;
            }
            return *state.common_props;
        }

        const LabelProps &label_props() const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->label_props;
            }
            if (compact_props_state != nullptr && compact_props_state->label_props.has_value()) {
                return *compact_props_state->label_props;
            }
            return definition->label_props;
        }

        LabelProps &mutable_label_props()
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->label_props;
            }
            auto &state = ensure_compact_props_state();
            if (!state.label_props.has_value()) {
                state.label_props = definition->label_props;
            }
            return *state.label_props;
        }

        const ImageProps &image_props() const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->image_props;
            }
            if (compact_props_state != nullptr && compact_props_state->image_state != nullptr) {
                return compact_props_state->image_state->props;
            }
            return definition->image_props;
        }

        ImageProps &mutable_image_props()
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->image_props;
            }
            auto &state = ensure_compact_props_state();
            if (state.image_state == nullptr) {
                state.image_state = std::make_unique<CompactPropsState::ImageState>(
                CompactPropsState::ImageState{
                    .props = definition->image_props,
                    .resolved = definition->resolved_image,
                }
                                    );
            }
            return state.image_state->props;
        }

        const ResolvedImageSpec &resolved_image() const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->resolved_image;
            }
            if (compact_props_state != nullptr && compact_props_state->image_state != nullptr) {
                return compact_props_state->image_state->resolved;
            }
            return definition->resolved_image;
        }

        ResolvedImageSpec &mutable_resolved_image()
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->resolved_image;
            }
            auto &state = ensure_compact_props_state();
            if (state.image_state == nullptr) {
                state.image_state = std::make_unique<CompactPropsState::ImageState>(
                CompactPropsState::ImageState{
                    .props = definition->image_props,
                    .resolved = definition->resolved_image,
                }
                                    );
            }
            return state.image_state->resolved;
        }

#define BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(Name, field) \
        const Name &field() const                                      \
        {                                                              \
            return typed_props(&Node::field);                           \
        }                                                              \
        Name &mutable_##field()                                        \
        {                                                              \
            return mutable_typed_props(&Node::field);                   \
        }

        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(FrameViewProps, frame_view_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(TextInputProps, text_input_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(RangeProps, range_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(ToggleProps, toggle_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(DropdownProps, dropdown_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(TableProps, table_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(LineProps, line_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(KeyboardProps, keyboard_props)
        BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS(CanvasProps, canvas_props)

#undef BROOKESIA_GUI_RUNTIME_DEFINE_TYPED_PROPS_ACCESSORS

        CompactPropsState::StyleState &ensure_style_state()
        {
            auto &state = ensure_compact_props_state();
            if (state.style_state == nullptr) {
                state.style_state = std::make_unique<CompactPropsState::StyleState>();
            }
            return *state.style_state;
        }

        CompactPropsState::NestedStyleState &ensure_nested_style_state()
        {
            auto &state = ensure_style_state();
            if (state.nested == nullptr) {
                state.nested = std::make_unique<CompactPropsState::NestedStyleState>(
                CompactPropsState::NestedStyleState{
                    .state_styles = definition->state_styles,
                    .part_styles = definition->part_styles,
                }
                               );
            }
            return *state.nested;
        }

        StateStyleMap &mutable_state_styles()
        {
            return transient_mutable_definition ? const_cast<Node *>(definition)->state_styles :
                   ensure_nested_style_state().state_styles;
        }

        PartStyleMap &mutable_part_styles()
        {
            return transient_mutable_definition ? const_cast<Node *>(definition)->part_styles :
                   ensure_nested_style_state().part_styles;
        }

        CompactPropsState::StyleOverride &ensure_style_override(BindingTarget target)
        {
            auto &overrides = ensure_style_state().overrides;
            auto it = std::find_if(overrides.begin(), overrides.end(), [target](const auto & item) {
                return item.target == target;
            });
            if (it == overrides.end()) {
                overrides.emplace_back();
                it = std::prev(overrides.end());
                it->target = target;
            }
            return *it;
        }

        void set_style_override(BindingTarget target, std::string value)
        {
            ensure_style_override(target).string_value = std::move(value);
        }

        void set_style_override(BindingTarget target, int32_t value)
        {
            ensure_style_override(target).numeric_value = value;
        }

        static void apply_style_override(Style &style, const CompactPropsState::StyleOverride &item)
        {
#define BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(Target, Field) \
            case BindingTarget::Target:                              \
                style.set_##Field(item.string_value);                 \
                break
#define BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(Target, Field) \
            case BindingTarget::Target:                              \
                style.set_##Field(item.numeric_value);                \
                break

            switch (item.target) {
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleBgColor, bg_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleBgGradientColor, bg_gradient_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleBgGradientDirection, bg_gradient_direction);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleTextColor, text_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleBorderColor, border_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleLineColor, line_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleArcColor, arc_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleArcGradientColor, arc_gradient_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleFont, font);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleShadowColor, shadow_color);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING(StyleImageRecolor, image_recolor);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleBgMainStop, bg_main_stop);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleBgGradientStop, bg_gradient_stop);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleBgGradientOpacity, bg_gradient_opacity);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleBorderWidth, border_width);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleRadius, radius);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StylePadding, padding);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StylePaddingLeft, padding_left);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StylePaddingRight, padding_right);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StylePaddingTop, padding_top);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StylePaddingBottom, padding_bottom);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleMargin, margin);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleMarginLeft, margin_left);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleMarginRight, margin_right);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleMarginTop, margin_top);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleMarginBottom, margin_bottom);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleShadowWidth, shadow_width);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleShadowOffsetX, shadow_offset_x);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleShadowOffsetY, shadow_offset_y);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleOpacity, opacity);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleLineWidth, line_width);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleImageOpacity, image_opacity);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleImageRecolorOpacity, image_recolor_opacity);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleFontSize, font_size);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleImageFontSize, image_font_size);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleArcWidth, arc_width);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleArcOpacity, arc_opacity);
                BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER(StyleArcGradientSegments, arc_gradient_segments);
            case BindingTarget::StyleArcRounded:
                style.set_arc_rounded(item.numeric_value != 0);
                break;
            default:
                break;
            }

#undef BROOKESIA_GUI_RUNTIME_APPLY_STYLE_NUMBER
#undef BROOKESIA_GUI_RUNTIME_APPLY_STYLE_STRING
        }

        void populate_style_snapshot(Node &snapshot) const
        {
            snapshot.style = definition->style;
            snapshot.state_styles = definition->state_styles;
            snapshot.part_styles = definition->part_styles;
            if (compact_props_state == nullptr || compact_props_state->style_state == nullptr) {
                return;
            }
            const auto &state = *compact_props_state->style_state;
            for (const auto &item : state.overrides) {
                apply_style_override(snapshot.style, item);
            }
            if (state.nested != nullptr) {
                snapshot.state_styles = state.nested->state_styles;
                snapshot.part_styles = state.nested->part_styles;
            }
        }

        const Layout &layout() const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->layout;
            }
            if (compact_props_state != nullptr && compact_props_state->layout != nullptr) {
                return *compact_props_state->layout;
            }
            return definition->layout;
        }

        Layout &mutable_layout()
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->layout;
            }
            auto &state = ensure_compact_props_state();
            if (state.layout == nullptr) {
                state.layout = std::make_unique<Layout>(definition->layout);
            }
            return *state.layout;
        }

        const Placement &placement() const
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->placement;
            }
            if (compact_props_state != nullptr && compact_props_state->placement != nullptr) {
                return *compact_props_state->placement;
            }
            return definition->placement;
        }

        Placement &mutable_placement()
        {
            if (transient_mutable_definition) {
                return const_cast<Node *>(definition)->placement;
            }
            auto &state = ensure_compact_props_state();
            if (state.placement == nullptr) {
                state.placement = std::make_unique<Placement>(definition->placement);
            }
            return *state.placement;
        }

    };

    struct SubtreeBuildProfile {
        size_t nodes = 0;
        int64_t copy_definition_us = 0;
        int64_t initial_bindings_us = 0;
        int64_t image_resolve_us = 0;
        int64_t create_node_us = 0;
        int64_t resolve_style_us = 0;
        int64_t store_record_us = 0;
        int64_t apply_props_us = 0;
        int64_t apply_layout_us = 0;
        int64_t apply_placement_us = 0;
        int64_t apply_transform_us = 0;
        int64_t apply_style_us = 0;
        int64_t apply_animations_us = 0;
        int64_t events_us = 0;
        int64_t subscribe_bindings_us = 0;
    };

    struct PreloadedImageRecord {
        RuntimeImageResource resource;
        std::size_t automatic_ref_count = 0;
        std::size_t manual_ref_count = 0;
    };

    struct InteractionRecord {
        NodeUid uid = 0;
        std::shared_ptr<NodeRecord::InteractionState> state;
    };

    enum class ImagePreloadOwner {
        Automatic,
        Manual,
        All,
    };

    struct TreeRecord {
        DocumentId document_id;
        std::string file_path;
        bool file_backed = false;
        EnvironmentDependencies environment_dependencies;
        bool theme_sensitive = false;
        boost::json::value constants;
        Environment environment;
        bool environment_dirty = false;
        bool styles_dirty = false;
        bool live_preview_enabled = false;
        LivePreviewOptions live_preview_options;
        std::vector<std::string> dependency_files;
        std::unordered_map<std::string, uint64_t> dependency_mtimes;
        std::chrono::steady_clock::time_point last_live_preview_poll = std::chrono::steady_clock::time_point::min();
        boost::unordered_flat_map<std::string, ImageAsset> images;
        std::map<std::string, StyleSet> styles;
        boost::unordered_flat_map<std::string, ScreenFlow> screen_flows;
        // NodeRecord keeps pointers into these canonical definitions. Node-based maps preserve
        // references across map growth and make that lifetime guarantee explicit.
        boost::unordered_node_map<std::string, Node> screens;
        boost::unordered_node_map<std::string, Node> templates;
        // Canonical definitions already have stable addresses. Cache their resolved style by pointer
        // instead of retaining a serialized Style/StateStyle/PartStyle string for every definition.
        boost::unordered_flat_map<const Node *, std::shared_ptr<const ResolvedStyle>> definition_style_cache;
        // Different definitions often resolve to the same immutable 708-byte ResolvedStyle. Keep only
        // weak references here: NodeRecord/definition caches own live results, while the
        // interner provides exact result sharing without extending their lifetime.
        // The pool is small (73 source cache entries in the measured workload). A contiguous vector
        // avoids one hash-table slot and one bucket allocation per unique result.
        std::vector<std::weak_ptr<const ResolvedStyle>> resolved_style_intern_cache;
        std::vector<PreloadedImageRecord> preloaded_images;
        boost::unordered_flat_map<std::string, uint64_t> screen_roots;
        // Eager screens not built during load; warmed on idle or first mount.
        std::vector<std::string> pending_warm_screens;
        DocumentLoadOptions load_options;
        bool idle_warm_scheduled = false;
        lib_utils::TaskScheduler::TaskId idle_warm_task_id = 0;
        std::shared_ptr<std::atomic<bool>> idle_warm_token;
        std::map<std::string, std::string> deferred_screen_files;
        std::map<std::string, std::string> view_template_json;
        std::map<std::string, std::string> interaction_template_json;
        // MEMORY/FRAGMENTATION-CRITICAL: must be a NODE-based map, not unordered_flat_map.
        // NodeRecord is large (full Node definition + signals + vectors). A flat_map stores values
        // inline in one contiguous table, so every rehash (on each doubling threshold) must reallocate
        // a single huge contiguous block and MOVE all NodeRecords - measured as a ~550KB transient
        // spike on a heap whose largest free block was only ~528KB, i.e. one growth step away from OOM
        // even though total free memory was sufficient. unordered_node_map keeps each NodeRecord in its
        // own stable allocation; a rehash only reshuffles a pointer array (~8B/entry), removing both the
        // giant transient spike and the contiguous-block fragmentation hazard. Stable node addresses
        // also make NodeRecord* held across child inserts safe (see find_node_record callers).
        // Keep the index value/key types aligned with the runtime's other 64-bit identity maps. On
        // ESP this reuses their existing Boost hash template code; specializing every index for a
        // 32-bit value costs several KB of flash. Stored values still originate from 32-bit NodeUid.
        boost::unordered_node_map<uint64_t, NodeRecord> nodes;
        boost::unordered_flat_map<BackendHandle::Value, uint64_t> handle_to_uid;
        // Keys borrow the immutable absolute_path owned by stable NodeRecord allocations. This
        // removes a second path string per node; erase the index entry before erasing the record.
        boost::unordered_flat_map<std::string_view, uint64_t> absolute_path_to_uid;
        // Local C++ signal state is rare (8 connected nodes among ~195 live nodes in the measured
        // workload). Keep it sparse instead of embedding an empty shared_ptr in every NodeRecord.
        std::vector<InteractionRecord> interaction_records;
        NodeUid next_uid = 1;
    };

    static std::shared_ptr<NodeRecord::InteractionState> find_interaction_state(
        const TreeRecord &tree, uint64_t uid)
    ;

    static std::shared_ptr<NodeRecord::InteractionState> &ensure_interaction_state(
        TreeRecord &tree, NodeUid uid)
    ;

    static void erase_interaction_state(TreeRecord &tree, NodeUid uid)
    ;

    static const NodeRecord *find_root_node_record(const TreeRecord &tree, const NodeRecord &record)
    ;

    static Path build_relative_node_path(const TreeRecord &tree, const NodeRecord &record)
    ;

    Impl(std::unique_ptr<IBackend> backend_in, RuntimeTaskConfig task_config_in = {});

    boost::unordered_flat_map<std::string, RuntimeFontResource> global_fonts;
    boost::unordered_flat_map<std::string, RuntimeImageResource> global_images;
    boost::unordered_flat_map<std::string, ThemeAsset> global_themes;
    boost::unordered_flat_set<std::string> unregistered_global_fonts;
    std::vector<std::string> font_registration_order;
    std::vector<std::string> theme_registration_order;
    boost::unordered_flat_map<std::string, std::string> default_fonts_by_language;
    std::string current_language = "en";
    std::string current_theme = "default";
    uint64_t current_style_revision_ = 1;
    // Tracks when the per-document definition caches and exact resolved-style intern pools must be
    // invalidated after the global style inputs change.
    uint64_t style_cache_revision_ = 0;
    boost::unordered_flat_map<std::string, RunningScreenFlow> running_screen_flows_;
    boost::unordered_flat_map<std::string, SubscriptionId> event_animation_ids_;
    SubscriptionId next_subscription_id_ = 1;
    std::shared_ptr<SubscriptionRegistry> subscription_registry_ = std::make_shared<SubscriptionRegistry>();
    // Maps every active SubscriptionId returned from subscribe_*_with_id / start_view_animation_with_result
    // to its owning document. Used by unload(document_id) to release subscriptions that the caller
    // forgot (or never bothered) to unsubscribe explicitly, e.g. fire-and-forget animations whose
    // RuntimeAnimationStartResult::subscription_id is discarded.
    boost::unordered_flat_map<SubscriptionId, DocumentId::Value> subscription_document_ids_;

    void register_document_subscription(SubscriptionId subscription_id, DocumentId document_id)
    ;

    bool unsubscribe_subscription(SubscriptionId subscription_id)
    ;

    ~Impl()
    ;

    std::expected<DocumentId, std::string> load(
        std::string_view file_path_view,
        Document document,
        const Environment &environment,
        bool file_backed = false,
        std::vector<std::string> dependency_files = {},
        DocumentLoadOptions options = {},
        std::map<std::string, std::string> deferred_screen_files = {},
        std::map<std::string, std::string> view_template_json = {},
        std::map<std::string, std::string> interaction_template_json = {})
    ;

    std::vector<RuntimeImageResource> collect_image_resources(const TreeRecord &tree) const
    ;

    static std::string image_resource_cache_key(const RuntimeImageResource &resource)
    ;

    static bool is_same_image_resource(
        const RuntimeImageResource &lhs,
        const RuntimeImageResource &rhs)
    ;

    bool should_preload_image_resource_automatically(const RuntimeImageResource &resource) const
    ;

    bool has_preloaded_image_resource(const TreeRecord &tree, const RuntimeImageResource &resource) const
    ;

    bool has_automatic_preloaded_image_resource(const TreeRecord &tree, const RuntimeImageResource &resource) const
    ;

    static RuntimeImageResource make_runtime_image_resource(
        const ImageProps &image_props,
        const ResolvedImageSpec &resolved_image
    )
    ;

    static RuntimeImageResource make_runtime_image_resource(const Node &node)
    ;

    static RuntimeImageResource make_runtime_image_resource(const NodeRecord &record)
    ;

    std::expected<void, std::string> preload_image_resource_for_tree(
        TreeRecord &tree,
        const RuntimeImageResource &resource,
        ImagePreloadOwner owner)
    ;

    std::expected<void, std::string> preload_tree_image_resources(TreeRecord &tree)
    ;

    std::expected<void, std::string> ensure_image_resource_preloaded_for_tree(
        TreeRecord &tree,
        const RuntimeImageResource &resource)
    ;

    std::expected<void, std::string> ensure_node_image_resources_preloaded(TreeRecord &tree, const Node &node)
    ;

    void release_image_resource_from_tree(
        TreeRecord &tree,
        const RuntimeImageResource &resource,
        ImagePreloadOwner owner = ImagePreloadOwner::All)
    ;

    bool tree_references_image_resource_except(
        const TreeRecord &tree,
        const NodeRecord &excluded_record,
        std::string_view image_id,
        const RuntimeImageResource &resource) const
    ;

    std::expected<void, std::string> update_image_source(
        TreeRecord &tree,
        NodeRecord &record,
        std::string_view src)
    ;

    void release_tree_image_resources(TreeRecord &tree)
    ;

    std::expected<RuntimeImageResource, std::string> resolve_image_resource_by_id(
        const TreeRecord &tree,
        std::string_view image_id) const
    ;

    std::expected<void, std::string> preload_images(DocumentId document_id, const std::vector<std::string> &image_ids)
    ;

    std::expected<void, std::string> release_preloaded_images(
        DocumentId document_id,
        const std::vector<std::string> &image_ids)
    ;

    std::expected<void, std::string> enable_live_preview(DocumentId document_id, const LivePreviewOptions &options)
    ;

    bool disable_live_preview(DocumentId document_id)
    ;

    bool unload(DocumentId document_id)
    ;

    void set_view_debug_enabled(bool enabled)
    ;

    bool is_view_debug_enabled() const
    ;

    void poll_live_preview(Runtime *runtime)
    ;

    std::vector<GuiLayer> list_layers() const
    ;

    std::vector<GuiDisplayInfo> list_displays() const
    ;

    std::optional<std::string> resolve_default_display_id() const
    ;

    std::expected<MountTarget, std::string> normalize_mount_target(const MountTarget &target) const
    ;

    std::string make_mounted_screen_key(
        DocumentId document_id,
        std::string_view absolute_path,
        const MountTarget &target) const
    ;

    void reorder_mounted_screens(const MountTarget &target)
    ;

    void remove_replace_screen_on_layer(const MountTarget &target)
    ;

    void remove_duplicate_stack_screen(DocumentId document_id, std::string_view absolute_path, const MountTarget &target)
    ;

    std::expected<View, std::string> mount_screen(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view absolute_path,
        const MountTarget &target)
    ;

    bool unmount_screen(DocumentId document_id, std::string_view absolute_path)
    ;

    std::expected<TransientMountId, std::string> push_transient_screen(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view absolute_path,
        const MountTarget &target)
    ;

    bool pop_transient_screen(TransientMountId id)
    ;

    void pop_transient_screens_for_document(DocumentId document_id)
    ;

    bool is_screen_flow_screen_mounted(const RunningScreenFlow &flow) const
    ;

    std::expected<void, std::string> start_screen_flow(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view flow_id,
        const MountTarget &target)
    ;

    std::expected<void, std::string> trigger_screen_flow(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view flow_id,
        std::string_view action)
    ;

    bool stop_screen_flow(DocumentId document_id, std::string_view flow_id)
    ;

    bool has_screen_flow(DocumentId document_id, std::string_view flow_id) const
    ;

    std::optional<std::string> get_screen_flow_state(DocumentId document_id, std::string_view flow_id) const
    ;

    void stop_screen_flows_for_document(DocumentId document_id)
    ;

    View find_view(Runtime *runtime, DocumentId document_id, std::string_view absolute_path)
    ;

    std::expected<View, std::string> create_view(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view template_id,
        std::string_view parent_absolute_path,
        std::string_view instance_id)
    ;

    bool destroy_view(DocumentId document_id, std::string_view absolute_path)
    ;

    static std::vector<InstanceSnapshot> capture_dynamic_instance_snapshots(const TreeRecord &tree)
    ;

    std::expected<void, std::string> update(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view file_path,
        const Environment &environment)
    ;

    std::expected<void, std::string> update(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view file_path,
        const Environment &environment,
        const ParsedDocument &parsed_document)
    ;

    size_t reapply_style_record(TreeRecord &tree, NodeRecord &record)
    ;

    size_t reapply_subtree_styles(TreeRecord &tree, NodeUid uid)
    ;

    size_t reapply_styles(TreeRecord &tree)
    ;

    void reapply_styles_for_all_trees()
    ;

    size_t reapply_mounted_styles_for_all_trees()
    ;

    size_t reapply_mounted_styles(TreeRecord &tree)
    ;

    std::expected<void, std::string> refresh_document_environment(Runtime *runtime, DocumentId document_id)
    ;

    std::expected<void, std::string> refresh_document_styles(DocumentId document_id)
    ;

    std::expected<void, std::string> refresh_document_if_dirty(Runtime *runtime, DocumentId document_id)
    ;

    std::expected<void, std::string> load_theme(const ThemeAsset &theme)
    ;

    std::vector<std::string> list_supported_themes() const
    ;

    Environment make_effective_environment(Environment environment) const
    ;

    Environment make_parse_environment(Environment environment) const
    ;

    std::expected<std::string, std::string> resolve_color_binding_value(
        const TreeRecord &tree,
        std::string_view value) const
    ;

    void resolve_style_color_field(
        const TreeRecord &tree,
        Style &style,
        std::optional<std::string> (Style::*getter)() const,
        void (Style::*setter)(std::optional<std::string>),
        std::string_view field_name) const
    ;

    void resolve_style_color_fields(const TreeRecord &tree, Style &style) const
    ;

    void resolve_style_set_color_fields(const TreeRecord &tree, StyleSet &style_set) const
    ;

    std::expected<void, std::string> set_theme(
        Runtime *runtime,
        std::string_view theme_id,
        bool reapply_loaded_documents)
    ;

    std::expected<void, std::string> set_theme(Runtime *runtime, std::string_view theme_id)
    ;

    std::expected<void, std::string> reapply_styles(DocumentId id)
    ;

    std::string get_theme() const
    ;

    std::expected<void, std::string> register_font(const RuntimeFontResource &resource)
    ;

    bool unregister_font(std::string_view id)
    ;

    std::vector<std::string> list_supported_fonts(std::string_view language = {}) const
    ;

    std::vector<std::string> list_supported_languages() const
    ;

    std::vector<std::string> list_supported_languages(std::string_view font_id) const
    ;

    std::expected<void, std::string> set_language(
        Runtime *runtime,
        std::string_view language,
        bool reapply_loaded_documents)
    ;

    std::expected<void, std::string> set_language(Runtime *runtime, std::string_view language)
    ;

    std::string get_language() const
    ;

    std::expected<void, std::string> set_default_font_for_language(
        std::string_view language,
        std::string_view font_id)
    ;

    std::optional<std::string> get_default_font_for_language(std::string_view language) const
    ;

    void refresh_image_references(TreeRecord &tree, std::string_view image_id)
    ;

    std::expected<void, std::string> register_image(const RuntimeImageResource &resource)
    ;

    bool unregister_image(std::string_view id)
    ;

    std::expected<void, std::string> load_theme_json(std::string_view json, std::string_view base_dir)
    ;

    std::expected<void, std::string> load_theme_file(std::string_view path)
    ;

    std::expected<void, std::string> register_font_json(std::string_view json, std::string_view base_dir)
    ;

    std::expected<void, std::string> register_font_file(std::string_view path)
    ;

    std::expected<void, std::string> register_image_json(std::string_view json, std::string_view base_dir)
    ;

    std::expected<void, std::string> register_image_file(std::string_view path)
    ;

    std::expected<boost::json::value, std::string> get_constant_value(
        DocumentId document_id,
        std::string_view path) const
    ;

    std::vector<RuntimeFontResource> list_font_resources(DocumentId document_id) const
    ;

    std::vector<RuntimeImageResource> list_image_resources(DocumentId document_id) const
    ;

    bool is_view_valid(const View &view) const
    ;

    std::string get_view_id(const View &view) const
    ;

    Node make_style_snapshot(const NodeRecord &record) const
    ;

    void apply_record_props(NodeRecord &record, PropsApplyMask mask)
    ;

    std::optional<ViewStateValue> get_view_state_internal(const View &view, ViewStateKind kind) const
    ;

    bool set_view_hidden(const View &view, bool hidden)
    ;

    bool scroll_view_to(const View &view, int32_t x, int32_t y, bool animated)
    ;

    bool scroll_view_to_visible(const View &view, bool animated)
    ;

    bool set_view_text(const View &view, std::string_view text)
    ;

    std::string get_view_text(const View &view) const
    ;

    bool set_view_src(const View &view, std::string_view src)
    ;

    std::string get_view_src(const View &view) const
    ;

    bool set_view_value(const View &view, int32_t value)
    ;

    int32_t get_view_value(const View &view) const
    ;

    bool set_view_checked(const View &view, bool checked)
    ;

    bool get_view_checked(const View &view) const
    ;

    bool set_view_selected_index(const View &view, int32_t index)
    ;

    int32_t get_view_selected_index(const View &view) const
    ;

    bool set_table_cell_text(const View &view, int32_t row, int32_t column, std::string_view text)
    ;

    esp_brookesia::lib_utils::connection connect_view_event(const View &view, EventType type, View::EventHandler handler)
    ;

    esp_brookesia::lib_utils::connection connect_button_click(const Button &button, Button::ClickHandler handler)
    ;

    static std::shared_ptr<std::function<void()>> make_action_disconnect_handler(
                const std::shared_ptr<ActionRegistry> &registry,
                const std::shared_ptr<ActionSlot> &listener)
            ;

    std::expected<std::shared_ptr<std::function<void()>>, std::string> connect_event_action_signal(
                Runtime *runtime,
                DocumentId document_id,
                std::string_view action,
                Runtime::ActionHandler handler
            )
            ;

    SubscriptionId subscribe_event_action_with_id(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view action,
        Runtime::ActionHandler handler
    )
    ;

    RuntimeAnimationStartResult start_view_animation_with_result(
        const View &view,
        const Animation &animation,
        Runtime::AnimationCompletedHandler completed_handler
    )
    ;

    SubscriptionId start_view_animation_with_id(
        const View &view,
        const Animation &animation,
        Runtime::AnimationCompletedHandler completed_handler
    )
    ;

    ScopedConnection start_view_animation(
        const View &view,
        const Animation &animation,
        Runtime::AnimationCompletedHandler completed_handler
    )
    ;

    bool dispatch_event_action_handlers(const Event &event)
    ;

    void register_fast_action_routes(const TreeRecord &tree, const NodeRecord &record)
    ;

    void unregister_fast_action_routes(const NodeRecord &record)
    ;

    bool try_dispatch_fast_action_event(const BackendEvent &event)
    ;

    void apply_view_debug_to_all_nodes()
    ;

    std::unique_ptr<IBackend> backend;
    std::shared_ptr<MemoryDataStore> store;
    RuntimeTaskConfig task_config;
    std::shared_ptr<ActionRegistry> action_registry_ = std::make_shared<ActionRegistry>();
    std::mutex fast_action_mutex_;
    boost::unordered_flat_map<std::string, Event> fast_action_events_;
    boost::unordered_flat_map<DocumentId::Value, TreeRecord> trees;
    std::unordered_map<std::string, MountedScreenRef> mounted_screens_;
    std::unordered_map<TransientMountId::Value, TransientScreenRef> transient_screens_;
    // Recently unmounted Dynamic screens kept alive for fast remount (capacity 2).
    struct DynamicScreenKeepAliveEntry {
        DocumentId::Value document_id = 0;
        std::string screen_id;
        uint64_t sequence = 0;
    };
    static constexpr size_t kDynamicScreenKeepAliveLimit = 2;
    /** Space out warm builds so GUI refresh is not starved (was 16ms). */
    static constexpr int kIdleScreenWarmDelayMs = 300;
    std::vector<DynamicScreenKeepAliveEntry> dynamic_screen_keep_alive_;
    uint64_t next_dynamic_keep_alive_sequence_ = 1;
    DocumentId::Value next_document_id_ = 1;
    TransientMountId::Value next_transient_mount_id_ = 1;
    uint64_t next_mounted_screen_sequence_ = 1;
    bool view_debug_enabled_ = false;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
    size_t dbg_create_view_count_ = 0;
    size_t dbg_create_subtree_count_ = 0;
    size_t dbg_destroy_subtree_count_ = 0;
    size_t dbg_set_view_text_count_ = 0;
    size_t dbg_set_view_src_count_ = 0;
    // Per-substep PSRAM attribution (bytes consumed = before - after), accumulated across a create_view.
    int64_t dbg_step_create_node_ = 0;
    int64_t dbg_step_noderecord_ = 0;
    int64_t dbg_step_apply_props_ = 0;
    int64_t dbg_step_apply_layout_ = 0;
    int64_t dbg_step_apply_placement_ = 0;
    int64_t dbg_step_apply_style_ = 0;
    int64_t dbg_step_apply_anim_ = 0;
    int64_t dbg_step_events_ = 0;
    int64_t dbg_step_subscribe_ = 0;
    int64_t dbg_step_node_copy_ = 0;
    int64_t dbg_step_resolve_style_ = 0;
    int64_t dbg_step_store_ = 0;
    size_t dbg_style_cache_hits_ = 0;
    size_t dbg_style_cache_misses_ = 0;
    size_t dbg_style_intern_hits_ = 0;
    size_t dbg_style_intern_misses_ = 0;
    std::array<size_t, static_cast<size_t>(NodeType::Max)> dbg_live_nodes_by_type_ {};
    static size_t dbg_ext_free()
    ;
#endif
    SubtreeBuildProfile subtree_build_profile_;

private:
    uint32_t current_applied_style_revision() const
    ;

    void advance_style_revision()
    ;

    static RuntimeProfileClock::time_point subtree_profile_now()
    ;

    static void add_subtree_profile_time(
        int64_t &bucket_us,
        const RuntimeProfileClock::time_point &start)
    ;

    void reset_subtree_build_profile()
    ;

    void log_subtree_build_profile(
        std::string_view stage,
        DocumentId document_id,
        std::string_view root_id,
        int64_t total_ms) const
    ;

    std::expected<NodeUid, std::string> create_subtree(
        DocumentId document_id,
        TreeRecord &tree,
        const Node &definition,
        BackendHandle parent_handle,
        NodeUid parent_uid,
        const std::string &root_id,
        const Path &current_path,
        const std::string &scope_root_absolute_path,
        const std::optional<std::string> &override_root_id,
        bool is_dynamic_template_root = false)
    ;

    std::expected<NodeUid, std::string> ensure_screen_built(
        DocumentId document_id,
        TreeRecord &tree,
        const std::string &screen_id)
    ;

    void remove_pending_warm_screen(TreeRecord &tree, std::string_view screen_id)
    ;

    void request_idle_screen_warm(DocumentId document_id)
    ;

    void schedule_idle_screen_warm(DocumentId document_id)
    ;

    void stop_idle_screen_warm(TreeRecord &tree)
    ;

    void warm_one_pending_screen(DocumentId document_id)
    ;

    void remember_dynamic_screen_keep_alive(DocumentId document_id, const std::string &screen_id)
    ;

    void forget_dynamic_screen_keep_alive(DocumentId document_id, std::string_view screen_id)
    ;

    void evict_dynamic_screen_keep_alive_overflow()
    ;

    void clear_dynamic_screen_keep_alive_for_document(DocumentId document_id)
    ;

    void unload_partial_tree(TreeRecord &tree)
    ;

    bool unmount_mounted_screen(TreeRecord &tree, std::string_view absolute_path)
    ;

    void destroy_subtree(TreeRecord &tree, NodeUid uid)
    ;

    void collect_subtree_uids(const TreeRecord &tree, NodeUid uid, std::vector<NodeUid> &uids) const
    ;

    std::expected<std::vector<Dimension>, std::string> parse_dimension_array_from_store_string(
        std::string_view value,
        const Environment &environment) const
    ;

    std::expected<std::vector<std::string>, std::string> parse_string_array_from_store_string(std::string_view value) const
    ;

    std::expected<std::vector<Point>, std::string> parse_points_from_store_string(std::string_view value) const
    ;

    std::expected<std::vector<TableCell>, std::string> parse_table_cells_from_store_string(std::string_view value) const
    ;

    std::expected<std::vector<CanvasCommand>, std::string> parse_canvas_commands_from_store_string(std::string_view value) const
    ;

    std::expected<void, std::string> apply_binding_value(
        TreeRecord &tree,
        NodeRecord &record,
        const BindingTargetInfo &target_info,
        std::string_view value)
    ;

    void reapply_binding_domain(TreeRecord &tree, NodeRecord &record, const BindingTargetInfo &target_info)
    ;

    void reapply_binding_masks(TreeRecord &tree, NodeRecord &record, const BindingApplyMasks &masks)
    ;

    void merge_binding_mask(BindingApplyMasks &masks, const BindingTargetInfo &target_info)
    ;

    static bool node_has_binding_key(const Node &node, std::string_view key)
    ;

    static const Node *find_child_node_by_path(
        const Node &node,
        const std::vector<std::string> &segments,
        size_t index
    )
    ;

    static std::vector<std::string> split_absolute_path_segments(std::string_view absolute_path)
    ;

    static bool tree_has_binding_declaration_for_update(
        const TreeRecord &tree,
        std::string_view absolute_path,
        std::string_view key
    )
    ;

    void set_binding_values(DocumentId document_id, std::span<const BindingValueUpdate> updates)
    ;

    BindingApplyMasks apply_initial_bindings(
        TreeRecord &tree,
        Node &node,
        std::string_view absolute_path,
        std::vector<InitialStyleBinding> &style_bindings)
    ;

    std::string resolve_event_effect_target_path(const NodeRecord &source_record, std::string_view target) const
    ;

    NodeRecord *resolve_event_effect_target(TreeRecord &tree, const NodeRecord &source_record, std::string_view target)
    ;

    static std::string build_event_animation_key(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view animation_id)
    ;

    std::expected<void, std::string> apply_event_property_update(
        TreeRecord &tree,
        const NodeRecord &source_record,
        const EventPropertyUpdate &update)
    ;

    std::expected<Animation, std::string> resolve_event_animation(
        const NodeRecord &record,
        const EventEffect &effect) const
    ;

    std::expected<void, std::string> start_event_animation(
        TreeRecord &tree,
        const NodeRecord &source_record,
        const EventEffect &effect)
    ;

    std::expected<void, std::string> stop_event_animation(
        TreeRecord &tree,
        const NodeRecord &source_record,
        const EventEffect &effect)
    ;

    void execute_event_effects(
        TreeRecord &tree,
        NodeRecord &source_record,
        const Event &event,
        std::vector<Event> &deferred_actions)
    ;

    static KeyboardKeyImageSpec make_keyboard_key_image_spec(const ImageAsset &image)
    ;

    static KeyboardKeyImageSpec make_keyboard_key_image_spec(const RuntimeImageResource &image)
    ;

    std::optional<KeyboardKeyImageSpec> resolve_keyboard_key_image(
        const TreeRecord &tree,
        std::string_view image_id) const
    ;

    void resolve_keyboard_key_images(const TreeRecord &tree, KeyboardProps &props) const
    ;

    void resolve_keyboard_key_images(const TreeRecord &tree, Node &node) const
    ;

    bool node_references_image(const Node &node, std::string_view image_id) const
    ;

    bool node_references_image(const NodeRecord &record, std::string_view image_id) const
    ;

    ResolvedImageSpec resolve_image_spec(const TreeRecord &tree, std::string_view image_id) const
    ;

    void resolve_image_source(const TreeRecord &tree, Node &node) const
    ;

    bool has_font_resource(const TreeRecord &tree, const std::string &font_id) const
    ;

    bool has_image_resource(const TreeRecord &tree, const std::string &image_id) const
    ;

    static void merge_style_layer(Style &destination, const Style &source)
    ;

    static void merge_style_set_layer(StyleSet &destination, const StyleSet &source)
    ;

    StyleSet compose_style_set(const TreeRecord &tree, const Node &node) const
    ;

    std::string resolve_keyboard_key_color(
        const TreeRecord &tree,
        const std::string &color,
        std::string_view field_name) const
    ;

    std::optional<KeyboardKeyStyle> resolve_keyboard_key_style_ref(
        const TreeRecord &tree,
        std::string_view style_ref) const
    ;

    void resolve_keyboard_key_style_refs(const TreeRecord &tree, KeyboardProps &props) const
    ;

    void resolve_keyboard_key_style_refs(const TreeRecord &tree, Node &node) const
    ;

    std::expected<void, std::string> validate_node_resource_references(
        const TreeRecord &tree, const Node &node, const std::string &absolute_path) const
    ;

    std::expected<void, std::string> validate_resource_references(const TreeRecord &tree) const
    ;

    ResolvedStyle resolve_style(const TreeRecord &tree, const Node &node) const
    ;

    std::shared_ptr<const ResolvedStyle> intern_resolved_style(
        TreeRecord &tree,
        ResolvedStyle resolved_style)
    ;

    // Returns a shared, immutable ResolvedStyle for `node`. Canonical definition pointers use a compact
    // per-document cache. Runtime-mutated styles resolve directly and then use exact result interning;
    // serializing the full Style into a content-cache key costs more memory than the values it avoids.
    std::shared_ptr<const ResolvedStyle> resolve_style_shared(
        TreeRecord &tree,
        const Node &node,
        const Node *canonical_definition = nullptr)
    ;

    void refresh_global_fonts_from_backend()
    ;

    void dispatch_backend_event(const BackendEvent &event)
    ;

    std::optional<NodeUid> resolve_any_uid(const TreeRecord &tree, std::string_view id) const
    ;

    static std::optional<NodeUid> resolve_normalized_uid(
        const TreeRecord &tree,
        std::string_view normalized_absolute_path)
    ;

    TreeRecord *resolve_tree(DocumentId document_id)
    ;

    const TreeRecord *resolve_tree_const(DocumentId document_id) const
    ;

    static NodeRecord *find_node_record(TreeRecord &tree, NodeUid uid)
    ;

    static const NodeRecord *find_node_record_const(const TreeRecord &tree, NodeUid uid)
    ;

    std::optional<NodeUid> resolve_view_uid(const View &view) const
    ;

    NodeRecord *resolve_view_record(const View &view)
    ;

    NodeRecord *resolve_view_record(const View &view) const
    ;
};

} // namespace esp_brookesia::gui

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
