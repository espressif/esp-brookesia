/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <type_traits>

#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/runtime_js.hpp"

#if !defined(BROOKESIA_RUNTIME_JS_TEST_APPS_HAS_BACKEND)
#if defined(ESP_PLATFORM)
#define BROOKESIA_RUNTIME_JS_TEST_APPS_HAS_BACKEND 1
#else
#define BROOKESIA_RUNTIME_JS_TEST_APPS_HAS_BACKEND 0
#endif
#endif

using namespace esp_brookesia::runtime;

BROOKESIA_TEST_CASE(
    test_runtime_js_backend_contract_and_error_paths,
    "Runtime JavaScript backend exposes the runtime backend contract",
    "[runtime][js]"
)
{
    static_assert(std::is_base_of_v<IRuntimeBackend, js::Backend>);

#if BROOKESIA_RUNTIME_JS_TEST_APPS_HAS_BACKEND
    auto &backend = js::Backend::get_instance();
    TEST_ASSERT_EQUAL(BackendType::JavaScript, backend.get_type());
    TEST_ASSERT_FALSE(backend.get_attributes().name.empty());

    backend.deinit();
    AppConfig config{
        .type = BackendType::JavaScript,
        .app_path = ".",
        .entry = "missing.js",
        .resource_dir = ".",
        .arguments = {},
    };

    TEST_ASSERT_FALSE(backend.load_app(101, config).has_value());
    TEST_ASSERT_TRUE(backend.init().has_value());
    TEST_ASSERT_EQUAL(AppState::Unloaded, backend.get_app_state(101));
    TEST_ASSERT_FALSE(backend.start_app(101).has_value());
    TEST_ASSERT_FALSE(backend.call_function(101, "host", "echo", {}).has_value());
    TEST_ASSERT_TRUE(backend.load_app(101, config).has_value());
    TEST_ASSERT_EQUAL(AppState::Loaded, backend.get_app_state(101));
    TEST_ASSERT_FALSE(backend.load_app(101, config).has_value());
    TEST_ASSERT_FALSE(backend.start_app(101).has_value());
    TEST_ASSERT_TRUE(backend.unload_app(101).has_value());
    TEST_ASSERT_EQUAL(AppState::Unloaded, backend.get_app_state(101));
    backend.deinit();
#else
    TEST_IGNORE_MESSAGE("QuickJS is not available; JavaScript backend execution is gated on this PC host");
#endif
}
