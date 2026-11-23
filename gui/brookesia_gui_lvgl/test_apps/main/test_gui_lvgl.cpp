/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "brookesia/gui_lvgl.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"

#if !defined(BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND)
#if defined(ESP_PLATFORM)
#define BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND 1
#else
#define BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND 0
#endif
#endif

#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
#if __has_include("lvgl/lvgl.h")
#include "lvgl/lvgl.h"
#else
#include "lvgl.h"
#endif
#include "brookesia/service_display/service_display.hpp"
#endif

using namespace esp_brookesia::gui;

#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
namespace {

constexpr int32_t TEST_DISPLAY_WIDTH = 320;
constexpr int32_t TEST_DISPLAY_HEIGHT = 240;
constexpr int32_t TEST_DRAW_BUFFER_ROWS = 10;

void ensure_test_display()
{
#if !defined(ESP_PLATFORM)
    static std::array<uint8_t, TEST_DISPLAY_WIDTH *TEST_DRAW_BUFFER_ROWS * 4> draw_buffer {};
    static const bool initialized = []() {
        lv_init();
        auto *display = lv_display_create(TEST_DISPLAY_WIDTH, TEST_DISPLAY_HEIGHT);
        if (display == nullptr) {
            return false;
        }
        lv_display_set_buffers(
            display,
            draw_buffer.data(),
            nullptr,
            draw_buffer.size(),
            LV_DISPLAY_RENDER_MODE_PARTIAL
        );
        return true;
    }();
    TEST_ASSERT_TRUE(initialized);
#else
    TEST_ASSERT_NOT_NULL(lv_display_get_default());
#endif
}

BackendHandle create_test_screen(lvgl::Backend &backend, std::string id)
{
    Node screen{
        .type = NodeType::Screen,
        .id = std::move(id),
    };
    screen.placement.width = Dimension{.mode = SizeMode::Match};
    screen.placement.height = Dimension{.mode = SizeMode::Match};

    auto handle = backend.create_node(screen, BackendHandle(), {}, "/" + screen.id);
    backend.apply_props(handle, screen);
    backend.apply_layout(handle, screen.layout);
    backend.apply_placement(handle, screen.placement);
    return handle;
}

BackendHandle create_test_frame(
    lvgl::Backend &backend,
    BackendHandle parent,
    std::string_view parent_path,
    std::string id,
    std::string output_name,
    Placement placement
)
{
    Node frame{
        .type = NodeType::FrameView,
        .id = std::move(id),
    };
    frame.frame_view_props.output_name = std::move(output_name);
    frame.placement = placement;

    auto handle = backend.create_node(frame, parent, parent_path, parent_path);
    backend.apply_props(handle, frame);
    backend.apply_layout(handle, frame.layout);
    backend.apply_placement(handle, frame.placement);
    return handle;
}

BackendHandle create_test_container(
    lvgl::Backend &backend,
    BackendHandle parent,
    std::string_view parent_path,
    std::string id
)
{
    Node container{
        .type = NodeType::Container,
        .id = std::move(id),
    };
    return backend.create_node(container, parent, parent_path, parent_path);
}

// Mirrors Runtime::with_native_object: a null native object must not enter the callback.
bool dispatch_native_object(lvgl::Backend &backend, BackendHandle handle, int &calls)
{
    void *object = backend.get_native_object(handle);
    if (object == nullptr) {
        return false;
    }
    ++calls;
    return true;
}

} // namespace
#endif

BROOKESIA_TEST_CASE(
    test_gui_lvgl_public_types_and_display_source_defaults,
    "GUI LVGL public types and display source defaults are covered",
    "[gui][lvgl]"
)
{
    static_assert(std::is_base_of_v<IBackend, lvgl::Backend>);

    lvgl::DisplaySourceConfig display_config;
    TEST_ASSERT_EQUAL_STRING("LVGL", display_config.source_name.c_str());
    TEST_ASSERT_EQUAL_STRING("gui", display_config.source_role.c_str());
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_FRAME_TIMEOUT_MS),
        display_config.frame_timeout_ms
    );
    TEST_ASSERT_EQUAL_INT(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_CORE_ID, display_config.task_core_id);
    TEST_ASSERT_EQUAL_INT(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_PRIORITY, display_config.task_priority);
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TICK_PERIOD_MS),
        display_config.tick_period_ms
    );
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_MIN_DELAY_MS),
        display_config.task_min_delay_ms
    );
    TEST_ASSERT_EQUAL_UINT32(
        static_cast<uint32_t>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_MAX_DELAY_MS),
        display_config.task_max_delay_ms
    );
    TEST_ASSERT_EQUAL_INT(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_STACK_SIZE, display_config.task_stack_size);
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_TASK_STACK_IN_PSRAM != 0),
        static_cast<int>(display_config.stack_in_psram)
    );
    TEST_ASSERT_EQUAL_UINT16(
        static_cast<uint16_t>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_BUFFER_HEIGHT),
        display_config.buffer_height
    );
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(BROOKESIA_GUI_LVGL_DISPLAY_SOURCE_DEFAULT_REQUIRE_DOUBLE_BUFFER != 0),
        static_cast<int>(display_config.require_double_buffer)
    );

    lvgl::FontRegistrationConfig font_config{
        .font_id = "default",
        .primary_src = "default.ttf",
        .native_src = 0x1234,
        .native_size = 16,
        .languages = {"en"},
        .fallback_ids = {"fallback"},
    };
    TEST_ASSERT_EQUAL_STRING("default", font_config.font_id.c_str());
    TEST_ASSERT_EQUAL_STRING("default.ttf", font_config.primary_src.c_str());
    TEST_ASSERT_EQUAL_size_t(1, font_config.languages.size());
    TEST_ASSERT_EQUAL_size_t(1, font_config.fallback_ids.size());

#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
#if defined(ESP_PLATFORM)
    static_assert(std::is_same_v<LvglBackend, lvgl::Backend>);
#endif

    ensure_test_display();

    auto &display_source = lvgl::DisplaySource::get_instance();
    TEST_ASSERT_FALSE(display_source.is_started());
    TEST_ASSERT_EQUAL_UINT32(0, display_source.source_id());
    TEST_ASSERT_EQUAL_UINT32(0, display_source.width());
    TEST_ASSERT_EQUAL_UINT32(0, display_source.height());

    lvgl::Backend backend;
    TEST_ASSERT_TRUE(backend.get_thread_guard().has_value());

    RuntimeImageResource jpeg_resource{
        .primary_src = "test.JPG",
        .width = 42,
        .height = 42,
    };
    TEST_ASSERT_TRUE(backend.requires_preloaded_image_resource(jpeg_resource));
    auto resolved_jpeg = backend.resolve_image_resource(jpeg_resource);
    TEST_ASSERT_TRUE(resolved_jpeg.has_value());
    TEST_ASSERT_EQUAL_INT32(42, resolved_jpeg->width);
    TEST_ASSERT_EQUAL_INT32(42, resolved_jpeg->height);
#else
    TEST_IGNORE_MESSAGE(
        "LVGL backend execution is gated on PC because this test app does not provision an lvgl target"
    );
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_frame_view_retries_registration_after_mount,
    "GUI LVGL retries fixed FrameView registration after its screen is mounted",
    "[gui][lvgl][frame_view]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();

    constexpr std::string_view OUTPUT_NAME = "FrameViewMountRetry";
    constexpr int32_t FRAME_WIDTH = 80;
    constexpr int32_t FRAME_HEIGHT = 60;
    static std::array<uint8_t, FRAME_WIDTH *FRAME_HEIGHT * 2> occupied_buffer{};
    auto occupied_output = esp_brookesia::service::Display::get_instance().register_output(
    esp_brookesia::service::Display::BufferOutputConfig{
        .name = std::string(OUTPUT_NAME),
        .width = FRAME_WIDTH,
        .height = FRAME_HEIGHT,
        .pixel_format = esp_brookesia::service::Display::PixelFormat::RGB565,
        .buffer = esp_brookesia::service::RawBuffer(occupied_buffer.data(), occupied_buffer.size()),
        .stride_bytes = FRAME_WIDTH * 2,
    });
    TEST_ASSERT_TRUE(occupied_output.has_value());

    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "fixed_retry_screen");
    Placement frame_placement{
        .mode = PlacementMode::Absolute,
        .width = Dimension{.mode = SizeMode::Fixed, .value = FRAME_WIDTH},
        .height = Dimension{.mode = SizeMode::Fixed, .value = FRAME_HEIGHT},
    };
    auto frame = create_test_frame(
                     backend,
                     screen,
                     "/fixed_retry_screen/",
                     "frame",
                     std::string(OUTPUT_NAME),
                     frame_placement
                 );
    TEST_ASSERT_TRUE(frame.is_valid());

    auto unregister_occupied = esp_brookesia::service::Display::get_instance().unregister_output(OUTPUT_NAME);
    TEST_ASSERT_TRUE(unregister_occupied.has_value());
    TEST_ASSERT_TRUE(backend.mount_screen(screen, MountTarget{}));

    auto mounted_output = esp_brookesia::service::Display::get_instance().get_buffer_output(OUTPUT_NAME);
    TEST_ASSERT_TRUE(mounted_output.has_value());
    TEST_ASSERT_EQUAL_UINT32(FRAME_WIDTH, mounted_output->info.width);
    TEST_ASSERT_EQUAL_UINT32(FRAME_HEIGHT, mounted_output->info.height);

    backend.destroy_node(screen);
    TEST_ASSERT_FALSE(
        esp_brookesia::service::Display::get_instance().get_buffer_output(OUTPUT_NAME).has_value()
    );
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_frame_view_uses_final_flex_size_after_mount,
    "GUI LVGL registers FrameView with its final mounted flex size",
    "[gui][lvgl][frame_view]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();

    constexpr std::string_view OUTPUT_NAME = "FrameViewFlexMount";
    lvgl::Backend backend;
    Node screen_node{
        .type = NodeType::Screen,
        .id = "flex_screen",
    };
    screen_node.layout.type = LayoutType::Flex;
    screen_node.layout.flex_flow = FlexFlow::Row;
    screen_node.placement.width = Dimension{.mode = SizeMode::Match};
    screen_node.placement.height = Dimension{.mode = SizeMode::Match};
    auto screen = backend.create_node(screen_node, BackendHandle(), {}, "/flex_screen");
    backend.apply_props(screen, screen_node);
    backend.apply_layout(screen, screen_node.layout);
    backend.apply_placement(screen, screen_node.placement);

    Placement frame_placement{
        .mode = PlacementMode::Flow,
        .width = Dimension{.mode = SizeMode::Wrap},
        .height = Dimension{.mode = SizeMode::Match},
        .flex_grow = 1,
    };
    auto frame = create_test_frame(
                     backend,
                     screen,
                     "/flex_screen/",
                     "frame",
                     std::string(OUTPUT_NAME),
                     frame_placement
                 );
    TEST_ASSERT_TRUE(frame.is_valid());
    TEST_ASSERT_TRUE(backend.mount_screen(screen, MountTarget{}));

    auto mounted_output = esp_brookesia::service::Display::get_instance().get_buffer_output(OUTPUT_NAME);
    TEST_ASSERT_TRUE(mounted_output.has_value());
    TEST_ASSERT_TRUE(mounted_output->info.width > 0);
    TEST_ASSERT_TRUE(mounted_output->info.height > 0);

    backend.destroy_node(screen);
    TEST_ASSERT_FALSE(
        esp_brookesia::service::Display::get_instance().get_buffer_output(OUTPUT_NAME).has_value()
    );
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_deleted_native_object_rejects_recycled_address,
    "GUI LVGL drops a handle when LVGL deletes its widget and reuses the address",
    "[gui][lvgl][native]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();

    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "recycle_screen");
    TEST_ASSERT_TRUE(screen.is_valid());
    auto child = create_test_container(backend, screen, "/recycle_screen/", "child");
    TEST_ASSERT_TRUE(child.is_valid());

    auto *freed = static_cast<lv_obj_t *>(backend.get_native_object(child));
    TEST_ASSERT_NOT_NULL(freed);
    // Bypass destroy_node, as a native view callback can.
    lv_obj_delete(freed);

    int stale_calls = 0;
    TEST_ASSERT_FALSE(dispatch_native_object(backend, child, stale_calls));
    TEST_ASSERT_TRUE(stale_calls == 0);

    BackendHandle replacement;
    lv_obj_t *recycled = nullptr;
    for (int attempt = 0; attempt < 16 && recycled == nullptr; ++attempt) {
        auto candidate = create_test_container(
                             backend, screen, "/recycle_screen/", "replacement" + std::to_string(attempt));
        TEST_ASSERT_TRUE(candidate.is_valid());
        auto *native = static_cast<lv_obj_t *>(backend.get_native_object(candidate));
        TEST_ASSERT_NOT_NULL(native);
        TEST_ASSERT_FALSE(dispatch_native_object(backend, child, stale_calls));
        if (native == freed) {
            replacement = candidate;
            recycled = native;
            break;
        }
        lv_obj_delete(native);
        backend.destroy_node(candidate);
    }
    TEST_ASSERT_NOT_NULL(recycled);
    TEST_ASSERT_TRUE(stale_calls == 0);

    int replacement_calls = 0;
    TEST_ASSERT_TRUE(dispatch_native_object(backend, replacement, replacement_calls));
    TEST_ASSERT_TRUE(replacement_calls == 1);
    TEST_ASSERT_TRUE(backend.get_native_object(replacement) == recycled);

    // Dropping the stale record must not delete the widget now living at the old address.
    backend.destroy_node(child);
    TEST_ASSERT_TRUE(backend.get_native_object(replacement) == recycled);
    int after_drop_calls = 0;
    TEST_ASSERT_FALSE(dispatch_native_object(backend, child, after_drop_calls));
    TEST_ASSERT_TRUE(after_drop_calls == 0);
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_parent_delete_invalidates_child_native_objects,
    "GUI LVGL invalidates child handles when a parent is cleaned or deleted",
    "[gui][lvgl][native]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();

    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "parent_delete_screen");
    TEST_ASSERT_TRUE(screen.is_valid());
    auto child = create_test_container(backend, screen, "/parent_delete_screen/", "child");
    auto grandchild = create_test_container(backend, child, "/parent_delete_screen/child/", "grandchild");
    TEST_ASSERT_TRUE(child.is_valid());
    TEST_ASSERT_TRUE(grandchild.is_valid());
    TEST_ASSERT_NOT_NULL(backend.get_native_object(child));
    TEST_ASSERT_NOT_NULL(backend.get_native_object(grandchild));

    auto *screen_object = static_cast<lv_obj_t *>(backend.get_native_object(screen));
    TEST_ASSERT_NOT_NULL(screen_object);
    lv_obj_clean(screen_object);

    int calls = 0;
    TEST_ASSERT_TRUE(backend.get_native_object(screen) == screen_object);
    TEST_ASSERT_FALSE(dispatch_native_object(backend, child, calls));
    TEST_ASSERT_FALSE(dispatch_native_object(backend, grandchild, calls));
    TEST_ASSERT_TRUE(calls == 0);

    auto survivor = create_test_container(backend, screen, "/parent_delete_screen/", "survivor");
    auto nested = create_test_container(backend, survivor, "/parent_delete_screen/survivor/", "nested");
    TEST_ASSERT_TRUE(survivor.is_valid());
    TEST_ASSERT_TRUE(nested.is_valid());
    TEST_ASSERT_NOT_NULL(backend.get_native_object(survivor));
    TEST_ASSERT_NOT_NULL(backend.get_native_object(nested));

    lv_obj_delete(screen_object);
    TEST_ASSERT_FALSE(dispatch_native_object(backend, screen, calls));
    TEST_ASSERT_FALSE(dispatch_native_object(backend, survivor, calls));
    TEST_ASSERT_FALSE(dispatch_native_object(backend, nested, calls));
    TEST_ASSERT_FALSE(dispatch_native_object(backend, child, calls));
    TEST_ASSERT_TRUE(calls == 0);
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}
