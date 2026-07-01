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
    static_assert(std::is_same_v<LvglBackend, lvgl::Backend>);

    lvgl::DisplaySourceConfig display_config;
    TEST_ASSERT_EQUAL_STRING("LVGL", display_config.source_name.c_str());
    TEST_ASSERT_EQUAL_STRING("gui", display_config.source_role.c_str());
    TEST_ASSERT_EQUAL_UINT32(6000, display_config.frame_timeout_ms);
    TEST_ASSERT_EQUAL_UINT16(20, display_config.buffer_height);
    TEST_ASSERT_FALSE(display_config.require_double_buffer);

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
    auto &display_source = lvgl::DisplaySource::get_instance();
    TEST_ASSERT_FALSE(display_source.is_started());
    TEST_ASSERT_EQUAL_UINT32(0, display_source.source_id());
    TEST_ASSERT_EQUAL_UINT32(0, display_source.width());
    TEST_ASSERT_EQUAL_UINT32(0, display_source.height());

    lvgl::Backend backend;
    TEST_ASSERT_TRUE(backend.get_thread_guard().has_value());
#else
    TEST_IGNORE_MESSAGE("LVGL backend execution is gated on PC because this test app does not provision an lvgl target");
#endif
}
