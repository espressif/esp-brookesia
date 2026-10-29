/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdint>
#include <functional>
#if !defined(ESP_PLATFORM)
#include <filesystem>
#include <fstream>
#include <sstream>
#endif
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/gui_interface.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#if !defined(ESP_PLATFORM)
#include "brookesia/service_manager.hpp"
#endif

using namespace esp_brookesia::gui;

namespace {

constexpr std::string_view ROOT_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewScreen",
            "id": "test_screen",
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": {
                        "text": "Root Base"
                    },
                    "style": {
                        "fontSize": "20sp",
                        "textColor": "#111827"
                    }
                },
                {
                    "type": "slider",
                    "id": "level",
                    "rangeProps": {
                        "value": 30,
                        "min": 0,
                        "max": 100
                    }
                },
                {
                    "type": "container",
                    "id": "grow",
                    "placement": {
                        "mode": "flow",
                        "width": "wrap",
                        "height": "1dp",
                        "flexGrow": 1
                    }
                }
            ]
        }
    ]
})";

constexpr std::string_view IMAGE_BINDING_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewScreen",
            "id": "image_screen",
            "children": [
                {
                    "type": "image",
                    "id": "icon_a",
                    "bindings": {
                        "imageProps.src": "src_a"
                    },
                    "imageProps": {
                        "src": ""
                    },
                    "placement": {
                        "width": "32dp",
                        "height": "32dp"
                    }
                },
                {
                    "type": "image",
                    "id": "icon_b",
                    "bindings": {
                        "imageProps.src": "src_b"
                    },
                    "imageProps": {
                        "src": ""
                    },
                    "placement": {
                        "width": "32dp",
                        "height": "32dp"
                    }
                }
            ]
        }
    ]
})";

constexpr std::string_view DEFERRED_EAGER_SCREENS_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewScreen",
            "id": "home",
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "Home" }
                },
                {
                    "type": "container",
                    "id": "list"
                }
            ]
        },
        {
            "type": "viewScreen",
            "id": "settings",
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": { "text": "Settings" }
                }
            ]
        },
        {
            "type": "viewTemplate",
            "id": "badge",
            "node": {
                "type": "label",
                "labelProps": { "text": "Badge" }
            }
        },
        {
            "type": "screenFlow",
            "id": "main",
            "screens": [ "home", "settings" ],
            "initial": "home",
            "transitions": [
                { "from": [], "action": "open_settings", "to": "settings" }
            ]
        }
    ]
})";

constexpr std::string_view DYNAMIC_SCREEN_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewScreen",
            "id": "dynamic_screen",
            "mount_mode": "dynamic",
            "children": [
                {
                    "type": "label",
                    "id": "left",
                    "labelProps": {
                        "text": "Definition text"
                    }
                },
                {
                    "type": "label",
                    "id": "right",
                    "labelProps": {
                        "text": "Definition text"
                    }
                }
            ]
        }
    ]
})";

constexpr std::string_view TEMPLATE_INSTANCE_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewTemplate",
            "id": "row",
            "node": {
                "type": "container",
                "children": [
                    {
                        "type": "label",
                        "id": "setter_label",
                        "labelProps": {
                            "text": "Setter definition"
                        }
                    },
                    {
                        "type": "label",
                        "id": "bound_label",
                        "bindings": {
                            "labelProps.text": "title"
                        },
                        "labelProps": {
                            "text": "Binding definition"
                        }
                    }
                ]
            }
        },
        {
            "type": "viewScreen",
            "id": "template_screen",
            "children": [
                {
                    "type": "container",
                    "id": "list"
                }
            ]
        }
    ]
})";

constexpr std::string_view THEME_LANGUAGE_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewTemplate",
            "id": "theme_row",
            "node": {
                "type": "label",
                "labelProps": {
                    "text": "Dynamically created row"
                }
            }
        },
        {
            "type": "viewScreen",
            "id": "theme_screen",
            "children": [
                {
                    "type": "label",
                    "id": "title",
                    "labelProps": {
                        "text": "Theme and language"
                    }
                },
                {
                    "type": "container",
                    "id": "list"
                }
            ]
        }
    ]
})";

constexpr std::string_view COMPACT_BINDING_DOMAINS_JSON = R"({
    "version": "0.1.1",
    "assets": [
        {
            "type": "viewScreen",
            "id": "binding_screen",
            "mount_mode": "dynamic",
            "children": [
                {
                    "type": "container",
                    "id": "panel",
                    "bindings": {
                        "style.bgColor": "background",
                        "style.opacity": "opacity",
                        "stateStyles.pressed.bgColor": "pressed_background",
                        "partStyles.indicator.textColor": "indicator_text",
                        "partStyles.indicator.stateStyles.checked.borderColor": "indicator_border",
                        "layout.gap": "gap",
                        "placement.width": "width"
                    },
                    "style": { "radius": "5dp" },
                    "stateStyles": {
                        "pressed": { "borderWidth": "2dp" }
                    },
                    "layout": { "type": "flex", "gap": "1dp" },
                    "placement": { "width": "10dp", "height": "10dp" }
                },
                {
                    "type": "textInput",
                    "id": "input",
                    "bindings": { "textInputProps.text": "text" }
                },
                {
                    "type": "switch",
                    "id": "toggle",
                    "bindings": { "toggleProps.checked": "checked" }
                },
                {
                    "type": "dropdown",
                    "id": "dropdown",
                    "bindings": { "dropdownProps.selectedIndex": "selected" },
                    "dropdownProps": { "options": ["zero", "one"] }
                },
                {
                    "type": "frameView",
                    "id": "frame",
                    "bindings": { "frameViewProps.outputName": "output" }
                },
                {
                    "type": "line",
                    "id": "line",
                    "bindings": { "lineProps.points": "points" }
                },
                {
                    "type": "table",
                    "id": "table",
                    "bindings": { "tableProps.cells": "cells" },
                    "tableProps": { "rows": 1, "columns": 1 }
                },
                {
                    "type": "keyboard",
                    "id": "keyboard",
                    "bindings": { "keyboardProps.mode": "mode" }
                },
                {
                    "type": "canvas",
                    "id": "canvas",
                    "bindings": { "canvasProps.commands": "commands" }
                }
            ]
        }
    ]
})";

#if !defined(ESP_PLATFORM)
class HostStorageService final : public esp_brookesia::service::ServiceBase {
public:
    HostStorageService()
        : ServiceBase({
        .name = "Storage",
        .description = "Host-only GUI interface test storage service",
        .version = "0.0.0",
    })
    {}

    std::vector<esp_brookesia::service::FunctionSchema> get_function_schemas() override
    {
        using namespace esp_brookesia::service;
        return {{
                .name = "FSReadText",
                .description = "Read a host test file as text",
                .parameters = {{
                        .name = "Path",
                        .description = "Host file path",
                        .type = FunctionValueType::String,
                    }
                },
                .require_scheduler = false,
                .return_value = FunctionReturnSchema{
                    .type = FunctionValueType::String,
                    .description = "File contents",
                },
            }};
    }

    FunctionHandlerList get_function_handlers() override
    {
        auto read_text = [](esp_brookesia::service::FunctionParameterMap && parameters) {
            using namespace esp_brookesia::service;
            const auto path_it = parameters.find("Path");
            if (path_it == parameters.end()) {
                return FunctionResult{.success = false, .error_message = "Path is missing"};
            }
            const auto *path = std::get_if<std::string>(&path_it->second);
            if (path == nullptr) {
                return FunctionResult{.success = false, .error_message = "Path is not a string"};
            }
            std::ifstream stream(*path, std::ios::binary);
            if (!stream.is_open()) {
                return FunctionResult{.success = false, .error_message = "Failed to open host test file"};
            }
            std::ostringstream content;
            content << stream.rdbuf();
            if (stream.bad()) {
                return FunctionResult{.success = false, .error_message = "Failed to read host test file"};
            }
            return FunctionResult{
                .success = true,
                .data = FunctionValue(content.str()),
            };
        };
        return {std::move(read_text)};
    }
};

class HostStorageServiceGuard {
public:
    ~HostStorageServiceGuard()
    {
        if (initialized_) {
            esp_brookesia::service::ServiceManager::get_instance().deinit();
        }
    }

    bool start()
    {
        auto &manager = esp_brookesia::service::ServiceManager::get_instance();
        initialized_ = manager.init();
        return initialized_ && manager.add_service(std::make_shared<HostStorageService>());
    }

private:
    bool initialized_ = false;
};

class TemporaryJsonFile {
public:
    TemporaryJsonFile()
        : path_(
              std::filesystem::temp_directory_path() /
              ("brookesia_gui_interface_" + std::to_string(reinterpret_cast<uintptr_t>(this)) + ".json")
          )
    {}

    ~TemporaryJsonFile()
    {
        std::error_code error;
        std::filesystem::remove(path_, error);
    }

    bool write(std::string_view content) const
    {
        std::ofstream stream(path_, std::ios::binary | std::ios::trunc);
        stream.write(content.data(), static_cast<std::streamsize>(content.size()));
        stream.close();
        return !stream.fail();
    }

    std::string path() const
    {
        return path_.string();
    }

private:
    std::filesystem::path path_;
};
#endif

std::string append_child_path(std::string_view parent_path, std::string_view id)
{
    if (parent_path.empty() || parent_path == "/") {
        return "/" + std::string(id);
    }
    auto result = std::string(parent_path);
    if (result.back() != '/') {
        result.push_back('/');
    }
    result.append(id);
    return result;
}

class MockBackend final: public IBackend {
public:
    struct AppliedImageState {
        ImageProps props;
        ResolvedImageSpec resolved;
        Placement placement;
    };

    void set_event_sink(EventSink sink) override
    {
        event_sink_ = std::move(sink);
    }

    BackendHandle create_node(
        const Node &node,
        BackendHandle parent,
        std::string_view parent_path,
        std::string_view scope_root_absolute_path
    ) override
    {
        (void)parent;
        (void)scope_root_absolute_path;
        const auto handle = BackendHandle(next_handle_++);
        const auto absolute_path = append_child_path(parent_path, node.id);
        path_to_handle_[absolute_path] = handle;
        node_types_[handle.value()] = node.type;
        create_count_++;
        return handle;
    }

    void destroy_node(BackendHandle handle) override
    {
        destroyed_handles_.push_back(handle);
    }

    void apply_props(BackendHandle handle, const Node &node, PropsApplyMask mask = PropsApplyMask::All) override
    {
        (void)mask;
        props_apply_count_++;
        node_types_[handle.value()] = node.type;
        if (node.type == NodeType::Image) {
            applied_images_.insert_or_assign(handle.value(), AppliedImageState{
                .props = node.image_props,
                .resolved = node.resolved_image,
                .placement = node.placement,
            });
        }
        auto hook = std::move(next_props_apply_hook_);
        if (hook) {
            hook();
        }
    }

    void apply_layout(BackendHandle handle, const Layout &layout, LayoutApplyMask mask = LayoutApplyMask::All) override
    {
        (void)handle;
        (void)layout;
        (void)mask;
        layout_apply_count_++;
    }

    void apply_placement(
        BackendHandle handle, const Placement &placement, PlacementApplyMask mask = PlacementApplyMask::All
    ) override
    {
        (void)handle;
        (void)placement;
        (void)mask;
        placement_apply_count_++;
    }

    void apply_style(
        BackendHandle handle,
        const ResolvedStyle &style,
        StyleApplyMask mask = StyleApplyMask::All
    ) override
    {
        (void)mask;
        applied_styles_.insert_or_assign(handle.value(), style);
        style_apply_count_++;
    }

    void apply_debug_visual(BackendHandle handle, bool enabled) override
    {
        (void)handle;
        debug_visual_enabled_ = enabled;
    }

    void apply_animations(BackendHandle handle, const std::vector<Animation> &animations) override
    {
        (void)handle;
        animation_apply_count_ += animations.size();
    }

    std::optional<BackendAnimationStartResult> start_animation(
        BackendHandle handle,
        const Animation &animation,
        std::function<void()> completed_handler = {}
    ) override
    {
        (void)handle;
        start_animation_count_++;
        if (completed_handler) {
            completed_handler();
        }
        return BackendAnimationStartResult{
            .connection = ScopedConnection([]() {}),
            .resolved_from = animation.from,
            .resolved_to = animation.to,
        };
    }

    void bind_events(BackendHandle handle, const std::vector<EventBinding> &events) override
    {
        bound_event_count_[handle.value()] = events.size();
    }

    std::vector<GuiDisplayInfo> list_displays() const override
    {
        return {{
                .id = "display0",
                .width_px = 320,
                .height_px = 480,
                .is_default = true,
            }};
    }

    std::vector<GuiLayer> list_layers() const override
    {
        return {GuiLayer::Default, GuiLayer::System};
    }

    bool mount_screen(BackendHandle handle, const MountTarget &target) override
    {
        (void)target;
        mounted_handles_.push_back(handle);
        return handle.is_valid();
    }

    bool unmount_screen(BackendHandle handle) override
    {
        unmounted_handles_.push_back(handle);
        return handle.is_valid();
    }

    bool register_font_resource(const RuntimeFontResource &resource) override
    {
        fonts_.push_back(resource);
        return !resource.id.empty();
    }

    std::vector<RuntimeFontResource> list_font_resources() const override
    {
        return fonts_;
    }

    std::expected<void, std::string> preload_image_resource(const RuntimeImageResource &resource) override
    {
        if (resource.id.empty()) {
            return std::unexpected("image id is empty");
        }
        images_.push_back(resource);
        return {};
    }

    bool requires_preloaded_image_resource(const RuntimeImageResource &resource) const override
    {
        return resource.native_src == 0 && !resource.primary_src.empty();
    }

    void release_image_resource(const RuntimeImageResource &resource) override
    {
        released_image_ids_.push_back(resource.id);
    }

    std::optional<ViewFrame> get_node_frame(BackendHandle handle) const override
    {
        if (!handle.is_valid()) {
            return std::nullopt;
        }
        return ViewFrame{.x = 1, .y = 2, .width = 100, .height = 40};
    }

    bool scroll_node_to(BackendHandle handle, int32_t x, int32_t y, bool animated) override
    {
        scroll_to_count_++;
        last_scroll_to_handle_ = handle;
        last_scroll_x_ = x;
        last_scroll_y_ = y;
        last_scroll_animated_ = animated;
        return handle.is_valid();
    }

    bool scroll_node_to_visible(BackendHandle handle, bool animated) override
    {
        (void)animated;
        scroll_count_++;
        return handle.is_valid();
    }

    BackendHandle handle_for_path(std::string_view absolute_path) const
    {
        auto it = path_to_handle_.find(std::string(absolute_path));
        return (it == path_to_handle_.end()) ? BackendHandle{} : it->second;
    }

    void emit_event(const BackendEvent &event)
    {
        if (event_sink_) {
            event_sink_(event);
        }
    }

    size_t create_count() const
    {
        return create_count_;
    }

    size_t start_animation_count() const
    {
        return start_animation_count_;
    }

    size_t props_apply_count() const
    {
        return props_apply_count_;
    }

    void set_next_props_apply_hook(std::function<void()> hook)
    {
        next_props_apply_hook_ = std::move(hook);
    }

    size_t style_apply_count() const
    {
        return style_apply_count_;
    }

    size_t destroyed_count() const
    {
        return destroyed_handles_.size();
    }

    const ResolvedStyle *style_for_handle(BackendHandle handle) const
    {
        auto it = applied_styles_.find(handle.value());
        return (it == applied_styles_.end()) ? nullptr : &it->second;
    }

    const std::vector<RuntimeImageResource> &preloaded_images() const
    {
        return images_;
    }

    const std::vector<std::string> &released_image_ids() const
    {
        return released_image_ids_;
    }

    const AppliedImageState *image_state_for_handle(BackendHandle handle) const
    {
        auto it = applied_images_.find(handle.value());
        return it == applied_images_.end() ? nullptr : &it->second;
    }

    bool debug_visual_enabled() const
    {
        return debug_visual_enabled_;
    }

    size_t scroll_to_count() const
    {
        return scroll_to_count_;
    }

    BackendHandle last_scroll_to_handle() const
    {
        return last_scroll_to_handle_;
    }

    int32_t last_scroll_x() const
    {
        return last_scroll_x_;
    }

    int32_t last_scroll_y() const
    {
        return last_scroll_y_;
    }

    bool last_scroll_animated() const
    {
        return last_scroll_animated_;
    }

private:
    EventSink event_sink_;
    BackendHandle::Value next_handle_ = 1;
    std::map<std::string, BackendHandle> path_to_handle_;
    std::map<BackendHandle::Value, NodeType> node_types_;
    std::map<BackendHandle::Value, size_t> bound_event_count_;
    std::map<BackendHandle::Value, ResolvedStyle> applied_styles_;
    std::map<BackendHandle::Value, AppliedImageState> applied_images_;
    std::vector<BackendHandle> mounted_handles_;
    std::vector<BackendHandle> unmounted_handles_;
    std::vector<BackendHandle> destroyed_handles_;
    std::vector<RuntimeFontResource> fonts_;
    std::vector<RuntimeImageResource> images_;
    std::vector<std::string> released_image_ids_;
    std::function<void()> next_props_apply_hook_;
    size_t create_count_ = 0;
    size_t props_apply_count_ = 0;
    size_t layout_apply_count_ = 0;
    size_t placement_apply_count_ = 0;
    size_t style_apply_count_ = 0;
    size_t animation_apply_count_ = 0;
    size_t start_animation_count_ = 0;
    size_t scroll_to_count_ = 0;
    size_t scroll_count_ = 0;
    BackendHandle last_scroll_to_handle_;
    int32_t last_scroll_x_ = 0;
    int32_t last_scroll_y_ = 0;
    bool last_scroll_animated_ = true;
    bool debug_visual_enabled_ = false;
};

} // namespace

BROOKESIA_TEST_CASE(
    test_gui_interface_parser_and_validator_accept_valid_document,
    "GUI interface parses and validates a screen document",
    "[gui][interface][parser]"
)
{
    Environment environment;
    auto parsed = parse_document(ROOT_JSON, "test", environment);
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL_STRING("0.1.1", parsed->version.c_str());
    TEST_ASSERT_EQUAL_size_t(1, parsed->screens.size());
    TEST_ASSERT_EQUAL_STRING("test_screen", parsed->screens.front().id.c_str());
    TEST_ASSERT_EQUAL_INT32(0, parsed->screens.front().children.front().placement.flex_grow);
    TEST_ASSERT_EQUAL_INT32(1, parsed->screens.front().children.at(2).placement.flex_grow);

    auto validation = validate_document(parsed.value());
    TEST_ASSERT_TRUE(validation.success);

    auto images = parse_image_asset_set_json(R"({
        "type": "imageSet",
        "images": [
            {"id": "lazy", "src": "lazy.png", "width": 10, "height": 10},
            {"id": "eager", "src": "eager.png", "width": 20, "height": 20, "preload": true}
        ]
    })", "test/images");
    TEST_ASSERT_TRUE(images.has_value());
    TEST_ASSERT_EQUAL_size_t(2, images->size());
    TEST_ASSERT_FALSE(images->at(0).preload);
    TEST_ASSERT_TRUE(images->at(1).preload);

    auto invalid_images = parse_image_asset_set_json(R"({
        "type": "imageSet",
        "images": [
            {"id": "bad", "src": "bad.png", "width": 10, "height": 10, "preload": "yes"}
        ]
    })", "test/images");
    TEST_ASSERT_FALSE(invalid_images.has_value());

    auto invalid = parsed.value();
    invalid.version = "999.0.0";
    auto invalid_validation = validate_document(invalid);
    TEST_ASSERT_FALSE(invalid_validation.success);
    TEST_ASSERT_FALSE(invalid_validation.errors.empty());
}

BROOKESIA_TEST_CASE(
    test_gui_interface_memory_data_store_compacts_listener_lifecycle,
    "GUI interface data store releases listener buckets without dropping values",
    "[gui][interface][data_store]"
)
{
    MemoryDataStore store;
    const DocumentId document_id(7);
    constexpr std::string_view PATH = "/screen/item";
    constexpr std::string_view KEY = "labelProps.text";
    size_t first_calls = 0;
    size_t second_calls = 0;
    std::string last_value;

    auto first_listener = [&first_calls, &last_value](std::string_view, std::string_view value) {
        ++first_calls;
        last_value = value;
    };
    auto second_listener = [&second_calls](std::string_view, std::string_view) {
        ++second_calls;
    };
    auto first_id = store.subscribe(document_id, PATH, KEY, first_listener);
    auto second_id = store.subscribe(document_id, PATH, KEY, second_listener);
    TEST_ASSERT_EQUAL_size_t(2, store.debug_connection_count());
    TEST_ASSERT_EQUAL_size_t(1, store.debug_signal_count());

    store.set_string(document_id, PATH, KEY, "first");
    TEST_ASSERT_EQUAL_size_t(1, first_calls);
    TEST_ASSERT_EQUAL_size_t(1, second_calls);
    TEST_ASSERT_EQUAL_STRING("first", last_value.c_str());

    store.unsubscribe(first_id);
    TEST_ASSERT_EQUAL_size_t(1, store.debug_connection_count());
    TEST_ASSERT_EQUAL_size_t(1, store.debug_signal_count());
    store.set_string(document_id, PATH, KEY, "second");
    TEST_ASSERT_EQUAL_size_t(1, first_calls);
    TEST_ASSERT_EQUAL_size_t(2, second_calls);

    store.unsubscribe(second_id);
    TEST_ASSERT_EQUAL_size_t(0, store.debug_connection_count());
    TEST_ASSERT_EQUAL_size_t(0, store.debug_signal_count());
    auto retained_value = store.get_string(document_id, PATH, KEY);
    TEST_ASSERT_TRUE(retained_value.has_value());
    TEST_ASSERT_EQUAL_STRING("second", retained_value->c_str());

    size_t cancelling_calls = 0;
    size_t cancelled_calls = 0;
    IDataStore::SubscriptionId cancelled_id = 0;
    auto cancelling_id = store.subscribe(document_id, PATH, KEY, [&store, &cancelling_calls, &cancelled_id]
    (std::string_view, std::string_view) {
        ++cancelling_calls;
        store.unsubscribe(cancelled_id);
    });
    cancelled_id = store.subscribe(document_id, PATH, KEY, [&cancelled_calls]
    (std::string_view, std::string_view) {
        ++cancelled_calls;
    });
    store.set_string(document_id, PATH, KEY, "cancelled");
    TEST_ASSERT_EQUAL_size_t(1, cancelling_calls);
    TEST_ASSERT_EQUAL_size_t(0, cancelled_calls);
    TEST_ASSERT_EQUAL_size_t(1, store.debug_connection_count());
    store.unsubscribe(cancelling_id);

    size_t staged_calls = 0;
    std::string staged_value;
    auto staged_listener_id = store.subscribe(document_id, PATH, KEY, [&staged_calls, &staged_value]
    (std::string_view, std::string_view value) {
        ++staged_calls;
        staged_value = value;
    });
    store.set_string_silent(document_id, PATH, KEY, "outer");
    store.set_string_silent(document_id, PATH, KEY, "inner");
    store.notify_string_if_value(document_id, PATH, KEY, "inner");
    store.notify_string_if_value(document_id, PATH, KEY, "outer");
    TEST_ASSERT_EQUAL_size_t(1, staged_calls);
    TEST_ASSERT_EQUAL_STRING("inner", staged_value.c_str());
    TEST_ASSERT_EQUAL_STRING("inner", store.get_string(document_id, PATH, KEY)->c_str());
    store.unsubscribe(staged_listener_id);

    std::vector<std::string> first_reentrant_values;
    std::vector<std::string> second_reentrant_values;
    auto first_reentrant_listener = [&store, &first_reentrant_values, document_id]
    (std::string_view, std::string_view value) {
        first_reentrant_values.emplace_back(value);
        if (value == "outer callback") {
            store.set_string(document_id, PATH, KEY, "inner callback");
        }
    };
    auto second_reentrant_listener = [&second_reentrant_values](std::string_view, std::string_view value) {
        second_reentrant_values.emplace_back(value);
    };
    auto first_reentrant_id = store.subscribe(document_id, PATH, KEY, first_reentrant_listener);
    auto second_reentrant_id = store.subscribe(document_id, PATH, KEY, second_reentrant_listener);
    store.set_string(document_id, PATH, KEY, "outer callback");
    TEST_ASSERT_EQUAL_size_t(2, first_reentrant_values.size());
    TEST_ASSERT_EQUAL_STRING("outer callback", first_reentrant_values[0].c_str());
    TEST_ASSERT_EQUAL_STRING("inner callback", first_reentrant_values[1].c_str());
    TEST_ASSERT_EQUAL_size_t(1, second_reentrant_values.size());
    TEST_ASSERT_EQUAL_STRING("inner callback", second_reentrant_values[0].c_str());
    store.unsubscribe(first_reentrant_id);
    store.unsubscribe(second_reentrant_id);

    store.forget_document(document_id);
    TEST_ASSERT_FALSE(store.get_string(document_id, PATH, KEY).has_value());
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_load_mount_update_and_events,
    "GUI interface runtime drives a mock backend",
    "[gui][interface][runtime]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json("test/root.json", ROOT_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());
    TEST_ASSERT_TRUE(document_id->is_valid());
    TEST_ASSERT_GREATER_THAN(1, backend_ptr->create_count());

    auto mounted = runtime.mount_screen(document_id.value(), "/test_screen");
    TEST_ASSERT_TRUE(mounted.has_value());
    TEST_ASSERT_TRUE(mounted->valid());
    TEST_ASSERT_EQUAL_STRING("/test_screen", mounted->absolute_path().c_str());

    auto label = runtime.find_view(document_id.value(), "/test_screen/title").as_label();
    TEST_ASSERT_TRUE(label.valid());
    TEST_ASSERT_EQUAL_STRING("Root Base", label.text().c_str());
    TEST_ASSERT_TRUE(label.set_text("Updated"));
    TEST_ASSERT_EQUAL_STRING("Updated", label.text().c_str());

    auto slider = runtime.find_view(document_id.value(), "/test_screen/level").as_slider();
    TEST_ASSERT_TRUE(slider.valid());
    TEST_ASSERT_EQUAL_INT32(30, slider.value());
    TEST_ASSERT_TRUE(slider.set_value(44));
    TEST_ASSERT_EQUAL_INT32(44, slider.value());

    TEST_ASSERT_EQUAL_size_t(1, runtime.list_displays().size());
    TEST_ASSERT_EQUAL_size_t(2, runtime.list_layers().size());
    runtime.set_view_debug_enabled(true);
    TEST_ASSERT_TRUE(runtime.is_view_debug_enabled());
    TEST_ASSERT_TRUE(backend_ptr->debug_visual_enabled());

    bool saw_action = false;
    auto subscription_id = runtime.subscribe_event_action_with_id(
                               document_id.value(), "tap_title",
    [&saw_action](const Event & event) {
        saw_action = event.action == "tap_title" && event.path == "/test_screen/title";
    }
                           );
    TEST_ASSERT_NOT_EQUAL(0, subscription_id);
    const auto title_handle = backend_ptr->handle_for_path("/test_screen/title");
    TEST_ASSERT_TRUE(title_handle.is_valid());
    backend_ptr->emit_event({
        .handle = title_handle,
        .type = EventType::Clicked,
        .action = "tap_title",
        .payload = {},
    });
    TEST_ASSERT_TRUE(saw_action);
    TEST_ASSERT_TRUE(runtime.unsubscribe_subscription(subscription_id));

    bool animation_completed = false;
    Animation animation = {
        .id = "fade",
        .from = 0,
        .to = 255,
    };
    auto animation_result = runtime.start_view_animation_with_result(
                                document_id.value(), "/test_screen/title", animation,
    [&animation_completed]() {
        animation_completed = true;
    }
                            );
    TEST_ASSERT_NOT_EQUAL(0, animation_result.subscription_id);
    TEST_ASSERT_EQUAL_INT32(0, animation_result.resolved_from);
    TEST_ASSERT_EQUAL_INT32(255, animation_result.resolved_to);
    TEST_ASSERT_TRUE(animation_completed);
    TEST_ASSERT_EQUAL_size_t(1, backend_ptr->start_animation_count());

    TEST_ASSERT_TRUE(runtime.scroll_view_to(document_id.value(), "/test_screen/title", 0, 0, false));
    TEST_ASSERT_EQUAL_size_t(1, backend_ptr->scroll_to_count());
    TEST_ASSERT_EQUAL(title_handle.value(), backend_ptr->last_scroll_to_handle().value());
    TEST_ASSERT_EQUAL_INT32(0, backend_ptr->last_scroll_x());
    TEST_ASSERT_EQUAL_INT32(0, backend_ptr->last_scroll_y());
    TEST_ASSERT_FALSE(backend_ptr->last_scroll_animated());

    TEST_ASSERT_TRUE(runtime.scroll_view_to_visible(document_id.value(), "/test_screen/title", false));
    TEST_ASSERT_TRUE(runtime.unmount_screen(document_id.value(), "/test_screen"));
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_keeps_binding_domains_as_independent_instance_state,
    "GUI interface applies every mutable binding domain without a full Node instance",
    "[gui][interface][runtime][binding][memory]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json(
                           "test/compact_binding_domains.json", COMPACT_BINDING_DOMAINS_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());

    runtime.set_binding_values(document_id.value(), {
        {"/binding_screen/panel", "background", "#102030"},
        {"/binding_screen/panel", "opacity", "123"},
        {"/binding_screen/panel", "pressed_background", "#405060"},
        {"/binding_screen/panel", "indicator_text", "#708090"},
        {"/binding_screen/panel", "indicator_border", "#A0B0C0"},
        {"/binding_screen/panel", "gap", "12dp"},
        {"/binding_screen/panel", "width", "123dp"},
        {"/binding_screen/input", "text", "updated text"},
        {"/binding_screen/toggle", "checked", "true"},
        {"/binding_screen/dropdown", "selected", "1"},
        {"/binding_screen/frame", "output", "preview"},
        {"/binding_screen/line", "points", R"([{"x":1,"y":2},{"x":3,"y":4}])"},
        {"/binding_screen/table", "cells", R"([{"row":0,"column":0,"text":"cell"}])"},
        {"/binding_screen/keyboard", "mode", "number"},
        {
            "/binding_screen/canvas", "commands",
            R"([{"type":"rect","x":1,"y":2,"width":3,"height":4,"color":"#112233"}])"
        },
    });
    auto mounted = runtime.mount_screen(document_id.value(), "/binding_screen");
    TEST_ASSERT_TRUE(mounted.has_value());

    auto state = runtime.get_view_state(
                     document_id.value(), "/binding_screen/panel", ViewStateKind::Style);
    TEST_ASSERT_TRUE(state.has_value());
    const auto *style = std::get_if<Style>(&state.value());
    TEST_ASSERT_NOT_NULL(style);
    TEST_ASSERT_TRUE(style->opacity.has_value());
    TEST_ASSERT_EQUAL_INT32(123, *style->opacity);
    TEST_ASSERT_TRUE(style->bg_color.has_value());
    TEST_ASSERT_EQUAL_STRING("#102030", style->bg_color->c_str());
    TEST_ASSERT_TRUE(style->radius.has_value());
    TEST_ASSERT_EQUAL_INT32(5, *style->radius);

    const auto panel_handle = backend_ptr->handle_for_path("/binding_screen/panel");
    const auto *resolved_style = backend_ptr->style_for_handle(panel_handle);
    TEST_ASSERT_NOT_NULL(resolved_style);
    TEST_ASSERT_EQUAL_STRING(
        "#405060", resolved_style->state_styles.at("pressed").bg_color->c_str()
    );
    TEST_ASSERT_EQUAL_INT32(2, *resolved_style->state_styles.at("pressed").border_width);
    TEST_ASSERT_EQUAL_STRING(
        "#708090", resolved_style->part_styles.at("indicator").style.text_color->c_str()
    );
    TEST_ASSERT_EQUAL_STRING(
        "#A0B0C0",
        resolved_style->part_styles.at("indicator").state_styles.at("checked").border_color->c_str()
    );

    runtime.set_binding_values(document_id.value(), {
        {"/binding_screen/panel", "background", "#112233"},
        {"/binding_screen/panel", "opacity", "201"},
        {"/binding_screen/panel", "pressed_background", "#445566"},
        {"/binding_screen/panel", "indicator_border", "#ABCDEF"},
    });
    state = runtime.get_view_state(document_id.value(), "/binding_screen/panel", ViewStateKind::Style);
    TEST_ASSERT_TRUE(state.has_value());
    style = std::get_if<Style>(&state.value());
    TEST_ASSERT_NOT_NULL(style);
    TEST_ASSERT_EQUAL_STRING("#112233", style->bg_color->c_str());
    TEST_ASSERT_EQUAL_INT32(201, *style->opacity);
    TEST_ASSERT_EQUAL_INT32(5, *style->radius);
    resolved_style = backend_ptr->style_for_handle(panel_handle);
    TEST_ASSERT_NOT_NULL(resolved_style);
    TEST_ASSERT_EQUAL_STRING(
        "#445566", resolved_style->state_styles.at("pressed").bg_color->c_str()
    );
    TEST_ASSERT_EQUAL_INT32(2, *resolved_style->state_styles.at("pressed").border_width);
    TEST_ASSERT_EQUAL_STRING(
        "#ABCDEF",
        resolved_style->part_styles.at("indicator").state_styles.at("checked").border_color->c_str()
    );

    state = runtime.get_view_state(document_id.value(), "/binding_screen/panel", ViewStateKind::Layout);
    TEST_ASSERT_TRUE(state.has_value());
    const auto *layout = std::get_if<Layout>(&state.value());
    TEST_ASSERT_NOT_NULL(layout);
    TEST_ASSERT_EQUAL_INT32(12, layout->gap);

    state = runtime.get_view_state(document_id.value(), "/binding_screen/panel", ViewStateKind::Placement);
    TEST_ASSERT_TRUE(state.has_value());
    const auto *placement = std::get_if<Placement>(&state.value());
    TEST_ASSERT_NOT_NULL(placement);
    TEST_ASSERT_TRUE(placement->width.mode == SizeMode::Fixed);
    TEST_ASSERT_EQUAL_INT32(123, placement->width.value);

    state = runtime.get_view_state(document_id.value(), "/binding_screen/input", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    const auto *typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *text_input_props = std::get_if<TextInputProps>(typed_props);
    TEST_ASSERT_NOT_NULL(text_input_props);
    TEST_ASSERT_EQUAL_STRING("updated text", text_input_props->text.c_str());

    state = runtime.get_view_state(document_id.value(), "/binding_screen/toggle", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *toggle_props = std::get_if<ToggleProps>(typed_props);
    TEST_ASSERT_NOT_NULL(toggle_props);
    TEST_ASSERT_TRUE(toggle_props->checked);

    state = runtime.get_view_state(document_id.value(), "/binding_screen/dropdown", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *dropdown_props = std::get_if<DropdownProps>(typed_props);
    TEST_ASSERT_NOT_NULL(dropdown_props);
    TEST_ASSERT_EQUAL_INT32(1, dropdown_props->selected_index);
    TEST_ASSERT_EQUAL_size_t(2, dropdown_props->options.size());

    state = runtime.get_view_state(document_id.value(), "/binding_screen/frame", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *frame_view_props = std::get_if<FrameViewProps>(typed_props);
    TEST_ASSERT_NOT_NULL(frame_view_props);
    TEST_ASSERT_EQUAL_STRING("preview", frame_view_props->output_name.c_str());

    state = runtime.get_view_state(document_id.value(), "/binding_screen/line", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *line_props = std::get_if<LineProps>(typed_props);
    TEST_ASSERT_NOT_NULL(line_props);
    TEST_ASSERT_EQUAL_size_t(2, line_props->points.size());
    TEST_ASSERT_EQUAL_INT32(4, line_props->points[1].y);

    state = runtime.get_view_state(document_id.value(), "/binding_screen/table", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *table_props = std::get_if<TableProps>(typed_props);
    TEST_ASSERT_NOT_NULL(table_props);
    TEST_ASSERT_EQUAL_size_t(1, table_props->cells.size());
    TEST_ASSERT_EQUAL_STRING("cell", table_props->cells[0].text.c_str());

    state = runtime.get_view_state(document_id.value(), "/binding_screen/keyboard", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *keyboard_props = std::get_if<KeyboardProps>(typed_props);
    TEST_ASSERT_NOT_NULL(keyboard_props);
    TEST_ASSERT_EQUAL_STRING("number", keyboard_props->mode.c_str());

    state = runtime.get_view_state(document_id.value(), "/binding_screen/canvas", ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *canvas_props = std::get_if<CanvasProps>(typed_props);
    TEST_ASSERT_NOT_NULL(canvas_props);
    TEST_ASSERT_EQUAL_size_t(1, canvas_props->commands.size());
    TEST_ASSERT_EQUAL_STRING("#112233", canvas_props->commands[0].color.c_str());

    TEST_ASSERT_TRUE(runtime.unmount_screen(document_id.value(), "/binding_screen"));
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_action_listener_lifecycle_and_reentry,
    "GUI interface action listeners disconnect immediately and permit reentrant API calls",
    "[gui][interface][runtime][action]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json("test/actions.json", ROOT_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());
    const auto title_handle = backend_ptr->handle_for_path("/test_screen/title");
    TEST_ASSERT_TRUE(title_handle.is_valid());

    std::vector<int> order;
    ScopedConnection first_connection;
    ScopedConnection second_connection;
    ScopedConnection third_connection;
    ScopedConnection added_during_dispatch;
    first_connection = runtime.subscribe_event_action(
                           document_id.value(),
                           "ordered_action",
    [&](const Event &) {
        order.push_back(1);
        first_connection.disconnect();
        third_connection.disconnect();
        added_during_dispatch = runtime.subscribe_event_action(
                                    document_id.value(),
                                    "ordered_action",
        [&](const Event &) {
            order.push_back(4);
        }
                                );
    }
                       );
    second_connection = runtime.subscribe_event_action(
                            document_id.value(),
                            "ordered_action",
    [&](const Event &) {
        order.push_back(2);
    }
                        );
    third_connection = runtime.subscribe_event_action(
                           document_id.value(),
                           "ordered_action",
    [&](const Event &) {
        order.push_back(3);
    }
                       );

    backend_ptr->emit_event({
        .handle = title_handle,
        .type = EventType::Clicked,
        .action = "ordered_action",
        .payload = {},
    });
    TEST_ASSERT_EQUAL_size_t(2, order.size());
    TEST_ASSERT_EQUAL_INT(1, order[0]);
    TEST_ASSERT_EQUAL_INT(2, order[1]);

    order.clear();
    backend_ptr->emit_event({
        .handle = title_handle,
        .type = EventType::Clicked,
        .action = "ordered_action",
        .payload = {},
    });
    TEST_ASSERT_EQUAL_size_t(2, order.size());
    TEST_ASSERT_EQUAL_INT(2, order[0]);
    TEST_ASSERT_EQUAL_INT(4, order[1]);

    bool unloaded_from_callback = false;
    auto unload_connection = runtime.subscribe_event_action(
                                 document_id.value(),
                                 "unload_action",
    [&](const Event &) {
        unloaded_from_callback = runtime.unload(document_id.value());
    }
                             );
    TEST_ASSERT_TRUE(unload_connection.connected());
    backend_ptr->emit_event({
        .handle = title_handle,
        .type = EventType::Clicked,
        .action = "unload_action",
        .payload = {},
    });
    TEST_ASSERT_TRUE(unloaded_from_callback);
    TEST_ASSERT_FALSE(unload_connection.connected());
    TEST_ASSERT_FALSE(runtime.find_view(document_id.value(), "/test_screen/title").valid());

    ScopedConnection late_connection;
    {
        auto late_backend = std::make_unique<MockBackend>();
        auto late_runtime = std::make_unique<Runtime>(std::move(late_backend));
        auto late_document_id = late_runtime->load_json("test/late.json", ROOT_JSON, "test", environment);
        TEST_ASSERT_TRUE(late_document_id.has_value());
        late_connection = late_runtime->subscribe_event_action(
                              late_document_id.value(),
                              "late_action",
        [](const Event &) {
        }
                          );
        TEST_ASSERT_TRUE(late_connection.connected());
    }
    TEST_ASSERT_FALSE(late_connection.connected());
    late_connection.disconnect();
    TEST_ASSERT_FALSE(late_connection.connected());
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_preloads_dynamic_bound_image_sources,
    "GUI interface runtime preloads image resources introduced by binding updates",
    "[gui][interface][runtime]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json("test/images.json", IMAGE_BINDING_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());
    auto mounted = runtime.mount_screen(document_id.value(), "/image_screen");
    TEST_ASSERT_TRUE(mounted.has_value());

    auto register_result = runtime.register_image(RuntimeImageResource{
        .id = "dynamic_icon",
        .primary_src = "dynamic.png",
        .native_src = 0,
        .width = 32,
        .height = 32,
    });
    TEST_ASSERT_TRUE(register_result.has_value());
    TEST_ASSERT_EQUAL_size_t(0, backend_ptr->preloaded_images().size());

    const auto props_before = backend_ptr->props_apply_count();
    runtime.set_binding_values(document_id.value(), {
        BindingValueUpdate{
            .absolute_path = "/image_screen/icon_a",
            .key = "src_a",
            .value = "dynamic_icon",
        },
        BindingValueUpdate{
            .absolute_path = "/image_screen/icon_b",
            .key = "src_b",
            .value = "dynamic_icon",
        },
    });

    TEST_ASSERT_EQUAL_size_t(1, backend_ptr->preloaded_images().size());
    TEST_ASSERT_EQUAL_STRING("dynamic_icon", backend_ptr->preloaded_images().front().id.c_str());
    TEST_ASSERT_GREATER_THAN(props_before, backend_ptr->props_apply_count());
    TEST_ASSERT_EQUAL_STRING(
        "dynamic_icon",
        runtime.find_view(document_id.value(), "/image_screen/icon_a").as_image().src().c_str()
    );
    TEST_ASSERT_EQUAL_STRING(
        "dynamic_icon",
        runtime.find_view(document_id.value(), "/image_screen/icon_b").as_image().src().c_str()
    );

    runtime.set_binding_values(document_id.value(), {
        BindingValueUpdate{
            .absolute_path = "/image_screen/icon_a",
            .key = "src_a",
            .value = "",
        },
    });
    TEST_ASSERT_EQUAL_size_t(0, backend_ptr->released_image_ids().size());

    runtime.set_binding_values(document_id.value(), {
        BindingValueUpdate{
            .absolute_path = "/image_screen/icon_b",
            .key = "src_b",
            .value = "",
        },
    });
    TEST_ASSERT_EQUAL_size_t(1, backend_ptr->released_image_ids().size());
    TEST_ASSERT_EQUAL_STRING("dynamic_icon", backend_ptr->released_image_ids().front().c_str());
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_path_index_survives_document_registry_rehash,
    "GUI interface path index remains valid while the document registry grows",
    "[gui][interface][runtime][path]"
)
{
    auto backend = std::make_unique<MockBackend>();
    Runtime runtime(std::move(backend));
    Environment environment;
    std::vector<DocumentId> document_ids;
    document_ids.reserve(64);

    for (size_t index = 0; index < 64; ++index) {
        auto document_id = runtime.load_json(
                               "test/rehash_" + std::to_string(index) + ".json",
                               ROOT_JSON,
                               "test",
                               environment
                           );
        TEST_ASSERT_TRUE(document_id.has_value());
        document_ids.push_back(document_id.value());
    }

    auto first_title = runtime.find_view(document_ids.front(), "/test_screen/title").as_label();
    TEST_ASSERT_TRUE(first_title.valid());
    TEST_ASSERT_EQUAL_STRING("Root Base", first_title.text().c_str());
    TEST_ASSERT_TRUE(first_title.set_text("After registry rehash"));
    TEST_ASSERT_EQUAL_STRING("After registry rehash", first_title.text().c_str());

    for (auto document_id : document_ids) {
        TEST_ASSERT_TRUE(runtime.unload(document_id));
    }
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_preloads_set_view_src_image_sources,
    "GUI interface runtime preloads image resources introduced by set_view_src",
    "[gui][interface][runtime]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json("test/images.json", IMAGE_BINDING_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());
    auto mounted = runtime.mount_screen(document_id.value(), "/image_screen");
    TEST_ASSERT_TRUE(mounted.has_value());

    auto register_result = runtime.register_image(RuntimeImageResource{
        .id = "set_icon",
        .primary_src = "set.png",
        .native_src = 0,
        .width = 40,
        .height = 40,
    });
    TEST_ASSERT_TRUE(register_result.has_value());

    auto image = runtime.find_view(document_id.value(), "/image_screen/icon_a").as_image();
    TEST_ASSERT_TRUE(image.valid());
    TEST_ASSERT_TRUE(image.set_src("set_icon"));
    TEST_ASSERT_EQUAL_size_t(1, backend_ptr->preloaded_images().size());
    TEST_ASSERT_EQUAL_STRING("set_icon", backend_ptr->preloaded_images().front().id.c_str());
    TEST_ASSERT_EQUAL_STRING("set_icon", image.src().c_str());
    const auto image_handle = backend_ptr->handle_for_path("/image_screen/icon_a");
    const auto *applied_image = backend_ptr->image_state_for_handle(image_handle);
    TEST_ASSERT_NOT_NULL(applied_image);
    TEST_ASSERT_EQUAL_STRING("set_icon", applied_image->props.src.c_str());
    TEST_ASSERT_EQUAL_STRING("set.png", applied_image->resolved.primary_src.c_str());
    TEST_ASSERT_EQUAL_INT32(40, applied_image->resolved.width);
    TEST_ASSERT_EQUAL_INT32(40, applied_image->resolved.height);
    TEST_ASSERT_TRUE(applied_image->placement.width.mode == SizeMode::Fixed);
    TEST_ASSERT_EQUAL_INT32(32, applied_image->placement.width.value);
    TEST_ASSERT_TRUE(applied_image->placement.height.mode == SizeMode::Fixed);
    TEST_ASSERT_EQUAL_INT32(32, applied_image->placement.height.value);

    auto resolved_state = runtime.get_view_state(image, ViewStateKind::ResolvedImage);
    TEST_ASSERT_TRUE(resolved_state.has_value());
    const auto *resolved_image = std::get_if<ResolvedImageSpec>(&resolved_state.value());
    TEST_ASSERT_NOT_NULL(resolved_image);
    TEST_ASSERT_EQUAL_STRING("set.png", resolved_image->primary_src.c_str());
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_static_node_setter_state_is_discarded_on_dynamic_screen_destroy,
    "GUI interface keeps static node setter state separate from immutable definitions",
    "[gui][interface][runtime][memory]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json("test/dynamic_screen.json", DYNAMIC_SCREEN_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());

    auto mounted = runtime.mount_screen(document_id.value(), "/dynamic_screen");
    TEST_ASSERT_TRUE(mounted.has_value());
    auto left = runtime.find_view(document_id.value(), "/dynamic_screen/left").as_label();
    auto right = runtime.find_view(document_id.value(), "/dynamic_screen/right").as_label();
    TEST_ASSERT_TRUE(left.valid());
    TEST_ASSERT_TRUE(right.valid());
    TEST_ASSERT_EQUAL_STRING("Definition text", left.text().c_str());
    TEST_ASSERT_EQUAL_STRING("Definition text", right.text().c_str());

    TEST_ASSERT_TRUE(left.set_text("Left instance state"));
    TEST_ASSERT_EQUAL_STRING("Left instance state", left.text().c_str());
    TEST_ASSERT_EQUAL_STRING("Definition text", right.text().c_str());

    const auto destroyed_before = backend_ptr->destroyed_count();
    TEST_ASSERT_TRUE(runtime.unmount_screen(document_id.value(), "/dynamic_screen"));
    TEST_ASSERT_EQUAL_size_t(destroyed_before + 1, backend_ptr->destroyed_count());
    TEST_ASSERT_FALSE(runtime.find_view(document_id.value(), "/dynamic_screen/left").valid());

    mounted = runtime.mount_screen(document_id.value(), "/dynamic_screen");
    TEST_ASSERT_TRUE(mounted.has_value());
    left = runtime.find_view(document_id.value(), "/dynamic_screen/left").as_label();
    right = runtime.find_view(document_id.value(), "/dynamic_screen/right").as_label();
    TEST_ASSERT_EQUAL_STRING("Definition text", left.text().c_str());
    TEST_ASSERT_EQUAL_STRING("Definition text", right.text().c_str());

    TEST_ASSERT_TRUE(runtime.unmount_screen(document_id.value(), "/dynamic_screen"));
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_find_view_materializes_deferred_eager_screens,
    "GUI interface materializes deferred Eager screens on find_view and create_view",
    "[gui][interface][runtime][memory]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    DocumentLoadOptions options;
    options.defer_non_entry_eager_screens = true;
    options.build_entry_screens = false;
    options.schedule_idle_warm = false;
    auto document_id = runtime.load_json(
                           "test/deferred_eager_screens.json",
                           DEFERRED_EAGER_SCREENS_JSON,
                           "test",
                           environment,
                           options
                       );
    TEST_ASSERT_TRUE(document_id.has_value());

    // Preload-style load leaves every Eager screen unbuilt until path access.
    TEST_ASSERT_FALSE(backend_ptr->handle_for_path("/home/title").is_valid());
    TEST_ASSERT_FALSE(backend_ptr->handle_for_path("/settings/title").is_valid());

    auto home_title = runtime.find_view(document_id.value(), "/home/title").as_label();
    TEST_ASSERT_TRUE(home_title.valid());
    TEST_ASSERT_EQUAL_STRING("Home", home_title.text().c_str());
    TEST_ASSERT_TRUE(backend_ptr->handle_for_path("/home/title").is_valid());
    TEST_ASSERT_FALSE(backend_ptr->handle_for_path("/settings/title").is_valid());

    auto settings_title = runtime.find_view(document_id.value(), "/settings/title").as_label();
    TEST_ASSERT_TRUE(settings_title.valid());
    TEST_ASSERT_EQUAL_STRING("Settings", settings_title.text().c_str());
    TEST_ASSERT_TRUE(backend_ptr->handle_for_path("/settings/title").is_valid());

    auto badge = runtime.create_view(document_id.value(), "badge", "/home/list", "one");
    TEST_ASSERT_TRUE(badge.has_value());
    TEST_ASSERT_TRUE(backend_ptr->handle_for_path("/home/list/one").is_valid());

    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_template_instance_state_is_isolated_and_destroyed,
    "GUI interface isolates setter and binding state between template instances",
    "[gui][interface][runtime][memory]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_json("test/template_instances.json", TEMPLATE_INSTANCE_JSON, "test", environment);
    TEST_ASSERT_TRUE(document_id.has_value());

    auto first = runtime.create_view(document_id.value(), "row", "/template_screen/list", "first");
    auto second = runtime.create_view(document_id.value(), "row", "/template_screen/list", "second");
    auto prebound = runtime.create_view(document_id.value(), "row", "/template_screen/list", "prebound");
    TEST_ASSERT_TRUE(first.has_value());
    TEST_ASSERT_TRUE(second.has_value());
    TEST_ASSERT_TRUE(prebound.has_value());
    TEST_ASSERT_EQUAL_STRING("prebound", prebound->id().c_str());
    runtime.set_binding_value(
        document_id.value(), "/template_screen/list/prebound/bound_label", "title", "Retained binding state"
    );
    TEST_ASSERT_TRUE(runtime.destroy_view(document_id.value(), "/template_screen/list/prebound"));
    prebound = runtime.create_view(document_id.value(), "row", "/template_screen/list", "prebound");
    TEST_ASSERT_TRUE(prebound.has_value());
    TEST_ASSERT_EQUAL_STRING(
        "Retained binding state",
        runtime.find_view(
            document_id.value(), "/template_screen/list/prebound/bound_label"
        ).as_label().text().c_str()
    );

    auto first_setter = runtime.find_view(
                            document_id.value(), "/template_screen/list/first/setter_label"
                        ).as_label();
    auto second_setter = runtime.find_view(
                             document_id.value(), "/template_screen/list/second/setter_label"
                         ).as_label();
    TEST_ASSERT_TRUE(first_setter.set_text("First setter state"));
    TEST_ASSERT_TRUE(second_setter.set_text("Second setter state"));
    TEST_ASSERT_EQUAL_STRING("First setter state", first_setter.text().c_str());
    TEST_ASSERT_EQUAL_STRING("Second setter state", second_setter.text().c_str());

    size_t public_binding_calls = 0;
    bool public_binding_saw_applied_state = false;
    auto public_binding_handler = [&runtime, &public_binding_calls, &public_binding_saw_applied_state, document_id]
    (std::string_view path, std::string_view, std::string_view value) {
        ++public_binding_calls;
        auto label = runtime.find_view(document_id.value(), path).as_label();
        public_binding_saw_applied_state = label.valid() && label.text() == value;
    };
    auto public_binding_connection = runtime.subscribe_binding_value(
                                         document_id.value(),
                                         "/template_screen/list/first/bound_label",
                                         "title",
                                         public_binding_handler
                                     );
    TEST_ASSERT_TRUE(public_binding_connection.connected());

    runtime.set_binding_value(
        document_id.value(), "/template_screen/list/first/bound_label", "title", "First binding state"
    );
    runtime.set_binding_value(
        document_id.value(), "/template_screen/list/second/bound_label", "title", "Second binding state"
    );
    auto first_bound = runtime.find_view(
                           document_id.value(), "/template_screen/list/first/bound_label"
                       ).as_label();
    auto second_bound = runtime.find_view(
                            document_id.value(), "/template_screen/list/second/bound_label"
                        ).as_label();
    TEST_ASSERT_EQUAL_STRING("First binding state", first_bound.text().c_str());
    TEST_ASSERT_EQUAL_STRING("Second binding state", second_bound.text().c_str());
    TEST_ASSERT_EQUAL_size_t(1, public_binding_calls);
    TEST_ASSERT_TRUE(public_binding_saw_applied_state);

    runtime.set_binding_values(document_id.value(), {
        BindingValueUpdate{
            .absolute_path = "/template_screen/list/first/bound_label",
            .key = "title",
            .value = "Shadowed binding state",
        },
        BindingValueUpdate{
            .absolute_path = "/template_screen/list/first/bound_label",
            .key = "title",
            .value = "Final binding state",
        },
    });
    TEST_ASSERT_EQUAL_STRING("Final binding state", first_bound.text().c_str());
    TEST_ASSERT_EQUAL_size_t(2, public_binding_calls);
    TEST_ASSERT_TRUE(public_binding_saw_applied_state);

    bool backend_hook_saw_staged_value = false;
    backend_ptr->set_next_props_apply_hook([&runtime, &backend_hook_saw_staged_value, document_id]() {
        auto staged_value = runtime.get_binding_value(
                                document_id.value(),
                                "/template_screen/list/first/bound_label",
                                "title"
                            );
        backend_hook_saw_staged_value = staged_value.has_value() && *staged_value == "Outer binding state";
        runtime.set_binding_value(
            document_id.value(),
            "/template_screen/list/first/bound_label",
            "title",
            "Reentrant binding state"
        );
    });
    runtime.set_binding_value(
        document_id.value(),
        "/template_screen/list/first/bound_label",
        "title",
        "Outer binding state"
    );
    TEST_ASSERT_TRUE(backend_hook_saw_staged_value);
    TEST_ASSERT_EQUAL_STRING("Reentrant binding state", first_bound.text().c_str());
    TEST_ASSERT_EQUAL_STRING(
        "Reentrant binding state",
        runtime.get_binding_value(
            document_id.value(), "/template_screen/list/first/bound_label", "title"
        )->c_str()
    );
    TEST_ASSERT_EQUAL_size_t(3, public_binding_calls);
    TEST_ASSERT_TRUE(public_binding_saw_applied_state);

    const auto destroyed_before = backend_ptr->destroyed_count();
    TEST_ASSERT_TRUE(runtime.destroy_view(document_id.value(), "/template_screen/list/first"));
    TEST_ASSERT_EQUAL_size_t(destroyed_before + 1, backend_ptr->destroyed_count());
    TEST_ASSERT_FALSE(runtime.find_view(document_id.value(), "/template_screen/list/first").valid());
    TEST_ASSERT_TRUE(runtime.find_view(document_id.value(), "/template_screen/list/second").valid());
    TEST_ASSERT_EQUAL_STRING("Second setter state", second_setter.text().c_str());
    TEST_ASSERT_EQUAL_STRING("Second binding state", second_bound.text().c_str());

    TEST_ASSERT_TRUE(runtime.destroy_view(document_id.value(), "/template_screen/list/second"));
    TEST_ASSERT_TRUE(runtime.destroy_view(document_id.value(), "/template_screen/list/prebound"));
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
    runtime.set_binding_value(
        document_id.value(), "/template_screen/list/first/bound_label", "title", "Unloaded value"
    );
    TEST_ASSERT_FALSE(
        runtime.get_binding_value(
            document_id.value(), "/template_screen/list/first/bound_label", "title"
        ).has_value()
    );
}

#if !defined(ESP_PLATFORM)
BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_file_update_keeps_node_definitions_alive,
    "GUI interface keeps updated file-backed definitions alive for state access and setters",
    "[gui][interface][runtime][update]"
)
{
    static constexpr std::string_view INITIAL_JSON = R"({
        "version": "0.1.1",
        "assets": [
            {
                "type": "viewScreen",
                "id": "update_screen",
                "children": [
                    {
                        "type": "label",
                        "id": "title",
                        "labelProps": {
                            "text": "Initial definition"
                        }
                    }
                ]
            }
        ]
    })";
    static constexpr std::string_view UPDATED_JSON = R"({
        "version": "0.1.1",
        "assets": [
            {
                "type": "viewScreen",
                "id": "update_screen",
                "children": [
                    {
                        "type": "label",
                        "id": "title",
                        "labelProps": {
                            "text": "Updated definition"
                        }
                    }
                ]
            }
        ]
    })";

    TemporaryJsonFile file;
    TEST_ASSERT_TRUE(file.write(INITIAL_JSON));
    HostStorageServiceGuard storage_service;
    TEST_ASSERT_TRUE(storage_service.start());
    auto backend = std::make_unique<MockBackend>();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_file(file.path(), environment);
    TEST_ASSERT_TRUE(document_id.has_value());
    TEST_ASSERT_TRUE(file.write(UPDATED_JSON));
    TEST_ASSERT_TRUE(runtime.update(document_id.value(), file.path(), environment).has_value());

    auto title = runtime.find_view(document_id.value(), "/update_screen/title").as_label();
    TEST_ASSERT_TRUE(title.valid());
    TEST_ASSERT_EQUAL_STRING("Updated definition", title.text().c_str());
    auto state = runtime.get_view_state(title, ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    const auto *typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    const auto *label_props = std::get_if<LabelProps>(typed_props);
    TEST_ASSERT_NOT_NULL(label_props);
    TEST_ASSERT_EQUAL_STRING("Updated definition", label_props->text.c_str());

    TEST_ASSERT_TRUE(title.set_text("Updated instance state"));
    TEST_ASSERT_EQUAL_STRING("Updated instance state", title.text().c_str());
    state = runtime.get_view_state(title, ViewStateKind::TypedProps);
    TEST_ASSERT_TRUE(state.has_value());
    typed_props = std::get_if<TypedPropsVariant>(&state.value());
    TEST_ASSERT_NOT_NULL(typed_props);
    label_props = std::get_if<LabelProps>(typed_props);
    TEST_ASSERT_NOT_NULL(label_props);
    TEST_ASSERT_EQUAL_STRING("Updated instance state", label_props->text.c_str());
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_file_update_rebuilds_live_dynamic_instances,
    "GUI interface derives live dynamic instance snapshots during document update",
    "[gui][interface][runtime][update][template]"
)
{
    TemporaryJsonFile file;
    TEST_ASSERT_TRUE(file.write(TEMPLATE_INSTANCE_JSON));
    HostStorageServiceGuard storage_service;
    TEST_ASSERT_TRUE(storage_service.start());
    auto backend = std::make_unique<MockBackend>();
    Runtime runtime(std::move(backend));

    Environment environment;
    auto document_id = runtime.load_file(file.path(), environment);
    TEST_ASSERT_TRUE(document_id.has_value());
    TEST_ASSERT_TRUE(
        runtime.create_view(document_id.value(), "row", "/template_screen/list", "removed").has_value()
    );
    TEST_ASSERT_TRUE(
        runtime.create_view(document_id.value(), "row", "/template_screen/list", "preserved").has_value()
    );
    TEST_ASSERT_TRUE(runtime.destroy_view(document_id.value(), "/template_screen/list/removed"));
    auto preserved_label = runtime.find_view(
                               document_id.value(),
                               "/template_screen/list/preserved/setter_label"
                           ).as_label();
    TEST_ASSERT_TRUE(preserved_label.valid());
    TEST_ASSERT_TRUE(preserved_label.set_text("Preserved runtime state"));

    TEST_ASSERT_TRUE(runtime.update(document_id.value(), file.path(), environment).has_value());
    TEST_ASSERT_FALSE(runtime.find_view(document_id.value(), "/template_screen/list/removed").valid());
    preserved_label = runtime.find_view(
                          document_id.value(),
                          "/template_screen/list/preserved/setter_label"
                      ).as_label();
    TEST_ASSERT_TRUE(preserved_label.valid());
    TEST_ASSERT_EQUAL_STRING("Setter definition", preserved_label.text().c_str());
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}
#endif

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_theme_and_language_changes_only_apply_to_new_documents,
    "GUI interface defers theme and language changes until a later document load",
    "[gui][interface][runtime][environment]"
)
{
    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));

    ThemeAsset first_theme;
    first_theme.id = "test.first";
    first_theme.styles["label"].style.text_color = "#111111";
    ThemeAsset second_theme;
    second_theme.id = "test.second";
    second_theme.styles["label"].style.text_color = "#222222";
    TEST_ASSERT_TRUE(runtime.load_theme(first_theme).has_value());
    TEST_ASSERT_TRUE(runtime.load_theme(second_theme).has_value());

    TEST_ASSERT_TRUE(runtime.register_font(RuntimeFontResource{
        .id = "test_font_en",
        .languages = {"en"},
        .native_fonts = {{.native_src = 1, .native_size = 16}},
    }).has_value());
    TEST_ASSERT_TRUE(runtime.register_font(RuntimeFontResource{
        .id = "test_font_zh",
        .languages = {"zh"},
        .native_fonts = {{.native_src = 2, .native_size = 16}},
    }).has_value());
    TEST_ASSERT_TRUE(runtime.set_default_font_for_language("en", "test_font_en").has_value());
    TEST_ASSERT_TRUE(runtime.set_default_font_for_language("zh", "test_font_zh").has_value());
    TEST_ASSERT_TRUE(runtime.set_theme("test.first").has_value());
    TEST_ASSERT_TRUE(runtime.set_language("en").has_value());

    Environment default_environment;
    default_environment.theme_id.clear();
    default_environment.language.clear();
    auto first_document = runtime.load_json(
                              "test/theme_first.json", THEME_LANGUAGE_JSON, "test", default_environment
                          );
    TEST_ASSERT_TRUE(first_document.has_value());
    const auto first_handle = backend_ptr->handle_for_path("/theme_screen/title");
    TEST_ASSERT_TRUE(first_handle.is_valid());
    const auto *first_style = backend_ptr->style_for_handle(first_handle);
    TEST_ASSERT_NOT_NULL(first_style);
    TEST_ASSERT_TRUE(first_style->style.text_color.has_value());
    TEST_ASSERT_EQUAL_STRING("#111111", first_style->style.text_color->c_str());
    TEST_ASSERT_EQUAL_STRING("test_font_en", first_style->resolved_font.font_id.c_str());

    const auto style_apply_count = backend_ptr->style_apply_count();
    TEST_ASSERT_TRUE(runtime.set_theme("test.second", true).has_value());
    TEST_ASSERT_TRUE(runtime.set_language("zh", true).has_value());
    TEST_ASSERT_EQUAL_size_t(style_apply_count, backend_ptr->style_apply_count());
    TEST_ASSERT_EQUAL_STRING("#111111", first_style->style.text_color->c_str());
    TEST_ASSERT_EQUAL_STRING("test_font_en", first_style->resolved_font.font_id.c_str());

    // A node instantiated lazily in an already-loaded document must retain that document's
    // load-time environment instead of reading the newly configured runtime defaults.
    auto old_document_row = runtime.create_view(
                                first_document.value(), "theme_row", "/theme_screen/list", "late"
                            );
    TEST_ASSERT_TRUE(old_document_row.has_value());
    const auto old_document_row_handle = backend_ptr->handle_for_path("/theme_screen/list/late");
    TEST_ASSERT_TRUE(old_document_row_handle.is_valid());
    const auto *old_document_row_style = backend_ptr->style_for_handle(old_document_row_handle);
    TEST_ASSERT_NOT_NULL(old_document_row_style);
    TEST_ASSERT_TRUE(old_document_row_style->style.text_color.has_value());
    TEST_ASSERT_EQUAL_STRING("#111111", old_document_row_style->style.text_color->c_str());
    TEST_ASSERT_EQUAL_STRING("test_font_en", old_document_row_style->resolved_font.font_id.c_str());

    auto second_document = runtime.load_json(
                               "test/theme_second.json", THEME_LANGUAGE_JSON, "test", default_environment
                           );
    TEST_ASSERT_TRUE(second_document.has_value());
    const auto second_handle = backend_ptr->handle_for_path("/theme_screen/title");
    TEST_ASSERT_TRUE(second_handle.is_valid());
    TEST_ASSERT_NOT_EQUAL(first_handle.value(), second_handle.value());
    const auto *second_style = backend_ptr->style_for_handle(second_handle);
    TEST_ASSERT_NOT_NULL(second_style);
    TEST_ASSERT_TRUE(second_style->style.text_color.has_value());
    TEST_ASSERT_EQUAL_STRING("#222222", second_style->style.text_color->c_str());
    TEST_ASSERT_EQUAL_STRING("test_font_zh", second_style->resolved_font.font_id.c_str());

    TEST_ASSERT_TRUE(runtime.unload(first_document.value()));
    TEST_ASSERT_TRUE(runtime.unload(second_document.value()));
}

BROOKESIA_TEST_CASE(
    test_gui_interface_runtime_public_document_styles,
    "GUI runtime imports public optional styles and returns independent state",
    "[gui][interface][runtime][style]"
)
{
    auto parsed = parse_document(ROOT_JSON, "test", Environment{});
    TEST_ASSERT_TRUE(parsed.has_value());
    auto &title = parsed->screens.at(0).children.at(0);
    std::optional<std::string> &color = title.style.text_color;
    color.emplace("#123456");
    title.style.radius = 0;
    title.style.clip_corner = false;
    title.state_styles["pressed"].opacity = 0;
    title.part_styles["indicator"].style.padding = 0;

    auto backend = std::make_unique<MockBackend>();
    auto *backend_ptr = backend.get();
    Runtime runtime(std::move(backend));
    auto loaded = runtime.load("test/public.json", *parsed, Environment{});
    TEST_ASSERT_TRUE(loaded.has_value());
    color = "#654321";
    const auto handle = backend_ptr->handle_for_path("/test_screen/title");
    const auto *applied = backend_ptr->style_for_handle(handle);
    TEST_ASSERT_NOT_NULL(applied);
    TEST_ASSERT_EQUAL_STRING("#123456", applied->style.text_color->c_str());
    TEST_ASSERT_EQUAL(0, applied->style.radius.value());
    TEST_ASSERT_FALSE(applied->style.clip_corner.value());
    TEST_ASSERT_EQUAL(0, applied->state_styles.at("pressed").opacity.value());
    TEST_ASSERT_EQUAL(0, applied->part_styles.at("indicator").style.padding.value());

    auto state = runtime.get_view_state(loaded.value(), "/test_screen/title", ViewStateKind::Style);
    TEST_ASSERT_TRUE(state.has_value());
    std::get<Style>(*state).text_color.reset();
    state = runtime.get_view_state(loaded.value(), "/test_screen/title", ViewStateKind::Style);
    TEST_ASSERT_TRUE(state.has_value());
    TEST_ASSERT_EQUAL_STRING("#123456", std::get<Style>(*state).text_color->c_str());
    TEST_ASSERT_TRUE(runtime.unload(loaded.value()));
}
