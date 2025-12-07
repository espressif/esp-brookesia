/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <variant>
#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service {

enum class FunctionValueType {
    Boolean,
    Number,
    String,
    Object,
    Array,
};

using FunctionValue = std::variant <bool, double, std::string, boost::json::object, boost::json::array>;
using FunctionParameterMap = std::map<std::string, FunctionValue>;

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
        default:
            return false;
        }
    }
    bool is_required() const
    {
        return !default_value.has_value();
    }
};

struct FunctionSchema {
    std::string name;
    std::string description = "";
    std::vector<FunctionParameterSchema> parameters = {};
};

struct FunctionResult {
    bool success = false;
    std::string error_message = "";
    std::optional<FunctionValue> data = std::nullopt;

    bool has_data() const
    {
        return data.has_value();
    }
};

BROOKESIA_DESCRIBE_ENUM(FunctionValueType, Boolean, Number, String, Object, Array);
BROOKESIA_DESCRIBE_STRUCT(FunctionParameterSchema, (), (name, description, type, default_value));
BROOKESIA_DESCRIBE_STRUCT(FunctionSchema, (), (name, description, parameters));
BROOKESIA_DESCRIBE_STRUCT(FunctionResult, (), (success, error_message, data));

} // namespace esp_brookesia::service
