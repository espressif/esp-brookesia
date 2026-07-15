/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/gui_interface.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"

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

    void apply_style(BackendHandle handle, const ResolvedStyle &style, StyleApplyMask mask = StyleApplyMask::All) override
    {
        (void)handle;
        (void)style;
        (void)mask;
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

    const std::vector<RuntimeImageResource> &preloaded_images() const
    {
        return images_;
    }

    const std::vector<std::string> &released_image_ids() const
    {
        return released_image_ids_;
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
    std::vector<BackendHandle> mounted_handles_;
    std::vector<BackendHandle> unmounted_handles_;
    std::vector<BackendHandle> destroyed_handles_;
    std::vector<RuntimeFontResource> fonts_;
    std::vector<RuntimeImageResource> images_;
    std::vector<std::string> released_image_ids_;
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
    TEST_ASSERT_TRUE(runtime.unload(document_id.value()));
}
