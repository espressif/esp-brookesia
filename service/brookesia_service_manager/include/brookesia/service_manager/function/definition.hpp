/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <variant>
#include <type_traits>
#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/common.hpp"

namespace esp_brookesia::service {

enum class FunctionValueType {
    Boolean,        // bool
    Number,         // double
    String,         // std::string
    Object,         // boost::json::object
    Array,          // boost::json::array
    RawBuffer,      // RawBuffer
};
BROOKESIA_DESCRIBE_ENUM(FunctionValueType, Boolean, Number, String, Object, Array, RawBuffer);

// FunctionValue: A variant that can hold different types of function parameter values
// Supports implicit conversion from any arithmetic type (except bool and double) to double
struct FunctionValue : std::variant<bool, double, std::string, boost::json::object, boost::json::array, RawBuffer> {
    using Base = std::variant<bool, double, std::string, boost::json::object, boost::json::array, RawBuffer>;

    // Inherit all constructors from base variant
    using Base::Base;

    // Template constructor for any arithmetic type (except bool and double) - converts to double
    template<typename T>
    requires (std::is_arithmetic_v<std::decay_t<T>> &&
              !std::is_same_v<std::decay_t<T>, bool> &&
              !std::is_same_v<std::decay_t<T>, double>)
    FunctionValue(T num) : Base(static_cast<double>(num)) {}
};

using FunctionParameterMap = std::map<std::string /* name */, FunctionValue /* value */>;

struct FunctionParameterSchema {
    std::string name;
    std::string description = "";
    FunctionValueType type = FunctionValueType::String;
    std::optional<FunctionValue> default_value = std::nullopt;

    bool is_compatible_value(const FunctionValue &value) const
    {
        switch (type) {
        case FunctionValueType::Boolean:
            return std::holds_alternative<bool>(value);
        case FunctionValueType::Number:
            return std::holds_alternative<double>(value);
        case FunctionValueType::String:
            return std::holds_alternative<std::string>(value);
        case FunctionValueType::Object:
            return std::holds_alternative<boost::json::object>(value);
        case FunctionValueType::Array:
            return std::holds_alternative<boost::json::array>(value);
        case FunctionValueType::RawBuffer:
            return std::holds_alternative<RawBuffer>(value);
        default:
            return false;
        }
    }
    bool is_required() const
    {
        return !default_value.has_value();
    }
};
BROOKESIA_DESCRIBE_STRUCT(FunctionParameterSchema, (), (name, description, type, default_value));

struct FunctionSchema {
    std::string name;
    std::string description = "";
    std::vector<FunctionParameterSchema> parameters = {};
    bool require_scheduler = true;
};
BROOKESIA_DESCRIBE_STRUCT(FunctionSchema, (), (name, description, parameters, require_scheduler));

struct FunctionResult {
    bool success = false;
    std::string error_message = "";
    std::optional<FunctionValue> data = std::nullopt;

    bool has_data() const
    {
        return data.has_value();
    }
};
BROOKESIA_DESCRIBE_STRUCT(FunctionResult, (), (success, error_message, data));

} // namespace esp_brookesia::service
