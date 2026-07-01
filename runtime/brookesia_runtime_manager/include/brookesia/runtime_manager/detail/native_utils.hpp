/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime::detail {

inline std::string make_entry_path(const AppConfig &config)
{
    if (config.entry.empty()) {
        return config.app_path;
    }
    if (config.app_path.empty()) {
        return config.entry;
    }
    if (config.app_path.back() == '/') {
        return config.app_path + config.entry;
    }
    return config.app_path + "/" + config.entry;
}

inline std::expected<const NativeFunctionSpec *, std::string> find_native_function(
    const std::vector<NativeModule> &modules, std::string_view module_name, std::string_view function_name
)
{
    for (const auto &module : modules) {
        if (module.name != module_name) {
            continue;
        }
        for (const auto &function : module.functions) {
            if (function.name == function_name) {
                return &function;
            }
        }
        return std::unexpected(
                   "Runtime native function not found: " + std::string(module_name) + "." + std::string(function_name)
               );
    }
    return std::unexpected("Runtime native module not found: " + std::string(module_name));
}

inline NativeResult call_native_module(
    const std::vector<NativeModule> &modules, std::string_view module_name, std::string_view function_name,
    const NativeArgs &args
)
{
    auto function = find_native_function(modules, module_name, function_name);
    if (!function) {
        return std::unexpected(function.error());
    }
    if (!function.value()->function) {
        return std::unexpected(
                   "Runtime native function is asynchronous and unsupported by this backend: " +
                   std::string(module_name) + "." + std::string(function_name)
               );
    }
    return function.value()->function(args);
}

} // namespace esp_brookesia::runtime::detail
