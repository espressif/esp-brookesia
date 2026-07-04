/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <cctype>
#include <expected>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime::wasm {

enum class ImportKind {
    NativeFunction,
};

struct NativeImportDescriptor {
    std::string symbol_name;
    std::string module_name;
    std::string function_name;
    ImportKind kind = ImportKind::NativeFunction;
};

struct NativeImportSet {
    std::vector<NativeImportDescriptor> descriptors;
};

inline constexpr std::array<std::string_view, 3> RESULT_HELPER_IMPORT_NAMES = {
    "brookesia_result_len",
    "brookesia_result_copy",
    "brookesia_result_free",
};

inline std::string sanitize_import_name(std::string_view name)
{
    std::string sanitized;
    sanitized.reserve(name.size());
    bool last_was_underscore = false;

    for (const unsigned char ch : name) {
        if (std::isalnum(ch) || (ch == '_')) {
            sanitized.push_back(static_cast<char>(ch));
            last_was_underscore = false;
            continue;
        }
        if (!last_was_underscore) {
            sanitized.push_back('_');
            last_was_underscore = true;
        }
    }

    while (!sanitized.empty() && (sanitized.front() == '_')) {
        sanitized.erase(sanitized.begin());
    }
    while (!sanitized.empty() && (sanitized.back() == '_')) {
        sanitized.pop_back();
    }

    return sanitized;
}

inline std::string make_import_symbol_name(std::string_view module_name, std::string_view function_name)
{
    const std::string sanitized_module_name = sanitize_import_name(module_name);
    const std::string sanitized_function_name = sanitize_import_name(function_name);
    if (sanitized_module_name.empty() || sanitized_function_name.empty()) {
        return {};
    }
    return sanitized_module_name + "_" + sanitized_function_name;
}

inline std::expected<NativeImportSet, std::string> build_native_import_set(const std::vector<NativeModule> &modules)
{
    NativeImportSet import_set;
    std::map<std::string, std::string> symbol_sources;

    for (const auto &helper_name : RESULT_HELPER_IMPORT_NAMES) {
        symbol_sources.emplace(std::string(helper_name), std::string(helper_name));
    }

    for (const auto &module : modules) {
        for (const auto &function : module.functions) {
            const std::string symbol_name = make_import_symbol_name(module.name, function.name);
            if (symbol_name.empty()) {
                return std::unexpected(
                           "WASM import symbol name is empty for runtime native function: " + module.name + "." +
                           function.name
                       );
            }

            const std::string source_name = module.name + "." + function.name;
            auto [it, inserted] = symbol_sources.emplace(symbol_name, source_name);
            if (!inserted) {
                return std::unexpected(
                           "WASM import symbol name is duplicated: " + symbol_name + " (" + it->second + ", " +
                           source_name + ")"
                       );
            }

            import_set.descriptors.push_back({
                .symbol_name = symbol_name,
                .module_name = module.name,
                .function_name = function.name,
            });
        }
    }

    return import_set;
}

} // namespace esp_brookesia::runtime::wasm
