/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <expected>
#include <limits>
#include <string>
#include <string_view>
#include <type_traits>

#include "boost/json.hpp"
#include "brookesia/runtime_manager/types.hpp"

namespace esp_brookesia::runtime {

inline boost::json::value native_value_to_json(const NativeValue &value)
{
    return std::visit(
    [](const auto & item) -> boost::json::value {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, std::monostate>)
        {
            return nullptr;
        } else
        {
            return boost::json::value(item);
        }
    },
    value
           );
}

inline std::expected<NativeValue, std::string> parse_native_value(const boost::json::value &value)
{
    if (value.is_null()) {
        return NativeValue{std::monostate{}};
    }
    if (value.is_bool()) {
        return NativeValue{value.as_bool()};
    }
    if (value.is_int64()) {
        return NativeValue{value.as_int64()};
    }
    if (value.is_uint64()) {
        const auto uint_value = value.as_uint64();
        if (uint_value <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            return NativeValue{static_cast<int64_t>(uint_value)};
        }
        return NativeValue{static_cast<double>(uint_value)};
    }
    if (value.is_double()) {
        return NativeValue{value.as_double()};
    }
    if (value.is_string()) {
        return NativeValue{std::string(value.as_string())};
    }
    return std::unexpected("Native argument must be null, bool, number, or string");
}

inline std::expected<NativeArgs, std::string> parse_native_args_json(std::string_view args_json)
{
    NativeArgs args;
    if (args_json.empty()) {
        return args;
    }

    boost::system::error_code error_code;
    auto parsed = boost::json::parse(args_json, error_code);
    if (error_code || !parsed.is_array()) {
        return std::unexpected("Native arguments must be a JSON array");
    }

    for (const auto &item : parsed.as_array()) {
        auto native_value = parse_native_value(item);
        if (!native_value) {
            return std::unexpected(native_value.error());
        }
        args.push_back(std::move(native_value.value()));
    }
    return args;
}

inline std::string native_result_to_json(const NativeResult &result)
{
    boost::json::object root;
    root["success"] = static_cast<bool>(result);
    if (result) {
        root["data"] = native_value_to_json(result.value());
    } else {
        root["error"] = result.error();
    }
    return boost::json::serialize(root);
}

} // namespace esp_brookesia::runtime
