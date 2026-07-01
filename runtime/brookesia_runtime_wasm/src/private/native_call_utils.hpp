/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>

#include "brookesia/runtime_manager/detail/native_json.hpp"
#include "brookesia/runtime_manager/detail/native_utils.hpp"

namespace esp_brookesia::runtime::wasm {

inline std::string call_registered_native_function(
    const std::vector<NativeModule> &modules, std::string_view module_name, std::string_view function_name,
    std::string_view args_json
)
{
    auto parsed_args = parse_native_args_json(args_json);
    if (!parsed_args) {
        return native_result_to_json(NativeResult(std::unexpected(parsed_args.error())));
    }

    auto result = detail::call_native_module(modules, module_name, function_name, parsed_args.value());
    return native_result_to_json(result);
}

} // namespace esp_brookesia::runtime::wasm
