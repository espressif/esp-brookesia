/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <type_traits>

#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/runtime_wasm.hpp"

#if !defined(BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND)
#if defined(ESP_PLATFORM)
#define BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND 1
#else
#define BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND 0
#endif
#endif

using namespace esp_brookesia::runtime;

BROOKESIA_TEST_CASE(
    test_runtime_wasm_backend_contract_and_lifecycle_errors,
    "Runtime WASM backend covers lifecycle and error paths",
    "[runtime][wasm]"
)
{
    static_assert(std::is_base_of_v<IRuntimeBackend, wasm::Backend>);

#if BROOKESIA_RUNTIME_WASM_TEST_APPS_HAS_BACKEND
    auto &backend = wasm::Backend::get_instance();
    TEST_ASSERT_EQUAL(BackendType::Wasm, backend.get_type());
    TEST_ASSERT_FALSE(backend.get_attributes().name.empty());

    backend.deinit();
    AppConfig config{
        .type = BackendType::Wasm,
        .app_path = ".",
        .entry = "missing.wasm",
        .resource_dir = ".",
        .arguments = {},
    };

    TEST_ASSERT_FALSE(backend.load_app(301, config).has_value());
    TEST_ASSERT_TRUE(backend.init().has_value());
    TEST_ASSERT_TRUE(backend.load_app(301, config).has_value());
    TEST_ASSERT_EQUAL(AppState::Loaded, backend.get_app_state(301));
    TEST_ASSERT_FALSE(backend.start_app(301).has_value());
    TEST_ASSERT_TRUE(backend.stop_app(301).has_value());
    TEST_ASSERT_EQUAL(AppState::Stopped, backend.get_app_state(301));
    TEST_ASSERT_TRUE(backend.unload_app(301).has_value());
    TEST_ASSERT_EQUAL(AppState::Unloaded, backend.get_app_state(301));
    TEST_ASSERT_FALSE(backend.call_function(301, "host", "echo", {}).has_value());
    backend.deinit();
#else
    TEST_IGNORE_MESSAGE("Wasmtime C API is not available; WASM backend execution is gated on this PC host");
#endif
}
