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
#include <mutex>
#include <optional>
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
        style.border_width = 0;
        style.radius = 0;
        style.padding = 0;
        style.margin = 0;
        style.shadow_width = 0;
        style.shadow_offset_x = 0;
        style.shadow_offset_y = 0;
        style.opacity = 255;
        style.line_width = 0;
        style.image_opacity = 255;
        // Leave font unset here so Runtime can select the language default font, e.g. zh_CN -> kaiti.
        // Setting "default" in the built-in theme forces every node onto the English font.
        style.font_size = 16;
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

    struct NodeRecord {
        uint64_t uid = 0;
        DocumentId document_id;
        std::string file_path;
        std::string root_id;
        Path path;
        std::string absolute_path;
        Node node;
        // Shared across all node instances that resolve to an identical style (same node type +
        // styleRefs + inline style/state/part overrides) within the current style revision. Template
        // list items (e.g. Wi-Fi scan rows) instantiate the same definition N times; storing one
        // ResolvedStyle per instance duplicated its resolved_font/state/part vectors N times. Sharing a
        // single immutable copy via shared_ptr removes that per-item PSRAM cost. ResolvedStyle is only
        // ever reassigned wholesale (never mutated in place), so const sharing is safe.
        std::shared_ptr<const ResolvedStyle> resolved_style;
        uint64_t applied_style_revision = 0;
        BackendHandle handle;
        uint64_t parent_uid = 0;
        std::vector<uint64_t> children;
        std::vector<IDataStore::SubscriptionId> subscriptions;
        esp_brookesia::lib_utils::signal<void()> click_signal;
        esp_brookesia::lib_utils::signal<void(const Event &)> event_signal;
        std::optional<std::string> template_id;
        bool press_lost_since_pressed = false;
    };

    struct BindingApplyMasks {
        PropsApplyMask props = PropsApplyMask::None;
        StyleApplyMask style = StyleApplyMask::None;
        LayoutApplyMask layout = LayoutApplyMask::None;
        PlacementApplyMask placement = PlacementApplyMask::None;
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
        boost::unordered_flat_map<std::string, Node> screens;
        boost::unordered_flat_map<std::string, Node> templates;
        std::vector<PreloadedImageRecord> preloaded_images;
        boost::unordered_flat_map<std::string, uint64_t> screen_roots;
        // MEMORY/FRAGMENTATION-CRITICAL: must be a NODE-based map, not unordered_flat_map.
        // NodeRecord is large (full Node definition + signals + vectors). A flat_map stores values
        // inline in one contiguous table, so every rehash (on each doubling threshold) must reallocate
        // a single huge contiguous block and MOVE all NodeRecords - measured as a ~550KB transient
        // spike on a heap whose largest free block was only ~528KB, i.e. one growth step away from OOM
        // even though total free memory was sufficient. unordered_node_map keeps each NodeRecord in its
        // own stable allocation; a rehash only reshuffles a pointer array (~8B/entry), removing both the
        // giant transient spike and the contiguous-block fragmentation hazard. Stable node addresses
        // also make NodeRecord* held across child inserts safe (see find_node_record callers).
        boost::unordered_node_map<uint64_t, NodeRecord> nodes;
        boost::unordered_flat_map<BackendHandle::Value, uint64_t> handle_to_uid;
        boost::unordered_flat_map<std::string, uint64_t> absolute_path_to_uid;
        std::vector<InstanceSnapshot> dynamic_instances;
        uint64_t next_uid = 1;
    };

    Impl(std::unique_ptr<IBackend> backend_in, RuntimeTaskConfig task_config_in = {})
        : backend(std::move(backend_in))
        , store(std::make_shared<MemoryDataStore>())
        , task_config(std::move(task_config_in))
    {
        if (backend != nullptr) {
            auto sink = [this](const BackendEvent & event) {
                if (try_dispatch_fast_action_event(event)) {
                    return;
                }
                if (task_config.task_scheduler && task_config.task_scheduler->is_running() &&
                        !task_config.event_group.empty()) {
                    auto backend_event = event;
                    auto post_result = task_config.task_scheduler->post(
                    [this, backend_event = std::move(backend_event)]() {
                        dispatch_backend_event(backend_event);
                    },
                    nullptr,
                    task_config.event_group
                                       );
                    if (!post_result) {
                        BROOKESIA_LOGW("Failed to post backend event, dispatch inline");
                        dispatch_backend_event(event);
                    }
                    return;
                }
                dispatch_backend_event(event);
            };
            backend->set_event_sink(std::move(sink));
            refresh_global_fonts_from_backend();
            for (const auto &[font_id, unused_font] : global_fonts) {
                (void)unused_font;
                font_registration_order.push_back(font_id);
            }
            std::sort(font_registration_order.begin(), font_registration_order.end());
        }
    }

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
    // Cross-instance ResolvedStyle de-duplication cache. Keyed by the style-determining fields of a
    // node (type + styleRefs + inline style/state/part). resolve_style() is independent of the tree
    // and depends only on these fields plus the global theme/language/font registries, all of which
    // bump current_style_revision_ when they change - so the cache is flushed on revision change.
    boost::unordered_flat_map<std::string, std::shared_ptr<const ResolvedStyle>> resolved_style_cache_;
    uint64_t resolved_style_cache_revision_ = 0;
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
    {
        if (subscription_id == 0 || !document_id.is_valid()) {
            return;
        }
        subscription_document_ids_[subscription_id] = document_id.value();
    }

    bool unsubscribe_subscription(SubscriptionId subscription_id)
    {
        if (subscription_registry_ == nullptr || subscription_id == 0) {
            return false;
        }

        subscription_document_ids_.erase(subscription_id);

        auto it = subscription_registry_->disconnect_handlers.find(subscription_id);
        if (it == subscription_registry_->disconnect_handlers.end()) {
            return false;
        }

        auto disconnect_handler = it->second;
        subscription_registry_->disconnect_handlers.erase(it);
        if (disconnect_handler == nullptr || !*disconnect_handler) {
            return false;
        }

        auto handler = std::move(*disconnect_handler);
        handler();
        return true;
    }

    ~Impl()
    {
        std::vector<DocumentId::Value> document_ids;
        document_ids.reserve(trees.size());
        for (const auto &[document_id, unused_tree] : trees) {
            (void)unused_tree;
            document_ids.push_back(document_id);
        }
        for (auto document_id : document_ids) {
            unload(DocumentId(document_id));
        }
    }

    std::expected<DocumentId, std::string> load(
        std::string_view file_path_view,
        Document document,
        const Environment &environment,
        bool file_backed = false,
        std::vector<std::string> dependency_files = {})
    {
        if (backend == nullptr) {
            return std::unexpected("GUI backend is null");
        }

        const std::string file_path = normalize_file_path(file_path_view);
        const auto total_start = RuntimeProfileClock::now();
        auto stage_start = total_start;
        auto validation = validate_document(document);
        auto stage_end = RuntimeProfileClock::now();
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load profile: file(%1%), stage(validate_document), elapsed_ms(%2%), total_ms(%3%)",
            file_path,
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        if (!validation.success) {
            return std::unexpected(validation.errors.empty() ? "Invalid GUI document" : validation.errors.front());
        }

        const DocumentId document_id(next_document_id_++);

        try {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_snap = []() {
                return ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            };
            auto hp_log = [document_id](const char *stage, size_t prev_ext, size_t cur_ext) {
                BROOKESIA_LOGI(
                    "[HeapTrace][gui.load.split] doc(%1%) stage(%2%) psram_free(%3%) delta(%4%)",
                    document_id.value(), stage, cur_ext,
                    static_cast<int64_t>(cur_ext) - static_cast<int64_t>(prev_ext)
                );
            };
            auto hp_entry = hp_snap();
#endif
            stage_start = RuntimeProfileClock::now();
            TreeRecord tree;
            tree.document_id = document_id;
            tree.file_path = file_path;
            tree.file_backed = file_backed;
            tree.environment_dependencies = document.environment_dependencies;
            tree.theme_sensitive = document.theme_sensitive;
            tree.constants = std::move(document.constants);
            tree.styles = std::move(document.styles);
            tree.environment = environment;
            if (!environment.language.empty()) {
                current_language = environment.language;
            }
            if (!environment.theme_id.empty()) {
                current_theme = environment.theme_id;
            }
            tree.dependency_files = normalize_dependency_files(std::move(dependency_files));
            stage_end = RuntimeProfileClock::now();
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(prepare_tree), "
                "dependencies(%3%), elapsed_ms(%4%), total_ms(%5%)",
                document_id.value(),
                file_path,
                tree.dependency_files.size(),
                runtime_profile_elapsed_ms(stage_start, stage_end),
                runtime_profile_elapsed_ms(total_start, stage_end)
            );

            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(capture_dependency_mtimes), "
                "dependencies(%3%), hits(0), skipped(true), elapsed_ms(0), total_ms(%4%)",
                document_id.value(),
                file_path,
                tree.dependency_files.size(),
                runtime_profile_elapsed_ms(total_start, RuntimeProfileClock::now())
            );

            stage_start = RuntimeProfileClock::now();
            for (auto &image : document.images) {
                auto image_id = image.id;
                BROOKESIA_LOGD(
                    "Loaded document image asset: id='%1%', src='%2%', width=%3%, height=%4%, preload=%5%",
                    image.id,
                    image.src,
                    image.width,
                    image.height,
                    image.preload
                );
                tree.images.emplace(std::move(image_id), std::move(image));
            }
            for (auto &flow : document.screen_flows) {
                auto flow_id = flow.id;
                tree.screen_flows.emplace(std::move(flow_id), std::move(flow));
            }
            for (auto &node : document.screens) {
                auto node_id = node.id;
                tree.screens.emplace(std::move(node_id), std::move(node));
            }
            for (auto &node : document.templates) {
                auto node_id = node.id;
                tree.templates.emplace(std::move(node_id), std::move(node));
            }
            stage_end = RuntimeProfileClock::now();
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(index_assets), "
                "images(%3%), flows(%4%), screens(%5%), templates(%6%), elapsed_ms(%7%), total_ms(%8%)",
                document_id.value(),
                file_path,
                tree.images.size(),
                tree.screen_flows.size(),
                tree.screens.size(),
                tree.templates.size(),
                runtime_profile_elapsed_ms(stage_start, stage_end),
                runtime_profile_elapsed_ms(total_start, stage_end)
            );

            trees.emplace(document_id.value(), std::move(tree));
            auto *stored_tree = resolve_tree(document_id);
            if (stored_tree == nullptr) {
                return std::unexpected("Failed to store GUI tree");
            }
            stage_start = RuntimeProfileClock::now();
            auto resource_validation = validate_resource_references(*stored_tree);
            stage_end = RuntimeProfileClock::now();
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(validate_resource_references), "
                "elapsed_ms(%3%), total_ms(%4%)",
                document_id.value(),
                file_path,
                runtime_profile_elapsed_ms(stage_start, stage_end),
                runtime_profile_elapsed_ms(total_start, stage_end)
            );
            if (!resource_validation) {
                unload(document_id);
                return std::unexpected(resource_validation.error());
            }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_before_preload = hp_snap();
            hp_log("after tree move+validate (H_C)", hp_entry.external_free, hp_before_preload.external_free);
#endif
            stage_start = RuntimeProfileClock::now();
            auto preload_result = preload_tree_image_resources(*stored_tree);
            stage_end = RuntimeProfileClock::now();
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(preload_image_resources), "
                "preloaded(%3%), elapsed_ms(%4%), total_ms(%5%)",
                document_id.value(),
                file_path,
                stored_tree->preloaded_images.size(),
                runtime_profile_elapsed_ms(stage_start, stage_end),
                runtime_profile_elapsed_ms(total_start, stage_end)
            );
            if (!preload_result) {
                unload(document_id);
                return std::unexpected(preload_result.error());
            }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_after_preload = hp_snap();
            hp_log("after image preload (H_A)", hp_before_preload.external_free, hp_after_preload.external_free);
#endif

            const auto eager_stage_start = RuntimeProfileClock::now();
            size_t eager_screen_count = 0;
            size_t dynamic_screen_count = 0;
            for (const auto &[unused_id, screen] : stored_tree->screens) {
                (void)unused_id;
                if (screen.mount_mode == MountMode::Dynamic) {
                    ++dynamic_screen_count;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
                    BROOKESIA_LOGI(
                        "[HeapTrace][gui.eager_mount] screen(%1%) mode(dynamic) action(skipped)", screen.id);
#endif
                    continue;
                }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
                auto hp_load_eager_before = hp_snap();
#endif
                stage_start = RuntimeProfileClock::now();
                reset_subtree_build_profile();
                auto root_uid = create_subtree(
                                    document_id,
                                    *stored_tree,
                                    screen,
                                    BackendHandle(),
                                    0,
                                    screen.id,
                                    Path(),
                                    "/" + screen.id,
                                    std::nullopt
                                );
                if (!root_uid) {
                    unload(document_id);
                    return std::unexpected(root_uid.error());
                }
                stored_tree->screen_roots.emplace(screen.id, *root_uid);
                stage_end = RuntimeProfileClock::now();
                const auto screen_build_ms = runtime_profile_elapsed_ms(stage_start, stage_end);
                ++eager_screen_count;
                log_subtree_build_profile("eager_screen_build", document_id, screen.id, screen_build_ms);
                GUI_INTERFACE_PROFILE_LOGI(
                    "GUI runtime load profile: doc(%1%), file(%2%), stage(eager_screen_build), "
                    "screen(%3%), elapsed_ms(%4%), total_ms(%5%)",
                    document_id.value(),
                    file_path,
                    screen.id,
                    screen_build_ms,
                    runtime_profile_elapsed_ms(total_start, stage_end)
                );
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
                auto hp_load_eager_after = hp_snap();
                hp_log((std::string("eager build screen ") + screen.id).c_str(),
                       hp_load_eager_before.external_free, hp_load_eager_after.external_free);
#endif
            }
            stage_end = RuntimeProfileClock::now();
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(eager_screen_build_total), "
                "eager_screens(%3%), dynamic_screens(%4%), elapsed_ms(%5%), total_ms(%6%)",
                document_id.value(),
                file_path,
                eager_screen_count,
                dynamic_screen_count,
                runtime_profile_elapsed_ms(eager_stage_start, stage_end),
                runtime_profile_elapsed_ms(total_start, stage_end)
            );

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_after_screens = hp_snap();
            hp_log("after eager screen build (H_B)", hp_after_preload.external_free, hp_after_screens.external_free);
            hp_log("total root load (H_A+H_B+H_C)", hp_entry.external_free, hp_after_screens.external_free);
#endif
            stage_end = RuntimeProfileClock::now();
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime load profile: doc(%1%), file(%2%), stage(total), elapsed_ms(%3%), total_ms(%4%)",
                document_id.value(),
                file_path,
                runtime_profile_elapsed_ms(total_start, stage_end),
                runtime_profile_elapsed_ms(total_start, stage_end)
            );
            return document_id;
        } catch (const std::bad_alloc &) {
            unload(document_id);
            return std::unexpected("Out of memory while loading GUI document: " + file_path);
        } catch (const std::exception &e) {
            unload(document_id);
            return std::unexpected(
                       "Exception while loading GUI document: " + file_path + ", error=" + std::string(e.what())
                   );
        } catch (...) {
            unload(document_id);
            return std::unexpected("Unknown exception while loading GUI document: " + file_path);
        }
    }

    std::vector<RuntimeImageResource> collect_image_resources(const TreeRecord &tree) const
    {
        boost::unordered_flat_map<std::string, std::size_t> resource_indices;
        std::vector<RuntimeImageResource> resources;
        resources.reserve(tree.images.size() + global_images.size());

        auto append_resource = [&resource_indices, &resources](RuntimeImageResource resource) {
            const auto key = image_resource_cache_key(resource);
            auto [it, inserted] = resource_indices.emplace(key, resources.size());
            if (!inserted) {
                resources[it->second].preload = resources[it->second].preload || resource.preload;
                return;
            }
            resources.push_back(std::move(resource));
        };

        for (const auto &[unused_id, image] : tree.images) {
            (void)unused_id;
            append_resource(RuntimeImageResource{
                .id = image.id,
                .primary_src = image.src,
                .native_src = 0,
                .width = image.width,
                .height = image.height,
                .preload = image.preload,
            });
        }
        for (const auto &[unused_id, image] : global_images) {
            (void)unused_id;
            append_resource(image);
        }
        return resources;
    }

    static std::string image_resource_cache_key(const RuntimeImageResource &resource)
    {
        return resource.primary_src.empty() ?
               std::string("native:") + std::to_string(resource.native_src) :
               std::string("file:") + resource.primary_src;
    }

    static bool is_same_image_resource(
        const RuntimeImageResource &lhs,
        const RuntimeImageResource &rhs)
    {
        return lhs.primary_src == rhs.primary_src && lhs.native_src == rhs.native_src;
    }

    bool should_preload_image_resource_automatically(const RuntimeImageResource &resource) const
    {
        return resource.preload || (backend != nullptr && backend->requires_preloaded_image_resource(resource));
    }

    bool has_preloaded_image_resource(const TreeRecord &tree, const RuntimeImageResource &resource) const
    {
        return std::any_of(
                   tree.preloaded_images.begin(),
                   tree.preloaded_images.end(),
        [&resource](const PreloadedImageRecord & preloaded) {
            return is_same_image_resource(preloaded.resource, resource) &&
                   (preloaded.automatic_ref_count > 0 || preloaded.manual_ref_count > 0);
        }
               );
    }

    bool has_automatic_preloaded_image_resource(const TreeRecord &tree, const RuntimeImageResource &resource) const
    {
        return std::any_of(
                   tree.preloaded_images.begin(),
                   tree.preloaded_images.end(),
        [&resource](const PreloadedImageRecord & preloaded) {
            return is_same_image_resource(preloaded.resource, resource) && preloaded.automatic_ref_count > 0;
        }
               );
    }

    static RuntimeImageResource make_runtime_image_resource(const Node &node)
    {
        const auto primary_src = node.resolved_image.primary_src.empty() ?
                                 node.image_props.src :
                                 node.resolved_image.primary_src;
        return RuntimeImageResource{
            .id = node.image_props.src,
            .primary_src = primary_src,
            .native_src = node.resolved_image.native_src,
            .width = node.resolved_image.width,
            .height = node.resolved_image.height,
        };
    }

    std::expected<void, std::string> preload_image_resource_for_tree(
        TreeRecord &tree,
        const RuntimeImageResource &resource,
        ImagePreloadOwner owner)
    {
        if (backend == nullptr) {
            return {};
        }

        auto it = std::find_if(
                      tree.preloaded_images.begin(),
                      tree.preloaded_images.end(),
        [&resource](const PreloadedImageRecord & preloaded) {
            return is_same_image_resource(preloaded.resource, resource);
        }
                  );
        if (it != tree.preloaded_images.end()) {
            if (owner == ImagePreloadOwner::Automatic) {
                ++it->automatic_ref_count;
            } else {
                ++it->manual_ref_count;
            }
            it->resource.preload = it->resource.preload || resource.preload;
            return {};
        }

        auto result = backend->preload_image_resource(resource);
        if (!result) {
            return result;
        }
        PreloadedImageRecord record {
            .resource = resource,
            .automatic_ref_count = owner == ImagePreloadOwner::Automatic ? 1U : 0U,
            .manual_ref_count = owner == ImagePreloadOwner::Manual ? 1U : 0U,
        };
        tree.preloaded_images.push_back(std::move(record));
        return {};
    }

    std::expected<void, std::string> preload_tree_image_resources(TreeRecord &tree)
    {
        for (const auto &resource : collect_image_resources(tree)) {
            if (!resource.preload) {
                continue;
            }
            auto result = preload_image_resource_for_tree(tree, resource, ImagePreloadOwner::Automatic);
            if (!result) {
                release_tree_image_resources(tree);
                return result;
            }
        }
        return {};
    }

    std::expected<void, std::string> ensure_image_resource_preloaded_for_tree(
        TreeRecord &tree,
        const RuntimeImageResource &resource)
    {
        if (!should_preload_image_resource_automatically(resource) || has_preloaded_image_resource(tree, resource)) {
            return {};
        }

        return preload_image_resource_for_tree(tree, resource, ImagePreloadOwner::Automatic);
    }

    std::expected<void, std::string> ensure_node_image_resources_preloaded(TreeRecord &tree, const Node &node)
    {
        if (node.type == NodeType::Image && !node.image_props.src.empty()) {
            const auto primary_src = node.resolved_image.primary_src.empty() ?
                                     node.image_props.src :
                                     node.resolved_image.primary_src;
            RuntimeImageResource resource{
                .id = node.resolved_image.image_id.empty() ? node.image_props.src : node.resolved_image.image_id,
                .primary_src = primary_src,
                .native_src = node.resolved_image.native_src,
                .width = node.resolved_image.width,
                .height = node.resolved_image.height,
            };
            auto result = ensure_image_resource_preloaded_for_tree(tree, resource);
            if (!result) {
                return std::unexpected(
                           "Failed to preload image resource '" + resource.id + "': " + result.error()
                       );
            }
        }

        if (node.type != NodeType::Keyboard) {
            return {};
        }
        for (const auto &[unused_mode, layout] : node.keyboard_props.layouts) {
            (void)unused_mode;
            for (const auto &row : layout.rows) {
                for (const auto &key : row) {
                    if (key.resolved_image.primary_src.empty() && key.resolved_image.native_src == 0) {
                        continue;
                    }
                    RuntimeImageResource resource{
                        .id = key.resolved_image.image_id.empty() ? key.image : key.resolved_image.image_id,
                        .primary_src = key.resolved_image.primary_src,
                        .native_src = key.resolved_image.native_src,
                        .width = key.resolved_image.width,
                        .height = key.resolved_image.height,
                    };
                    auto result = ensure_image_resource_preloaded_for_tree(tree, resource);
                    if (!result) {
                        return std::unexpected(
                                   "Failed to preload keyboard image resource '" + resource.id + "': " +
                                   result.error()
                               );
                    }
                }
            }
        }
        return {};
    }

    void release_image_resource_from_tree(
        TreeRecord &tree,
        const RuntimeImageResource &resource,
        ImagePreloadOwner owner = ImagePreloadOwner::All)
    {
        if (backend == nullptr) {
            return;
        }
        auto it = std::find_if(
                      tree.preloaded_images.begin(),
                      tree.preloaded_images.end(),
        [&resource](const PreloadedImageRecord & preloaded) {
            return is_same_image_resource(preloaded.resource, resource);
        }
                  );
        if (it == tree.preloaded_images.end()) {
            return;
        }

        if (owner == ImagePreloadOwner::Automatic) {
            if (it->automatic_ref_count > 0) {
                --it->automatic_ref_count;
            }
        } else if (owner == ImagePreloadOwner::All) {
            it->automatic_ref_count = 0;
        }
        if (owner == ImagePreloadOwner::Manual) {
            if (it->manual_ref_count > 0) {
                --it->manual_ref_count;
            }
        } else if (owner == ImagePreloadOwner::All) {
            it->manual_ref_count = 0;
        }
        if (it->automatic_ref_count > 0 || it->manual_ref_count > 0) {
            return;
        }
        backend->release_image_resource(it->resource);
        tree.preloaded_images.erase(it);
    }

    bool tree_references_image_resource_except(
        const TreeRecord &tree,
        const NodeRecord &excluded_record,
        std::string_view image_id,
        const RuntimeImageResource &resource) const
    {
        for (const auto &[unused_uid, record] : tree.nodes) {
            (void)unused_uid;
            if (record.uid == excluded_record.uid) {
                continue;
            }
            if (node_references_image(record.node, image_id)) {
                return true;
            }
            if (record.node.type == NodeType::Image && !record.node.image_props.src.empty() &&
                    is_same_image_resource(make_runtime_image_resource(record.node), resource)) {
                return true;
            }
        }
        return false;
    }

    std::expected<void, std::string> update_image_source(
        TreeRecord &tree,
        NodeRecord &record,
        std::string_view src)
    {
        if (record.node.type != NodeType::Image) {
            return std::unexpected("Node is not an image");
        }

        const auto previous_src = record.node.image_props.src;
        const auto previous_resolved_image = record.node.resolved_image;
        const RuntimeImageResource previous_resource = make_runtime_image_resource(record.node);

        record.node.image_props.src = std::string(src);
        record.node.resolved_image = {};
        resolve_image_source(tree, record.node);

        const RuntimeImageResource next_resource = make_runtime_image_resource(record.node);
        if (!record.node.image_props.src.empty() && should_preload_image_resource_automatically(next_resource) &&
                !has_preloaded_image_resource(tree, next_resource)) {
            auto preload_result =
                preload_image_resource_for_tree(tree, next_resource, ImagePreloadOwner::Automatic);
            if (!preload_result) {
                record.node.image_props.src = previous_src;
                record.node.resolved_image = previous_resolved_image;
                return std::unexpected(
                           "Failed to preload image resource '" + std::string(src) + "': " + preload_result.error()
                       );
            }
        }

        if (!previous_src.empty() && previous_src != record.node.image_props.src &&
                !is_same_image_resource(previous_resource, next_resource) &&
                !tree_references_image_resource_except(tree, record, previous_src, previous_resource)) {
            release_image_resource_from_tree(tree, previous_resource, ImagePreloadOwner::Automatic);
        }
        return {};
    }

    void release_tree_image_resources(TreeRecord &tree)
    {
        if (backend != nullptr) {
            for (const auto &record : tree.preloaded_images) {
                backend->release_image_resource(record.resource);
            }
        }
        tree.preloaded_images.clear();
    }

    std::expected<RuntimeImageResource, std::string> resolve_image_resource_by_id(
        const TreeRecord &tree,
        std::string_view image_id) const
    {
        if (image_id.empty()) {
            return std::unexpected("Image id must not be empty");
        }

        RuntimeImageResource resource;
        auto image_it = tree.images.find(std::string(image_id));
        if (image_it != tree.images.end()) {
            resource = RuntimeImageResource{
                .id = image_it->second.id,
                .primary_src = image_it->second.src,
                .native_src = 0,
                .width = image_it->second.width,
                .height = image_it->second.height,
                .preload = image_it->second.preload,
            };
        } else {
            auto global_it = global_images.find(std::string(image_id));
            if (global_it == global_images.end()) {
                return std::unexpected("Image resource is not visible to this document: " + std::string(image_id));
            }
            resource = global_it->second;
        }

        if ((resource.width <= 0 || resource.height <= 0) && backend != nullptr) {
            auto resolved = backend->resolve_image_resource(resource);
            if (!resolved) {
                return std::unexpected("Failed to resolve image resource '" + std::string(image_id) + "': " +
                                       resolved.error());
            }
            resource = std::move(*resolved);
        }
        if (resource.width <= 0 || resource.height <= 0) {
            return std::unexpected("Image resource size must be positive: " + std::string(image_id));
        }
        return resource;
    }

    std::expected<void, std::string> preload_images(DocumentId document_id, const std::vector<std::string> &image_ids)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("Document not loaded");
        }

        std::vector<RuntimeImageResource> acquired_resources;
        std::vector<std::string> refreshed_ids;
        acquired_resources.reserve(image_ids.size());
        refreshed_ids.reserve(image_ids.size());
        for (const auto &image_id : image_ids) {
            auto resource = resolve_image_resource_by_id(*tree, image_id);
            if (!resource) {
                for (const auto &acquired : acquired_resources) {
                    release_image_resource_from_tree(*tree, acquired, ImagePreloadOwner::Manual);
                }
                return std::unexpected(resource.error());
            }
            auto result = preload_image_resource_for_tree(*tree, *resource, ImagePreloadOwner::Manual);
            if (!result) {
                for (const auto &acquired : acquired_resources) {
                    release_image_resource_from_tree(*tree, acquired, ImagePreloadOwner::Manual);
                }
                return std::unexpected(result.error());
            }
            acquired_resources.push_back(std::move(*resource));
            refreshed_ids.push_back(image_id);
        }
        boost::unordered_flat_set<std::string> refreshed;
        for (const auto &image_id : refreshed_ids) {
            if (refreshed.insert(image_id).second) {
                refresh_image_references(*tree, image_id);
            }
        }
        return {};
    }

    std::expected<void, std::string> release_preloaded_images(
        DocumentId document_id,
        const std::vector<std::string> &image_ids)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("Document not loaded");
        }

        std::vector<std::pair<std::string, RuntimeImageResource>> resources;
        resources.reserve(image_ids.size());
        for (const auto &image_id : image_ids) {
            auto resource = resolve_image_resource_by_id(*tree, image_id);
            if (!resource) {
                return std::unexpected(resource.error());
            }
            resources.emplace_back(image_id, std::move(*resource));
        }
        boost::unordered_flat_set<std::string> refreshed;
        for (const auto &[image_id, resource] : resources) {
            release_image_resource_from_tree(*tree, resource, ImagePreloadOwner::Manual);
            if (refreshed.insert(image_id).second) {
                refresh_image_references(*tree, image_id);
            }
        }
        return {};
    }

    std::expected<void, std::string> enable_live_preview(DocumentId document_id, const LivePreviewOptions &options)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }
        if (!tree->file_backed || tree->file_path.empty()) {
            return std::unexpected("Live preview is only available for documents loaded via load_file(...)");
        }
        tree->live_preview_enabled = true;
        tree->live_preview_options = options;
        tree->last_live_preview_poll = std::chrono::steady_clock::time_point::min();
        tree->dependency_mtimes = capture_dependency_mtimes(tree->dependency_files);
        return {};
    }

    bool disable_live_preview(DocumentId document_id)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return false;
        }
        tree->live_preview_enabled = false;
        return true;
    }

    bool unload(DocumentId document_id)
    {
        auto tree_it = trees.find(document_id.value());
        if (tree_it == trees.end()) {
            return false;
        }

        pop_transient_screens_for_document(document_id);
        stop_screen_flows_for_document(document_id);
        const auto event_animation_prefix = std::to_string(document_id.value()) + '\x1f';
        std::vector<std::string> event_animation_keys;
        for (const auto &[key, unused_subscription_id] : event_animation_ids_) {
            (void)unused_subscription_id;
            if (key.rfind(event_animation_prefix, 0) == 0) {
                event_animation_keys.push_back(key);
            }
        }
        for (const auto &key : event_animation_keys) {
            auto animation_it = event_animation_ids_.find(key);
            if (animation_it == event_animation_ids_.end()) {
                continue;
            }
            (void)unsubscribe_subscription(animation_it->second);
            event_animation_ids_.erase(key);
        }
        unload_partial_tree(tree_it->second);
        release_tree_image_resources(tree_it->second);
        trees.erase(tree_it);

        // Drop this document's resolved-style cache entries. Their cache key is prefixed with
        // "<document_id>\x1f" (see resolve_style_shared), and the cache is otherwise only flushed on a
        // global style-revision bump - so without this, every unloaded document leaks all of its
        // ResolvedStyle entries until that bump, which never happens during normal app open/close.
        {
            const auto style_cache_prefix = std::to_string(document_id.value()) + '\x1f';
            std::vector<std::string> style_cache_keys_to_erase;
            for (const auto &[cache_key, unused_resolved] : resolved_style_cache_) {
                (void)unused_resolved;
                if (cache_key.compare(0, style_cache_prefix.size(), style_cache_prefix) == 0) {
                    style_cache_keys_to_erase.push_back(cache_key);
                }
            }
            for (const auto &cache_key : style_cache_keys_to_erase) {
                resolved_style_cache_.erase(cache_key);
            }
        }

        std::vector<std::string> mounted_targets_to_erase;
        for (const auto &[target_key, mounted_ref] : mounted_screens_) {
            if (mounted_ref.document_id == document_id) {
                mounted_targets_to_erase.push_back(target_key);
            }
        }
        for (const auto &target_key : mounted_targets_to_erase) {
            mounted_screens_.erase(target_key);
        }

        {
            std::lock_guard lock(event_action_mutex_);
            const auto prefix = build_event_action_route_prefix(document_id);
            std::vector<std::string> route_keys;
            route_keys.reserve(event_action_signals.size());
            for (const auto &[key, unused_signal] : event_action_signals) {
                (void)unused_signal;
                if (key.compare(0, prefix.size(), prefix) == 0) {
                    route_keys.push_back(key);
                }
            }
            for (const auto &key : route_keys) {
                event_action_signals.erase(key);
            }
        }
        if (store != nullptr) {
            store->forget_document(document_id);
        }
        // Release any per-document subscriptions whose owners forgot to unsubscribe (e.g.
        // fire-and-forget animations whose RuntimeAnimationStartResult was discarded).
        {
            const auto doc_value = document_id.value();
            std::vector<SubscriptionId> stale_subscriptions;
            stale_subscriptions.reserve(subscription_document_ids_.size());
            for (const auto &[subscription_id, owning_doc] : subscription_document_ids_) {
                if (owning_doc == doc_value) {
                    stale_subscriptions.push_back(subscription_id);
                }
            }
            for (auto subscription_id : stale_subscriptions) {
                (void)unsubscribe_subscription(subscription_id);
            }
        }
        return true;
    }

    void set_view_debug_enabled(bool enabled)
    {
        if (view_debug_enabled_ == enabled) {
            return;
        }
        view_debug_enabled_ = enabled;
        apply_view_debug_to_all_nodes();
    }

    bool is_view_debug_enabled() const
    {
        return view_debug_enabled_;
    }

    void poll_live_preview(Runtime *runtime)
    {
        const auto now = std::chrono::steady_clock::now();
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            if (!tree.live_preview_enabled || !tree.file_backed || tree.file_path.empty()) {
                continue;
            }

            const auto interval_ms = std::max<int32_t>(0, tree.live_preview_options.poll_interval_ms);
            const auto interval = std::chrono::milliseconds(interval_ms);
            if (tree.last_live_preview_poll != std::chrono::steady_clock::time_point::min() &&
                    now - tree.last_live_preview_poll < interval) {
                continue;
            }
            tree.last_live_preview_poll = now;

            std::string changed_file;
            bool changed = false;
            for (const auto &dependency_file : tree.dependency_files) {
                auto current_info = StorageHelper::fs_stat(dependency_file);
                if (!current_info || !current_info->exists) {
                    changed = true;
                    changed_file = dependency_file;
                    break;
                }
                auto previous_it = tree.dependency_mtimes.find(dependency_file);
                if (previous_it == tree.dependency_mtimes.end() ||
                        previous_it->second != current_info->mtime_ms) {
                    changed = true;
                    changed_file = dependency_file;
                    break;
                }
            }
            if (!changed) {
                continue;
            }

            if (tree.live_preview_options.log_reload) {
                BROOKESIA_LOGI(
                    "Live preview reload triggered: document_id=%1%, root='%2%', changed_file='%3%'",
                    tree.document_id,
                    tree.file_path,
                    changed_file
                );
            }

            const auto parse_environment = make_parse_environment(tree.environment);
            auto parsed_document = parse_document_file_with_metadata(tree.file_path, parse_environment);
            if (!parsed_document) {
                BROOKESIA_LOGW(
                    "Live preview parse failed: document_id=%1%, root='%2%', error=%3%",
                    tree.document_id,
                    tree.file_path,
                    parsed_document.error()
                );
                continue;
            }

            auto update_result = update(runtime, tree.document_id, tree.file_path, tree.environment, *parsed_document);
            if (!update_result) {
                BROOKESIA_LOGW(
                    "Live preview update failed: document_id=%1%, root='%2%', error=%3%",
                    tree.document_id,
                    tree.file_path,
                    update_result.error()
                );
                continue;
            }

            if (tree.live_preview_options.log_reload) {
                BROOKESIA_LOGI(
                    "Live preview reload applied: document_id=%1%, root='%2%'",
                    tree.document_id,
                    tree.file_path
                );
            }
        }
    }

    std::vector<GuiLayer> list_layers() const
    {
        return backend == nullptr ? std::vector<GuiLayer> {} : backend->list_layers();
    }

    std::vector<GuiDisplayInfo> list_displays() const
    {
        return backend == nullptr ? std::vector<GuiDisplayInfo> {} : backend->list_displays();
    }

    std::optional<std::string> resolve_default_display_id() const
    {
        const auto displays = list_displays();
        if (displays.empty()) {
            return std::nullopt;
        }

        for (const auto &display : displays) {
            if (display.is_default) {
                return display.id;
            }
        }

        return displays.front().id;
    }

    std::expected<MountTarget, std::string> normalize_mount_target(const MountTarget &target) const
    {
        auto normalized = target;
        if (normalized.display_id.empty()) {
            auto default_display_id = resolve_default_display_id();
            if (!default_display_id) {
                return std::unexpected("No GUI display registered");
            }
            normalized.display_id = *default_display_id;
            return normalized;
        }

        const auto displays = list_displays();
        const auto found = std::find_if(displays.begin(), displays.end(), [&](const GuiDisplayInfo & display) {
            return display.id == normalized.display_id;
        });
        if (found == displays.end()) {
            return std::unexpected("Display not found: " + normalized.display_id);
        }
        return normalized;
    }

    std::string make_mounted_screen_key(
        DocumentId document_id,
        std::string_view absolute_path,
        const MountTarget &target) const
    {
        if (target.mount_mode == MountStackMode::Stack) {
            return build_stack_mount_target_key(target.display_id, target.layer, document_id, absolute_path);
        }
        return build_replace_mount_target_key(target.display_id, target.layer);
    }

    void reorder_mounted_screens(const MountTarget &target)
    {
        if (backend == nullptr) {
            return;
        }

        std::vector<MountedScreenRef> layer_refs;
        for (const auto &[unused_key, mounted_ref] : mounted_screens_) {
            (void)unused_key;
            if (same_mount_layer(mounted_ref.target, target)) {
                layer_refs.push_back(mounted_ref);
            }
        }
        std::sort(layer_refs.begin(), layer_refs.end(), [](const MountedScreenRef & lhs, const MountedScreenRef & rhs) {
            if (lhs.target.z_order != rhs.target.z_order) {
                return lhs.target.z_order < rhs.target.z_order;
            }
            return lhs.sequence < rhs.sequence;
        });

        for (const auto &mounted_ref : layer_refs) {
            auto *tree = resolve_tree(mounted_ref.document_id);
            if (tree == nullptr) {
                continue;
            }
            const auto screen_id = trim_slashes(mounted_ref.absolute_path);
            auto screen_root_it = tree->screen_roots.find(screen_id);
            if (screen_root_it == tree->screen_roots.end()) {
                continue;
            }
            auto *record = find_node_record(*tree, screen_root_it->second);
            if (record == nullptr) {
                continue;
            }
            (void)backend->mount_screen(record->handle, mounted_ref.target);
        }
    }

    void remove_replace_screen_on_layer(const MountTarget &target)
    {
        const auto replace_key = build_replace_mount_target_key(target.display_id, target.layer);
        auto mounted_it = mounted_screens_.find(replace_key);
        if (mounted_it == mounted_screens_.end()) {
            return;
        }
        (void)unmount_screen(mounted_it->second.document_id, mounted_it->second.absolute_path);
    }

    void remove_duplicate_stack_screen(DocumentId document_id, std::string_view absolute_path, const MountTarget &target)
    {
        if (target.mount_mode != MountStackMode::Stack) {
            return;
        }
        std::vector<std::string> duplicate_keys;
        for (const auto &[target_key, mounted_ref] : mounted_screens_) {
            if (mounted_ref.target.mount_mode == MountStackMode::Stack &&
                    mounted_ref.document_id == document_id &&
                    mounted_ref.absolute_path == absolute_path &&
                    same_mount_layer(mounted_ref.target, target)) {
                duplicate_keys.push_back(target_key);
            }
        }
        for (const auto &target_key : duplicate_keys) {
            auto mounted_it = mounted_screens_.find(target_key);
            if (mounted_it == mounted_screens_.end()) {
                continue;
            }
            (void)unmount_screen(mounted_it->second.document_id, mounted_it->second.absolute_path);
        }
    }

    std::expected<View, std::string> mount_screen(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view absolute_path,
        const MountTarget &target)
    {
        const auto normalized_path = normalize_absolute_path(absolute_path);
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }
        auto refresh_result = refresh_document_if_dirty(runtime, document_id);
        if (!refresh_result) {
            return std::unexpected(refresh_result.error());
        }
        tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }

        const auto screen_id = trim_slashes(normalized_path);
        if (screen_id.empty() || screen_id.find('/') != std::string::npos) {
            return std::unexpected("Screen absolute path must be in the form '/screen_id'");
        }

        auto screen_def_it = tree->screens.find(screen_id);
        if (screen_def_it == tree->screens.end()) {
            return std::unexpected("Screen not found: " + normalized_path);
        }

        auto screen_root_it = tree->screen_roots.find(screen_id);
        if (screen_root_it == tree->screen_roots.end()) {
            if (screen_def_it->second.mount_mode != MountMode::Dynamic) {
                return std::unexpected("Screen root not found: " + normalized_path);
            }

            refresh_global_fonts_from_backend();

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_dyn_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
            const auto dynamic_stage_start = RuntimeProfileClock::now();
            reset_subtree_build_profile();
            auto root_uid = create_subtree(
                                document_id,
                                *tree,
                                screen_def_it->second,
                                BackendHandle(),
                                0,
                                screen_def_it->second.id,
                                Path(),
                                "/" + screen_def_it->second.id,
                                std::nullopt
                            );
            if (!root_uid) {
                return std::unexpected(root_uid.error());
            }
            screen_root_it = tree->screen_roots.emplace(screen_id, *root_uid).first;
            const auto dynamic_stage_end = RuntimeProfileClock::now();
            const auto dynamic_build_ms = runtime_profile_elapsed_ms(dynamic_stage_start, dynamic_stage_end);
            log_subtree_build_profile(
                "dynamic_screen_build",
                document_id,
                screen_def_it->second.id,
                dynamic_build_ms
            );
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime dynamic mount profile: doc(%1%), screen(%2%), elapsed_ms(%3%)",
                document_id.value(),
                screen_def_it->second.id,
                dynamic_build_ms
            );
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_dyn_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            BROOKESIA_LOGI(
                "[HeapTrace][gui.dynamic_mount] screen(%1%) built_on_navigation psram_delta(%2%)",
                screen_def_it->second.id,
                static_cast<int64_t>(hp_dyn_after.external_free) -
                static_cast<int64_t>(hp_dyn_before.external_free));
#endif
        }

        auto normalized_target = normalize_mount_target(target);
        if (!normalized_target) {
            return std::unexpected(normalized_target.error());
        }

        if (normalized_target->mount_mode == MountStackMode::Replace) {
            remove_replace_screen_on_layer(*normalized_target);
        } else {
            remove_duplicate_stack_screen(document_id, normalized_path, *normalized_target);
        }

        if (tree->styles_dirty) {
            (void)reapply_subtree_styles(*tree, screen_root_it->second);
        }

        auto *record = find_node_record(*tree, screen_root_it->second);
        if (record == nullptr || !backend->mount_screen(record->handle, *normalized_target)) {
            return std::unexpected("Failed to mount screen: " + normalized_path);
        }

        const auto target_key = make_mounted_screen_key(document_id, normalized_path, *normalized_target);
        mounted_screens_.insert_or_assign(target_key, MountedScreenRef{
            .document_id = document_id,
            .absolute_path = normalized_path,
            .target = *normalized_target,
            .sequence = next_mounted_screen_sequence_++,
        });
        reorder_mounted_screens(*normalized_target);
        return View(runtime, document_id, normalized_path, NodeType::Screen);
    }

    bool unmount_screen(DocumentId document_id, std::string_view absolute_path)
    {
        if (!document_id.is_valid()) {
            return false;
        }

        const auto normalized_path = normalize_absolute_path(absolute_path);
        std::vector<std::pair<std::string, MountTarget>> mounted_targets;
        for (const auto &[target_key, mounted_ref] : mounted_screens_) {
            if (mounted_ref.document_id == document_id && mounted_ref.absolute_path == normalized_path) {
                mounted_targets.emplace_back(target_key, mounted_ref.target);
            }
        }
        if (mounted_targets.empty()) {
            return false;
        }
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            for (const auto &[target_key, unused_target] : mounted_targets) {
                (void)unused_target;
                mounted_screens_.erase(target_key);
            }
            return false;
        }
        const bool result = unmount_mounted_screen(*tree, normalized_path);
        for (const auto &[target_key, unused_target] : mounted_targets) {
            (void)unused_target;
            mounted_screens_.erase(target_key);
        }
        for (const auto &[unused_key, target] : mounted_targets) {
            (void)unused_key;
            reorder_mounted_screens(target);
        }
        return result;
    }

    std::expected<TransientMountId, std::string> push_transient_screen(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view absolute_path,
        const MountTarget &target)
    {
        const auto normalized_path = normalize_absolute_path(absolute_path);
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }
        auto refresh_result = refresh_document_if_dirty(runtime, document_id);
        if (!refresh_result) {
            return std::unexpected(refresh_result.error());
        }
        tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }

        const auto screen_id = trim_slashes(normalized_path);
        if (screen_id.empty() || screen_id.find('/') != std::string::npos) {
            return std::unexpected("Screen absolute path must be in the form '/screen_id'");
        }

        auto screen_def_it = tree->screens.find(screen_id);
        if (screen_def_it == tree->screens.end()) {
            return std::unexpected("Screen not found: " + normalized_path);
        }

        auto screen_root_it = tree->screen_roots.find(screen_id);
        if (screen_root_it == tree->screen_roots.end()) {
            if (screen_def_it->second.mount_mode != MountMode::Dynamic) {
                return std::unexpected("Screen root not found: " + normalized_path);
            }

            refresh_global_fonts_from_backend();
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_tdyn_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
            const auto dynamic_stage_start = RuntimeProfileClock::now();
            reset_subtree_build_profile();
            auto root_uid = create_subtree(
                                document_id,
                                *tree,
                                screen_def_it->second,
                                BackendHandle(),
                                0,
                                screen_def_it->second.id,
                                Path(),
                                "/" + screen_def_it->second.id,
                                std::nullopt
                            );
            if (!root_uid) {
                return std::unexpected(root_uid.error());
            }
            screen_root_it = tree->screen_roots.emplace(screen_id, *root_uid).first;
            const auto dynamic_stage_end = RuntimeProfileClock::now();
            const auto dynamic_build_ms = runtime_profile_elapsed_ms(dynamic_stage_start, dynamic_stage_end);
            log_subtree_build_profile(
                "dynamic_screen_build_transient",
                document_id,
                screen_def_it->second.id,
                dynamic_build_ms
            );
            GUI_INTERFACE_PROFILE_LOGI(
                "GUI runtime dynamic mount profile: doc(%1%), screen(%2%), transient(true), elapsed_ms(%3%)",
                document_id.value(),
                screen_def_it->second.id,
                dynamic_build_ms
            );
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_tdyn_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            BROOKESIA_LOGI(
                "[HeapTrace][gui.dynamic_mount] screen(%1%) built_on_transient psram_delta(%2%)",
                screen_def_it->second.id,
                static_cast<int64_t>(hp_tdyn_after.external_free) -
                static_cast<int64_t>(hp_tdyn_before.external_free));
#endif
        }

        auto normalized_target = normalize_mount_target(target);
        if (!normalized_target) {
            return std::unexpected(normalized_target.error());
        }

        if (tree->styles_dirty) {
            (void)reapply_subtree_styles(*tree, screen_root_it->second);
        }

        auto *record = find_node_record(*tree, screen_root_it->second);
        if (record == nullptr || !backend->mount_screen(record->handle, *normalized_target)) {
            return std::unexpected("Failed to push transient screen: " + normalized_path);
        }

        const auto transient_id = TransientMountId(next_transient_mount_id_++);
        transient_screens_.insert_or_assign(transient_id.value(), TransientScreenRef{
            .document_id = document_id,
            .absolute_path = normalized_path,
            .target = *normalized_target,
        });
        return transient_id;
    }

    bool pop_transient_screen(TransientMountId id)
    {
        auto transient_it = transient_screens_.find(id.value());
        if (transient_it == transient_screens_.end()) {
            return false;
        }

        auto *tree = resolve_tree(transient_it->second.document_id);
        const auto absolute_path = transient_it->second.absolute_path;
        transient_screens_.erase(transient_it);
        if (tree == nullptr) {
            return false;
        }
        return unmount_mounted_screen(*tree, absolute_path);
    }

    void pop_transient_screens_for_document(DocumentId document_id)
    {
        std::vector<TransientMountId> transient_ids;
        for (const auto &[id, transient] : transient_screens_) {
            if (transient.document_id == document_id) {
                transient_ids.push_back(TransientMountId(id));
            }
        }
        for (auto id : transient_ids) {
            (void)pop_transient_screen(id);
        }
    }

    bool is_screen_flow_screen_mounted(const RunningScreenFlow &flow) const
    {
        auto normalized_target = normalize_mount_target(flow.target);
        if (!normalized_target) {
            return false;
        }

        return std::any_of(mounted_screens_.begin(), mounted_screens_.end(), [&](const auto & entry) {
            const auto &mounted_ref = entry.second;
            return mounted_ref.document_id == flow.document_id &&
                   mounted_ref.absolute_path == flow.current_screen &&
                   same_mount_layer(mounted_ref.target, *normalized_target) &&
                   mounted_ref.target.mount_mode == normalized_target->mount_mode;
        });
    }

    std::expected<void, std::string> start_screen_flow(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view flow_id,
        const MountTarget &target)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }
        auto flow_it = tree->screen_flows.find(std::string(flow_id));
        if (flow_it == tree->screen_flows.end()) {
            return std::unexpected("Screen flow not found: " + std::string(flow_id));
        }

        const auto flow_key = build_screen_flow_key(document_id, flow_id);
        if (running_screen_flows_.contains(flow_key)) {
            return {};
        }

        const auto initial_screen = make_screen_flow_screen_path(flow_it->second.initial);
        auto mount_result = mount_screen(runtime, document_id, initial_screen, target);
        if (!mount_result) {
            return std::unexpected(mount_result.error());
        }
        running_screen_flows_.insert_or_assign(flow_key, RunningScreenFlow{
            .document_id = document_id,
            .flow_id = std::string(flow_id),
            .current_state = flow_it->second.initial,
            .current_screen = initial_screen,
            .target = target,
        });
        return {};
    }

    std::expected<void, std::string> trigger_screen_flow(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view flow_id,
        std::string_view action)
    {
        const auto flow_key = build_screen_flow_key(document_id, flow_id);
        auto running_it = running_screen_flows_.find(flow_key);
        if (running_it == running_screen_flows_.end()) {
            return std::unexpected("Screen flow is not running: " + std::string(flow_id));
        }
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            running_screen_flows_.erase(running_it);
            return std::unexpected("GUI document not loaded");
        }
        auto flow_it = tree->screen_flows.find(std::string(flow_id));
        if (flow_it == tree->screen_flows.end()) {
            running_screen_flows_.erase(running_it);
            return std::unexpected("Screen flow not found: " + std::string(flow_id));
        }
        if (flow_it->second.transitions.empty()) {
            return std::unexpected("Screen flow has no transitions");
        }

        const auto *transition = find_screen_flow_transition(
                                     flow_it->second,
                                     running_it->second.current_state,
                                     action
                                 );
        if (transition == nullptr) {
            return std::unexpected(
                       "Screen flow transition not found: state=" + running_it->second.current_state +
                       ", action=" + std::string(action)
                   );
        }
        BROOKESIA_LOGI(
            "Screen flow transition: flow(%1%), from(%2%), action(%3%), to(%4%)",
            flow_id,
            running_it->second.current_state,
            action,
            transition->to
        );
        if (transition->to == running_it->second.current_state && is_screen_flow_screen_mounted(running_it->second)) {
            return {};
        }

        const auto old_screen = running_it->second.current_screen;
        const auto old_state = running_it->second.current_state;
        const auto target_screen = make_screen_flow_screen_path(transition->to);
        const auto target = running_it->second.target;
        (void)unmount_screen(document_id, old_screen);

        auto mount_result = mount_screen(runtime, document_id, target_screen, target);
        if (!mount_result) {
            auto restore_result = mount_screen(runtime, document_id, old_screen, target);
            if (restore_result) {
                running_it->second.current_state = old_state;
                running_it->second.current_screen = old_screen;
            }
            return std::unexpected(mount_result.error());
        }

        running_it->second.current_state = transition->to;
        running_it->second.current_screen = target_screen;
        return {};
    }

    bool stop_screen_flow(DocumentId document_id, std::string_view flow_id)
    {
        const auto flow_key = build_screen_flow_key(document_id, flow_id);
        auto running_it = running_screen_flows_.find(flow_key);
        if (running_it == running_screen_flows_.end()) {
            return false;
        }
        const auto current_screen = running_it->second.current_screen;
        running_screen_flows_.erase(running_it);
        (void)unmount_screen(document_id, current_screen);
        return true;
    }

    bool has_screen_flow(DocumentId document_id, std::string_view flow_id) const
    {
        const auto *tree = const_cast<Impl *>(this)->resolve_tree(document_id);
        return (tree != nullptr) && tree->screen_flows.contains(std::string(flow_id));
    }

    std::optional<std::string> get_screen_flow_state(DocumentId document_id, std::string_view flow_id) const
    {
        const auto flow_key = build_screen_flow_key(document_id, flow_id);
        auto running_it = running_screen_flows_.find(flow_key);
        if (running_it == running_screen_flows_.end()) {
            return std::nullopt;
        }
        return running_it->second.current_state;
    }

    void stop_screen_flows_for_document(DocumentId document_id)
    {
        std::vector<std::string> flow_ids;
        const auto prefix = std::to_string(document_id.value()) + '\x1f';
        for (const auto &[key, flow] : running_screen_flows_) {
            if (key.rfind(prefix, 0) == 0) {
                flow_ids.push_back(flow.flow_id);
            }
        }
        for (const auto &flow_id : flow_ids) {
            (void)stop_screen_flow(document_id, flow_id);
        }
    }

    View find_view(Runtime *runtime, DocumentId document_id, std::string_view absolute_path)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return View();
        }
        auto refresh_result = refresh_document_if_dirty(runtime, document_id);
        if (!refresh_result) {
            BROOKESIA_LOGW(
                "Failed to refresh dirty document before find_view: document_id=%1%, path='%2%', error=%3%",
                document_id,
                absolute_path,
                refresh_result.error()
            );
            return View();
        }
        tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return View();
        }

        const auto query = normalize_absolute_path(absolute_path);
        if (query == "/") {
            return View();
        }

        if (auto uid = resolve_any_uid(*tree, query)) {
            auto *record = find_node_record_const(*tree, *uid);
            if (record != nullptr) {
                return View(runtime, document_id, record->absolute_path, record->node.type);
            }
        }

        return View();
    }

    std::expected<View, std::string> create_view(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view template_id,
        std::string_view parent_absolute_path,
        std::string_view instance_id)
    {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        ++dbg_create_view_count_;
        auto hp_cv_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
        dbg_step_create_node_ = 0;
        dbg_step_noderecord_ = 0;
        dbg_step_apply_props_ = 0;
        dbg_step_apply_layout_ = 0;
        dbg_step_apply_placement_ = 0;
        dbg_step_apply_style_ = 0;
        dbg_step_apply_anim_ = 0;
        dbg_step_events_ = 0;
        dbg_step_subscribe_ = 0;
        dbg_step_node_copy_ = 0;
        dbg_step_resolve_style_ = 0;
        dbg_step_store_ = 0;
        const size_t dbg_subtree_before = dbg_create_subtree_count_;
#endif
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }
        auto refresh_result = refresh_document_if_dirty(runtime, document_id);
        if (!refresh_result) {
            return std::unexpected(refresh_result.error());
        }
        tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }

        auto template_it = tree->templates.find(std::string(template_id));
        if (template_it == tree->templates.end()) {
            return std::unexpected("Template not found: " + std::string(template_id));
        }

        const auto parent_query = normalize_absolute_path(parent_absolute_path);
        if (parent_query == "/") {
            return std::unexpected("Parent path must not be empty");
        }
        const auto instance_name = trim_slashes(instance_id);
        if (instance_name.empty() || instance_name.find('/') != std::string::npos) {
            return std::unexpected("Instance id must be a single path segment");
        }

        auto parent_uid = resolve_any_uid(*tree, parent_query);
        if (!parent_uid.has_value()) {
            return std::unexpected("Parent view not found: " + parent_query);
        }

        auto *parent_record = find_node_record(*tree, *parent_uid);
        if (parent_record == nullptr) {
            return std::unexpected("Parent record not found");
        }
        // Copy parent fields before create_subtree(); inserting child records can rehash tree.nodes.
        const BackendHandle parent_handle = parent_record->handle;
        const uint64_t parent_record_uid = parent_record->uid;
        const std::string parent_root_id = parent_record->root_id;
        const Path parent_path = parent_record->path;

        const auto created_absolute_path = parent_query + "/" + instance_name;
        if (tree->absolute_path_to_uid.contains(created_absolute_path)) {
            return std::unexpected("View already exists: " + created_absolute_path);
        }
        refresh_global_fonts_from_backend();
        auto root_uid = create_subtree(
                            document_id,
                            *tree,
                            template_it->second,
                            parent_handle,
                            parent_record_uid,
                            parent_root_id,
                            append_path(parent_path, instance_name),
                            created_absolute_path,
                            instance_name,
                            std::string(template_id)
                        );
        if (!root_uid) {
            return std::unexpected(root_uid.error());
        }

        auto snapshot_it = std::find_if(tree->dynamic_instances.begin(), tree->dynamic_instances.end(),
        [&created_absolute_path](const InstanceSnapshot & snapshot) {
            return snapshot.parent_absolute_path + "/" + snapshot.instance_id == created_absolute_path;
        });
        if (snapshot_it == tree->dynamic_instances.end()) {
            const auto *updated_parent_record = find_node_record_const(*tree, *parent_uid);
            const size_t sibling_order =
                (updated_parent_record == nullptr || updated_parent_record->children.empty()) ?
                0 : updated_parent_record->children.size() - 1;
            tree->dynamic_instances.push_back(InstanceSnapshot{
                .template_id = std::string(template_id),
                .parent_absolute_path = parent_query,
                .instance_id = instance_name,
                .sibling_order = sibling_order,
            });
        }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        {
            auto hp_cv_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            const size_t dbg_nodes_built = dbg_create_subtree_count_ - dbg_subtree_before;
            BROOKESIA_LOGI(
                "[HeapTrace][gui.create_view] count(%1%) template(%2%) nodes_built(%3%) "
                "psram_before(%4%) psram_after(%5%) delta(%6%)",
                dbg_create_view_count_, std::string(template_id), dbg_nodes_built,
                hp_cv_before.external_free, hp_cv_after.external_free,
                static_cast<int64_t>(hp_cv_after.external_free) - static_cast<int64_t>(hp_cv_before.external_free)
            );
            BROOKESIA_LOGI(
                "[HeapTrace][gui.create_view.breakdown] count(%1%) nodes_built(%2%) create_node(%3%) noderecord(%4%) "
                "apply_props(%5%) apply_layout(%6%) apply_placement(%7%) apply_style(%8%) apply_anim(%9%) "
                "events(%10%) subscribe(%11%)",
                dbg_create_view_count_, dbg_nodes_built, dbg_step_create_node_, dbg_step_noderecord_,
                dbg_step_apply_props_, dbg_step_apply_layout_, dbg_step_apply_placement_, dbg_step_apply_style_,
                dbg_step_apply_anim_, dbg_step_events_, dbg_step_subscribe_
            );
            BROOKESIA_LOGI(
                "[HeapTrace][gui.create_view.noderecord] count(%1%) nodes_built(%2%) node_copy(%3%) "
                "resolve_style(%4%) store(%5%)",
                dbg_create_view_count_, dbg_nodes_built, dbg_step_node_copy_,
                dbg_step_resolve_style_, dbg_step_store_
            );
            BROOKESIA_LOGI(
                "[HeapTrace][gui.style_cache] count(%1%) hits(%2%) misses(%3%) entries(%4%)",
                dbg_create_view_count_, dbg_style_cache_hits_, dbg_style_cache_misses_,
                resolved_style_cache_.size()
            );
        }
#endif
        return View(runtime, document_id, created_absolute_path, template_it->second.type);
    }

    bool destroy_view(DocumentId document_id, std::string_view absolute_path)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return false;
        }

        const auto query = normalize_absolute_path(absolute_path);
        auto uid = resolve_any_uid(*tree, query);
        if (!uid.has_value()) {
            return false;
        }

        auto *record = find_node_record(*tree, *uid);
        if (record == nullptr || record->node.type == NodeType::Screen) {
            return false;
        }

        if (record->template_id.has_value()) {
            tree->dynamic_instances.erase(
                std::remove_if(tree->dynamic_instances.begin(), tree->dynamic_instances.end(),
            [&query](const InstanceSnapshot & snapshot) {
                return snapshot.parent_absolute_path + "/" + snapshot.instance_id == query;
            }),
            tree->dynamic_instances.end()
            );
        }

        destroy_subtree(*tree, *uid);
        return true;
    }

    std::expected<void, std::string> update(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view file_path,
        const Environment &environment)
    {
        const auto parse_environment = make_parse_environment(environment);
        auto parsed_document = parse_document_file_with_metadata(file_path, parse_environment);
        if (!parsed_document) {
            return std::unexpected(parsed_document.error());
        }
        return update(runtime, document_id, file_path, environment, *parsed_document);
    }

    std::expected<void, std::string> update(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view file_path,
        const Environment &environment,
        const ParsedDocument &parsed_document)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }

        auto validation = validate_document(parsed_document.document);
        if (!validation.success) {
            return std::unexpected(validation.errors.empty() ? "Invalid GUI document" : validation.errors.front());
        }

        std::vector<RunningScreenFlowSnapshot> running_flows;
        for (const auto &[unused_key, flow] : running_screen_flows_) {
            (void)unused_key;
            if (flow.document_id == document_id) {
                running_flows.push_back(RunningScreenFlowSnapshot{
                    .document_id = flow.document_id,
                    .flow_id = flow.flow_id,
                    .current_state = flow.current_state,
                    .current_screen = flow.current_screen,
                    .target = flow.target,
                    .was_mounted = is_screen_flow_screen_mounted(flow),
                });
            }
        }
        stop_screen_flows_for_document(document_id);

        std::vector<std::string> instantiated_dynamic_screens;
        std::vector<NodeStateSnapshot> state_snapshots;
        std::vector<MountedScreenRef> mounted_refs;
        for (const auto &[unused_target_key, mounted_ref] : mounted_screens_) {
            (void)unused_target_key;
            if (mounted_ref.document_id == document_id) {
                mounted_refs.push_back(mounted_ref);
            }
        }
        pop_transient_screens_for_document(document_id);

        for (const auto &[screen_id, uid] : tree->screen_roots) {
            auto screen_it = tree->screens.find(screen_id);
            if (screen_it != tree->screens.end() && screen_it->second.mount_mode == MountMode::Dynamic) {
                instantiated_dynamic_screens.push_back("/" + screen_id);
            }
            (void)uid;
        }

        for (const auto &[uid, record] : tree->nodes) {
            (void)uid;
            std::optional<Point> runtime_position;
            if (backend != nullptr && record.node.placement.mode == PlacementMode::Absolute) {
                auto frame = backend->get_node_frame(record.handle);
                if (frame && record.node.placement.x.mode == PlacementOffsetMode::Fixed &&
                        record.node.placement.y.mode == PlacementOffsetMode::Fixed &&
                        (frame->x != record.node.placement.x.value || frame->y != record.node.placement.y.value)) {
                    runtime_position = Point{
                        .x = frame->x,
                        .y = frame->y,
                    };
                }
            }
            state_snapshots.push_back({
                .absolute_path = record.absolute_path,
                .hidden = record.node.common_props.hidden,
                .runtime_position = runtime_position,
            });

        }

        auto instance_snapshots = tree->dynamic_instances;
        std::sort(instance_snapshots.begin(), instance_snapshots.end(), [](const InstanceSnapshot & lhs, const InstanceSnapshot & rhs) {
            if (lhs.parent_absolute_path.size() != rhs.parent_absolute_path.size()) {
                return lhs.parent_absolute_path.size() < rhs.parent_absolute_path.size();
            }
            if (lhs.parent_absolute_path != rhs.parent_absolute_path) {
                return lhs.parent_absolute_path < rhs.parent_absolute_path;
            }
            if (lhs.sibling_order != rhs.sibling_order) {
                return lhs.sibling_order < rhs.sibling_order;
            }
            return lhs.instance_id < rhs.instance_id;
        });

        for (const auto &mounted_ref : mounted_refs) {
            (void)unmount_screen(mounted_ref.document_id, mounted_ref.absolute_path);
        }

        unload_partial_tree(*tree);
        release_tree_image_resources(*tree);
        tree->file_path = normalize_file_path(file_path);
        tree->file_backed = true;
        tree->environment_dependencies = parsed_document.document.environment_dependencies;
        tree->theme_sensitive = parsed_document.document.theme_sensitive;
        tree->constants = parsed_document.document.constants;
        tree->styles = parsed_document.document.styles;
        current_style_revision_++;
        tree->environment = environment;
        if (!environment.language.empty()) {
            current_language = environment.language;
        }
        if (!environment.theme_id.empty()) {
            current_theme = environment.theme_id;
        }
        tree->dependency_files = normalize_dependency_files(parsed_document.dependency_files);
        if (tree->live_preview_enabled) {
            tree->dependency_mtimes = capture_dependency_mtimes(tree->dependency_files);
        } else {
            tree->dependency_mtimes.clear();
        }
        tree->environment_dirty = false;
        tree->styles_dirty = false;
        tree->images.clear();
        tree->screen_flows.clear();
        for (const auto &flow : parsed_document.document.screen_flows) {
            tree->screen_flows.emplace(flow.id, flow);
        }
        tree->screens.clear();
        tree->templates.clear();
        for (const auto &image : parsed_document.document.images) {
            tree->images.emplace(image.id, image);
            BROOKESIA_LOGD("Updated document image asset: id='%1%', src='%2%', width=%3%, height=%4%, preload=%5%",
                           image.id, image.src, image.width, image.height, image.preload);
        }
        for (const auto &node : parsed_document.document.screens) {
            tree->screens.emplace(node.id, node);
        }
        for (const auto &node : parsed_document.document.templates) {
            tree->templates.emplace(node.id, node);
        }
        auto resource_validation = validate_resource_references(*tree);
        if (!resource_validation) {
            return std::unexpected(resource_validation.error());
        }
        auto preload_result = preload_tree_image_resources(*tree);
        if (!preload_result) {
            release_tree_image_resources(*tree);
            return std::unexpected(preload_result.error());
        }

        for (const auto &screen : parsed_document.document.screens) {
            if (screen.mount_mode == MountMode::Dynamic) {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
                BROOKESIA_LOGI(
                    "[HeapTrace][gui.eager_mount] screen(%1%) mode(dynamic) action(skipped)", screen.id);
#endif
                continue;
            }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_eager_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
            auto root_uid = create_subtree(
                                document_id,
                                *tree,
                                screen,
                                BackendHandle(),
                                0,
                                screen.id,
                                Path(),
                                "/" + screen.id,
                                std::nullopt
                            );
            if (!root_uid) {
                return std::unexpected(root_uid.error());
            }
            tree->screen_roots.emplace(screen.id, *root_uid);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            auto hp_eager_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            BROOKESIA_LOGI(
                "[HeapTrace][gui.eager_mount] screen(%1%) mode(eager) action(built) psram_delta(%2%)",
                screen.id,
                static_cast<int64_t>(hp_eager_after.external_free) -
                static_cast<int64_t>(hp_eager_before.external_free));
#endif
        }

        for (const auto &screen_path : instantiated_dynamic_screens) {
            const auto screen_id = trim_slashes(screen_path);
            if (tree->screen_roots.contains(screen_id) || !tree->screens.contains(screen_id)) {
                continue;
            }
            const auto &screen = tree->screens.at(screen_id);
            if (screen.mount_mode != MountMode::Dynamic) {
                continue;
            }
            auto root_uid = create_subtree(
                                document_id,
                                *tree,
                                screen,
                                BackendHandle(),
                                0,
                                screen.id,
                                Path(),
                                "/" + screen.id,
                                std::nullopt
                            );
            if (!root_uid) {
                return std::unexpected(root_uid.error());
            }
            tree->screen_roots.emplace(screen.id, *root_uid);
        }

        for (const auto &snapshot : instance_snapshots) {
            auto create_result = create_view(
                                     runtime,
                                     document_id,
                                     snapshot.template_id,
                                     snapshot.parent_absolute_path,
                                     snapshot.instance_id
                                 );
            if (!create_result) {
                continue;
            }
        }

        for (const auto &snapshot : state_snapshots) {
            auto view = find_view(runtime, document_id, snapshot.absolute_path);
            if (!view.valid()) {
                continue;
            }
            set_view_hidden(view, snapshot.hidden);
            if (snapshot.runtime_position && backend != nullptr) {
                auto uid = resolve_any_uid(*tree, snapshot.absolute_path);
                if (!uid) {
                    continue;
                }
                auto *record = find_node_record(*tree, *uid);
                if (record == nullptr) {
                    continue;
                }
                auto placement = record->node.placement;
                placement.x = PlacementOffset(snapshot.runtime_position->x);
                placement.y = PlacementOffset(snapshot.runtime_position->y);
                backend->apply_placement(record->handle, placement, PlacementApplyMask::Position);
            }
        }

        for (const auto &mounted_ref : mounted_refs) {
            auto mount_result = mount_screen(runtime, document_id, mounted_ref.absolute_path, mounted_ref.target);
            if (!mount_result) {
                BROOKESIA_LOGW(
                    "Failed to restore mounted screen after update: document_id=%1%, path='%2%', display_id='%3%', layer=%4%",
                    document_id,
                    mounted_ref.absolute_path,
                    mounted_ref.target.display_id,
                    BROOKESIA_DESCRIBE_ENUM_TO_STR(mounted_ref.target.layer)
                );
            }
        }

        for (const auto &flow : running_flows) {
            auto flow_it = tree->screen_flows.find(flow.flow_id);
            if (flow_it == tree->screen_flows.end()) {
                BROOKESIA_LOGW(
                    "Failed to restore screen flow after update: document_id=%1%, flow='%2%', reason=flow missing",
                    document_id,
                    flow.flow_id
                );
                continue;
            }

            auto state = flow.current_state;
            if (!screen_flow_contains_state(flow_it->second, state)) {
                state = flow_it->second.initial;
            }
            auto screen_path = make_screen_flow_screen_path(state);
            if (!flow.was_mounted) {
                running_screen_flows_.insert_or_assign(
                    build_screen_flow_key(document_id, flow.flow_id),
                RunningScreenFlow{
                    .document_id = document_id,
                    .flow_id = flow.flow_id,
                    .current_state = state,
                    .current_screen = screen_path,
                    .target = flow.target,
                }
                );
                continue;
            }

            auto mount_result = mount_screen(runtime, document_id, screen_path, flow.target);
            if (!mount_result) {
                BROOKESIA_LOGW(
                    "Failed to restore screen flow after update: document_id=%1%, flow='%2%', state='%3%', error=%4%",
                    document_id,
                    flow.flow_id,
                    state,
                    mount_result.error()
                );
                continue;
            }
            running_screen_flows_.insert_or_assign(
                build_screen_flow_key(document_id, flow.flow_id),
            RunningScreenFlow{
                .document_id = document_id,
                .flow_id = flow.flow_id,
                .current_state = state,
                .current_screen = screen_path,
                .target = flow.target,
            }
            );
        }

        return {};
    }

    size_t reapply_style_record(TreeRecord &tree, NodeRecord &record)
    {
        if (record.applied_style_revision == current_style_revision_) {
            return 0;
        }
        if (record.node.type == NodeType::Keyboard && !record.node.keyboard_props.key_style_refs.empty()) {
            resolve_keyboard_key_style_refs(tree, record.node);
            if (backend != nullptr) {
                backend->apply_props(record.handle, record.node, PropsApplyMask::KeyboardConfig);
            }
        }
        record.resolved_style = resolve_style_shared(tree, record.node);
        record.applied_style_revision = current_style_revision_;
        if (backend != nullptr) {
            backend->apply_style(record.handle, *record.resolved_style);
        }
        return 1;
    }

    size_t reapply_subtree_styles(TreeRecord &tree, uint64_t uid)
    {
        auto *record = find_node_record(tree, uid);
        if (record == nullptr) {
            return 0;
        }

        size_t applied_count = reapply_style_record(tree, *record);
        const auto children = record->children;
        for (auto child_uid : children) {
            applied_count += reapply_subtree_styles(tree, child_uid);
        }
        return applied_count;
    }

    size_t reapply_styles(TreeRecord &tree)
    {
        size_t applied_count = 0;
        for (auto &[unused_uid, record] : tree.nodes) {
            (void)unused_uid;
            record.applied_style_revision = 0;
            applied_count += reapply_style_record(tree, record);
        }
        return applied_count;
    }

    void reapply_styles_for_all_trees()
    {
        current_style_revision_++;
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            reapply_styles(tree);
            tree.styles_dirty = false;
        }
    }

    size_t reapply_mounted_styles_for_all_trees()
    {
        current_style_revision_++;
        size_t applied_count = 0;
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            // Keep preloaded but unmounted documents lazy. Applying a language font switch to every
            // retained DOM can instantiate large FreeType font caches for apps the user has not opened.
            tree.styles_dirty = true;
            applied_count += reapply_mounted_styles(tree);
        }
        return applied_count;
    }

    size_t reapply_mounted_styles(TreeRecord &tree)
    {
        boost::unordered_flat_set<uint64_t> refreshed_roots;
        size_t applied_count = 0;
        auto refresh_screen = [&](std::string_view absolute_path) {
            const auto screen_id = trim_slashes(absolute_path);
            auto screen_root_it = tree.screen_roots.find(screen_id);
            if (screen_root_it == tree.screen_roots.end() || !refreshed_roots.insert(screen_root_it->second).second) {
                return;
            }
            applied_count += reapply_subtree_styles(tree, screen_root_it->second);
        };

        for (const auto &[unused_key, mounted_ref] : mounted_screens_) {
            (void)unused_key;
            if (mounted_ref.document_id == tree.document_id) {
                refresh_screen(mounted_ref.absolute_path);
            }
        }
        for (const auto &[unused_id, transient_ref] : transient_screens_) {
            (void)unused_id;
            if (transient_ref.document_id == tree.document_id) {
                refresh_screen(transient_ref.absolute_path);
            }
        }
        return applied_count;
    }

    std::expected<void, std::string> refresh_document_environment(Runtime *runtime, DocumentId document_id)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("Document not loaded: " + std::to_string(document_id.value()));
        }
        if (!tree->file_backed || tree->file_path.empty()) {
            return std::unexpected(
                       "Lazy environment refresh requires file-backed document: " + std::to_string(document_id.value())
                   );
        }

        return update(runtime, document_id, tree->file_path, tree->environment);
    }

    std::expected<void, std::string> refresh_document_styles(DocumentId document_id)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("Document not loaded: " + std::to_string(document_id.value()));
        }

        reapply_styles(*tree);
        tree->styles_dirty = false;
        return {};
    }

    std::expected<void, std::string> refresh_document_if_dirty(Runtime *runtime, DocumentId document_id)
    {
        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("Document not loaded: " + std::to_string(document_id.value()));
        }
        if (tree->environment_dirty) {
            return refresh_document_environment(runtime, document_id);
        }
        return {};
    }

    std::expected<void, std::string> load_theme(const ThemeAsset &theme)
    {
        if (theme.id.empty()) {
            return std::unexpected("Theme id must not be empty");
        }
        const bool inserted = global_themes.emplace(theme.id, theme).second;
        if (!inserted) {
            global_themes.insert_or_assign(theme.id, theme);
        } else {
            theme_registration_order.push_back(theme.id);
        }
        if (theme.id == current_theme) {
            reapply_styles_for_all_trees();
        }
        return {};
    }

    std::vector<std::string> list_supported_themes() const
    {
        return theme_registration_order;
    }

    Environment make_parse_environment(Environment environment) const
    {
        const std::string theme_id = environment.theme_id.empty() ? current_theme : environment.theme_id;
        environment.theme_id = theme_id;
        environment.colors.clear();
        auto theme_it = global_themes.find(theme_id);
        if (theme_it != global_themes.end()) {
            environment.colors = theme_it->second.colors;
        }
        return environment;
    }

    std::expected<std::string, std::string> resolve_color_binding_value(
        const TreeRecord &tree,
        std::string_view value) const
    {
        auto color_path = parse_color_reference_path(value);
        if (!color_path.has_value()) {
            return std::string(value);
        }
        if (color_path->empty()) {
            return std::unexpected("Invalid color reference: " + std::string(value));
        }

        const std::string theme_id = tree.environment.theme_id.empty() ? current_theme : tree.environment.theme_id;
        auto theme_it = global_themes.find(theme_id);
        if (theme_it == global_themes.end() && theme_id != current_theme) {
            theme_it = global_themes.find(current_theme);
        }
        if (theme_it == global_themes.end()) {
            return std::unexpected("Current theme is not registered: " + theme_id);
        }
        auto color_it = theme_it->second.colors.find(*color_path);
        if (color_it == theme_it->second.colors.end()) {
            return std::unexpected("Failed to resolve color reference: " + std::string(value));
        }
        return color_it->second;
    }

    void resolve_style_color_field(
        const TreeRecord &tree,
        std::optional<std::string> &field,
        std::string_view field_name) const
    {
        if (!field.has_value()) {
            return;
        }
        auto resolved = resolve_color_binding_value(tree, *field);
        if (!resolved) {
            BROOKESIA_LOGW(
                "Failed to resolve style color field '%1%' value '%2%': %3%",
                field_name,
                *field,
                resolved.error()
            );
            return;
        }
        field = std::move(*resolved);
    }

    void resolve_style_color_fields(const TreeRecord &tree, Style &style) const
    {
        resolve_style_color_field(tree, style.bg_color, "bgColor");
        resolve_style_color_field(tree, style.bg_gradient_color, "bgGradientColor");
        resolve_style_color_field(tree, style.text_color, "textColor");
        resolve_style_color_field(tree, style.border_color, "borderColor");
        resolve_style_color_field(tree, style.line_color, "lineColor");
        resolve_style_color_field(tree, style.arc_color, "arcColor");
        resolve_style_color_field(tree, style.arc_gradient_color, "arcGradientColor");
        resolve_style_color_field(tree, style.shadow_color, "shadowColor");
        resolve_style_color_field(tree, style.image_recolor, "imageRecolor");
    }

    void resolve_style_set_color_fields(const TreeRecord &tree, StyleSet &style_set) const
    {
        resolve_style_color_fields(tree, style_set.style);
        for (auto &[unused_state, state_style] : style_set.state_styles) {
            (void)unused_state;
            resolve_style_color_fields(tree, state_style);
        }
        for (auto &[unused_part, part_style] : style_set.part_styles) {
            (void)unused_part;
            resolve_style_color_fields(tree, part_style.style);
            for (auto &[unused_state, state_style] : part_style.state_styles) {
                (void)unused_state;
                resolve_style_color_fields(tree, state_style);
            }
        }
    }

    std::expected<void, std::string> set_theme(
        Runtime *runtime,
        std::string_view theme_id,
        bool reapply_loaded_documents)
    {
        const auto started_at = std::chrono::steady_clock::now();
        const std::string normalized_theme_id(theme_id);
        if (normalized_theme_id.empty()) {
            return std::unexpected("Theme id must not be empty");
        }
        if (!global_themes.contains(normalized_theme_id)) {
            return std::unexpected("Theme not registered: " + normalized_theme_id);
        }
        current_theme = normalized_theme_id;
        current_style_revision_++;
        std::vector<DocumentId> document_ids;
        document_ids.reserve(trees.size());
        for (auto &[document_id, tree] : trees) {
            tree.environment.theme_id = normalized_theme_id;
            tree.environment.colors.clear();
            document_ids.emplace_back(DocumentId(document_id));
        }
        size_t reparse_count = 0;
        size_t style_apply_count = 0;
        if (reapply_loaded_documents) {
            for (const auto document_id : document_ids) {
                auto *tree = resolve_tree(document_id);
                if (tree == nullptr) {
                    continue;
                }
                if (!tree->environment_dependencies.theme) {
                    tree->styles_dirty = true;
                    style_apply_count += reapply_mounted_styles(*tree);
                    continue;
                }
                if (!tree->file_backed || tree->file_path.empty()) {
                    BROOKESIA_LOGW(
                        "Theme-sensitive non-file-backed document cannot refresh variants: document_id=%1%",
                        document_id
                    );
                    tree->styles_dirty = true;
                    style_apply_count += reapply_mounted_styles(*tree);
                    continue;
                }
                auto update_result = update(runtime, document_id, tree->file_path, tree->environment);
                if (!update_result) {
                    return update_result;
                }
                reparse_count++;
            }
        } else {
            for (auto &[unused_document_id, tree] : trees) {
                (void)unused_document_id;
                if (tree.environment_dependencies.theme && tree.file_backed && !tree.file_path.empty()) {
                    tree.environment_dirty = true;
                } else {
                    tree.styles_dirty = true;
                }
            }
        }
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started_at
                                )
                                .count();
        BROOKESIA_LOGD(
            "Runtime theme updated: id='%1%', reapply=%2%, reparsed=%3%, styled_nodes=%4%, elapsed_ms=%5%",
            normalized_theme_id,
            reapply_loaded_documents,
            reparse_count,
            style_apply_count,
            elapsed_ms
        );
        (void)elapsed_ms;
        (void)reparse_count;
        (void)style_apply_count;
        return {};
    }

    std::expected<void, std::string> set_theme(Runtime *runtime, std::string_view theme_id)
    {
        return set_theme(runtime, theme_id, true);
    }

    std::expected<void, std::string> reapply_styles(DocumentId id)
    {
        return refresh_document_styles(id);
    }

    std::string get_theme() const
    {
        return current_theme;
    }

    std::expected<void, std::string> register_font(const RuntimeFontResource &resource)
    {
        if (resource.id.empty()) {
            return std::unexpected("Font id must not be empty");
        }
        if (resource.kind != "file" && resource.kind != "imageFont") {
            return std::unexpected("Font kind must be 'file' or 'imageFont': " + resource.id);
        }
        if (resource.kind == "file" && resource.primary_src.empty() && resource.native_fonts.empty()) {
            return std::unexpected("Font resource must define primary_src or native_fonts");
        }
        if (resource.kind == "file" && resource.languages.empty()) {
            return std::unexpected("Font languages must not be empty");
        }
        boost::unordered_flat_set<std::string> unique_languages;
        for (const auto &language : resource.languages) {
            if (language.empty()) {
                return std::unexpected("Font languages must not contain empty values");
            }
            if (!unique_languages.insert(language).second) {
                return std::unexpected("Font languages must not contain duplicates: " + language);
            }
        }
        if (resource.kind == "imageFont") {
            std::vector<ImageFontSize> image_font_sizes = resource.image_font_sizes;
            if (image_font_sizes.empty() && resource.image_font_height > 0 && !resource.image_font_glyphs.empty()) {
                image_font_sizes.push_back(ImageFontSize {
                    .height = resource.image_font_height,
                    .glyphs = resource.image_font_glyphs,
                });
            }
            if (image_font_sizes.empty()) {
                return std::unexpected("imageFont resource sizes must not be empty: " + resource.id);
            }
            boost::unordered_flat_set<int32_t> heights;
            std::optional<std::vector<uint32_t>> reference_codepoints;
            for (const auto &size : image_font_sizes) {
                if (size.height <= 0) {
                    return std::unexpected("imageFont resource size height must be positive: " + resource.id);
                }
                if (size.glyphs.empty()) {
                    return std::unexpected("imageFont resource size glyphs must not be empty: " + resource.id);
                }
                if (!heights.insert(size.height).second) {
                    return std::unexpected("imageFont resource size heights must not contain duplicates: " + resource.id);
                }
                std::vector<uint32_t> size_codepoints;
                size_codepoints.reserve(size.glyphs.size());
                boost::unordered_flat_set<uint32_t> codepoints;
                for (const auto &glyph : size.glyphs) {
                    if (glyph.codepoint == 0) {
                        return std::unexpected("imageFont resource glyph codepoint must not be zero: " + resource.id);
                    }
                    if (glyph.src.empty()) {
                        return std::unexpected("imageFont resource glyph src must not be empty: " + resource.id);
                    }
                    if (!codepoints.insert(glyph.codepoint).second) {
                        return std::unexpected("imageFont resource glyph codepoint duplicated: " + resource.id);
                    }
                    size_codepoints.push_back(glyph.codepoint);
                }
                std::sort(size_codepoints.begin(), size_codepoints.end());
                if (!reference_codepoints.has_value()) {
                    reference_codepoints = std::move(size_codepoints);
                } else if (*reference_codepoints != size_codepoints) {
                    return std::unexpected("imageFont resource sizes must contain the same glyph codepoints: " + resource.id);
                }
            }
            boost::unordered_flat_set<uint32_t> codepoints;
            for (const auto &glyph : resource.image_font_glyphs) {
                if (glyph.codepoint == 0) {
                    return std::unexpected("imageFont resource glyph codepoint must not be zero: " + resource.id);
                }
                if (glyph.src.empty()) {
                    return std::unexpected("imageFont resource glyph src must not be empty: " + resource.id);
                }
                if (!codepoints.insert(glyph.codepoint).second) {
                    return std::unexpected("imageFont resource glyph codepoint duplicated: " + resource.id);
                }
            }
        }

        if (backend != nullptr && !resource.native_fonts.empty() && !backend->register_font_resource(resource)) {
            return std::unexpected("Failed to register native font resource in backend: " + resource.id);
        }

        unregistered_global_fonts.erase(resource.id);
        global_fonts.insert_or_assign(resource.id, resource);
        refresh_global_fonts_from_backend();
        if (std::find(font_registration_order.begin(), font_registration_order.end(), resource.id) == font_registration_order.end()) {
            font_registration_order.push_back(resource.id);
        }
        reapply_styles_for_all_trees();
        return {};
    }

    bool unregister_font(std::string_view id)
    {
        const std::string font_id(id);
        auto erased_count = global_fonts.erase(font_id);
        if (erased_count == 0) {
            return false;
        }
        unregistered_global_fonts.insert(font_id);
        std::erase(font_registration_order, font_id);
        for (auto it = default_fonts_by_language.begin(); it != default_fonts_by_language.end();) {
            if (it->second == font_id) {
                it = default_fonts_by_language.erase(it);
            } else {
                ++it;
            }
        }
        reapply_styles_for_all_trees();
        return true;
    }

    std::vector<std::string> list_supported_fonts(std::string_view language = {}) const
    {
        std::vector<std::string> font_ids;
        for (const auto &font_id : font_registration_order) {
            auto it = global_fonts.find(font_id);
            if (it == global_fonts.end()) {
                continue;
            }
            if (!language.empty() &&
                    std::find(it->second.languages.begin(), it->second.languages.end(), language) == it->second.languages.end()) {
                continue;
            }
            font_ids.push_back(font_id);
        }
        return font_ids;
    }

    std::vector<std::string> list_supported_languages() const
    {
        std::vector<std::string> languages;
        boost::unordered_flat_set<std::string> seen;
        for (const auto &font_id : font_registration_order) {
            auto it = global_fonts.find(font_id);
            if (it == global_fonts.end()) {
                continue;
            }
            for (const auto &language : it->second.languages) {
                if (seen.insert(language).second) {
                    languages.push_back(language);
                }
            }
        }
        return languages;
    }

    std::vector<std::string> list_supported_languages(std::string_view font_id) const
    {
        auto it = global_fonts.find(std::string(font_id));
        if (it == global_fonts.end()) {
            return {};
        }
        return it->second.languages;
    }

    std::expected<void, std::string> set_language(
        Runtime *runtime,
        std::string_view language,
        bool reapply_loaded_documents)
    {
        const auto started_at = std::chrono::steady_clock::now();
        const std::string normalized_language(language);
        if (normalized_language.empty()) {
            return std::unexpected("Language must not be empty");
        }
        const auto supported_languages = list_supported_languages();
        if (std::find(supported_languages.begin(), supported_languages.end(), normalized_language) == supported_languages.end()) {
            return std::unexpected("Language is not supported by any registered font: " + normalized_language);
        }
        current_language = normalized_language;
        current_style_revision_++;
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            tree.environment.language = normalized_language;
        }
        size_t reparse_count = 0;
        size_t style_apply_count = 0;
        if (reapply_loaded_documents) {
            std::vector<DocumentId> document_ids;
            document_ids.reserve(trees.size());
            for (const auto &[document_id, unused_tree] : trees) {
                (void)unused_tree;
                document_ids.emplace_back(DocumentId(document_id));
            }
            for (const auto document_id : document_ids) {
                auto *tree = resolve_tree(document_id);
                if (tree == nullptr) {
                    continue;
                }
                if (!tree->environment_dependencies.language) {
                    tree->styles_dirty = true;
                    style_apply_count += reapply_mounted_styles(*tree);
                    continue;
                }
                if (!tree->file_backed || tree->file_path.empty()) {
                    BROOKESIA_LOGW(
                        "Environment-sensitive non-file-backed document cannot refresh variants: document_id=%1%",
                        document_id
                    );
                    tree->styles_dirty = true;
                    style_apply_count += reapply_mounted_styles(*tree);
                    continue;
                }
                auto update_result = update(runtime, document_id, tree->file_path, tree->environment);
                if (!update_result) {
                    return update_result;
                }
                reparse_count++;
            }
        } else {
            for (auto &[unused_document_id, tree] : trees) {
                (void)unused_document_id;
                if (tree.environment_dependencies.language && tree.file_backed && !tree.file_path.empty()) {
                    tree.environment_dirty = true;
                } else {
                    tree.styles_dirty = true;
                }
            }
        }
        const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - started_at
                                )
                                .count();
        BROOKESIA_LOGD(
            "Runtime language updated: language='%1%', reapply=%2%, reparsed=%3%, styled_nodes=%4%, elapsed_ms=%5%",
            normalized_language,
            reapply_loaded_documents,
            reparse_count,
            style_apply_count,
            elapsed_ms
        );
        (void)elapsed_ms;
        (void)reparse_count;
        (void)style_apply_count;
        return {};
    }

    std::expected<void, std::string> set_language(Runtime *runtime, std::string_view language)
    {
        return set_language(runtime, language, true);
    }

    std::string get_language() const
    {
        return current_language;
    }

    std::expected<void, std::string> set_default_font_for_language(
        std::string_view language,
        std::string_view font_id)
    {
        const std::string normalized_language(language);
        const std::string normalized_font_id(font_id);
        if (normalized_language.empty()) {
            return std::unexpected("Language must not be empty");
        }
        if (normalized_font_id.empty()) {
            return std::unexpected("Font id must not be empty");
        }
        auto font_it = global_fonts.find(normalized_font_id);
        if (font_it == global_fonts.end()) {
            return std::unexpected("Font not registered: " + normalized_font_id);
        }
        if (std::find(font_it->second.languages.begin(), font_it->second.languages.end(), normalized_language) ==
                font_it->second.languages.end()) {
            return std::unexpected(
                       "Font '" + normalized_font_id + "' does not support language '" + normalized_language + "'"
                   );
        }
        default_fonts_by_language.insert_or_assign(normalized_language, normalized_font_id);
        if (normalized_language == current_language) {
            (void)reapply_mounted_styles_for_all_trees();
        }
        return {};
    }

    std::optional<std::string> get_default_font_for_language(std::string_view language) const
    {
        auto it = default_fonts_by_language.find(std::string(language));
        if (it == default_fonts_by_language.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    void refresh_image_references(TreeRecord &tree, std::string_view image_id)
    {
        for (auto &[unused_uid, record] : tree.nodes) {
            (void)unused_uid;
            if (!node_references_image(record.node, image_id)) {
                continue;
            }
            auto refreshed_node = record.node;
            resolve_image_source(tree, refreshed_node);
            if (record.node.type == NodeType::Image) {
                record.node.resolved_image = refreshed_node.resolved_image;
            } else if (record.node.type == NodeType::Keyboard) {
                record.node.keyboard_props = std::move(refreshed_node.keyboard_props);
            }
            auto preload_result = ensure_node_image_resources_preloaded(tree, record.node);
            if (!preload_result) {
                BROOKESIA_LOGW("Failed to refresh image resource '%1%': %2%", image_id, preload_result.error());
                continue;
            }
            if (backend == nullptr) {
                continue;
            }
            if (record.node.type == NodeType::Image) {
                auto apply_node = record.node;
                if (apply_node.resolved_image.primary_src.empty() && apply_node.resolved_image.native_src == 0) {
                    apply_node.image_props.src.clear();
                }
                backend->apply_props(record.handle, apply_node, PropsApplyMask::ImageSource);
                backend->apply_placement(record.handle, record.node.placement, PlacementApplyMask::Size);
            } else if (record.node.type == NodeType::Keyboard) {
                backend->apply_props(record.handle, record.node, PropsApplyMask::KeyboardConfig);
            }
        }
    }

    std::expected<void, std::string> register_image(const RuntimeImageResource &resource)
    {
        if (resource.id.empty()) {
            return std::unexpected("Image id must not be empty");
        }
        if (resource.primary_src.empty() && resource.native_src == 0) {
            return std::unexpected("Image resource must define primary_src or native_src");
        }
        RuntimeImageResource resolved_resource = resource;
        if ((resolved_resource.width <= 0 || resolved_resource.height <= 0) && backend != nullptr) {
            auto resolved = backend->resolve_image_resource(resolved_resource);
            if (!resolved) {
                return std::unexpected(
                           "Image resource size must be positive or resolvable by backend metadata: " +
                           resolved.error()
                       );
            }
            resolved_resource = std::move(*resolved);
        }
        if (resolved_resource.width <= 0 || resolved_resource.height <= 0) {
            return std::unexpected("Image resource size must be positive");
        }
        std::optional<RuntimeImageResource> old_resource;
        if (auto old_it = global_images.find(resolved_resource.id); old_it != global_images.end()) {
            old_resource = old_it->second;
        }
        std::vector<TreeRecord *> affected_trees;
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            bool affected = false;
            for (auto &[unused_uid, record] : tree.nodes) {
                (void)unused_uid;
                if (!node_references_image(record.node, resolved_resource.id)) {
                    continue;
                }
                affected = true;
                break;
            }
            if (affected) {
                affected_trees.push_back(&tree);
            }
        }
        std::vector<TreeRecord *> preloaded_trees;
        const bool new_requires_auto_preload = should_preload_image_resource_automatically(resolved_resource);
        if (new_requires_auto_preload) {
            for (auto *tree : affected_trees) {
                if (has_automatic_preloaded_image_resource(*tree, resolved_resource)) {
                    continue;
                }
                auto preload_result =
                    preload_image_resource_for_tree(*tree, resolved_resource, ImagePreloadOwner::Automatic);
                if (!preload_result) {
                    for (auto *preloaded_tree : preloaded_trees) {
                        release_image_resource_from_tree(
                            *preloaded_tree, resolved_resource, ImagePreloadOwner::Automatic);
                    }
                    return std::unexpected(preload_result.error());
                }
                preloaded_trees.push_back(tree);
            }
        }
        const bool replaced_resource = old_resource.has_value() &&
                                       !is_same_image_resource(*old_resource, resolved_resource);
        const bool old_requires_auto_preload =
            old_resource.has_value() && should_preload_image_resource_automatically(*old_resource);
        if (replaced_resource) {
            for (auto *tree : affected_trees) {
                release_image_resource_from_tree(*tree, *old_resource);
            }
        } else if (old_requires_auto_preload && !new_requires_auto_preload) {
            for (auto *tree : affected_trees) {
                release_image_resource_from_tree(*tree, *old_resource, ImagePreloadOwner::Automatic);
            }
        }
        global_images.insert_or_assign(resolved_resource.id, resolved_resource);
        for (auto *tree : affected_trees) {
            refresh_image_references(*tree, resolved_resource.id);
        }
        return {};
    }

    bool unregister_image(std::string_view id)
    {
        const std::string image_id(id);
        auto image_it = global_images.find(image_id);
        if (image_it == global_images.end()) {
            return false;
        }
        const auto old_resource = image_it->second;
        global_images.erase(image_it);
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            release_image_resource_from_tree(tree, old_resource);
            refresh_image_references(tree, image_id);
        }
        return true;
    }

    std::expected<void, std::string> load_theme_json(std::string_view json, std::string_view base_dir)
    {
        auto theme = parse_theme_asset_json(json, base_dir);
        if (!theme) {
            return std::unexpected(theme.error());
        }
        return load_theme(*theme);
    }

    std::expected<void, std::string> load_theme_file(std::string_view path)
    {
        auto theme = parse_theme_asset_file(path);
        if (!theme) {
            return std::unexpected(theme.error());
        }
        return load_theme(*theme);
    }

    std::expected<void, std::string> register_font_json(std::string_view json, std::string_view base_dir)
    {
        auto fonts = parse_font_asset_set_json(json, base_dir);
        if (!fonts) {
            return std::unexpected(fonts.error());
        }
        std::vector<std::pair<std::string, std::optional<RuntimeFontResource>>> previous_resources;
        for (const auto &font : *fonts) {
            std::optional<RuntimeFontResource> previous_resource;
            if (const auto old_it = global_fonts.find(font.id); old_it != global_fonts.end()) {
                previous_resource = old_it->second;
            }
            previous_resources.emplace_back(font.id, std::move(previous_resource));
            auto result = register_font(RuntimeFontResource{
                .id = font.id,
                .kind = font.kind,
                .primary_src = font.src,
                .languages = font.languages,
                .fallback_ids = font.fallbacks,
                .native_fonts = {},
                .image_font_height = font.height,
                .image_font_glyphs = font.glyphs,
                .image_font_sizes = font.sizes,
            });
            if (result) {
                continue;
            }
            for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
                if (it->second.has_value()) {
                    auto rollback_result = register_font(*it->second);
                    if (!rollback_result) {
                        BROOKESIA_LOGW(
                            "Failed to rollback font resource '%1%': %2%",
                            it->first,
                            rollback_result.error()
                        );
                    }
                } else {
                    unregister_font(it->first);
                }
            }
            return std::unexpected("Failed to register font asset set: " + result.error());
        }
        return {};
    }

    std::expected<void, std::string> register_font_file(std::string_view path)
    {
        auto fonts = parse_font_asset_set_file(path);
        if (!fonts) {
            return std::unexpected(fonts.error());
        }
        std::vector<std::pair<std::string, std::optional<RuntimeFontResource>>> previous_resources;
        for (const auto &font : *fonts) {
            std::optional<RuntimeFontResource> previous_resource;
            if (const auto old_it = global_fonts.find(font.id); old_it != global_fonts.end()) {
                previous_resource = old_it->second;
            }
            previous_resources.emplace_back(font.id, std::move(previous_resource));
            auto result = register_font(RuntimeFontResource{
                .id = font.id,
                .kind = font.kind,
                .primary_src = font.src,
                .languages = font.languages,
                .fallback_ids = font.fallbacks,
                .native_fonts = {},
                .image_font_height = font.height,
                .image_font_glyphs = font.glyphs,
                .image_font_sizes = font.sizes,
            });
            if (result) {
                continue;
            }
            for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
                if (it->second.has_value()) {
                    auto rollback_result = register_font(*it->second);
                    if (!rollback_result) {
                        BROOKESIA_LOGW(
                            "Failed to rollback font resource '%1%': %2%",
                            it->first,
                            rollback_result.error()
                        );
                    }
                } else {
                    unregister_font(it->first);
                }
            }
            return std::unexpected(
                       "Failed to register font asset set file '" + std::string(path) + "': " + result.error()
                   );
        }
        return {};
    }

    std::expected<void, std::string> register_image_json(std::string_view json, std::string_view base_dir)
    {
        auto images = parse_image_asset_set_json(json, base_dir);
        if (!images) {
            return std::unexpected(images.error());
        }
        std::vector<std::pair<std::string, std::optional<RuntimeImageResource>>> previous_resources;
        for (const auto &image : *images) {
            std::optional<RuntimeImageResource> previous_resource;
            if (const auto old_it = global_images.find(image.id); old_it != global_images.end()) {
                previous_resource = old_it->second;
            }
            previous_resources.emplace_back(image.id, std::move(previous_resource));
            auto result = register_image(RuntimeImageResource{
                .id = image.id,
                .primary_src = image.src,
                .native_src = 0,
                .width = image.width,
                .height = image.height,
                .preload = image.preload,
            });
            if (result) {
                continue;
            }
            for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
                if (it->second.has_value()) {
                    auto rollback_result = register_image(*it->second);
                    if (!rollback_result) {
                        BROOKESIA_LOGW(
                            "Failed to rollback image resource '%1%': %2%",
                            it->first,
                            rollback_result.error()
                        );
                    }
                } else {
                    unregister_image(it->first);
                }
            }
            return std::unexpected("Failed to register image asset set: " + result.error());
        }
        return {};
    }

    std::expected<void, std::string> register_image_file(std::string_view path)
    {
        auto images = parse_image_asset_set_file(path);
        if (!images) {
            return std::unexpected(images.error());
        }
        std::vector<std::pair<std::string, std::optional<RuntimeImageResource>>> previous_resources;
        for (const auto &image : *images) {
            std::optional<RuntimeImageResource> previous_resource;
            if (const auto old_it = global_images.find(image.id); old_it != global_images.end()) {
                previous_resource = old_it->second;
            }
            previous_resources.emplace_back(image.id, std::move(previous_resource));
            auto result = register_image(RuntimeImageResource{
                .id = image.id,
                .primary_src = image.src,
                .native_src = 0,
                .width = image.width,
                .height = image.height,
                .preload = image.preload,
            });
            if (result) {
                continue;
            }
            for (auto it = previous_resources.rbegin(); it != previous_resources.rend(); ++it) {
                if (it->second.has_value()) {
                    auto rollback_result = register_image(*it->second);
                    if (!rollback_result) {
                        BROOKESIA_LOGW(
                            "Failed to rollback image resource '%1%': %2%",
                            it->first,
                            rollback_result.error()
                        );
                    }
                } else {
                    unregister_image(it->first);
                }
            }
            return std::unexpected(
                       "Failed to register image asset set file '" + std::string(path) + "': " + result.error()
                   );
        }
        return {};
    }

    std::expected<boost::json::value, std::string> get_constant_value(
        DocumentId document_id,
        std::string_view path) const
    {
        const auto *tree = resolve_tree_const(document_id);
        if (tree == nullptr) {
            return std::unexpected("GUI document not loaded");
        }
        return get_json_value_by_dot_path(tree->constants, path);
    }

    std::vector<RuntimeFontResource> list_font_resources(DocumentId document_id) const
    {
        const auto *tree = resolve_tree_const(document_id);
        if (tree == nullptr) {
            return {};
        }

        boost::unordered_flat_map<std::string, RuntimeFontResource> merged_fonts;
        for (const auto &[font_id, font] : global_fonts) {
            merged_fonts.insert_or_assign(font_id, font);
        }

        std::vector<RuntimeFontResource> resources;
        resources.reserve(merged_fonts.size());
        for (auto &[unused_font_id, font] : merged_fonts) {
            (void)unused_font_id;
            resources.push_back(std::move(font));
        }
        std::sort(resources.begin(), resources.end(), [](const RuntimeFontResource & lhs, const RuntimeFontResource & rhs) {
            return lhs.id < rhs.id;
        });
        return resources;
    }

    std::vector<RuntimeImageResource> list_image_resources(DocumentId document_id) const
    {
        const auto *tree = resolve_tree_const(document_id);
        if (tree == nullptr) {
            return {};
        }

        boost::unordered_flat_map<std::string, RuntimeImageResource> merged_images;
        for (const auto &[image_id, image] : global_images) {
            merged_images.insert_or_assign(image_id, image);
        }
        for (const auto &[image_id, image] : tree->images) {
            merged_images.insert_or_assign(image_id, RuntimeImageResource {
                .id = image.id,
                .primary_src = image.src,
                .native_src = 0,
                .width = image.width,
                .height = image.height,
                .preload = image.preload,
            });
        }

        std::vector<RuntimeImageResource> resources;
        resources.reserve(merged_images.size());
        for (auto &[unused_image_id, image] : merged_images) {
            (void)unused_image_id;
            resources.push_back(std::move(image));
        }
        std::sort(resources.begin(), resources.end(), [](const RuntimeImageResource & lhs, const RuntimeImageResource & rhs) {
            return lhs.id < rhs.id;
        });
        return resources;
    }

    bool is_view_valid(const View &view) const
    {
        return resolve_view_uid(view).has_value();
    }

    std::string get_view_id(const View &view) const
    {
        auto *record = resolve_view_record(view);
        return record == nullptr ? std::string() : record->node.id;
    }

    std::optional<ViewStateValue> get_view_state_internal(const View &view, ViewStateKind kind) const
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr) {
            return std::nullopt;
        }

        switch (kind) {
        case ViewStateKind::CommonProps:
            return ViewStateValue(record->node.common_props);
        case ViewStateKind::TypedProps:
            switch (record->node.type) {
            case NodeType::Label:
                return ViewStateValue(TypedPropsVariant(record->node.label_props));
            case NodeType::Image:
                return ViewStateValue(TypedPropsVariant(record->node.image_props));
            case NodeType::FrameView:
                return ViewStateValue(TypedPropsVariant(record->node.frame_view_props));
            case NodeType::TextInput:
                return ViewStateValue(TypedPropsVariant(record->node.text_input_props));
            case NodeType::Slider:
            case NodeType::ProgressBar:
            case NodeType::Arc:
                return ViewStateValue(TypedPropsVariant(record->node.range_props));
            case NodeType::Switch:
            case NodeType::Checkbox:
                return ViewStateValue(TypedPropsVariant(record->node.toggle_props));
            case NodeType::Dropdown:
                return ViewStateValue(TypedPropsVariant(record->node.dropdown_props));
            case NodeType::Table:
                return ViewStateValue(TypedPropsVariant(record->node.table_props));
            case NodeType::Line:
                return ViewStateValue(TypedPropsVariant(record->node.line_props));
            case NodeType::Keyboard:
                return ViewStateValue(TypedPropsVariant(record->node.keyboard_props));
            case NodeType::Canvas:
                return ViewStateValue(TypedPropsVariant(record->node.canvas_props));
            case NodeType::Screen:
            case NodeType::Container:
            case NodeType::Button:
            case NodeType::Spinner:
            case NodeType::Max:
                return std::nullopt;
            }
            return std::nullopt;
        case ViewStateKind::Layout:
            return ViewStateValue(record->node.layout);
        case ViewStateKind::Placement:
            return ViewStateValue(record->node.placement);
        case ViewStateKind::Style:
            return ViewStateValue(record->resolved_style->style);
        case ViewStateKind::ResolvedFont:
            return ViewStateValue(record->resolved_style->resolved_font);
        case ViewStateKind::ResolvedImage:
            if (record->node.type != NodeType::Image) {
                return std::nullopt;
            }
            return ViewStateValue(record->node.resolved_image);
        case ViewStateKind::Frame:
            if (backend == nullptr) {
                return std::nullopt;
            }
            if (auto frame = backend->get_node_frame(record->handle)) {
                return ViewStateValue(*frame);
            }
            return std::nullopt;
        case ViewStateKind::Max:
            return std::nullopt;
        }

        return std::nullopt;
    }

    bool set_view_hidden(const View &view, bool hidden)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr) {
            return false;
        }
        record->node.common_props.hidden = hidden;
        backend->apply_props(record->handle, record->node, PropsApplyMask::CommonHidden);
        return true;
    }

    bool scroll_view_to(const View &view, int32_t x, int32_t y, bool animated)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || backend == nullptr) {
            return false;
        }
        return backend->scroll_node_to(record->handle, x, y, animated);
    }

    bool scroll_view_to_visible(const View &view, bool animated)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || backend == nullptr) {
            return false;
        }
        return backend->scroll_node_to_visible(record->handle, animated);
    }

    bool set_view_text(const View &view, std::string_view text)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || (record->node.type != NodeType::Label && record->node.type != NodeType::TextInput &&
                                  record->node.type != NodeType::Checkbox)) {
            return false;
        }
        if (record->node.type == NodeType::TextInput) {
            record->node.text_input_props.text = std::string(text);
        } else {
            record->node.label_props.text = std::string(text);
        }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_text_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
        backend->apply_props(
            record->handle,
            record->node,
            record->node.type == NodeType::TextInput ? PropsApplyMask::TextInputText : PropsApplyMask::LabelText
        );
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        {
            auto hp_text_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            ++dbg_set_view_text_count_;
            BROOKESIA_LOGI(
                "[HeapTrace][gui.set_view_text] count(%1%) text_len(%2%) psram_before(%3%) psram_after(%4%) delta(%5%)",
                dbg_set_view_text_count_, text.size(), hp_text_before.external_free, hp_text_after.external_free,
                static_cast<int64_t>(hp_text_after.external_free) - static_cast<int64_t>(hp_text_before.external_free)
            );
        }
#endif
        return true;
    }

    std::string get_view_text(const View &view) const
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr) {
            return {};
        }
        if (record->node.type == NodeType::Label || record->node.type == NodeType::Checkbox) {
            return record->node.label_props.text;
        }
        if (record->node.type == NodeType::TextInput) {
            return record->node.text_input_props.text;
        }
        return {};
    }

    bool set_view_src(const View &view, std::string_view src)
    {
        auto *record = resolve_view_record(view);
        auto *tree = resolve_tree(view.document_id_);
        if (record == nullptr || tree == nullptr || record->node.type != NodeType::Image) {
            return false;
        }
        const auto previous_src = record->node.image_props.src;
        const auto previous_resolved_image = record->node.resolved_image;
        const auto previous_image_width = record->node.resolved_image.width;
        const auto previous_image_height = record->node.resolved_image.height;
        auto update_result = update_image_source(*tree, *record, src);
        if (!update_result) {
            BROOKESIA_LOGW("Reject image source: %1%", update_result.error());
            return false;
        }
        const auto resolved_src = record->node.resolved_image.primary_src.empty() ?
                                  record->node.image_props.src :
                                  record->node.resolved_image.primary_src;
        const RuntimeImageResource resource {
            .id = record->node.image_props.src,
            .primary_src = resolved_src,
            .native_src = record->node.resolved_image.native_src,
            .width = record->node.resolved_image.width,
            .height = record->node.resolved_image.height,
        };
        auto preload_result = ensure_image_resource_preloaded_for_tree(*tree, resource);
        if (!preload_result) {
            BROOKESIA_LOGW(
                "Reject image source because preload failed: path='%1%', error=%2%",
                resolved_src,
                preload_result.error()
            );
            record->node.image_props.src = previous_src;
            record->node.resolved_image = previous_resolved_image;
            return false;
        }
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        auto hp_src_before = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
#endif
        backend->apply_props(record->handle, record->node, PropsApplyMask::ImageSource);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        {
            auto hp_src_after = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            ++dbg_set_view_src_count_;
            BROOKESIA_LOGI(
                "[HeapTrace][gui.set_view_src] count(%1%) src(%2%) psram_before(%3%) psram_after(%4%) delta(%5%)",
                dbg_set_view_src_count_, resolved_src, hp_src_before.external_free, hp_src_after.external_free,
                static_cast<int64_t>(hp_src_after.external_free) - static_cast<int64_t>(hp_src_before.external_free)
            );
        }
#endif
        const bool needs_placement_reapply = (
                record->node.placement.width.mode != SizeMode::Fixed ||
                record->node.placement.height.mode != SizeMode::Fixed ||
                previous_image_width != record->node.resolved_image.width ||
                previous_image_height != record->node.resolved_image.height
                                             );
        if (needs_placement_reapply) {
            backend->apply_placement(record->handle, record->node.placement, PlacementApplyMask::Size);
        }
        return true;
    }

    std::string get_view_src(const View &view) const
    {
        auto *record = resolve_view_record(view);
        return record == nullptr ? std::string() : record->node.image_props.src;
    }

    bool set_view_value(const View &view, int32_t value)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || (record->node.type != NodeType::Slider && record->node.type != NodeType::ProgressBar &&
                                  record->node.type != NodeType::Arc)) {
            return false;
        }
        record->node.range_props.value = value;
        backend->apply_props(record->handle, record->node, PropsApplyMask::RangeValue);
        return true;
    }

    int32_t get_view_value(const View &view) const
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || (record->node.type != NodeType::Slider && record->node.type != NodeType::ProgressBar &&
                                  record->node.type != NodeType::Arc)) {
            return 0;
        }
        return record->node.range_props.value;
    }

    bool set_view_checked(const View &view, bool checked)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || (record->node.type != NodeType::Switch && record->node.type != NodeType::Checkbox)) {
            return false;
        }
        record->node.toggle_props.checked = checked;
        backend->apply_props(record->handle, record->node, PropsApplyMask::ToggleChecked);
        return true;
    }

    bool get_view_checked(const View &view) const
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || (record->node.type != NodeType::Switch && record->node.type != NodeType::Checkbox)) {
            return false;
        }
        return record->node.toggle_props.checked;
    }

    bool set_view_selected_index(const View &view, int32_t index)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || record->node.type != NodeType::Dropdown || index < 0) {
            return false;
        }
        record->node.dropdown_props.selected_index = index;
        backend->apply_props(record->handle, record->node, PropsApplyMask::DropdownSelectedIndex);
        return true;
    }

    int32_t get_view_selected_index(const View &view) const
    {
        auto *record = resolve_view_record(view);
        return record != nullptr && record->node.type == NodeType::Dropdown ?
               record->node.dropdown_props.selected_index : 0;
    }

    bool set_table_cell_text(const View &view, int32_t row, int32_t column, std::string_view text)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || record->node.type != NodeType::Table || row < 0 || column < 0) {
            return false;
        }
        auto &cells = record->node.table_props.cells;
        auto cell_it = std::find_if(cells.begin(), cells.end(), [row, column](const TableCell & cell) {
            return cell.row == row && cell.column == column;
        });
        if (cell_it == cells.end()) {
            cells.push_back(TableCell{.row = row, .column = column, .text = std::string(text)});
        } else {
            cell_it->text = std::string(text);
        }
        backend->apply_props(record->handle, record->node, PropsApplyMask::TableCells);
        return true;
    }

    esp_brookesia::lib_utils::connection connect_view_event(const View &view, EventType type, View::EventHandler handler)
    {
        auto *record = resolve_view_record(view);
        if (record == nullptr || type == EventType::Max) {
            return esp_brookesia::lib_utils::connection();
        }
        return record->event_signal.connect([type, handler = std::move(handler)](const Event & event) {
            if (handler && event.type == type) {
                handler(event);
            }
        });
    }

    esp_brookesia::lib_utils::connection connect_button_click(const Button &button, Button::ClickHandler handler)
    {
        auto *record = resolve_view_record(button);
        if (record == nullptr || record->node.type != NodeType::Button) {
            return esp_brookesia::lib_utils::connection();
        }
        return record->click_signal.connect(handler);
    }

    std::expected<esp_brookesia::lib_utils::connection, std::string> connect_event_action_signal(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view action,
        Runtime::ActionHandler handler
    )
    {
#if defined(__EMSCRIPTEN__)
        (void)runtime;
#endif
        if (!document_id.is_valid() || !handler) {
            return std::unexpected("invalid subscription request");
        }

        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            const auto error = "document not loaded";
            BROOKESIA_LOGW(
                "Failed to connect event action handler: document not loaded (document_id=%1%, action='%2%')",
                document_id,
                action
            );
            return std::unexpected(error);
        }
#if !defined(__EMSCRIPTEN__)
        auto refresh_result = refresh_document_if_dirty(runtime, document_id);
        if (!refresh_result) {
            BROOKESIA_LOGW(
                "Failed to refresh dirty document before connecting event action handler: "
                "document_id=%1%, action='%2%', error=%3%",
                document_id,
                action,
                refresh_result.error()
            );
            return std::unexpected(refresh_result.error());
        }
#endif
        tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return std::unexpected("document not loaded");
        }

        if (action.empty()) {
            const auto error = "invalid target";
            BROOKESIA_LOGW(
                "Failed to connect event action handler: invalid target (document_id=%1%, action='%2%')",
                document_id,
                action
            );
            return std::unexpected(error);
        }

        std::lock_guard lock(event_action_mutex_);
        auto &signal = event_action_signals[build_event_action_route_key(document_id, action)];
        auto connection = signal.connect(handler);
        return connection;
    }

    SubscriptionId subscribe_event_action_with_id(
        Runtime *runtime,
        DocumentId document_id,
        std::string_view action,
        Runtime::ActionHandler handler
    )
    {
        auto connection = connect_event_action_signal(runtime, document_id, action, std::move(handler));
        if (!connection) {
            return 0;
        }
        const auto subscription_id = next_subscription_id_++;
        auto disconnect_handler = std::make_shared<std::function<void()>>();
        *disconnect_handler = [registry = std::weak_ptr<SubscriptionRegistry>(subscription_registry_),
                                        subscription_id,
                 connection = std::move(*connection)]() mutable {
            connection.disconnect();
            if (auto locked_registry = registry.lock(); locked_registry != nullptr)
            {
                locked_registry->disconnect_handlers.erase(subscription_id);
            }
        };
        subscription_registry_->disconnect_handlers[subscription_id] = disconnect_handler;
        register_document_subscription(subscription_id, document_id);
        return subscription_id;
    }

    RuntimeAnimationStartResult start_view_animation_with_result(
        const View &view,
        const Animation &animation,
        Runtime::AnimationCompletedHandler completed_handler
    )
    {
        RuntimeAnimationStartResult start_result;
        if (backend == nullptr) {
            return start_result;
        }

        auto *record = resolve_view_record(view);
        if (record == nullptr) {
            return start_result;
        }

        const auto subscription_id = next_subscription_id_++;
        // Wrap completion handler so the registry/document maps are released automatically
        // when the animation finishes naturally. Many callers (e.g. launch transitions) discard
        // the returned subscription_id, so without this, every completed animation would leak an
        // entry in subscription_registry_->disconnect_handlers and subscription_document_ids_.
        auto self_releasing_handler = [registry = std::weak_ptr<SubscriptionRegistry>(subscription_registry_),
                                                doc_ids_self = this,
                                                subscription_id,
                 user_handler = std::move(completed_handler)]() mutable {
            if (user_handler)
            {
                user_handler();
            }
            // Snapshot captures into locals before any erase. Erasing the entry from
            // disconnect_handlers below releases the ScopedConnection that owns this very
            // handler, destroying this closure mid-execution. After that point no capture
            // (doc_ids_self, subscription_id, registry) may be touched, so resolve them now
            // and perform the self-owning erase last.
            auto *const impl_self = doc_ids_self;
            const auto id = subscription_id;
            auto locked_registry = registry.lock();
            if (impl_self != nullptr)
            {
                impl_self->subscription_document_ids_.erase(id);
            }
            if (locked_registry != nullptr)
            {
                locked_registry->disconnect_handlers.erase(id);
            }
        };

        auto backend_result = backend->start_animation(record->handle, animation, std::move(self_releasing_handler));
        if (!backend_result || !backend_result->connection.connected()) {
            return start_result;
        }

        auto connection = std::make_shared<ScopedConnection>(std::move(backend_result->connection));
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
        register_document_subscription(subscription_id, record->document_id);
        start_result.subscription_id = subscription_id;
        start_result.resolved_from = backend_result->resolved_from;
        start_result.resolved_to = backend_result->resolved_to;
        return start_result;
    }

    SubscriptionId start_view_animation_with_id(
        const View &view,
        const Animation &animation,
        Runtime::AnimationCompletedHandler completed_handler
    )
    {
        return start_view_animation_with_result(view, animation, std::move(completed_handler)).subscription_id;
    }

    ScopedConnection start_view_animation(
        const View &view,
        const Animation &animation,
        Runtime::AnimationCompletedHandler completed_handler
    )
    {
        if (backend == nullptr) {
            return {};
        }

        auto *record = resolve_view_record(view);
        if (record == nullptr) {
            return {};
        }

        auto backend_result = backend->start_animation(record->handle, animation, std::move(completed_handler));
        if (!backend_result) {
            return {};
        }
        return std::move(backend_result->connection);
    }

    bool dispatch_event_action_handlers(const Event &event)
    {
        std::lock_guard lock(event_action_mutex_);
        auto it = event_action_signals.find(build_event_action_route_key(event.document_id, event.action));
        if (it == event_action_signals.end()) {
            return false;
        }
        it->second(event);
        return true;
    }

    void register_fast_action_routes(const NodeRecord &record)
    {
        if (!task_config.enable_fast_action_dispatch || !record.handle.is_valid()) {
            return;
        }

        std::lock_guard lock(fast_action_mutex_);
        for (const auto &event : record.node.events) {
            if (!is_fast_action_event_type(event.type) || event.action.empty() || !event.effects.empty()) {
                continue;
            }
            fast_action_events_[build_fast_action_route_key(record.handle, event.type, event.action)] = Event{
                .document_id = record.document_id,
                .root_id = record.root_id,
                .node_id = record.node.id,
                .path = record.absolute_path,
                .type = event.type,
                .action = event.action,
                .payload = {},
            };
        }
    }

    void unregister_fast_action_routes(const NodeRecord &record)
    {
        if (!task_config.enable_fast_action_dispatch || !record.handle.is_valid()) {
            return;
        }

        std::lock_guard lock(fast_action_mutex_);
        for (const auto &event : record.node.events) {
            if (!is_fast_action_event_type(event.type) || event.action.empty() || !event.effects.empty()) {
                continue;
            }
            fast_action_events_.erase(build_fast_action_route_key(record.handle, event.type, event.action));
        }
    }

    bool try_dispatch_fast_action_event(const BackendEvent &event)
    {
        if (!task_config.enable_fast_action_dispatch || !is_fast_action_backend_event(event)) {
            return false;
        }

        Event action_event;
        {
            std::lock_guard lock(fast_action_mutex_);
            auto route_it = fast_action_events_.find(
                                build_fast_action_route_key(event.handle, event.type, event.action)
                            );
            if (route_it == fast_action_events_.end()) {
                return false;
            }
            action_event = route_it->second;
        }
        return dispatch_event_action_handlers(action_event);
    }

    void apply_view_debug_to_all_nodes()
    {
        if (backend == nullptr) {
            return;
        }
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            for (auto &[unused_uid, record] : tree.nodes) {
                (void)unused_uid;
                backend->apply_debug_visual(record.handle, view_debug_enabled_);
            }
        }
    }

    std::unique_ptr<IBackend> backend;
    std::shared_ptr<IDataStore> store;
    RuntimeTaskConfig task_config;
    bool suppress_binding_listener_apply_ = false;
    std::mutex event_action_mutex_;
    std::mutex fast_action_mutex_;
    boost::unordered_flat_map<std::string, esp_brookesia::lib_utils::signal<void(const Event &)>> event_action_signals;
    boost::unordered_flat_map<std::string, Event> fast_action_events_;
    boost::unordered_flat_map<DocumentId::Value, TreeRecord> trees;
    std::unordered_map<std::string, MountedScreenRef> mounted_screens_;
    std::unordered_map<TransientMountId::Value, TransientScreenRef> transient_screens_;
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
    static size_t dbg_ext_free()
    {
        return ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot().external_free;
    }
#endif
    SubtreeBuildProfile subtree_build_profile_;

private:
    static RuntimeProfileClock::time_point subtree_profile_now()
    {
#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
        return RuntimeProfileClock::now();
#else
        return {};
#endif
    }

    static void add_subtree_profile_time(
        int64_t &bucket_us,
        const RuntimeProfileClock::time_point &start)
    {
#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
        bucket_us += runtime_profile_elapsed_us(start, RuntimeProfileClock::now());
#else
        (void)bucket_us;
        (void)start;
#endif
    }

    void reset_subtree_build_profile()
    {
        subtree_build_profile_ = {};
    }

    void log_subtree_build_profile(
        std::string_view stage,
        DocumentId document_id,
        std::string_view root_id,
        int64_t total_ms) const
    {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime subtree build profile: doc(%1%), stage(%2%), root(%3%), nodes(%4%), total_ms(%5%), "
            "copy_definition_ms(%6%), initial_bindings_ms(%7%), image_resolve_ms(%8%), create_node_ms(%9%), "
            "resolve_style_ms(%10%), store_record_ms(%11%)",
            document_id.value(),
            stage,
            root_id,
            subtree_build_profile_.nodes,
            total_ms,
            runtime_profile_us_to_ms(subtree_build_profile_.copy_definition_us),
            runtime_profile_us_to_ms(subtree_build_profile_.initial_bindings_us),
            runtime_profile_us_to_ms(subtree_build_profile_.image_resolve_us),
            runtime_profile_us_to_ms(subtree_build_profile_.create_node_us),
            runtime_profile_us_to_ms(subtree_build_profile_.resolve_style_us),
            runtime_profile_us_to_ms(subtree_build_profile_.store_record_us)
        );
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime subtree build profile: doc(%1%), stage(%2%), root(%3%), apply_props_ms(%4%), "
            "apply_layout_ms(%5%), apply_placement_ms(%6%), apply_transform_ms(%7%), apply_style_ms(%8%), "
            "apply_animations_ms(%9%), events_ms(%10%), subscribe_bindings_ms(%11%)",
            document_id.value(),
            stage,
            root_id,
            runtime_profile_us_to_ms(subtree_build_profile_.apply_props_us),
            runtime_profile_us_to_ms(subtree_build_profile_.apply_layout_us),
            runtime_profile_us_to_ms(subtree_build_profile_.apply_placement_us),
            runtime_profile_us_to_ms(subtree_build_profile_.apply_transform_us),
            runtime_profile_us_to_ms(subtree_build_profile_.apply_style_us),
            runtime_profile_us_to_ms(subtree_build_profile_.apply_animations_us),
            runtime_profile_us_to_ms(subtree_build_profile_.events_us),
            runtime_profile_us_to_ms(subtree_build_profile_.subscribe_bindings_us)
        );
    }

    std::expected<uint64_t, std::string> create_subtree(
        DocumentId document_id,
        TreeRecord &tree,
        const Node &definition,
        BackendHandle parent_handle,
        uint64_t parent_uid,
        const std::string &root_id,
        const Path &current_path,
        const std::string &scope_root_absolute_path,
        const std::optional<std::string> &override_root_id,
        const std::optional<std::string> &template_id = std::nullopt)
    {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        ++dbg_create_subtree_count_;
#endif
#if BROOKESIA_GUI_INTERFACE_ENABLE_PROFILE_LOG
        ++subtree_build_profile_.nodes;
#endif
        auto step_start = subtree_profile_now();
        // root_id may originate from a NodeRecord string; keep it stable while descendants are inserted.
        const std::string stable_root_id = root_id;
        Node node = definition;
        if (override_root_id.has_value()) {
            node.id = *override_root_id;
        }
        add_subtree_profile_time(subtree_build_profile_.copy_definition_us, step_start);

        const auto absolute_node_path = absolute_node_path_to_string(stable_root_id, current_path);
        step_start = subtree_profile_now();
        apply_initial_bindings(tree, node, absolute_node_path);
        add_subtree_profile_time(subtree_build_profile_.initial_bindings_us, step_start);
        step_start = subtree_profile_now();
        resolve_image_source(tree, node);
        auto image_preload_result = ensure_node_image_resources_preloaded(tree, node);
        add_subtree_profile_time(subtree_build_profile_.image_resolve_us, step_start);
        if (!image_preload_result) {
            return std::unexpected(image_preload_result.error());
        }

        const auto parent_absolute_path = parent_absolute_path_to_string(stable_root_id, current_path);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s0 = dbg_ext_free();
#endif
        step_start = subtree_profile_now();
        BackendHandle handle = backend->create_node(node, parent_handle, parent_absolute_path, scope_root_absolute_path);
        add_subtree_profile_time(subtree_build_profile_.create_node_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s1 = dbg_ext_free();
        dbg_step_create_node_ += static_cast<int64_t>(dbg_s0) - static_cast<int64_t>(dbg_s1);
#endif
        if (!handle.is_valid()) {
            const auto node_path = path_to_string(current_path);
            return std::unexpected("Failed to create GUI node: " + (node_path.empty() ? node.id : node_path));
        }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s2 = dbg_ext_free();
#endif
        const uint64_t uid = tree.next_uid++;
        NodeRecord record;
        record.uid = uid;
        record.document_id = document_id;
        record.file_path = tree.file_path;
        record.root_id = stable_root_id;
        record.path = current_path;
        record.absolute_path = absolute_node_path;
        // resolve_style_shared() only READS the node definition; compute it BEFORE we move `node` below
        // so the move can consume `node` instead of forcing a copy. It also de-duplicates the resolved
        // style across identically-styled nodes (e.g. repeated template list items).
        step_start = subtree_profile_now();
        record.resolved_style = resolve_style_shared(tree, node);
        add_subtree_profile_time(subtree_build_profile_.resolve_style_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s2b = dbg_ext_free();
        dbg_step_resolve_style_ += static_cast<int64_t>(dbg_s2) - static_cast<int64_t>(dbg_s2b);
#endif
        // ---------------------------------------------------------------------------------------
        // MEMORY-CRITICAL: two rules below, each independently proven by HeapTrace to roughly halve
        // the per-node PSRAM cost. Do NOT revert either without re-measuring create_view.noderecord.
        //
        // 1) MOVE, don't copy. `node` is a per-instance working copy (already customized in place by
        //    apply_initial_bindings()/resolve_image_source() above) and is NOT used after this point
        //    - the child recursion at the bottom walks `definition.children`, not `node`, and every
        //    later read goes through `stored_record->node`. Copying here (record.node = node) duplicated
        //    every per-node heap container (bindings / events / style maps / prop strings) a second
        //    time for no reason; std::move transfers them with zero allocation. resolve_style() above
        //    is the only remaining reader of the local `node`, hence the reorder.
        //
        // 2) Drop descendant definitions. Each NodeRecord only needs THIS node's own definition; each
        //    descendant gets its own NodeRecord (via the recursion below), parent/child topology is
        //    tracked via record.children (uids) + tree.nodes, and any rebuild (hot-reload, dynamic
        //    mount, template instancing) is driven from tree.templates / tree.screens - never from
        //    record.node.children. Keeping children here stored the full subtree definition redundantly
        //    at every level (~O(sum of subtree sizes) instead of O(node count)).
        //
        // Note: true cross-instance definition sharing is NOT safe here because set_view_text()/
        // set_view_src() and interactive widgets mutate record.node per instance; these two rules are
        // the safe equivalent. Together they fixed std::bad_alloc when populating long lists (Wi-Fi
        // scan results) on PSRAM-constrained targets. To walk children, iterate `record.children`
        // (uids) and look them up in `tree.nodes`; do NOT reintroduce `record.node.children`.
        // ---------------------------------------------------------------------------------------
        step_start = subtree_profile_now();
        record.node = std::move(node);
        record.node.children.clear();
        record.node.children.shrink_to_fit();
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s2a = dbg_ext_free();
        dbg_step_node_copy_ += static_cast<int64_t>(dbg_s2b) - static_cast<int64_t>(dbg_s2a);
#endif
        record.applied_style_revision = current_style_revision_;
        record.handle = handle;
        record.parent_uid = parent_uid;
        record.template_id = template_id;

        tree.handle_to_uid.emplace(handle.value(), uid);
        tree.absolute_path_to_uid.emplace(record.absolute_path, uid);
        tree.nodes.emplace(uid, std::move(record));

        auto *stored_record = find_node_record(tree, uid);
        if (stored_record == nullptr) {
            return std::unexpected("Failed to store GUI node record");
        }
        add_subtree_profile_time(subtree_build_profile_.store_record_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s3 = dbg_ext_free();
        dbg_step_store_ += static_cast<int64_t>(dbg_s2a) - static_cast<int64_t>(dbg_s3);
        dbg_step_noderecord_ += static_cast<int64_t>(dbg_s2) - static_cast<int64_t>(dbg_s3);
#endif

        step_start = subtree_profile_now();
        backend->apply_props(handle, stored_record->node);
        add_subtree_profile_time(subtree_build_profile_.apply_props_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s4 = dbg_ext_free();
        dbg_step_apply_props_ += static_cast<int64_t>(dbg_s3) - static_cast<int64_t>(dbg_s4);
#endif
        step_start = subtree_profile_now();
        backend->apply_layout(handle, stored_record->node.layout);
        add_subtree_profile_time(subtree_build_profile_.apply_layout_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s5 = dbg_ext_free();
        dbg_step_apply_layout_ += static_cast<int64_t>(dbg_s4) - static_cast<int64_t>(dbg_s5);
#endif
        step_start = subtree_profile_now();
        backend->apply_placement(handle, stored_record->node.placement);
        add_subtree_profile_time(subtree_build_profile_.apply_placement_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s6 = dbg_ext_free();
        dbg_step_apply_placement_ += static_cast<int64_t>(dbg_s5) - static_cast<int64_t>(dbg_s6);
#endif
        step_start = subtree_profile_now();
        backend->apply_props(handle, stored_record->node, PropsApplyMask::CommonTransform);
        add_subtree_profile_time(subtree_build_profile_.apply_transform_us, step_start);
        step_start = subtree_profile_now();
        backend->apply_style(handle, *stored_record->resolved_style);
        add_subtree_profile_time(subtree_build_profile_.apply_style_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s7 = dbg_ext_free();
        dbg_step_apply_style_ += static_cast<int64_t>(dbg_s6) - static_cast<int64_t>(dbg_s7);
#endif
        step_start = subtree_profile_now();
        backend->apply_debug_visual(handle, view_debug_enabled_);
        backend->apply_animations(handle, stored_record->node.animations);
        add_subtree_profile_time(subtree_build_profile_.apply_animations_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s8 = dbg_ext_free();
        dbg_step_apply_anim_ += static_cast<int64_t>(dbg_s7) - static_cast<int64_t>(dbg_s8);
#endif
        step_start = subtree_profile_now();
        register_fast_action_routes(*stored_record);
        backend->bind_events(handle, stored_record->node.events);
        add_subtree_profile_time(subtree_build_profile_.events_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s9 = dbg_ext_free();
        dbg_step_events_ += static_cast<int64_t>(dbg_s8) - static_cast<int64_t>(dbg_s9);
#endif
        step_start = subtree_profile_now();
        subscribe_bindings(document_id, *stored_record);
        add_subtree_profile_time(subtree_build_profile_.subscribe_bindings_us, step_start);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        const size_t dbg_s10 = dbg_ext_free();
        dbg_step_subscribe_ += static_cast<int64_t>(dbg_s9) - static_cast<int64_t>(dbg_s10);
#endif

        if (parent_uid != 0) {
            auto *parent_record = find_node_record(tree, parent_uid);
            if (parent_record != nullptr) {
                parent_record->children.push_back(uid);
            }
        }

        for (const auto &child : definition.children) {
            auto child_uid = create_subtree(
                                 document_id,
                                 tree,
                                 child,
                                 handle,
                                 uid,
                                 stable_root_id,
                                 append_path(current_path, child.id),
                                 scope_root_absolute_path,
                                 std::nullopt
                             );
            if (!child_uid) {
                return std::unexpected(child_uid.error());
            }
        }

        return uid;
    }

    void unload_partial_tree(TreeRecord &tree)
    {
        std::vector<uint64_t> roots;
        roots.reserve(tree.nodes.size());
        for (const auto &[uid, record] : tree.nodes) {
            if (record.parent_uid == 0) {
                roots.push_back(uid);
            }
        }

        for (auto uid : roots) {
            destroy_subtree(tree, uid);
        }

        tree.handle_to_uid.clear();
        tree.absolute_path_to_uid.clear();
        tree.nodes.clear();
        tree.screen_roots.clear();
    }

    bool unmount_mounted_screen(TreeRecord &tree, std::string_view absolute_path)
    {
        const auto screen_id = trim_slashes(absolute_path);
        auto mounted_it = tree.screen_roots.find(screen_id);
        if (mounted_it == tree.screen_roots.end()) {
            return false;
        }

        auto *record = find_node_record(tree, mounted_it->second);
        if (record == nullptr) {
            return false;
        }

        const bool result = backend->unmount_screen(record->handle);
        auto screen_def_it = tree.screens.find(screen_id);
        const bool should_destroy =
            screen_def_it != tree.screens.end() && screen_def_it->second.mount_mode == MountMode::Dynamic;
        if (should_destroy) {
            destroy_subtree(tree, mounted_it->second);
            tree.screen_roots.erase(screen_id);
        }
        return result;
    }

    void destroy_subtree(TreeRecord &tree, uint64_t uid)
    {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        ++dbg_destroy_subtree_count_;
#endif
        auto *record = find_node_record(tree, uid);
        if (record == nullptr) {
            return;
        }

        std::vector<uint64_t> subtree_uids;
        collect_subtree_uids(tree, uid, subtree_uids);

        const uint64_t parent_uid = record->parent_uid;
        const BackendHandle handle = record->handle;

        backend->destroy_node(handle);

        if (parent_uid != 0) {
            auto *parent_record = find_node_record(tree, parent_uid);
            if (parent_record != nullptr) {
                auto &siblings = parent_record->children;
                siblings.erase(std::remove(siblings.begin(), siblings.end(), uid), siblings.end());
            }
        }

        for (auto current_uid : subtree_uids) {
            auto current_it = tree.nodes.find(current_uid);
            if (current_it == tree.nodes.end()) {
                continue;
            }

            for (auto subscription_id : current_it->second.subscriptions) {
                store->unsubscribe(subscription_id);
            }
            unregister_fast_action_routes(current_it->second);
            tree.absolute_path_to_uid.erase(current_it->second.absolute_path);
            tree.handle_to_uid.erase(current_it->second.handle.value());
            tree.nodes.erase(current_it);
        }
    }

    void collect_subtree_uids(const TreeRecord &tree, uint64_t uid, std::vector<uint64_t> &uids) const
    {
        auto *record = find_node_record_const(tree, uid);
        if (record == nullptr) {
            return;
        }
        uids.push_back(uid);
        for (auto child_uid : record->children) {
            collect_subtree_uids(tree, child_uid, uids);
        }
    }

    std::expected<std::vector<Dimension>, std::string> parse_dimension_array_from_store_string(
        std::string_view value,
        const Environment &environment) const
    {
        auto parsed = parse_json_fragment(value, "dimension array");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (!parsed->is_array()) {
            return std::unexpected("dimension array must be a JSON array");
        }

        std::vector<Dimension> result;
        for (const auto &entry : parsed->as_array()) {
            if (entry.is_string()) {
                auto dimension = parse_dimension_from_store_string(entry.as_string().c_str(), environment);
                if (!dimension) {
                    return std::unexpected(dimension.error());
                }
                result.push_back(*dimension);
            } else if (entry.is_int64()) {
                result.push_back(Dimension{.mode = SizeMode::Fixed, .value = static_cast<int32_t>(entry.as_int64())});
            } else {
                return std::unexpected("dimension array entries must be strings or integers");
            }
        }
        return result;
    }

    std::expected<std::vector<std::string>, std::string> parse_string_array_from_store_string(std::string_view value) const
    {
        auto parsed = parse_json_fragment(value, "string array");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (!parsed->is_array()) {
            return std::unexpected("string array must be a JSON array");
        }

        std::vector<std::string> result;
        for (const auto &entry : parsed->as_array()) {
            if (!entry.is_string()) {
                return std::unexpected("string array entries must be strings");
            }
            result.emplace_back(entry.as_string().c_str());
        }
        return result;
    }

    std::expected<std::vector<Point>, std::string> parse_points_from_store_string(std::string_view value) const
    {
        auto parsed = parse_json_fragment(value, "points");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (!parsed->is_array()) {
            return std::unexpected("points must be a JSON array");
        }

        std::vector<Point> result;
        for (const auto &entry : parsed->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("point entries must be objects");
            }
            const auto &point_object = entry.as_object();
            const auto *x_value = point_object.if_contains("x");
            const auto *y_value = point_object.if_contains("y");
            if (x_value == nullptr || y_value == nullptr || !x_value->is_int64() || !y_value->is_int64()) {
                return std::unexpected("point entries must contain integer x and y");
            }
            result.push_back(Point{
                .x = static_cast<int32_t>(x_value->as_int64()),
                .y = static_cast<int32_t>(y_value->as_int64()),
            });
        }
        return result;
    }

    std::expected<std::vector<TableCell>, std::string> parse_table_cells_from_store_string(std::string_view value) const
    {
        auto parsed = parse_json_fragment(value, "table cells");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (!parsed->is_array()) {
            return std::unexpected("table cells must be a JSON array");
        }

        std::vector<TableCell> result;
        for (const auto &entry : parsed->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("table cell entries must be objects");
            }
            const auto &cell_object = entry.as_object();
            const auto *row_value = cell_object.if_contains("row");
            const auto *column_value = cell_object.if_contains("column");
            const auto *text_value = cell_object.if_contains("text");
            if (row_value == nullptr || column_value == nullptr || text_value == nullptr || !row_value->is_int64() ||
                    !column_value->is_int64() || !text_value->is_string()) {
                return std::unexpected("table cell entries must contain integer row/column and string text");
            }
            result.push_back(TableCell{
                .row = static_cast<int32_t>(row_value->as_int64()),
                .column = static_cast<int32_t>(column_value->as_int64()),
                .text = text_value->as_string().c_str(),
            });
        }
        return result;
    }

    std::expected<std::vector<CanvasCommand>, std::string> parse_canvas_commands_from_store_string(std::string_view value) const
    {
        auto parsed = parse_json_fragment(value, "canvas commands");
        if (!parsed) {
            return std::unexpected(parsed.error());
        }
        if (!parsed->is_array()) {
            return std::unexpected("canvas commands must be a JSON array");
        }

        std::vector<CanvasCommand> result;
        for (const auto &entry : parsed->as_array()) {
            if (!entry.is_object()) {
                return std::unexpected("canvas command entries must be objects");
            }
            const auto &command_object = entry.as_object();
            const auto *type_value = command_object.if_contains("type");
            const auto *x_value = command_object.if_contains("x");
            const auto *y_value = command_object.if_contains("y");
            const auto *width_value = command_object.if_contains("width");
            const auto *height_value = command_object.if_contains("height");
            const auto *color_value = command_object.if_contains("color");
            if (type_value == nullptr || x_value == nullptr || y_value == nullptr || width_value == nullptr ||
                    height_value == nullptr || color_value == nullptr || !type_value->is_string() || !x_value->is_int64() ||
                    !y_value->is_int64() || !width_value->is_int64() || !height_value->is_int64() || !color_value->is_string()) {
                return std::unexpected("canvas command entries must contain type/x/y/width/height/color");
            }
            result.push_back(CanvasCommand{
                .type = type_value->as_string().c_str(),
                .x = static_cast<int32_t>(x_value->as_int64()),
                .y = static_cast<int32_t>(y_value->as_int64()),
                .width = static_cast<int32_t>(width_value->as_int64()),
                .height = static_cast<int32_t>(height_value->as_int64()),
                .color = color_value->as_string().c_str(),
            });
        }
        return result;
    }

    std::expected<void, std::string> apply_binding_value(
        TreeRecord &tree,
        NodeRecord &record,
        const BindingTargetInfo &target_info,
        std::string_view value)
    {
        const auto target = target_info.target;
        Style *target_style = nullptr;
        if (target_info.style_part.empty()) {
            target_style = target_info.style_state.empty() ?
                           &record.node.style :
                           &record.node.state_styles[target_info.style_state];
        } else {
            auto &part_style = record.node.part_styles[target_info.style_part];
            target_style = target_info.style_state.empty() ?
                           &part_style.style :
                           &part_style.state_styles[target_info.style_state];
        }
        switch (target) {
        case BindingTarget::CommonPropsHidden: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.hidden = *parsed;
            break;
        }
        case BindingTarget::CommonPropsDisabled: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.disabled = *parsed;
            break;
        }
        case BindingTarget::CommonPropsClickable: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.clickable = *parsed;
            break;
        }
        case BindingTarget::CommonPropsScrollable: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.scrollable = *parsed;
            break;
        }
        case BindingTarget::CommonPropsPressLock: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.press_lock = *parsed;
            break;
        }
        case BindingTarget::CommonPropsAngle: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.angle = *parsed;
            break;
        }
        case BindingTarget::CommonPropsZoom: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.zoom = *parsed;
            break;
        }
        case BindingTarget::CommonPropsPivotX: {
            auto parsed = parse_pivot_from_store_string(value, "commonProps.pivotX");
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.pivot_x = *parsed;
            break;
        }
        case BindingTarget::CommonPropsPivotY: {
            auto parsed = parse_pivot_from_store_string(value, "commonProps.pivotY");
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.common_props.pivot_y = *parsed;
            break;
        }
        case BindingTarget::LabelPropsText:
            record.node.label_props.text = std::string(value);
            break;
        case BindingTarget::ImagePropsSrc:
            if (!value.empty() && !has_image_resource(tree, std::string(value))) {
                return std::unexpected("unknown image resource id: " + std::string(value));
            }
            if (record.handle.is_valid()) {
                return update_image_source(tree, record, value);
            }
            record.node.image_props.src = std::string(value);
            break;
        case BindingTarget::ImagePropsRecolor:
            if (!is_valid_color(value)) {
                return std::unexpected("expected empty string or color in '#RRGGBB' format");
            }
            record.node.image_props.recolor = std::string(value);
            break;
        case BindingTarget::ImagePropsRecolorOpacity: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            if (*parsed < 0 || *parsed > 255) {
                return std::unexpected("expected integer in range 0..255");
            }
            record.node.image_props.recolor_opacity = *parsed;
            break;
        }
        case BindingTarget::ImagePropsAngle: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.image_props.angle = *parsed;
            break;
        }
        case BindingTarget::ImagePropsOffsetX: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.image_props.offset_x = *parsed;
            break;
        }
        case BindingTarget::ImagePropsOffsetY: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.image_props.offset_y = *parsed;
            break;
        }
        case BindingTarget::ImagePropsZoom: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.image_props.zoom = *parsed;
            break;
        }
        case BindingTarget::ImagePropsPivotX: {
            auto parsed = parse_pivot_from_store_string(value, "imageProps.pivotX");
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.image_props.pivot_x = *parsed;
            break;
        }
        case BindingTarget::ImagePropsPivotY: {
            auto parsed = parse_pivot_from_store_string(value, "imageProps.pivotY");
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.image_props.pivot_y = *parsed;
            break;
        }
        case BindingTarget::FrameViewPropsAutoRegisterOutput: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.frame_view_props.auto_register_output = *parsed;
            break;
        }
        case BindingTarget::FrameViewPropsOutputName:
            record.node.frame_view_props.output_name = std::string(value);
            break;
        case BindingTarget::FrameViewPropsColorFormat: {
            FrameColorFormat parsed = FrameColorFormat::Max;
            if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed) ||
                    parsed == FrameColorFormat::Max) {
                return std::unexpected("unsupported frameViewProps.colorFormat: " + std::string(value));
            }
            record.node.frame_view_props.color_format = parsed;
            break;
        }
        case BindingTarget::TextInputPropsText:
            record.node.text_input_props.text = std::string(value);
            break;
        case BindingTarget::TextInputPropsPlaceholder:
            record.node.text_input_props.placeholder = std::string(value);
            break;
        case BindingTarget::TextInputPropsPassword: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.text_input_props.password = *parsed;
            break;
        }
        case BindingTarget::TextInputPropsMultiline: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.text_input_props.multiline = *parsed;
            break;
        }
        case BindingTarget::TextInputPropsMaxLength: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.text_input_props.max_length = *parsed;
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
                record.node.range_props.value = *parsed;
            } else if (target == BindingTarget::RangePropsMin) {
                record.node.range_props.min = *parsed;
            } else if (target == BindingTarget::RangePropsMax) {
                record.node.range_props.max = *parsed;
            } else {
                record.node.range_props.step = *parsed;
            }
            break;
        }
        case BindingTarget::TogglePropsChecked: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.toggle_props.checked = *parsed;
            break;
        }
        case BindingTarget::DropdownPropsOptions: {
            auto parsed = parse_string_array_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.dropdown_props.options = std::move(*parsed);
            break;
        }
        case BindingTarget::DropdownPropsSelectedIndex: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.dropdown_props.selected_index = *parsed;
            break;
        }
        case BindingTarget::TablePropsRows:
        case BindingTarget::TablePropsColumns: {
            auto parsed = parse_int_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            if (target == BindingTarget::TablePropsRows) {
                record.node.table_props.rows = *parsed;
            } else {
                record.node.table_props.columns = *parsed;
            }
            break;
        }
        case BindingTarget::TablePropsCells: {
            auto parsed = parse_table_cells_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.table_props.cells = std::move(*parsed);
            break;
        }
        case BindingTarget::LinePropsPoints: {
            auto parsed = parse_points_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.line_props.points = std::move(*parsed);
            break;
        }
        case BindingTarget::KeyboardPropsMode:
            record.node.keyboard_props.mode = std::string(value);
            break;
        case BindingTarget::KeyboardPropsPopovers: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.keyboard_props.popovers = *parsed;
            break;
        }
        case BindingTarget::KeyboardPropsAllowedModes: {
            auto parsed = parse_keyboard_modes_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.keyboard_props.allowed_modes = std::move(*parsed);
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
            record.node.keyboard_props.icon_size = *parsed;
            break;
        }
        case BindingTarget::CanvasPropsCommands: {
            auto parsed = parse_canvas_commands_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.canvas_props.commands = std::move(*parsed);
            break;
        }
        case BindingTarget::StyleBgColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->bg_color = *color;
            break;
        }
        case BindingTarget::StyleBgGradientColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->bg_gradient_color = *color;
            break;
        }
        case BindingTarget::StyleBgGradientDirection:
            if (value != "none" && value != "horizontal" && value != "vertical") {
                return std::unexpected("invalid style.bgGradientDirection");
            }
            target_style->bg_gradient_direction = std::string(value);
            break;
        case BindingTarget::StyleTextColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->text_color = *color;
            break;
        }
        case BindingTarget::StyleBorderColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->border_color = *color;
            break;
        }
        case BindingTarget::StyleLineColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->line_color = *color;
            break;
        }
        case BindingTarget::StyleArcColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->arc_color = *color;
            break;
        }
        case BindingTarget::StyleArcGradientColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->arc_gradient_color = *color;
            break;
        }
        case BindingTarget::StyleImageRecolor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->image_recolor = *color;
            break;
        }
        case BindingTarget::StyleFont:
            if (!target_info.style_state.empty() || !target_info.style_part.empty()) {
                return std::unexpected("stateStyles and partStyles do not support font");
            }
            if (!has_font_resource(tree, std::string(value))) {
                return std::unexpected("unknown font resource id: " + std::string(value));
            }
            target_style->font = std::string(value);
            break;
        case BindingTarget::StyleShadowColor: {
            auto color = resolve_color_binding_value(tree, value);
            if (!color) {
                return std::unexpected(color.error());
            }
            target_style->shadow_color = *color;
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
                target_style->bg_main_stop = *parsed;
            } else if (target == BindingTarget::StyleBgGradientStop) {
                target_style->bg_gradient_stop = *parsed;
            } else if (target == BindingTarget::StyleBgGradientOpacity) {
                target_style->bg_gradient_opacity = *parsed;
            } else if (target == BindingTarget::StyleOpacity) {
                target_style->opacity = *parsed;
            } else if (target == BindingTarget::StyleImageOpacity) {
                target_style->image_opacity = *parsed;
            } else if (target == BindingTarget::StyleImageRecolorOpacity) {
                target_style->image_recolor_opacity = *parsed;
            } else if (target == BindingTarget::StyleArcOpacity) {
                target_style->arc_opacity = *parsed;
            } else {
                target_style->arc_gradient_segments = *parsed;
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
                target_style->border_width = *parsed;
                break;
            case BindingTarget::StyleRadius: target_style->radius = *parsed; break;
            case BindingTarget::StylePadding:
                target_style->padding = *parsed;
                break;
            case BindingTarget::StylePaddingLeft:
                target_style->padding_left = *parsed;
                break;
            case BindingTarget::StylePaddingRight:
                target_style->padding_right = *parsed;
                break;
            case BindingTarget::StylePaddingTop:
                target_style->padding_top = *parsed;
                break;
            case BindingTarget::StylePaddingBottom:
                target_style->padding_bottom = *parsed;
                break;
            case BindingTarget::StyleMargin: target_style->margin = *parsed; break;
            case BindingTarget::StyleMarginLeft: target_style->margin_left = *parsed; break;
            case BindingTarget::StyleMarginRight: target_style->margin_right = *parsed; break;
            case BindingTarget::StyleMarginTop: target_style->margin_top = *parsed; break;
            case BindingTarget::StyleMarginBottom: target_style->margin_bottom = *parsed; break;
            case BindingTarget::StyleShadowWidth: target_style->shadow_width = *parsed; break;
            case BindingTarget::StyleShadowOffsetX: target_style->shadow_offset_x = *parsed; break;
            case BindingTarget::StyleShadowOffsetY: target_style->shadow_offset_y = *parsed; break;
            case BindingTarget::StyleLineWidth: target_style->line_width = *parsed; break;
            case BindingTarget::StyleArcWidth: target_style->arc_width = *parsed; break;
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
            target_style->font_size = *parsed;
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
            target_style->image_font_size = *parsed;
            break;
        }
        case BindingTarget::StyleArcRounded: {
            auto parsed = parse_bool_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            target_style->arc_rounded = *parsed;
            break;
        }
        case BindingTarget::LayoutType: {
            LayoutType parsed {};
            if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
                return std::unexpected("invalid layout.type enum");
            }
            record.node.layout.type = parsed;
            break;
        }
        case BindingTarget::LayoutFlexFlow: {
            FlexFlow parsed {};
            if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
                return std::unexpected("invalid layout.flexFlow enum");
            }
            record.node.layout.flex_flow = parsed;
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
                record.node.layout.main_align = parsed;
            } else if (target == BindingTarget::LayoutCrossAlign) {
                record.node.layout.cross_align = parsed;
            } else {
                record.node.placement.align_self = parsed;
            }
            break;
        }
        case BindingTarget::LayoutGap: {
            auto parsed = parse_scaled_from_store_string(value, "layout.gap", "dp", tree.environment.density);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.layout.gap = *parsed;
            break;
        }
        case BindingTarget::LayoutGridTemplateColumns:
        case BindingTarget::LayoutGridTemplateRows: {
            auto parsed = parse_dimension_array_from_store_string(value, tree.environment);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            if (target == BindingTarget::LayoutGridTemplateColumns) {
                record.node.layout.grid_template_columns = std::move(*parsed);
            } else {
                record.node.layout.grid_template_rows = std::move(*parsed);
            }
            break;
        }
        case BindingTarget::PlacementMode: {
            PlacementMode parsed {};
            if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
                return std::unexpected("invalid placement.mode enum");
            }
            record.node.placement.mode = parsed;
            break;
        }
        case BindingTarget::PlacementWidth:
        case BindingTarget::PlacementHeight: {
            auto parsed = parse_dimension_from_store_string(value, tree.environment);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            if (target == BindingTarget::PlacementWidth) {
                record.node.placement.width = *parsed;
            } else {
                record.node.placement.height = *parsed;
            }
            break;
        }
        case BindingTarget::PlacementAspectRatio: {
            auto parsed = parse_aspect_ratio_from_store_string(value);
            if (!parsed) {
                return std::unexpected(parsed.error());
            }
            record.node.placement.aspect_ratio = *parsed;
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
                record.node.placement.x = *parsed;
            } else {
                record.node.placement.y = *parsed;
            }
            break;
        }
        case BindingTarget::PlacementAlign: {
            PlacementAlign parsed {};
            if (!BROOKESIA_DESCRIBE_STR_TO_ENUM_FLEXIBLE(std::string(value), parsed)) {
                return std::unexpected("invalid placement.align enum");
            }
            record.node.placement.align = parsed;
            break;
        }
        case BindingTarget::PlacementRelativeTo:
            record.node.placement.relative_to = std::string(value);
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
                record.node.placement.grid_column = *parsed;
            } else if (target == BindingTarget::PlacementGridRow) {
                record.node.placement.grid_row = *parsed;
            } else if (target == BindingTarget::PlacementGridColumnSpan) {
                record.node.placement.grid_column_span = *parsed;
            } else if (target == BindingTarget::PlacementGridRowSpan) {
                record.node.placement.grid_row_span = *parsed;
            } else {
                record.node.placement.flex_grow = *parsed;
            }
            break;
        }
        }
        return {};
    }

    void reapply_binding_domain(TreeRecord &tree, NodeRecord &record, const BindingTargetInfo &target_info)
    {
        if (backend == nullptr) {
            return;
        }
        switch (target_info.domain) {
        case BindingApplyDomain::Props:
            if (record.node.type == NodeType::Image && has_mask(target_info.props_mask, PropsApplyMask::ImageSource)) {
                record.node.resolved_image = {};
                resolve_image_source(tree, record.node);
                auto preload_result = ensure_node_image_resources_preloaded(tree, record.node);
                if (!preload_result) {
                    BROOKESIA_LOGW("Failed to preload image during binding update: %1%", preload_result.error());
                    return;
                }
            }
            backend->apply_props(record.handle, record.node, target_info.props_mask);
            if (record.node.type == NodeType::Image && has_mask(target_info.props_mask, PropsApplyMask::ImageSource)) {
                backend->apply_placement(record.handle, record.node.placement, PlacementApplyMask::Size);
            }
            break;
        case BindingApplyDomain::Style:
            record.resolved_style = resolve_style_shared(tree, record.node);
            backend->apply_style(record.handle, *record.resolved_style, target_info.style_mask);
            backend->apply_debug_visual(record.handle, view_debug_enabled_);
            break;
        case BindingApplyDomain::Layout:
            backend->apply_layout(record.handle, record.node.layout, target_info.layout_mask);
            break;
        case BindingApplyDomain::Placement:
            backend->apply_placement(record.handle, record.node.placement, target_info.placement_mask);
            if (has_mask(target_info.placement_mask, PlacementApplyMask::Size)) {
                backend->apply_props(record.handle, record.node, PropsApplyMask::CommonTransform);
            }
            break;
        }
    }

    void reapply_binding_masks(TreeRecord &tree, NodeRecord &record, const BindingApplyMasks &masks)
    {
        if (backend == nullptr) {
            return;
        }
        if (masks.props != PropsApplyMask::None) {
            if (record.node.type == NodeType::Image && has_mask(masks.props, PropsApplyMask::ImageSource)) {
                record.node.resolved_image = {};
                resolve_image_source(tree, record.node);
                auto preload_result = ensure_node_image_resources_preloaded(tree, record.node);
                if (!preload_result) {
                    BROOKESIA_LOGW("Failed to preload image during binding update: %1%", preload_result.error());
                    return;
                }
            }
            backend->apply_props(record.handle, record.node, masks.props);
            if (record.node.type == NodeType::Image && has_mask(masks.props, PropsApplyMask::ImageSource)) {
                backend->apply_placement(record.handle, record.node.placement, PlacementApplyMask::Size);
            }
        }
        if (masks.style != StyleApplyMask::None) {
            record.resolved_style = resolve_style_shared(tree, record.node);
            backend->apply_style(record.handle, *record.resolved_style, masks.style);
            backend->apply_debug_visual(record.handle, view_debug_enabled_);
        }
        if (masks.layout != LayoutApplyMask::None) {
            backend->apply_layout(record.handle, record.node.layout, masks.layout);
        }
        if (masks.placement != PlacementApplyMask::None) {
            backend->apply_placement(record.handle, record.node.placement, masks.placement);
            if (has_mask(masks.placement, PlacementApplyMask::Size)) {
                backend->apply_props(record.handle, record.node, PropsApplyMask::CommonTransform);
            }
        }
    }

    void merge_binding_mask(BindingApplyMasks &masks, const BindingTargetInfo &target_info)
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

    static bool node_has_binding_key(const Node &node, std::string_view key)
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

    static const Node *find_child_node_by_path(
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

    static std::vector<std::string> split_absolute_path_segments(std::string_view absolute_path)
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

    static bool tree_has_binding_declaration_for_update(
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

    void set_binding_values(DocumentId document_id, const std::vector<BindingValueUpdate> &updates)
    {
        if (store == nullptr || updates.empty()) {
            return;
        }

        auto *tree = resolve_tree(document_id);
        if (tree == nullptr) {
            return;
        }

        const bool previous_suppress = suppress_binding_listener_apply_;
        suppress_binding_listener_apply_ = true;
        for (const auto &update : updates) {
            store->set_string(document_id, update.absolute_path, update.key, update.value);
        }
        suppress_binding_listener_apply_ = previous_suppress;

        boost::unordered_flat_map<uint64_t, BindingApplyMasks> dirty_nodes;
        for (const auto &update : updates) {
            const auto query = normalize_absolute_path(update.absolute_path);
            auto uid = resolve_any_uid(*tree, query);
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
            for (const auto &[binding_path, expression] : record->node.bindings) {
                auto store_key = normalize_binding_store_key(expression);
                if (!store_key || *store_key != update.key) {
                    continue;
                }
                matched_binding = true;
                auto binding_target = resolve_binding_target(record->node.type, binding_path);
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
                merge_binding_mask(dirty_nodes[record->uid], *binding_target);
            }
            if (!matched_binding) {
                BROOKESIA_LOGW(
                    "Binding update path exists but no binding declaration matched key: "
                    "document_id=%1%, path='%2%', key='%3%', value='%4%', node_type=%5%",
                    document_id.value(), record->absolute_path, update.key, update.value,
                    get_theme_style_key(record->node.type)
                );
            }
        }

        for (auto &[uid, masks] : dirty_nodes) {
            auto *record = find_node_record(*tree, uid);
            if (record == nullptr) {
                continue;
            }
            reapply_binding_masks(*tree, *record, masks);
        }

#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        {
            auto hp = ::esp_brookesia::lib_utils::MemoryProfiler::take_raw_heap_snapshot();
            BROOKESIA_LOGI(
                "[HeapTrace][gui.binding.signals] doc(%1%) updates(%2%) nodes(%3%) action_signals(%4%) "
                "store_connections(%5%) store_signals(%6%) create_view(%7%) create_subtree(%8%) "
                "destroy_subtree(%9%) psram_free(%10%)",
                document_id.value(), updates.size(), tree->nodes.size(), event_action_signals.size(),
                store ? store->debug_connection_count() : 0,
                store ? store->debug_signal_count() : 0,
                dbg_create_view_count_, dbg_create_subtree_count_, dbg_destroy_subtree_count_,
                hp.external_free
            );
        }
#endif
    }

    void apply_initial_bindings(TreeRecord &tree, Node &node, std::string_view absolute_path)
    {
        if (store == nullptr) {
            return;
        }

        NodeRecord temporary_record;
        temporary_record.node = node;
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
            }
        }

        node = std::move(temporary_record.node);
    }

    std::string resolve_event_effect_target_path(const NodeRecord &source_record, std::string_view target) const
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

    NodeRecord *resolve_event_effect_target(TreeRecord &tree, const NodeRecord &source_record, std::string_view target)
    {
        auto uid = resolve_any_uid(tree, resolve_event_effect_target_path(source_record, target));
        if (!uid) {
            return nullptr;
        }
        return find_node_record(tree, *uid);
    }

    static std::string build_event_animation_key(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view animation_id)
    {
        return std::to_string(document_id.value()) + '\x1f' + std::string(absolute_path) + '\x1f' +
               std::string(animation_id);
    }

    std::expected<void, std::string> apply_event_property_update(
        TreeRecord &tree,
        const NodeRecord &source_record,
        const EventPropertyUpdate &update)
    {
        auto *target_record = resolve_event_effect_target(tree, source_record, update.target);
        if (target_record == nullptr) {
            return std::unexpected("Event effect target not found: " + update.target);
        }
        auto target_info = resolve_binding_target(target_record->node.type, update.field);
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

    std::expected<Animation, std::string> resolve_event_animation(
        const NodeRecord &record,
        const EventEffect &effect) const
    {
        if (!effect.animation.id.empty() || effect.animation_id.empty()) {
            return effect.animation;
        }
        auto animation_it = std::find_if(
                                record.node.animations.begin(),
                                record.node.animations.end(),
        [&effect](const Animation & animation) {
            return animation.id == effect.animation_id;
        }
                            );
        if (animation_it == record.node.animations.end()) {
            return std::unexpected("Animation not found: " + effect.animation_id);
        }
        return *animation_it;
    }

    std::expected<void, std::string> start_event_animation(
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
                                target_record->document_id, target_record->absolute_path, animation_id
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
        register_document_subscription(subscription_id, target_record->document_id);
        if (animation_key) {
            event_animation_ids_[*animation_key] = subscription_id;
        }
        return {};
    }

    std::expected<void, std::string> stop_event_animation(
        TreeRecord &tree,
        const NodeRecord &source_record,
        const EventEffect &effect)
    {
        auto *target_record = resolve_event_effect_target(tree, source_record, effect.target);
        if (target_record == nullptr) {
            return std::unexpected("Event effect animation target not found: " + effect.target);
        }
        const auto key = build_event_animation_key(
                             target_record->document_id, target_record->absolute_path, effect.animation_id
                         );
        auto animation_it = event_animation_ids_.find(key);
        if (animation_it == event_animation_ids_.end()) {
            return {};
        }
        (void)unsubscribe_subscription(animation_it->second);
        event_animation_ids_.erase(animation_it);
        return {};
    }

    void execute_event_effects(TreeRecord &tree, NodeRecord &source_record, const Event &event)
    {
        for (const auto &binding : source_record.node.events) {
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
                    (void)dispatch_event_action_handlers(emitted_event);
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

    static KeyboardKeyImageSpec make_keyboard_key_image_spec(const ImageAsset &image)
    {
        return KeyboardKeyImageSpec{
            .image_id = image.id,
            .primary_src = image.src,
            .native_src = 0,
            .width = image.width,
            .height = image.height,
        };
    }

    static KeyboardKeyImageSpec make_keyboard_key_image_spec(const RuntimeImageResource &image)
    {
        return KeyboardKeyImageSpec{
            .image_id = image.id,
            .primary_src = image.primary_src,
            .native_src = image.native_src,
            .width = image.width,
            .height = image.height,
        };
    }

    std::optional<KeyboardKeyImageSpec> resolve_keyboard_key_image(
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

    void resolve_keyboard_key_images(const TreeRecord &tree, Node &node) const
    {
        if (node.type != NodeType::Keyboard) {
            return;
        }
        for (auto &[unused_mode, layout] : node.keyboard_props.layouts) {
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

    bool node_references_image(const Node &node, std::string_view image_id) const
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

    void resolve_image_source(const TreeRecord &tree, Node &node) const
    {
        resolve_keyboard_key_style_refs(tree, node);
        resolve_keyboard_key_images(tree, node);

        if (node.type != NodeType::Image || node.image_props.src.empty()) {
            node.resolved_image = {};
            return;
        }

        node.resolved_image = {};
        auto image_it = tree.images.find(node.image_props.src);
        if (image_it != tree.images.end()) {
            BROOKESIA_LOGD("Resolved image from JSON asset: id='%1%', src='%2%', width=%3%, height=%4%",
                           image_it->second.id,
                           image_it->second.src,
                           image_it->second.width,
                           image_it->second.height);
            node.resolved_image = {
                .image_id = image_it->second.id,
                .primary_src = image_it->second.src,
                .native_src = 0,
                .width = image_it->second.width,
                .height = image_it->second.height,
            };
            return;
        }

        auto global_image_it = global_images.find(node.image_props.src);
        if (global_image_it != global_images.end()) {
            BROOKESIA_LOGD(
                "Resolved image from runtime resource: id='%1%', primary_src='%2%', native_src=%3%, width=%4%, height=%5%",
                global_image_it->second.id,
                global_image_it->second.primary_src,
                global_image_it->second.native_src,
                global_image_it->second.width,
                global_image_it->second.height);
            node.resolved_image = {
                .image_id = global_image_it->second.id,
                .primary_src = global_image_it->second.primary_src,
                .native_src = global_image_it->second.native_src,
                .width = global_image_it->second.width,
                .height = global_image_it->second.height,
            };
        }
    }

    bool has_font_resource(const TreeRecord &tree, const std::string &font_id) const
    {
        (void)tree;
        return global_fonts.contains(font_id);
    }

    bool has_image_resource(const TreeRecord &tree, const std::string &image_id) const
    {
        return tree.images.contains(image_id) || global_images.contains(image_id);
    }

    static void merge_style_layer(Style &destination, const Style &source)
    {
        if (source.bg_color.has_value()) {
            destination.bg_color = source.bg_color;
        }
        if (source.bg_gradient_color.has_value()) {
            destination.bg_gradient_color = source.bg_gradient_color;
        }
        if (source.bg_gradient_direction.has_value()) {
            destination.bg_gradient_direction = source.bg_gradient_direction;
        }
        if (source.text_color.has_value()) {
            destination.text_color = source.text_color;
        }
        if (source.border_color.has_value()) {
            destination.border_color = source.border_color;
        }
        if (source.line_color.has_value()) {
            destination.line_color = source.line_color;
        }
        if (source.arc_color.has_value()) {
            destination.arc_color = source.arc_color;
        }
        if (source.arc_gradient_color.has_value()) {
            destination.arc_gradient_color = source.arc_gradient_color;
        }
        if (source.font.has_value()) {
            destination.font = source.font;
        }
        if (source.bg_main_stop.has_value()) {
            destination.bg_main_stop = source.bg_main_stop;
        }
        if (source.bg_gradient_stop.has_value()) {
            destination.bg_gradient_stop = source.bg_gradient_stop;
        }
        if (source.bg_gradient_opacity.has_value()) {
            destination.bg_gradient_opacity = source.bg_gradient_opacity;
        }
        if (source.border_width.has_value()) {
            destination.border_width = source.border_width;
        }
        if (source.radius.has_value()) {
            destination.radius = source.radius;
        }
        if (source.padding.has_value()) {
            destination.padding = source.padding;
        }
        if (source.padding_left.has_value()) {
            destination.padding_left = source.padding_left;
        }
        if (source.padding_right.has_value()) {
            destination.padding_right = source.padding_right;
        }
        if (source.padding_top.has_value()) {
            destination.padding_top = source.padding_top;
        }
        if (source.padding_bottom.has_value()) {
            destination.padding_bottom = source.padding_bottom;
        }
        if (source.margin.has_value()) {
            destination.margin = source.margin;
        }
        if (source.margin_left.has_value()) {
            destination.margin_left = source.margin_left;
        }
        if (source.margin_right.has_value()) {
            destination.margin_right = source.margin_right;
        }
        if (source.margin_top.has_value()) {
            destination.margin_top = source.margin_top;
        }
        if (source.margin_bottom.has_value()) {
            destination.margin_bottom = source.margin_bottom;
        }
        if (source.shadow_width.has_value()) {
            destination.shadow_width = source.shadow_width;
        }
        if (source.shadow_offset_x.has_value()) {
            destination.shadow_offset_x = source.shadow_offset_x;
        }
        if (source.shadow_offset_y.has_value()) {
            destination.shadow_offset_y = source.shadow_offset_y;
        }
        if (source.shadow_color.has_value()) {
            destination.shadow_color = source.shadow_color;
        }
        if (source.opacity.has_value()) {
            destination.opacity = source.opacity;
        }
        if (source.line_width.has_value()) {
            destination.line_width = source.line_width;
        }
        if (source.image_opacity.has_value()) {
            destination.image_opacity = source.image_opacity;
        }
        if (source.image_recolor.has_value()) {
            destination.image_recolor = source.image_recolor;
        }
        if (source.image_recolor_opacity.has_value()) {
            destination.image_recolor_opacity = source.image_recolor_opacity;
        }
        if (source.font_size.has_value()) {
            destination.font_size = source.font_size;
        }
        if (source.image_font_size.has_value()) {
            destination.image_font_size = source.image_font_size;
        }
        if (source.text_align.has_value()) {
            destination.text_align = source.text_align;
        }
        if (source.arc_width.has_value()) {
            destination.arc_width = source.arc_width;
        }
        if (source.arc_opacity.has_value()) {
            destination.arc_opacity = source.arc_opacity;
        }
        if (source.arc_gradient_segments.has_value()) {
            destination.arc_gradient_segments = source.arc_gradient_segments;
        }
        if (source.arc_rounded.has_value()) {
            destination.arc_rounded = source.arc_rounded;
        }
        if (source.clip_corner.has_value()) {
            destination.clip_corner = source.clip_corner;
        }
    }

    static void merge_style_set_layer(StyleSet &destination, const StyleSet &source)
    {
        merge_style_layer(destination.style, source.style);
        for (const auto &[state_name, state_style] : source.state_styles) {
            merge_style_layer(destination.state_styles[state_name], state_style);
        }
        for (const auto &[part_name, part_style] : source.part_styles) {
            merge_style_layer(destination.part_styles[part_name].style, part_style.style);
            for (const auto &[state_name, state_style] : part_style.state_styles) {
                merge_style_layer(destination.part_styles[part_name].state_styles[state_name], state_style);
            }
        }
    }

    StyleSet compose_style_set(const TreeRecord &tree, const Node &node) const
    {
        StyleSet composed_style;

        const auto &builtin_theme = get_builtin_default_theme();
        if (auto all_style_it = builtin_theme.styles.find("all"); all_style_it != builtin_theme.styles.end()) {
            merge_style_set_layer(composed_style, all_style_it->second);
        }
        const auto builtin_style_key = get_theme_style_key(node.type);
        if (auto subtype_style_it = builtin_theme.styles.find(builtin_style_key);
                subtype_style_it != builtin_theme.styles.end()) {
            merge_style_set_layer(composed_style, subtype_style_it->second);
        }

        auto theme_it = global_themes.find(current_theme);
        if (theme_it != global_themes.end()) {
            auto all_style_it = theme_it->second.styles.find("all");
            if (all_style_it != theme_it->second.styles.end()) {
                merge_style_set_layer(composed_style, all_style_it->second);
            }
            const auto style_key = get_theme_style_key(node.type);
            auto subtype_style_it = theme_it->second.styles.find(style_key);
            if (subtype_style_it != theme_it->second.styles.end()) {
                merge_style_set_layer(composed_style, subtype_style_it->second);
            }
        }

        for (const auto &style_ref : node.style_refs) {
            auto local_style_it = tree.styles.find(style_ref);
            if (local_style_it != tree.styles.end()) {
                merge_style_set_layer(composed_style, local_style_it->second);
                continue;
            }
            if (theme_it != global_themes.end()) {
                auto named_style_it = theme_it->second.styles.find(style_ref);
                if (named_style_it != theme_it->second.styles.end()) {
                    merge_style_set_layer(composed_style, named_style_it->second);
                    continue;
                }
            }
            BROOKESIA_LOGW(
                "Document %1% and theme '%2%' do not contain named style ref: %3%",
                tree.document_id,
                current_theme,
                style_ref
            );
        }

        merge_style_layer(composed_style.style, node.style);
        for (const auto &[state_name, state_style] : node.state_styles) {
            merge_style_layer(composed_style.state_styles[state_name], state_style);
        }
        for (const auto &[part_name, part_style] : node.part_styles) {
            merge_style_layer(composed_style.part_styles[part_name].style, part_style.style);
            for (const auto &[state_name, state_style] : part_style.state_styles) {
                merge_style_layer(composed_style.part_styles[part_name].state_styles[state_name], state_style);
            }
        }
        return composed_style;
    }

    std::string resolve_keyboard_key_color(
        const TreeRecord &tree,
        const std::string &color,
        std::string_view field_name) const
    {
        auto resolved = resolve_color_binding_value(tree, color);
        if (!resolved) {
            BROOKESIA_LOGW(
                "Failed to resolve keyboard key color field '%1%' value '%2%': %3%",
                field_name,
                color,
                resolved.error()
            );
            return color;
        }
        return *resolved;
    }

    std::optional<KeyboardKeyStyle> resolve_keyboard_key_style_ref(
        const TreeRecord &tree,
        std::string_view style_ref) const
    {
        const StyleSet *style_set = nullptr;
        auto local_style_it = tree.styles.find(std::string(style_ref));
        if (local_style_it != tree.styles.end()) {
            style_set = &local_style_it->second;
        }

        auto theme_it = global_themes.find(current_theme);
        if (style_set == nullptr && theme_it == global_themes.end()) {
            BROOKESIA_LOGD(
                "Skip keyboard key style ref '%1%' until current theme '%2%' is registered",
                style_ref,
                current_theme
            );
            return std::nullopt;
        }

        if (style_set == nullptr) {
            auto style_it = theme_it->second.styles.find(std::string(style_ref));
            if (style_it == theme_it->second.styles.end()) {
                BROOKESIA_LOGW(
                    "Document %1% and theme '%2%' do not contain keyboard key style ref: %3%",
                    tree.document_id,
                    current_theme,
                    style_ref
                );
                return std::nullopt;
            }
            style_set = &style_it->second;
        }

        const auto pressed_it = style_set->state_styles.find("pressed");
        const auto *pressed_style = pressed_it != style_set->state_styles.end() ? &pressed_it->second : nullptr;

        KeyboardKeyStyle key_style;
        if (style_set->style.bg_color.has_value()) {
            key_style.bg_color = resolve_keyboard_key_color(tree, *style_set->style.bg_color, "bgColor");
        }
        if (style_set->style.text_color.has_value()) {
            key_style.text_color = resolve_keyboard_key_color(tree, *style_set->style.text_color, "textColor");
        }
        if (pressed_style != nullptr) {
            if (pressed_style->bg_color.has_value()) {
                key_style.pressed_bg_color =
                    resolve_keyboard_key_color(tree, *pressed_style->bg_color, "pressed.bgColor");
            }
            if (pressed_style->text_color.has_value()) {
                key_style.pressed_text_color =
                    resolve_keyboard_key_color(tree, *pressed_style->text_color, "pressed.textColor");
            }
        }
        if (style_set->style.radius.has_value()) {
            key_style.radius = std::max<int32_t>(0, *style_set->style.radius);
        }
        return key_style;
    }

    void resolve_keyboard_key_style_refs(const TreeRecord &tree, Node &node) const
    {
        if (node.type != NodeType::Keyboard) {
            return;
        }

        auto resolved_styles = node.keyboard_props.key_styles;
        for (const auto &[style_class, style_ref] : node.keyboard_props.key_style_refs) {
            auto resolved = resolve_keyboard_key_style_ref(tree, style_ref);
            if (resolved.has_value()) {
                resolved_styles.insert_or_assign(style_class, std::move(*resolved));
            }
        }
        node.keyboard_props.resolved_key_styles = std::move(resolved_styles);
    }

    std::expected<void, std::string> validate_node_resource_references(
        const TreeRecord &tree, const Node &node, const std::string &absolute_path) const
    {
        if (node.style.font.has_value() && !node.style.font->empty() &&
                !has_font_resource(tree, *node.style.font) &&
                !is_builtin_default_font_id(*node.style.font)) {
            return std::unexpected(
                       "Node '" + absolute_path + "' references missing font resource: " + *node.style.font
                   );
        }
        if (node.type == NodeType::Image && !node.image_props.src.empty() &&
                !has_image_resource(tree, node.image_props.src)) {
            return std::unexpected(
                       "Node '" + absolute_path + "' references missing image resource: " + node.image_props.src
                   );
        }
        if (node.type == NodeType::Keyboard) {
            for (const auto &[unused_mode, layout] : node.keyboard_props.layouts) {
                (void)unused_mode;
                for (const auto &row : layout.rows) {
                    for (const auto &key : row) {
                        if (!key.image.empty() && !has_image_resource(tree, key.image)) {
                            return std::unexpected(
                                       "Node '" + absolute_path +
                                       "' references missing keyboard key image resource: " + key.image
                                   );
                        }
                    }
                }
            }
        }

        for (const auto &child : node.children) {
            auto child_path = absolute_path + "/" + child.id;
            auto child_validation = validate_node_resource_references(tree, child, child_path);
            if (!child_validation) {
                return child_validation;
            }
        }
        return {};
    }

    std::expected<void, std::string> validate_resource_references(const TreeRecord &tree) const
    {
        const auto &builtin_theme = get_builtin_default_theme();
        for (const auto &[style_key, style_set] : builtin_theme.styles) {
            if (style_set.style.font.has_value() && !style_set.style.font->empty() &&
                    !has_font_resource(tree, *style_set.style.font) &&
                    !is_builtin_default_font_id(*style_set.style.font)) {
                return std::unexpected(
                           "Built-in default theme style '" + style_key +
                           "' references missing font resource: " + *style_set.style.font
                       );
            }
        }
        for (const auto &[theme_id, theme] : global_themes) {
            for (const auto &[style_key, style_set] : theme.styles) {
                if (style_set.style.font.has_value() && !style_set.style.font->empty() &&
                        !has_font_resource(tree, *style_set.style.font) &&
                        !is_builtin_default_font_id(*style_set.style.font)) {
                    std::string message = "Theme '";
                    message += theme_id;
                    message += "' style '";
                    message += style_key;
                    message += "' references missing font resource: ";
                    message += *style_set.style.font;
                    return std::unexpected(message);
                }
            }
        }
        for (const auto &[screen_id, screen] : tree.screens) {
            auto validation = validate_node_resource_references(tree, screen, "/" + screen_id);
            if (!validation) {
                return validation;
            }
        }
        for (const auto &[template_id, template_node] : tree.templates) {
            auto validation = validate_node_resource_references(tree, template_node, "<template:" + template_id + ">");
            if (!validation) {
                return validation;
            }
        }
        return {};
    }

    ResolvedStyle resolve_style(const TreeRecord &tree, const Node &node) const
    {
        ResolvedStyle resolved_style;
        auto style_set = compose_style_set(tree, node);
        resolve_style_set_color_fields(tree, style_set);
        resolved_style.style = style_set.style;
        resolved_style.state_styles = style_set.state_styles;
        resolved_style.part_styles = style_set.part_styles;

        struct FontDescriptor {
            std::string id;
            std::string kind;
            std::string primary_src;
            std::vector<std::string> fallback_ids;
            std::vector<NativeFontVariant> native_fonts;
            int32_t image_font_height = 0;
            std::vector<ImageFontGlyph> image_font_glyphs;
            std::vector<ImageFontSize> image_font_sizes;
        };

        auto find_font_descriptor = [&](std::string_view font_id, bool log_missing = true) -> std::optional<FontDescriptor> {
            auto dynamic_font_it = global_fonts.find(std::string(font_id));
            if (dynamic_font_it != global_fonts.end())
            {
                BROOKESIA_LOGD("Resolved font from dynamic backend resource: id='%1%', primary_src='%2%', fallback_count=%3%",
                               dynamic_font_it->second.id,
                               dynamic_font_it->second.primary_src,
                               dynamic_font_it->second.fallback_ids.size());
                return FontDescriptor {
                    .id = dynamic_font_it->second.id,
                    .kind = dynamic_font_it->second.kind,
                    .primary_src = dynamic_font_it->second.primary_src,
                    .fallback_ids = dynamic_font_it->second.fallback_ids,
                    .native_fonts = dynamic_font_it->second.native_fonts,
                    .image_font_height = dynamic_font_it->second.image_font_height,
                    .image_font_glyphs = dynamic_font_it->second.image_font_glyphs,
                    .image_font_sizes = dynamic_font_it->second.image_font_sizes,
                };
            }

            if (log_missing)
            {
                BROOKESIA_LOGW("Unable to resolve font id: '%1%'", font_id);
            }
            return std::nullopt;
        };

        auto resolve_font_chain = [&](std::string_view requested_font_id, bool allow_builtin_fallback = false) {
            const std::string font_id = requested_font_id.empty() ? std::string("default") : std::string(requested_font_id);
            auto font = find_font_descriptor(font_id, !allow_builtin_fallback || font_id != "default");
            if (!font.has_value()) {
                if (allow_builtin_fallback && font_id == "default") {
                    BROOKESIA_LOGD(
                        "No runtime/json 'default' font resource found; fallback to backend built-in font"
                    );
                    return;
                }
                BROOKESIA_LOGW("Font chain resolution failed: requested_font_id='%1%'", font_id);
                return;
            }

            resolved_style.resolved_font.font_id = font->id;
            resolved_style.resolved_font.kind = font->kind;
            resolved_style.resolved_font.primary_src = font->primary_src;
            resolved_style.resolved_font.native_fonts = font->native_fonts;
            resolved_style.resolved_font.image_font_height = font->image_font_height;
            resolved_style.resolved_font.image_font_glyphs = font->image_font_glyphs;
            resolved_style.resolved_font.image_font_sizes = font->image_font_sizes;

            boost::unordered_flat_set<std::string> visited_font_ids;
            std::vector<std::string> pending_font_ids(font->fallback_ids.begin(), font->fallback_ids.end());
            while (!pending_font_ids.empty()) {
                const auto current_font_id = pending_font_ids.front();
                pending_font_ids.erase(pending_font_ids.begin());
                if (!visited_font_ids.insert(current_font_id).second) {
                    continue;
                }
                auto fallback_font = find_font_descriptor(current_font_id);
                if (!fallback_font.has_value()) {
                    BROOKESIA_LOGW("Skipping unresolved fallback font id: '%1%'", current_font_id);
                    continue;
                }
                resolved_style.resolved_font.fallback_srcs.push_back(fallback_font->primary_src);
                for (const auto &nested_fallback : fallback_font->fallback_ids) {
                    pending_font_ids.push_back(nested_fallback);
                }
            }

            BROOKESIA_LOGD("Resolved font chain: requested_font_id='%1%', resolved_font_id='%2%', primary_src='%3%', fallback_count=%4%, native_count=%5%",
                           font_id,
                           resolved_style.resolved_font.font_id,
                           resolved_style.resolved_font.primary_src,
                           resolved_style.resolved_font.fallback_srcs.size(),
                           resolved_style.resolved_font.native_fonts.size());
        };

        if (resolved_style.style.font.has_value() && !resolved_style.style.font->empty()) {
            resolve_font_chain(
                *resolved_style.style.font,
                is_builtin_default_font_id(*resolved_style.style.font)
            );
            return resolved_style;
        }

        auto default_font_it = default_fonts_by_language.find(current_language);
        if (default_font_it != default_fonts_by_language.end() && !default_font_it->second.empty()) {
            resolve_font_chain(default_font_it->second, true);
            if (!resolved_style.resolved_font.font_id.empty() || !resolved_style.resolved_font.primary_src.empty() ||
                    !resolved_style.resolved_font.native_fonts.empty()) {
                return resolved_style;
            }
        }

        resolve_font_chain("default", true);
        return resolved_style;
    }

    // Returns a shared, immutable ResolvedStyle for `node`, reusing a cached instance when another node
    // resolves to the identical style. The document id is part of the key because local styleSet assets can
    // define the same styleRef name differently in different documents.
    std::shared_ptr<const ResolvedStyle> resolve_style_shared(const TreeRecord &tree, const Node &node)
    {
        if (resolved_style_cache_revision_ != current_style_revision_) {
            resolved_style_cache_.clear();
            resolved_style_cache_revision_ = current_style_revision_;
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            dbg_style_cache_hits_ = 0;
            dbg_style_cache_misses_ = 0;
#endif
        }

        std::string cache_key;
        cache_key += std::to_string(tree.document_id.value());
        cache_key += '\x1f';
        cache_key += BROOKESIA_DESCRIBE_ENUM_TO_STR(node.type);
        cache_key += '\x1f';
        for (const auto &style_ref : node.style_refs) {
            cache_key += style_ref;
            cache_key += '\x1e';
        }
        cache_key += '\x1f';
        cache_key += lib_utils::describe_json_serialize(node.style);
        cache_key += '\x1f';
        cache_key += lib_utils::describe_json_serialize(node.state_styles);
        cache_key += '\x1f';
        cache_key += lib_utils::describe_json_serialize(node.part_styles);

        auto cached_it = resolved_style_cache_.find(cache_key);
        if (cached_it != resolved_style_cache_.end()) {
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
            ++dbg_style_cache_hits_;
#endif
            return cached_it->second;
        }

        auto resolved = std::make_shared<const ResolvedStyle>(resolve_style(tree, node));
        resolved_style_cache_.emplace(std::move(cache_key), resolved);
#if BROOKESIA_GUI_INTERFACE_ENABLE_MEMORY_TRACE
        ++dbg_style_cache_misses_;
#endif
        return resolved;
    }

    void refresh_global_fonts_from_backend()
    {
        if (backend == nullptr) {
            return;
        }

        for (const auto &font_resource : backend->list_font_resources()) {
            if (unregistered_global_fonts.contains(font_resource.id)) {
                continue;
            }
            global_fonts.insert_or_assign(font_resource.id, font_resource);
            BROOKESIA_LOGD("Registered dynamic runtime font: id='%1%', primary_src='%2%', fallback_count=%3%, native_count=%4%",
                           font_resource.id,
                           font_resource.primary_src,
                           font_resource.fallback_ids.size(),
                           font_resource.native_fonts.size());
        }
    }

    void subscribe_bindings(DocumentId document_id, NodeRecord &record)
    {
        if (store == nullptr) {
            return;
        }
        const uint64_t uid = record.uid;

        for (const auto &[path, expression] : record.node.bindings) {
            auto binding_target = resolve_binding_target(record.node.type, path);
            if (!binding_target) {
                BROOKESIA_LOGW(
                    "Skipping invalid binding subscription path '%1%' on '%2%': %3%",
                    path,
                    record.absolute_path,
                    binding_target.error()
                );
                continue;
            }

            auto store_key = normalize_binding_store_key(expression);
            if (!store_key) {
                BROOKESIA_LOGW(
                    "Skipping invalid binding subscription value '%1%' on '%2%': %3%",
                    expression,
                    record.absolute_path,
                    store_key.error()
                );
                continue;
            }

            const auto target_info = *binding_target;
            auto listener = [this, document_id, uid, path = std::string(path), target_info]
            (std::string_view unused_key, std::string_view value) {
                (void)unused_key;
                if (suppress_binding_listener_apply_) {
                    return;
                }
                auto *tree = resolve_tree(document_id);
                if (tree == nullptr) {
                    return;
                }
                auto *node_record = find_node_record(*tree, uid);
                if (node_record == nullptr) {
                    return;
                }
                auto apply_result = apply_binding_value(*tree, *node_record, target_info, value);
                if (!apply_result) {
                    BROOKESIA_LOGW(
                        "Failed to apply binding update: node='%1%', path='%2%', value='%3%', reason='%4%'",
                        node_record->absolute_path,
                        path,
                        value,
                        apply_result.error()
                    );
                    return;
                }
                reapply_binding_domain(*tree, *node_record, target_info);
            };
            auto subscription_id = store->subscribe(document_id, record.absolute_path, *store_key, std::move(listener));
            record.subscriptions.push_back(subscription_id);
        }
    }

    void dispatch_backend_event(const BackendEvent &event)
    {
        for (auto &[unused_document_id, tree] : trees) {
            (void)unused_document_id;
            auto uid_it = tree.handle_to_uid.find(event.handle.value());
            if (uid_it == tree.handle_to_uid.end()) {
                continue;
            }

            auto *record = find_node_record(tree, uid_it->second);
            if (record == nullptr) {
                return;
            }

            Event action_event{
                .document_id = tree.document_id,
                .root_id = record->root_id,
                .node_id = record->node.id,
                .path = record->absolute_path,
                .type = event.type,
                .action = event.action,
                .payload = event.payload,
            };
            if (event.type == EventType::Pressed) {
                record->press_lost_since_pressed = false;
            } else if (event.type == EventType::PressLost) {
                record->press_lost_since_pressed = true;
            }
            if (event.type == EventType::ValueChanged) {
                if (record->node.type == NodeType::TextInput) {
                    if (auto text = action_event.get_string("text"); text.has_value()) {
                        record->node.text_input_props.text = std::string(*text);
                    }
                } else if (record->node.type == NodeType::Slider || record->node.type == NodeType::ProgressBar ||
                           record->node.type == NodeType::Arc) {
                    if (auto value = action_event.get_int64("value"); value.has_value()) {
                        record->node.range_props.value = static_cast<int32_t>(*value);
                    }
                } else if (record->node.type == NodeType::Switch || record->node.type == NodeType::Checkbox) {
                    if (auto checked = action_event.get_bool("checked"); checked.has_value()) {
                        record->node.toggle_props.checked = *checked;
                    }
                } else if (record->node.type == NodeType::Dropdown) {
                    if (auto selected_index = action_event.get_int64("selectedIndex"); selected_index.has_value()) {
                        record->node.dropdown_props.selected_index = static_cast<int32_t>(*selected_index);
                    }
                }
            }
            execute_event_effects(tree, *record, action_event);
            dispatch_event_action_handlers(action_event);
            record->event_signal(action_event);
            if (record->node.type == NodeType::Button && event.type == EventType::Clicked) {
                record->click_signal();
            }
            return;
        }
    }

    std::optional<uint64_t> resolve_any_uid(const TreeRecord &tree, std::string_view id) const
    {
        const auto query = normalize_absolute_path(id);
        if (query == "/") {
            return std::nullopt;
        }

        if (auto absolute_it = tree.absolute_path_to_uid.find(query); absolute_it != tree.absolute_path_to_uid.end()) {
            return absolute_it->second;
        }
        return std::nullopt;
    }

    TreeRecord *resolve_tree(DocumentId document_id)
    {
        auto it = trees.find(document_id.value());
        return it == trees.end() ? nullptr : &it->second;
    }

    const TreeRecord *resolve_tree_const(DocumentId document_id) const
    {
        auto it = trees.find(document_id.value());
        return it == trees.end() ? nullptr : &it->second;
    }

    static NodeRecord *find_node_record(TreeRecord &tree, uint64_t uid)
    {
        auto it = tree.nodes.find(uid);
        return it == tree.nodes.end() ? nullptr : &it->second;
    }

    static const NodeRecord *find_node_record_const(const TreeRecord &tree, uint64_t uid)
    {
        auto it = tree.nodes.find(uid);
        return it == tree.nodes.end() ? nullptr : &it->second;
    }

    std::optional<uint64_t> resolve_view_uid(const View &view) const
    {
        const auto *tree = resolve_tree_const(view.document_id_);
        if (tree == nullptr) {
            return std::nullopt;
        }

        return resolve_any_uid(*tree, view.absolute_path_);
    }

    NodeRecord *resolve_view_record(const View &view)
    {
        auto *tree = resolve_tree(view.document_id_);
        auto uid = resolve_view_uid(view);
        if (tree == nullptr || !uid.has_value()) {
            return nullptr;
        }
        return find_node_record(*tree, *uid);
    }

    NodeRecord *resolve_view_record(const View &view) const
    {
        auto *tree = resolve_tree_const(view.document_id_);
        auto uid = resolve_view_uid(view);
        if (tree == nullptr || !uid.has_value()) {
            return nullptr;
        }
        return const_cast<NodeRecord *>(find_node_record_const(*tree, *uid));
    }
};

Runtime::Runtime(std::unique_ptr<IBackend> backend)
    : impl_(std::make_unique<Impl>(std::move(backend)))
{}

Runtime::Runtime(std::unique_ptr<IBackend> backend, RuntimeTaskConfig task_config)
    : impl_(std::make_unique<Impl>(std::move(backend), std::move(task_config)))
{}

Runtime::~Runtime() = default;

std::expected<DocumentId, std::string> Runtime::load(
    std::string_view root_path, const Document &document, const Environment &environment)
{
    return impl_->load(root_path, document, environment);
}

std::expected<DocumentId, std::string> Runtime::load_json(
    std::string_view root_path, std::string_view json, std::string_view base_dir, const Environment &environment)
{
    const auto parse_environment = impl_->make_parse_environment(environment);
    auto document = parse_document(json, base_dir, parse_environment);
    if (!document) {
        return std::unexpected(document.error());
    }
    return impl_->load(root_path, std::move(*document), environment);
}

std::expected<DocumentId, std::string> Runtime::load_file(std::string_view path, const Environment &environment)
{
    const std::string path_string(path);
    const auto total_start = RuntimeProfileClock::now();
    auto stage_start = total_start;
    const auto parse_environment = impl_->make_parse_environment(environment);
    auto parsed_document = parse_document_file_with_metadata(path, parse_environment);
    auto stage_end = RuntimeProfileClock::now();
    if (!parsed_document) {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load file profile: file(%1%), stage(parse_document_failed), elapsed_ms(%2%), "
            "total_ms(%3%)",
            path_string,
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        return std::unexpected(parsed_document.error());
    }

    const auto dependency_count = parsed_document->dependency_files.size();
    const auto image_count = parsed_document->document.images.size();
    const auto screen_count = parsed_document->document.screens.size();
    const auto template_count = parsed_document->document.templates.size();
    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime load file profile: file(%1%), stage(parse_document), dependencies(%2%), images(%3%), "
        "screens(%4%), templates(%5%), elapsed_ms(%6%), total_ms(%7%)",
        path_string,
        dependency_count,
        image_count,
        screen_count,
        template_count,
        runtime_profile_elapsed_ms(stage_start, stage_end),
        runtime_profile_elapsed_ms(total_start, stage_end)
    );

    stage_start = RuntimeProfileClock::now();
    auto result = impl_->load(
                      path,
                      std::move(parsed_document->document),
                      environment,
                      true,
                      std::move(parsed_document->dependency_files)
                  );
    stage_end = RuntimeProfileClock::now();
    if (!result) {
        GUI_INTERFACE_PROFILE_LOGI(
            "GUI runtime load file profile: file(%1%), stage(load_document_failed), elapsed_ms(%2%), "
            "total_ms(%3%)",
            path_string,
            runtime_profile_elapsed_ms(stage_start, stage_end),
            runtime_profile_elapsed_ms(total_start, stage_end)
        );
        return result;
    }

    GUI_INTERFACE_PROFILE_LOGI(
        "GUI runtime load file profile: file(%1%), doc(%2%), stage(load_document), elapsed_ms(%3%), total_ms(%4%)",
        path_string,
        result->value(),
        runtime_profile_elapsed_ms(stage_start, stage_end),
        runtime_profile_elapsed_ms(total_start, stage_end)
    );
    return result;
}

std::expected<void, std::string> Runtime::load_theme(const ThemeAsset &theme)
{
    return impl_->load_theme(theme);
}

std::expected<void, std::string> Runtime::load_theme_json(std::string_view json, std::string_view base_dir)
{
    return impl_->load_theme_json(json, base_dir);
}

std::expected<void, std::string> Runtime::load_theme_file(std::string_view path)
{
    return impl_->load_theme_file(path);
}

std::vector<std::string> Runtime::list_supported_themes() const
{
    return impl_->list_supported_themes();
}

std::expected<void, std::string> Runtime::set_theme(std::string_view theme_id)
{
    return impl_->set_theme(this, theme_id);
}

std::expected<void, std::string> Runtime::set_theme(std::string_view theme_id, bool reapply_loaded_documents)
{
    return impl_->set_theme(this, theme_id, reapply_loaded_documents);
}

std::string Runtime::get_theme() const
{
    return impl_->get_theme();
}

std::expected<void, std::string> Runtime::register_font(const RuntimeFontResource &resource)
{
    return impl_->register_font(resource);
}

bool Runtime::unregister_font(std::string_view id)
{
    return impl_->unregister_font(id);
}

std::expected<void, std::string> Runtime::register_font_json(std::string_view json, std::string_view base_dir)
{
    return impl_->register_font_json(json, base_dir);
}

std::expected<void, std::string> Runtime::register_font_file(std::string_view path)
{
    return impl_->register_font_file(path);
}

std::vector<std::string> Runtime::list_supported_fonts(std::string_view language) const
{
    return impl_->list_supported_fonts(language);
}

std::vector<std::string> Runtime::list_supported_languages() const
{
    return impl_->list_supported_languages();
}

std::vector<std::string> Runtime::list_supported_languages(std::string_view font_id) const
{
    return impl_->list_supported_languages(font_id);
}

std::expected<void, std::string> Runtime::set_language(std::string_view language)
{
    return impl_->set_language(this, language);
}

std::expected<void, std::string> Runtime::set_language(std::string_view language, bool reapply_loaded_documents)
{
    return impl_->set_language(this, language, reapply_loaded_documents);
}

std::string Runtime::get_language() const
{
    return impl_->get_language();
}

std::expected<void, std::string> Runtime::set_default_font_for_language(
    std::string_view language,
    std::string_view font_id)
{
    return impl_->set_default_font_for_language(language, font_id);
}

std::optional<std::string> Runtime::get_default_font_for_language(std::string_view language) const
{
    return impl_->get_default_font_for_language(language);
}

std::expected<void, std::string> Runtime::register_image(const RuntimeImageResource &resource)
{
    return impl_->register_image(resource);
}

bool Runtime::unregister_image(std::string_view id)
{
    return impl_->unregister_image(id);
}

std::expected<void, std::string> Runtime::register_image_json(std::string_view json, std::string_view base_dir)
{
    return impl_->register_image_json(json, base_dir);
}

std::expected<void, std::string> Runtime::register_image_file(std::string_view path)
{
    return impl_->register_image_file(path);
}

std::expected<void, std::string> Runtime::preload_image(DocumentId id, std::string_view image_id)
{
    return preload_images(id, {std::string(image_id)});
}

std::expected<void, std::string> Runtime::preload_images(DocumentId id, const std::vector<std::string> &image_ids)
{
    return impl_->preload_images(id, image_ids);
}

std::expected<void, std::string> Runtime::release_preloaded_image(DocumentId id, std::string_view image_id)
{
    return release_preloaded_images(id, {std::string(image_id)});
}

std::expected<void, std::string> Runtime::release_preloaded_images(
    DocumentId id,
    const std::vector<std::string> &image_ids)
{
    return impl_->release_preloaded_images(id, image_ids);
}

void Runtime::process_backend()
{
    if (impl_->backend != nullptr) {
        impl_->backend->process_timers();
    }
}

void Runtime::set_view_debug_enabled(bool enabled)
{
    impl_->set_view_debug_enabled(enabled);
}

bool Runtime::is_view_debug_enabled() const
{
    return impl_->is_view_debug_enabled();
}

std::expected<void, std::string> Runtime::enable_live_preview(DocumentId id, const LivePreviewOptions &options)
{
    return impl_->enable_live_preview(id, options);
}

bool Runtime::disable_live_preview(DocumentId id)
{
    return impl_->disable_live_preview(id);
}

void Runtime::poll_live_preview()
{
    impl_->poll_live_preview(this);
}

bool Runtime::unload(DocumentId id)
{
    return impl_->unload(id);
}

SubscriptionId Runtime::subscribe_event_action_with_id(
    DocumentId id,
    std::string_view action,
    ActionHandler handler)
{
    return impl_->subscribe_event_action_with_id(this, id, action, std::move(handler));
}

bool Runtime::unsubscribe_subscription(SubscriptionId subscription_id)
{
    return impl_->unsubscribe_subscription(subscription_id);
}

ScopedConnection Runtime::subscribe_event_action(
    DocumentId id,
    std::string_view action,
    ActionHandler handler)
{
    auto connection = impl_->connect_event_action_signal(this, id, action, std::move(handler));
    if (!connection) {
        return {};
    }
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [connection = std::move(*connection)]() mutable {
        connection.disconnect();
    };
    return ScopedConnection(disconnect_handler);
}

std::vector<GuiLayer> Runtime::list_layers() const
{
    return impl_->list_layers();
}

std::vector<GuiDisplayInfo> Runtime::list_displays() const
{
    return impl_->list_displays();
}

std::expected<View, std::string> Runtime::mount_screen(
    DocumentId id,
    std::string_view absolute_path,
    const MountTarget &target)
{
    return impl_->mount_screen(this, id, absolute_path, target);
}

bool Runtime::unmount_screen(DocumentId id, std::string_view absolute_path)
{
    return impl_->unmount_screen(id, absolute_path);
}

std::expected<TransientMountId, std::string> Runtime::push_transient_screen(
    DocumentId id,
    std::string_view absolute_path,
    const MountTarget &target)
{
    return impl_->push_transient_screen(this, id, absolute_path, target);
}

bool Runtime::pop_transient_screen(TransientMountId id)
{
    return impl_->pop_transient_screen(id);
}

std::expected<void, std::string> Runtime::start_screen_flow(
    DocumentId id,
    std::string_view flow_id,
    const MountTarget &target)
{
    return impl_->start_screen_flow(this, id, flow_id, target);
}

std::expected<void, std::string> Runtime::trigger_screen_flow(
    DocumentId id,
    std::string_view flow_id,
    std::string_view action)
{
    return impl_->trigger_screen_flow(this, id, flow_id, action);
}

bool Runtime::stop_screen_flow(DocumentId id, std::string_view flow_id)
{
    return impl_->stop_screen_flow(id, flow_id);
}

bool Runtime::has_screen_flow(DocumentId id, std::string_view flow_id) const
{
    return impl_->has_screen_flow(id, flow_id);
}

std::optional<std::string> Runtime::get_screen_flow_state(DocumentId id, std::string_view flow_id) const
{
    return impl_->get_screen_flow_state(id, flow_id);
}

View Runtime::find_view(DocumentId id, std::string_view absolute_path) const
{
    return impl_->find_view(const_cast<Runtime *>(this), id, absolute_path);
}

std::expected<View, std::string> Runtime::create_view(
    DocumentId id, std::string_view template_id, std::string_view parent_absolute_path, std::string_view instance_id)
{
    return impl_->create_view(this, id, template_id, parent_absolute_path, instance_id);
}

bool Runtime::destroy_view(DocumentId id, std::string_view absolute_path)
{
    return impl_->destroy_view(id, absolute_path);
}

std::expected<void, std::string> Runtime::update(DocumentId id, std::string_view file_path, const Environment &environment)
{
    return impl_->update(this, id, file_path, environment);
}

std::expected<void, std::string> Runtime::reapply_styles(DocumentId id)
{
    return impl_->reapply_styles(id);
}

std::expected<boost::json::value, std::string> Runtime::get_constant_value(
    DocumentId id,
    std::string_view path
) const
{
    return impl_->get_constant_value(id, path);
}

std::vector<RuntimeFontResource> Runtime::list_font_resources(DocumentId id) const
{
    return impl_->list_font_resources(id);
}

std::vector<RuntimeImageResource> Runtime::list_image_resources(DocumentId id) const
{
    return impl_->list_image_resources(id);
}

std::optional<ViewStateValue> Runtime::get_view_state(const View &view, ViewStateKind kind) const
{
    return impl_->get_view_state_internal(view, kind);
}

void Runtime::set_binding_value(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value) const
{
    if (impl_->store == nullptr) {
        return;
    }
    impl_->store->set_string(id, absolute_path, key, std::move(value));
}

void Runtime::set_binding_values(DocumentId id, const std::vector<BindingValueUpdate> &updates) const
{
    impl_->set_binding_values(id, updates);
}

std::optional<std::string> Runtime::get_binding_value(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key) const
{
    return impl_->store == nullptr ? std::nullopt : impl_->store->get_string(id, absolute_path, key);
}

SubscriptionId Runtime::subscribe_binding_value_with_id(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key,
    BindingValueHandler handler) const
{
    if (impl_->store == nullptr || !id.is_valid() || absolute_path.empty() || key.empty() || !handler) {
        return 0;
    }

    const auto public_subscription_id = impl_->next_subscription_id_++;
    auto store_subscription_id = impl_->store->subscribe(
                                     id,
                                     absolute_path,
                                     key,
                                     [absolute_path = std::string(absolute_path), key = std::string(key), handler = std::move(handler)]
    (std::string_view unused_scoped_key, std::string_view value) {
        (void)unused_scoped_key;
        handler(absolute_path, key, value);
    }
                                 );
    if (store_subscription_id == 0) {
        return 0;
    }

    auto store = impl_->store;
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [registry = std::weak_ptr<Runtime::Impl::SubscriptionRegistry>(impl_->subscription_registry_),
                                    public_subscription_id,
                                    store = std::move(store),
             store_subscription_id]() mutable {
        if (store != nullptr)
        {
            store->unsubscribe(store_subscription_id);
        }
        if (auto locked_registry = registry.lock(); locked_registry != nullptr)
        {
            locked_registry->disconnect_handlers.erase(public_subscription_id);
        }
    };
    impl_->subscription_registry_->disconnect_handlers[public_subscription_id] = disconnect_handler;
    impl_->register_document_subscription(public_subscription_id, id);
    return public_subscription_id;
}

ScopedConnection Runtime::subscribe_binding_value(
    DocumentId id,
    std::string_view absolute_path,
    std::string_view key,
    BindingValueHandler handler) const
{
    if (impl_->store == nullptr || !id.is_valid() || absolute_path.empty() || key.empty() || !handler) {
        return {};
    }

    auto store_subscription_id = impl_->store->subscribe(
                                     id,
                                     absolute_path,
                                     key,
                                     [absolute_path = std::string(absolute_path), key = std::string(key), handler = std::move(handler)]
    (std::string_view unused_scoped_key, std::string_view value) {
        (void)unused_scoped_key;
        handler(absolute_path, key, value);
    }
                                 );
    if (store_subscription_id == 0) {
        return {};
    }

    auto store = impl_->store;
    auto disconnect_handler = std::make_shared<std::function<void()>>();
    *disconnect_handler = [store = std::move(store), store_subscription_id]() mutable {
        if (store != nullptr)
        {
            store->unsubscribe(store_subscription_id);
        }
    };
    return ScopedConnection(disconnect_handler);
}

SubscriptionId Runtime::start_view_animation_with_id(
    DocumentId id,
    std::string_view absolute_path,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return start_view_animation_with_id(find_view(id, absolute_path), animation, std::move(completed_handler));
}

RuntimeAnimationStartResult Runtime::start_view_animation_with_result(
    DocumentId id,
    std::string_view absolute_path,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return start_view_animation_with_result(find_view(id, absolute_path), animation, std::move(completed_handler));
}

ScopedConnection Runtime::start_view_animation(
    DocumentId id,
    std::string_view absolute_path,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return start_view_animation(find_view(id, absolute_path), animation, std::move(completed_handler));
}

SubscriptionId Runtime::start_view_animation_with_id(
    const View &view,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return impl_->start_view_animation_with_id(view, animation, std::move(completed_handler));
}

RuntimeAnimationStartResult Runtime::start_view_animation_with_result(
    const View &view,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return impl_->start_view_animation_with_result(view, animation, std::move(completed_handler));
}

ScopedConnection Runtime::start_view_animation(
    const View &view,
    const Animation &animation,
    AnimationCompletedHandler completed_handler)
{
    return impl_->start_view_animation(view, animation, std::move(completed_handler));
}

bool Runtime::is_view_valid(const View &view) const
{
    return impl_->is_view_valid(view);
}

std::string Runtime::get_view_id(const View &view) const
{
    return impl_->get_view_id(view);
}

std::string Runtime::get_view_absolute_path(const View &view) const
{
    return view.absolute_path_;
}

std::optional<ViewStateValue> Runtime::get_view_state_internal(const View &view, ViewStateKind kind) const
{
    return impl_->get_view_state_internal(view, kind);
}

std::optional<ViewStateValue> Runtime::get_view_state(DocumentId id, std::string_view absolute_path, ViewStateKind kind) const
{
    return get_view_state(find_view(id, absolute_path), kind);
}

bool Runtime::scroll_view_to_visible(DocumentId id, std::string_view absolute_path, bool animated) const
{
    return scroll_view_to_visible(find_view(id, absolute_path), animated);
}

bool Runtime::scroll_view_to(DocumentId id, std::string_view absolute_path, int32_t x, int32_t y, bool animated) const
{
    return scroll_view_to(find_view(id, absolute_path), x, y, animated);
}

bool Runtime::scroll_view_to(const View &view, int32_t x, int32_t y, bool animated) const
{
    return impl_->scroll_view_to(view, x, y, animated);
}

bool Runtime::scroll_view_to_visible(const View &view, bool animated) const
{
    return impl_->scroll_view_to_visible(view, animated);
}

bool Runtime::set_view_hidden(const View &view, bool hidden) const
{
    return impl_->set_view_hidden(view, hidden);
}

bool Runtime::set_view_text(const View &view, std::string_view text) const
{
    return impl_->set_view_text(view, text);
}

std::string Runtime::get_view_text(const View &view) const
{
    return impl_->get_view_text(view);
}

bool Runtime::set_view_src(const View &view, std::string_view src) const
{
    return impl_->set_view_src(view, src);
}

bool Runtime::set_view_src(DocumentId id, std::string_view absolute_path, std::string_view src) const
{
    return set_view_src(find_view(id, absolute_path), src);
}

std::string Runtime::get_view_src(const View &view) const
{
    return impl_->get_view_src(view);
}

bool Runtime::set_view_value(const View &view, int32_t value) const
{
    return impl_->set_view_value(view, value);
}

int32_t Runtime::get_view_value(const View &view) const
{
    return impl_->get_view_value(view);
}

bool Runtime::set_view_checked(const View &view, bool checked) const
{
    return impl_->set_view_checked(view, checked);
}

bool Runtime::get_view_checked(const View &view) const
{
    return impl_->get_view_checked(view);
}

bool Runtime::set_view_selected_index(const View &view, int32_t index) const
{
    return impl_->set_view_selected_index(view, index);
}

int32_t Runtime::get_view_selected_index(const View &view) const
{
    return impl_->get_view_selected_index(view);
}

bool Runtime::set_table_cell_text(const View &view, int32_t row, int32_t column, std::string_view text) const
{
    return impl_->set_table_cell_text(view, row, column, text);
}

esp_brookesia::lib_utils::connection Runtime::connect_view_event(
    const View &view, EventType type, View::EventHandler handler) const
{
    return impl_->connect_view_event(view, type, std::move(handler));
}

esp_brookesia::lib_utils::connection Runtime::connect_button_click(const Button &button, Button::ClickHandler handler) const
{
    return impl_->connect_button_click(button, std::move(handler));
}

} // namespace esp_brookesia::gui
