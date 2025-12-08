/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <vector>
#include <variant>
#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service {

enum class EventItemType {
    Boolean,
    Number,
    String,
    Object,
    Array,
};
BROOKESIA_DESCRIBE_ENUM(EventItemType, Boolean, Number, String, Object, Array)

using EventItem = std::variant <bool, double, std::string, boost::json::object, boost::json::array>;
using EventItemMap = std::map<std::string /* name */, EventItem /* value */>;

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
};
BROOKESIA_DESCRIBE_STRUCT(EventSchema, (), (name, description, items))

} // namespace esp_brookesia::service
