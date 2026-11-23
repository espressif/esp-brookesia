/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <array>
#include <cstdint>
#include <cstdio>
#include <fstream>
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
#if __has_include("lvgl/src/core/lv_obj_class_private.h")
#   include "lvgl/src/core/lv_obj_class_private.h"
#elif __has_include("src/core/lv_obj_class_private.h")
#   include "src/core/lv_obj_class_private.h"
#endif
#include "brookesia/service_display/service_display.hpp"
#endif

using namespace esp_brookesia::gui;

namespace esp_brookesia::gui::lvgl {
void drain_widget_lifetimes();
void suppress_image_source_owner(bool suppress);
bool keyboard_layout_storage_retains(const char *const *map);
}

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

BROOKESIA_TEST_CASE(
    test_gui_lvgl_reparented_child_unbinds_delete_callback_when_old_parent_is_destroyed,
    "GUI LVGL unbinds a moved widget before freeing the record owned by its old parent",
    "[gui][lvgl][native]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();

    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "reparent_screen");
    auto parent_a = create_test_container(backend, screen, "/reparent_screen/", "parent_a");
    auto parent_b = create_test_container(backend, screen, "/reparent_screen/", "parent_b");
    auto child = create_test_container(backend, parent_a, "/reparent_screen/parent_a/", "child");
    auto grandchild = create_test_container(backend, child, "/reparent_screen/parent_a/child/", "grandchild");
    TEST_ASSERT_TRUE(parent_a.is_valid());
    TEST_ASSERT_TRUE(parent_b.is_valid());
    TEST_ASSERT_TRUE(child.is_valid());
    TEST_ASSERT_TRUE(grandchild.is_valid());

    Layout grid{
        .type = LayoutType::Grid,
    };
    grid.grid_template_columns = {
        Dimension{.mode = SizeMode::Fixed, .value = 80},
        Dimension{.mode = SizeMode::Fixed, .value = 40},
    };
    grid.grid_template_rows = {
        Dimension{.mode = SizeMode::Fixed, .value = 30},
    };
    backend.apply_layout(child, grid);

    Placement cell{
        .mode = PlacementMode::Flow,
        .grid_column = 1,
    };
    cell.width = Dimension{.mode = SizeMode::Fixed, .value = 40};
    cell.height = Dimension{.mode = SizeMode::Fixed, .value = 30};
    backend.apply_placement(grandchild, cell);
    backend.bind_events(grandchild, {EventBinding{.type = EventType::Clicked, .action = "clicked"}});

    auto *parent_b_object = static_cast<lv_obj_t *>(backend.get_native_object(parent_b));
    auto *child_object = static_cast<lv_obj_t *>(backend.get_native_object(child));
    auto *grandchild_object = static_cast<lv_obj_t *>(backend.get_native_object(grandchild));
    TEST_ASSERT_NOT_NULL(parent_b_object);
    TEST_ASSERT_NOT_NULL(child_object);
    TEST_ASSERT_NOT_NULL(grandchild_object);
    lv_obj_set_size(parent_b_object, 220, 120);
    lv_obj_set_size(child_object, 160, 80);
    lv_obj_set_style_pad_all(child_object, 0, LV_PART_MAIN);
    lv_obj_update_layout(child_object);

    const auto *columns_before = lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN);
    TEST_ASSERT_NOT_NULL(columns_before);
    const int32_t column_width = columns_before[0];
    const int32_t second_column_width = columns_before[1];
    const int32_t column_terminator = columns_before[2];
    const int32_t grandchild_x_before = lv_obj_get_x(grandchild_object);
    // lv_obj_get_x() is relative to the content origin and already excludes the parent's
    // border and padding, so the second track starts at the first track plus the column gap.
    const int32_t column_gap = lv_obj_get_style_pad_column(child_object, LV_PART_MAIN);
    TEST_ASSERT_EQUAL_INT32(80, column_width);
    TEST_ASSERT_EQUAL_INT32(column_width + column_gap, grandchild_x_before);

    // A focused local style hides the default arrays from the resolved getter.
    static const int32_t focused_columns[] = {20, 24, LV_GRID_TEMPLATE_LAST};
    static const int32_t focused_rows[] = {16, LV_GRID_TEMPLATE_LAST};
    const auto focused = static_cast<lv_style_selector_t>(
                             static_cast<unsigned>(LV_PART_MAIN) | static_cast<unsigned>(LV_STATE_FOCUSED)
                         );
    lv_obj_set_style_grid_column_dsc_array(child_object, focused_columns, focused);
    lv_obj_set_style_grid_row_dsc_array(child_object, focused_rows, focused);
    lv_obj_add_state(child_object, LV_STATE_FOCUSED);
    TEST_ASSERT_TRUE(lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN) == focused_columns);

    int sink_calls = 0;
    backend.set_event_sink([&sink_calls](const BackendEvent &) {
        ++sink_calls;
    });

    lv_obj_set_parent(child_object, parent_b_object);
    TEST_ASSERT_TRUE(lv_obj_get_parent(child_object) == parent_b_object);
    const uint32_t child_events_before = lv_obj_get_event_count(child_object);
    const uint32_t grandchild_events_before = lv_obj_get_event_count(grandchild_object);
    TEST_ASSERT_TRUE(child_events_before >= 1);
    TEST_ASSERT_TRUE(grandchild_events_before >= 2);

    backend.destroy_node(parent_a);

    TEST_ASSERT_TRUE(lv_obj_is_valid(child_object));
    TEST_ASSERT_TRUE(lv_obj_is_valid(grandchild_object));
    TEST_ASSERT_TRUE(lv_obj_get_parent(child_object) == parent_b_object);
    TEST_ASSERT_TRUE(lv_obj_get_parent(grandchild_object) == child_object);
    TEST_ASSERT_TRUE(backend.get_native_object(child) == nullptr);
    TEST_ASSERT_TRUE(backend.get_native_object(grandchild) == nullptr);
    // The lifetime callback stays so a later delete can still drop the tracks. Destroy
    // removes only the clicked binding on the grandchild.
    TEST_ASSERT_EQUAL_UINT32(child_events_before, lv_obj_get_event_count(child_object));
    TEST_ASSERT_EQUAL_UINT32(grandchild_events_before - 1, lv_obj_get_event_count(grandchild_object));

    TEST_ASSERT_TRUE(lv_obj_has_state(child_object, LV_STATE_FOCUSED));
    TEST_ASSERT_TRUE(lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN) == focused_columns);
    TEST_ASSERT_TRUE(lv_obj_get_style_grid_row_dsc_array(child_object, LV_PART_MAIN) == focused_rows);

    lv_obj_remove_state(child_object, LV_STATE_FOCUSED);
    const auto *columns_after = lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN);
    TEST_ASSERT_NOT_NULL(columns_after);
    TEST_ASSERT_TRUE(columns_after == columns_before);
    TEST_ASSERT_TRUE(columns_after != focused_columns);
    TEST_ASSERT_EQUAL_INT32(column_width, columns_after[0]);
    TEST_ASSERT_EQUAL_INT32(second_column_width, columns_after[1]);
    TEST_ASSERT_EQUAL_INT32(column_terminator, columns_after[2]);
    lv_obj_update_layout(child_object);
    TEST_ASSERT_EQUAL_INT32(grandchild_x_before, lv_obj_get_x(grandchild_object));
    lv_obj_add_state(child_object, LV_STATE_FOCUSED);
    TEST_ASSERT_TRUE(lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN) == focused_columns);
    lv_obj_remove_state(child_object, LV_STATE_FOCUSED);

    sink_calls = 0;
    lv_obj_send_event(grandchild_object, LV_EVENT_CLICKED, nullptr);
    TEST_ASSERT_EQUAL_INT(0, sink_calls);

    struct GridDeleteProbe {
        const int32_t *columns = nullptr;
        bool alive = false;
    } probe;
    probe.columns = columns_after;
    auto on_grid_delete = [](lv_event_t *event) {
        lvgl::drain_widget_lifetimes();
        auto *seen = static_cast<GridDeleteProbe *>(lv_event_get_user_data(event));
        const auto *columns = lv_obj_get_style_grid_column_dsc_array(
                                  static_cast<lv_obj_t *>(lv_event_get_target(event)), LV_PART_MAIN
                              );
        seen->alive = columns != nullptr && columns == seen->columns && columns[0] == 80 && columns[1] == 40;
    };
    lv_obj_add_event_cb(child_object, on_grid_delete, LV_EVENT_DELETE, &probe);
    lv_obj_delete(parent_b_object);
    TEST_ASSERT_TRUE(probe.alive);
    lvgl::drain_widget_lifetimes();
    backend.destroy_node(parent_b);
    TEST_ASSERT_FALSE(lv_obj_is_valid(child_object));
    TEST_ASSERT_FALSE(lv_obj_is_valid(grandchild_object));
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_reparented_grid_keeps_shared_focus_layout,
    "GUI LVGL keeps a shared focused grid layout after the old parent is destroyed",
    "[gui][lvgl][native]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();
    const auto focused = static_cast<lv_style_selector_t>(
                             static_cast<unsigned>(LV_PART_MAIN) | static_cast<unsigned>(LV_STATE_FOCUSED)
                         );

    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "grid_shared_screen");
    auto parent_a = create_test_container(backend, screen, "/grid_shared_screen/", "parent_a");
    auto parent_b = create_test_container(backend, screen, "/grid_shared_screen/", "parent_b");
    auto child = create_test_container(backend, parent_a, "/grid_shared_screen/parent_a/", "child");
    auto grandchild = create_test_container(backend, child, "/grid_shared_screen/parent_a/child/", "grandchild");

    Layout parent_grid{.type = LayoutType::Grid};
    parent_grid.grid_template_columns = {Dimension{.mode = SizeMode::Fixed, .value = 160}};
    parent_grid.grid_template_rows = {Dimension{.mode = SizeMode::Fixed, .value = 80}};
    backend.apply_layout(parent_b, parent_grid);

    Layout child_grid{.type = LayoutType::Grid};
    child_grid.grid_template_columns = {
        Dimension{.mode = SizeMode::Fixed, .value = 80},
        Dimension{.mode = SizeMode::Fixed, .value = 40},
    };
    child_grid.grid_template_rows = {Dimension{.mode = SizeMode::Fixed, .value = 30}};
    backend.apply_layout(child, child_grid);

    Placement cell{.mode = PlacementMode::Flow, .grid_column = 1};
    cell.width = Dimension{.mode = SizeMode::Fixed, .value = 40};
    cell.height = Dimension{.mode = SizeMode::Fixed, .value = 30};
    backend.apply_placement(grandchild, cell);

    auto *parent_b_object = static_cast<lv_obj_t *>(backend.get_native_object(parent_b));
    auto *child_object = static_cast<lv_obj_t *>(backend.get_native_object(child));
    auto *grandchild_object = static_cast<lv_obj_t *>(backend.get_native_object(grandchild));
    TEST_ASSERT_NOT_NULL(parent_b_object);
    TEST_ASSERT_NOT_NULL(child_object);
    TEST_ASSERT_NOT_NULL(grandchild_object);
    lv_obj_set_size(parent_b_object, 180, 100);
    lv_obj_set_size(child_object, 160, 80);
    lv_obj_set_style_pad_all(child_object, 0, LV_PART_MAIN);
    lv_obj_update_layout(child_object);
    const auto *columns_before = lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN);
    const int32_t grandchild_x_before = lv_obj_get_x(grandchild_object);
    TEST_ASSERT_NOT_NULL(columns_before);
    TEST_ASSERT_EQUAL_INT32(80, columns_before[0]);

    lv_style_t shared_focus;
    lv_style_init(&shared_focus);
    lv_style_set_layout(&shared_focus, LV_LAYOUT_GRID);
    lv_obj_add_style(child_object, &shared_focus, focused);
    lv_obj_add_state(child_object, LV_STATE_FOCUSED);
    TEST_ASSERT_EQUAL_INT(LV_LAYOUT_GRID, lv_obj_get_style_layout(child_object, LV_PART_MAIN));

    lv_obj_set_parent(child_object, parent_b_object);
    backend.destroy_node(parent_a);

    TEST_ASSERT_TRUE(lv_obj_is_valid(child_object));
    TEST_ASSERT_TRUE(lv_obj_is_valid(grandchild_object));
    TEST_ASSERT_TRUE(lv_obj_has_state(child_object, LV_STATE_FOCUSED));
    TEST_ASSERT_EQUAL_INT(LV_LAYOUT_GRID, lv_obj_get_style_layout(child_object, LV_PART_MAIN));
    TEST_ASSERT_TRUE(lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN) == columns_before);
    lv_obj_update_layout(parent_b_object);
    TEST_ASSERT_EQUAL_INT32(grandchild_x_before, lv_obj_get_x(grandchild_object));

    lv_obj_remove_state(child_object, LV_STATE_FOCUSED);
    TEST_ASSERT_TRUE(lv_obj_get_style_grid_column_dsc_array(child_object, LV_PART_MAIN) == columns_before);
    lv_obj_update_layout(parent_b_object);
    TEST_ASSERT_EQUAL_INT32(grandchild_x_before, lv_obj_get_x(grandchild_object));

    backend.destroy_node(parent_b);
    TEST_ASSERT_FALSE(lv_obj_is_valid(child_object));
    lv_style_reset(&shared_focus);
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_reparented_image_keeps_descriptor_after_shared_bin_is_released,
    "GUI LVGL keeps a moved image descriptor alive after the shared preload is released",
    "[gui][lvgl][native]"
)
{
#if !BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#elif defined(ESP_PLATFORM)
    TEST_IGNORE_MESSAGE("Shared bin reparent test writes a host file");
#else
    ensure_test_display();
    const char *path = "/tmp/brookesia-reparent-image.bin";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        TEST_ASSERT_TRUE(out.is_open());
        const uint8_t bytes[] = {
            LV_IMAGE_HEADER_MAGIC,
            static_cast<uint8_t>(LV_COLOR_FORMAT_RGB565),
            0, 0,
            1, 0,
            1, 0,
            2, 0,
            0, 0,
            0x12, 0x34,
        };
        out.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
        TEST_ASSERT_TRUE(out.good());
    }

    lvgl::Backend backend;
    auto preloaded = backend.preload_image_resource(RuntimeImageResource{
        .primary_src = path,
        .width = 1,
        .height = 1,
    });
    TEST_ASSERT_TRUE(preloaded.has_value());

    auto screen = create_test_screen(backend, "image_reparent_screen");
    auto parent_a = create_test_container(backend, screen, "/image_reparent_screen/", "parent_a");
    auto parent_b = create_test_container(backend, screen, "/image_reparent_screen/", "parent_b");

    Node image_a_node{.type = NodeType::Image, .id = "image_a"};
    image_a_node.image_props.src = path;
    image_a_node.resolved_image.primary_src = path;
    image_a_node.resolved_image.width = 1;
    image_a_node.resolved_image.height = 1;
    auto image_a = backend.create_node(image_a_node, parent_a, "/image_reparent_screen/parent_a/", "/image_reparent_screen/parent_a/");
    backend.apply_props(image_a, image_a_node);

    Node image_b_node{.type = NodeType::Image, .id = "image_b"};
    image_b_node.image_props.src = path;
    image_b_node.resolved_image.primary_src = path;
    image_b_node.resolved_image.width = 1;
    image_b_node.resolved_image.height = 1;
    auto image_b = backend.create_node(image_b_node, parent_b, "/image_reparent_screen/parent_b/", "/image_reparent_screen/parent_b/");
    backend.apply_props(image_b, image_b_node);

    auto *parent_b_object = static_cast<lv_obj_t *>(backend.get_native_object(parent_b));
    auto *image_a_object = static_cast<lv_obj_t *>(backend.get_native_object(image_a));
    TEST_ASSERT_NOT_NULL(parent_b_object);
    TEST_ASSERT_NOT_NULL(image_a_object);
    const auto *descriptor_before = static_cast<const lv_image_dsc_t *>(lv_image_get_src(image_a_object));
    TEST_ASSERT_NOT_NULL(descriptor_before);
    TEST_ASSERT_EQUAL_UINT8(LV_IMAGE_HEADER_MAGIC, descriptor_before->header.magic);

    lv_obj_set_parent(image_a_object, parent_b_object);
    backend.destroy_node(parent_a);
    TEST_ASSERT_TRUE(lv_obj_is_valid(image_a_object));
    TEST_ASSERT_TRUE(backend.get_native_object(image_a) == nullptr);

    backend.release_image_resource(RuntimeImageResource{.primary_src = path, .width = 1, .height = 1});
    image_b_node.image_props.src = "/tmp/brookesia-reparent-image-other.bin";
    image_b_node.resolved_image.primary_src = image_b_node.image_props.src;
    backend.apply_props(image_b, image_b_node);

    const auto *descriptor_after = static_cast<const lv_image_dsc_t *>(lv_image_get_src(image_a_object));
    TEST_ASSERT_NOT_NULL(descriptor_after);
    TEST_ASSERT_EQUAL_UINT8(LV_IMAGE_HEADER_MAGIC, descriptor_after->header.magic);
    TEST_ASSERT_EQUAL_UINT8(0x12, descriptor_after->data[0]);
    TEST_ASSERT_EQUAL_UINT8(0x34, descriptor_after->data[1]);

    bool descriptor_alive = false;
    auto on_image_delete = [](lv_event_t *event) {
        lvgl::drain_widget_lifetimes();
        auto *alive = static_cast<bool *>(lv_event_get_user_data(event));
        const auto *descriptor = static_cast<const lv_image_dsc_t *>(
                                     lv_image_get_src(static_cast<lv_obj_t *>(lv_event_get_target(event)))
                                 );
        *alive = descriptor != nullptr && descriptor->header.magic == LV_IMAGE_HEADER_MAGIC &&
                 descriptor->data != nullptr && descriptor->data[0] == 0x12 && descriptor->data[1] == 0x34;
    };
    lv_obj_add_event_cb(image_a_object, on_image_delete, LV_EVENT_DELETE, &descriptor_alive);
    lv_obj_delete(parent_b_object);
    TEST_ASSERT_TRUE(descriptor_alive);
    lvgl::drain_widget_lifetimes();
    backend.destroy_node(parent_b);
    TEST_ASSERT_FALSE(lv_obj_is_valid(image_a_object));
    std::remove(path);
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_reparented_image_clears_src_without_a_new_delete_callback,
    "GUI LVGL clears a moved image without registering another delete callback",
    "[gui][lvgl][native]"
)
{
#if !BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#elif defined(ESP_PLATFORM)
    TEST_IGNORE_MESSAGE("Shared bin reparent test writes a host file");
#else
    ensure_test_display();
    const char *path = "/tmp/brookesia-reparent-image-clear.bin";
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        TEST_ASSERT_TRUE(out.is_open());
        const uint8_t bytes[] = {
            LV_IMAGE_HEADER_MAGIC,
            static_cast<uint8_t>(LV_COLOR_FORMAT_RGB565),
            0, 0, 1, 0, 1, 0, 2, 0, 0, 0, 0x12, 0x34,
        };
        out.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
        TEST_ASSERT_TRUE(out.good());
    }

    lvgl::Backend backend;
    auto preloaded = backend.preload_image_resource(RuntimeImageResource{
        .primary_src = path, .width = 1, .height = 1,
    });
    TEST_ASSERT_TRUE(preloaded.has_value());
    auto screen = create_test_screen(backend, "image_clear_screen");
    auto parent_a = create_test_container(backend, screen, "/image_clear_screen/", "parent_a");
    auto parent_b = create_test_container(backend, screen, "/image_clear_screen/", "parent_b");
    Node image_node{.type = NodeType::Image, .id = "image_a"};
    image_node.image_props.src = path;
    image_node.resolved_image.primary_src = path;
    image_node.resolved_image.width = 1;
    image_node.resolved_image.height = 1;
    lvgl::suppress_image_source_owner(true);
    auto image = backend.create_node(
                     image_node, parent_a, "/image_clear_screen/parent_a/", "/image_clear_screen/parent_a/"
                 );
    lvgl::suppress_image_source_owner(false);
    backend.apply_props(image, image_node);
    auto *parent_b_object = static_cast<lv_obj_t *>(backend.get_native_object(parent_b));
    auto *image_object = static_cast<lv_obj_t *>(backend.get_native_object(image));
    TEST_ASSERT_NOT_NULL(image_object);
    const auto events_before = lv_obj_get_event_count(image_object);
    lv_obj_set_parent(image_object, parent_b_object);
    backend.destroy_node(parent_a);
    TEST_ASSERT_TRUE(lv_obj_is_valid(image_object));
    TEST_ASSERT_TRUE(lv_image_get_src(image_object) == nullptr);
    TEST_ASSERT_EQUAL_UINT32(events_before, lv_obj_get_event_count(image_object));
    backend.destroy_node(parent_b);
    TEST_ASSERT_FALSE(lv_obj_is_valid(image_object));
    std::remove(path);
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_reparented_canvas_keeps_pixels_until_delete_listeners_finish,
    "GUI LVGL keeps canvas pixels alive for delete listeners registered after creation",
    "[gui][lvgl][native]"
)
{
#if !BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#else
    ensure_test_display();
    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "canvas_reparent_screen");
    auto parent_a = create_test_container(backend, screen, "/canvas_reparent_screen/", "parent_a");
    auto parent_b = create_test_container(backend, screen, "/canvas_reparent_screen/", "parent_b");

    Node canvas_node{.type = NodeType::Canvas, .id = "canvas"};
    canvas_node.placement.width = Dimension{.mode = SizeMode::Fixed, .value = 2};
    canvas_node.placement.height = Dimension{.mode = SizeMode::Fixed, .value = 2};
    canvas_node.canvas_props.commands.push_back(
        CanvasCommand{.type = "pixel", .x = 0, .y = 0, .color = "#ff0000"}
    );
    auto canvas = backend.create_node(
                      canvas_node, parent_a, "/canvas_reparent_screen/parent_a/", "/canvas_reparent_screen/parent_a/"
                  );
    backend.apply_props(canvas, canvas_node);
    backend.apply_placement(canvas, canvas_node.placement);

    auto *parent_b_object = static_cast<lv_obj_t *>(backend.get_native_object(parent_b));
    auto *canvas_object = static_cast<lv_obj_t *>(backend.get_native_object(canvas));
    TEST_ASSERT_NOT_NULL(parent_b_object);
    TEST_ASSERT_NOT_NULL(canvas_object);
    const auto events_before = lv_obj_get_event_count(canvas_object);
    auto *draw_buf = lv_canvas_get_draw_buf(canvas_object);
    TEST_ASSERT_NOT_NULL(draw_buf);
    TEST_ASSERT_NOT_NULL(draw_buf->data);
    TEST_ASSERT_EQUAL_INT32(2, draw_buf->header.w);
    TEST_ASSERT_EQUAL_INT32(2, draw_buf->header.h);
    const auto *pixels = draw_buf->data;
    const auto stride = draw_buf->header.stride;
    TEST_ASSERT_TRUE(stride >= 2);

    lv_obj_set_parent(canvas_object, parent_b_object);
    backend.destroy_node(parent_a);
    TEST_ASSERT_TRUE(lv_obj_is_valid(canvas_object));
    TEST_ASSERT_EQUAL_UINT32(events_before, lv_obj_get_event_count(canvas_object));
    TEST_ASSERT_TRUE(lv_canvas_get_draw_buf(canvas_object)->data == pixels);

    struct CanvasDeleteProbe {
        const uint8_t *pixels = nullptr;
        uint32_t stride = 0;
        bool alive = false;
    } probe;
    probe.pixels = pixels;
    probe.stride = stride;
    auto on_canvas_delete = [](lv_event_t *event) {
        lvgl::drain_widget_lifetimes();
        auto *seen = static_cast<CanvasDeleteProbe *>(lv_event_get_user_data(event));
        auto *buf = lv_canvas_get_draw_buf(static_cast<lv_obj_t *>(lv_event_get_target(event)));
        seen->alive = buf != nullptr && buf->data == seen->pixels && buf->header.w == 2 &&
                      buf->header.stride == seen->stride && buf->data[0] == seen->pixels[0];
    };
    lv_obj_add_event_cb(canvas_object, on_canvas_delete, LV_EVENT_DELETE, &probe);
    lv_obj_delete(parent_b_object);
    TEST_ASSERT_TRUE(probe.alive);
    lvgl::drain_widget_lifetimes();
    backend.destroy_node(parent_b);
    TEST_ASSERT_FALSE(lv_obj_is_valid(canvas_object));
#endif
}

BROOKESIA_TEST_CASE(
    test_gui_lvgl_native_keyboard_delete_keeps_map_while_lvgl_references_it,
    "GUI LVGL keeps a keyboard map until LVGL drops the global entry and no keyboard displays it",
    "[gui][lvgl][native]"
)
{
#if BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND
    ensure_test_display();

    lvgl::Backend backend;
    auto screen = create_test_screen(backend, "keyboard_delete_screen");
    auto *screen_object = static_cast<lv_obj_t *>(backend.get_native_object(screen));
    TEST_ASSERT_NOT_NULL(screen_object);

    auto create_labeled_keyboard = [&](const char *id, const char *label) {
        Node node{.type = NodeType::Keyboard, .id = id};
        node.keyboard_props.layouts.insert_or_assign(
            "text",
        KeyboardLayout{.rows = {{KeyboardKey{.text = label, .width = 1}}}}
        );
        auto handle = backend.create_node(
                          node,
                          screen,
                          "/keyboard_delete_screen/",
                          "/keyboard_delete_screen/"
                      );
        backend.apply_props(handle, node);
        return handle;
    };

    auto keyboard = create_labeled_keyboard("keyboard", "Keep");
    auto *keyboard_object = static_cast<lv_obj_t *>(backend.get_native_object(keyboard));
    TEST_ASSERT_NOT_NULL(keyboard_object);
    TEST_ASSERT_EQUAL_STRING("Keep", lv_keyboard_get_button_text(keyboard_object, 0));
    const auto *kept_map = lv_keyboard_get_map_array(keyboard_object);
    TEST_ASSERT_NOT_NULL(kept_map);

    auto *original_display = lv_display_get_default();
    auto *extra = lv_display_create(TEST_DISPLAY_WIDTH, TEST_DISPLAY_HEIGHT);
    TEST_ASSERT_NOT_NULL(extra);
    lv_display_set_default(extra);
    auto *inactive = lv_obj_create(nullptr);
    TEST_ASSERT_NOT_NULL(inactive);
    TEST_ASSERT_TRUE(inactive != lv_screen_active());
    // A subclass still runs the Keyboard constructor and borrows the global map.
    // lv_obj_check_type() does not match it.
    static lv_obj_class_t keyboard_subclass {};
    keyboard_subclass.base_class = &lv_keyboard_class;
    keyboard_subclass.name = "brookesia_test_keyboard";
    auto *parked = lv_obj_class_create_obj(&keyboard_subclass, inactive);
    TEST_ASSERT_NOT_NULL(parked);
    lv_obj_class_init_obj(parked);
    TEST_ASSERT_FALSE(lv_obj_check_type(parked, &lv_keyboard_class));
    TEST_ASSERT_TRUE(lv_obj_has_class(parked, &lv_keyboard_class));
    TEST_ASSERT_TRUE(lv_keyboard_get_map_array(parked) == kept_map);
    lv_display_set_default(original_display);

    lv_obj_delete(keyboard_object);
    backend.destroy_node(keyboard);
    TEST_ASSERT_TRUE(lvgl::keyboard_layout_storage_retains(kept_map));
    TEST_ASSERT_EQUAL_STRING("Keep", lv_keyboard_get_button_text(parked, 0));
    TEST_ASSERT_TRUE(lv_keyboard_get_map_array(parked) == kept_map);

    lv_display_delete(extra);
    Node sweep_node{.type = NodeType::Keyboard, .id = "sweep"};
    auto sweep = backend.create_node(
                     sweep_node,
                     screen,
                     "/keyboard_delete_screen/",
                     "/keyboard_delete_screen/"
                 );
    TEST_ASSERT_TRUE(sweep.is_valid());
    backend.destroy_node(sweep);
    TEST_ASSERT_FALSE(lvgl::keyboard_layout_storage_retains(kept_map));

    auto *rebuilt = lv_keyboard_create(screen_object);
    TEST_ASSERT_NOT_NULL(rebuilt);
    TEST_ASSERT_TRUE(lv_keyboard_get_button_text(rebuilt, 0) == nullptr);
    lv_obj_delete(rebuilt);

    auto older = create_labeled_keyboard("older", "Alpha");
    auto later = create_labeled_keyboard("later", "Beta");
    auto *older_object = static_cast<lv_obj_t *>(backend.get_native_object(older));
    auto *later_object = static_cast<lv_obj_t *>(backend.get_native_object(later));
    TEST_ASSERT_NOT_NULL(older_object);
    TEST_ASSERT_NOT_NULL(later_object);
    const auto *older_map = lv_keyboard_get_map_array(older_object);
    const auto *later_map = lv_keyboard_get_map_array(later_object);
    TEST_ASSERT_NOT_NULL(older_map);
    TEST_ASSERT_TRUE(older_map != later_map);
    TEST_ASSERT_EQUAL_STRING("Alpha", lv_keyboard_get_button_text(older_object, 0));
    TEST_ASSERT_EQUAL_STRING("Beta", lv_keyboard_get_button_text(later_object, 0));

    lv_obj_delete(later_object);
    backend.destroy_node(later);
    TEST_ASSERT_EQUAL_STRING("Alpha", lv_keyboard_get_button_text(older_object, 0));
    TEST_ASSERT_TRUE(lv_keyboard_get_map_array(older_object) == older_map);
    TEST_ASSERT_TRUE(lvgl::keyboard_layout_storage_retains(older_map));
    TEST_ASSERT_TRUE(lvgl::keyboard_layout_storage_retains(later_map));
    backend.destroy_node(older);
    TEST_ASSERT_FALSE(lvgl::keyboard_layout_storage_retains(older_map));
    TEST_ASSERT_FALSE(lvgl::keyboard_layout_storage_retains(later_map));
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution requires a provisioned lvgl target");
#endif
}
