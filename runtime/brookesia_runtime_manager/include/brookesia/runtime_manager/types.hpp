/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <variant>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::runtime {

using AppId = uint32_t;
inline constexpr AppId INVALID_APP_ID = 0;

enum class BackendType {
    Unknown,
    Lua,
    JavaScript,
    Wasm,
    Elf,
};
BROOKESIA_DESCRIBE_ENUM(BackendType, Unknown, Lua, JavaScript, Wasm, Elf)

enum class AppState {
    Unloaded,
    Loaded,
    Running,
    Stopped,
    Error,
};
BROOKESIA_DESCRIBE_ENUM(AppState, Unloaded, Loaded, Running, Stopped, Error)

using NativeValue = std::variant<std::monostate, bool, int64_t, double, std::string>;
using NativeArgs = std::vector<NativeValue>;
using NativeResult = std::expected<NativeValue, std::string>;
using NativeFunction = std::function<NativeResult(const NativeArgs &)>;
using NativeAsyncCallback = std::function < void(NativeResult &&) >;
using NativeAsyncFunction = std::function<void(const NativeArgs &, NativeAsyncCallback)>;

struct NativeFunctionSpec {
    std::string name;
    NativeFunction function;
    NativeAsyncFunction async_function;
};
BROOKESIA_DESCRIBE_STRUCT(NativeFunctionSpec, (), (name, function, async_function))

struct NativeModule {
    std::string name;
    std::vector<NativeFunctionSpec> functions;
};
BROOKESIA_DESCRIBE_STRUCT(NativeModule, (), (name, functions))

struct AppConfig {
    BackendType type = BackendType::Unknown;
    std::string app_path;
    std::string entry;
    std::string resource_dir;
    std::vector<std::string> arguments;
};
BROOKESIA_DESCRIBE_STRUCT(AppConfig, (), (type, app_path, entry, resource_dir, arguments))

struct RuntimeApp {
    AppId id = INVALID_APP_ID;
    AppConfig config;
    AppState state = AppState::Unloaded;
    std::string last_error;
};
BROOKESIA_DESCRIBE_STRUCT(RuntimeApp, (), (id, config, state, last_error))

} // namespace esp_brookesia::runtime
