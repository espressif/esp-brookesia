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

// Concept to check if a type can be converted to FunctionValue
template<typename T>
concept ConvertibleToFunctionValue =
    std::convertible_to<std::decay_t<T>, bool> ||
    std::convertible_to<std::decay_t<T>, double> ||
    std::convertible_to<std::decay_t<T>, std::string> ||
    std::convertible_to<std::decay_t<T>, boost::json::object> ||
    std::convertible_to<std::decay_t<T>, boost::json::array> ||
    std::convertible_to<std::decay_t<T>, RawBuffer>;

/**
 * @brief Supported value categories for service function parameters and return values.
 */
enum class FunctionValueType {
    Boolean,  ///< `bool`.
    Number,   ///< `double`.
    String,   ///< `std::string`.
    Object,   ///< `boost::json::object`.
    Array,    ///< `boost::json::array`.
    RawBuffer ///< `RawBuffer`.
};
BROOKESIA_DESCRIBE_ENUM(FunctionValueType, Boolean, Number, String, Object, Array, RawBuffer);

/**
 * @brief Variant container used for function parameter values and return data.
 *
 * Arithmetic types other than `bool` and `double` are converted to `double`.
 */
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

/**
 * @brief Named function-parameter map keyed by schema parameter name.
 */
using FunctionParameterMap = std::map<std::string /* name */, FunctionValue /* value */>;

/**
 * @brief Schema description for one function parameter.
 */
struct FunctionParameterSchema {
    std::string name;
    std::string description = "";
    FunctionValueType type = FunctionValueType::String;
    std::optional<FunctionValue> default_value = std::nullopt;

    /**
     * @brief Check whether a runtime value matches the schema type.
     *
     * @param value Runtime function value to validate.
     * @return `true` when the value type is compatible with this schema item.
     */
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
    /**
     * @brief Check whether the parameter is required.
     *
     * @return `true` when no default value is provided.
     */
    bool is_required() const
    {
        return !default_value.has_value();
    }
};
BROOKESIA_DESCRIBE_STRUCT(FunctionParameterSchema, (), (name, description, type, default_value));

/**
 * @brief Schema description for one callable service function.
 */
struct FunctionSchema {
    std::string name;
    std::string description = "";
    std::vector<FunctionParameterSchema> parameters = {};
    bool require_scheduler = true;
};
BROOKESIA_DESCRIBE_STRUCT(FunctionSchema, (), (name, description, parameters, require_scheduler));

/**
 * @brief Standard result wrapper returned by service function calls.
 */
struct FunctionResult {
    bool success = false;
    std::string error_message = "";
    std::optional<FunctionValue> data = std::nullopt;

    /**
     * @brief Check whether the result contains payload data.
     *
     * @return `true` when `data` has a value.
     */
    bool has_data() const
    {
        return data.has_value();
    }

    /**
     * @brief Get the data as a specific type.
     *
     * @tparam T The type to convert to.
     * @return The data as the specified type.
     */
    template<typename T>
    requires ConvertibleToFunctionValue<T>
    T &get_data()
    {
        return std::get<T>(data.value());
    }
};
BROOKESIA_DESCRIBE_STRUCT(FunctionResult, (), (success, error_message, data));

} // namespace esp_brookesia::service
