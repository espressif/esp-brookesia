/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <type_traits>

#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/runtime_lua.hpp"

#if !defined(BROOKESIA_RUNTIME_LUA_TEST_APPS_HAS_BACKEND)
#if defined(ESP_PLATFORM)
#define BROOKESIA_RUNTIME_LUA_TEST_APPS_HAS_BACKEND 1
#else
#define BROOKESIA_RUNTIME_LUA_TEST_APPS_HAS_BACKEND 0
#endif
#endif

using namespace esp_brookesia::runtime;

BROOKESIA_TEST_CASE(
    test_runtime_lua_backend_contract_and_lifecycle_errors,
    "Runtime Lua backend covers lifecycle and error paths",
    "[runtime][lua]"
)
{
    static_assert(std::is_base_of_v<IRuntimeBackend, lua::Backend>);

#if BROOKESIA_RUNTIME_LUA_TEST_APPS_HAS_BACKEND
    auto &backend = lua::Backend::get_instance();
    TEST_ASSERT_EQUAL(BackendType::Lua, backend.get_type());
    TEST_ASSERT_FALSE(backend.get_attributes().name.empty());

    backend.deinit();
    AppConfig config{
        .type = BackendType::Lua,
        .app_path = ".",
        .entry = "missing.lua",
        .resource_dir = ".",
        .arguments = {},
    };

    TEST_ASSERT_FALSE(backend.load_app(201, config).has_value());
    TEST_ASSERT_TRUE(backend.init().has_value());
    TEST_ASSERT_TRUE(backend.load_app(201, config).has_value());
    TEST_ASSERT_EQUAL(AppState::Loaded, backend.get_app_state(201));
    TEST_ASSERT_FALSE(backend.start_app(201).has_value());
    TEST_ASSERT_EQUAL(AppState::Error, backend.get_app_state(201));
    TEST_ASSERT_TRUE(backend.stop_app(201).has_value());
    TEST_ASSERT_EQUAL(AppState::Stopped, backend.get_app_state(201));
    TEST_ASSERT_TRUE(backend.unload_app(201).has_value());
    TEST_ASSERT_EQUAL(AppState::Unloaded, backend.get_app_state(201));
    TEST_ASSERT_FALSE(backend.call_function(201, "host", "echo", {}).has_value());
    backend.deinit();
#else
    TEST_IGNORE_MESSAGE("lua5.4 is not available; Lua backend execution is gated on this PC host");
#endif
}
