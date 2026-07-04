/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <type_traits>

#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/runtime_elf.hpp"

#if !defined(BROOKESIA_RUNTIME_ELF_TEST_APPS_HAS_BACKEND)
#if defined(ESP_PLATFORM)
#define BROOKESIA_RUNTIME_ELF_TEST_APPS_HAS_BACKEND 1
#else
#define BROOKESIA_RUNTIME_ELF_TEST_APPS_HAS_BACKEND 0
#endif
#endif

using namespace esp_brookesia::runtime;

BROOKESIA_TEST_CASE(
    test_runtime_elf_backend_contract_and_platform_gate,
    "Runtime ELF backend covers ESP lifecycle errors and PC platform gate",
    "[runtime][elf]"
)
{
    static_assert(std::is_base_of_v<IRuntimeBackend, elf::Backend>);

#if BROOKESIA_RUNTIME_ELF_TEST_APPS_HAS_BACKEND
    auto &backend = elf::Backend::get_instance();
    TEST_ASSERT_EQUAL(BackendType::Elf, backend.get_type());
    TEST_ASSERT_FALSE(backend.get_attributes().name.empty());

    backend.deinit();
    AppConfig config{
        .type = BackendType::Elf,
        .app_path = ".",
        .entry = "missing.elf",
        .resource_dir = ".",
        .arguments = {},
    };

    TEST_ASSERT_FALSE(backend.load_app(401, config).has_value());
    TEST_ASSERT_TRUE(backend.init().has_value());
    TEST_ASSERT_FALSE(backend.load_app(401, config).has_value());
    TEST_ASSERT_EQUAL(AppState::Unloaded, backend.get_app_state(401));
    TEST_ASSERT_FALSE(backend.start_app(401).has_value());
    TEST_ASSERT_FALSE(backend.stop_app(401).has_value());
    TEST_ASSERT_FALSE(backend.call_function(401, "host", "echo", {}).has_value());
    backend.deinit();
#else
    TEST_IGNORE_MESSAGE("ELF runtime backend execution is ESP-IDF only; PC host performs public header coverage");
#endif
}
