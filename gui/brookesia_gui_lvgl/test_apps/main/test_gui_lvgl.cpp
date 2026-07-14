/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <type_traits>

#include "brookesia/gui_lvgl.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"

#if !defined(BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND)
#if defined(ESP_PLATFORM)
#define BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND 1
#else
#define BROOKESIA_GUI_LVGL_TEST_APPS_HAS_BACKEND 0
#endif
#endif

using namespace esp_brookesia::gui;

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
    static_assert(std::is_same_v<LvglBackend, lvgl::Backend>);

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
    TEST_IGNORE_MESSAGE("LVGL backend execution is gated on PC because this test app does not provision an lvgl target");
#endif
}
