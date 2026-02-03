/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>
#include <variant>
#include <type_traits>
#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/common.hpp"

namespace esp_brookesia::service {

enum class EventItemType {
    Boolean,        // bool
    Number,         // double
    String,         // std::string
    Object,         // boost::json::object
    Array,          // boost::json::array
    RawBuffer,      // RawBuffer
};
BROOKESIA_DESCRIBE_ENUM(EventItemType, Boolean, Number, String, Object, Array, RawBuffer);

// EventItem: A variant that can hold different types of event item values
// Supports implicit conversion from any arithmetic type (except bool and double) to double
struct EventItem : std::variant<bool, double, std::string, boost::json::object, boost::json::array, RawBuffer> {
    using Base = std::variant<bool, double, std::string, boost::json::object, boost::json::array, RawBuffer>;

    // Inherit all constructors from base variant
    using Base::Base;

    // Template constructor for any arithmetic type (except bool and double) - converts to double
    template<typename T>
    requires (std::is_arithmetic_v<std::decay_t<T>> &&
              !std::is_same_v<std::decay_t<T>, bool> &&
              !std::is_same_v<std::decay_t<T>, double>)
    EventItem(T num) : Base(static_cast<double>(num)) {}
};

using EventItemMap = std::map<std::string /* name */, EventItem /* item */>;

struct EventItemSchema {
    std::string name;
    std::string description = "";
    EventItemType type = EventItemType::String;

    bool is_compatible_item(const EventItem &item) const
    {
        switch (type) {
        case EventItemType::Boolean:
            return std::holds_alternative<bool>(item);
        case EventItemType::Number:
            return std::holds_alternative<double>(item);
        case EventItemType::String:
            return std::holds_alternative<std::string>(item);
        case EventItemType::Object:
            return std::holds_alternative<boost::json::object>(item);
        case EventItemType::Array:
            return std::holds_alternative<boost::json::array>(item);
        case EventItemType::RawBuffer:
            return std::holds_alternative<RawBuffer>(item);
        default:
            return false;
        }
    }
};
BROOKESIA_DESCRIBE_STRUCT(EventItemSchema, (), (name, description, type))

struct EventSchema {
    std::string name;
    std::string description = "";
    std::vector<EventItemSchema> items = {};
    bool require_scheduler = true;
};
BROOKESIA_DESCRIBE_STRUCT(EventSchema, (), (name, description, items, require_scheduler))

} // namespace esp_brookesia::service
