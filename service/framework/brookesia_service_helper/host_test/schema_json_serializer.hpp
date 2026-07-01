/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include "boost/json.hpp"
#include "brookesia/service_manager/common.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/event/definition.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace brookesia_host {

using esp_brookesia::service::EventItemSchema;
using esp_brookesia::service::EventSchema;
using esp_brookesia::service::FunctionParameterSchema;
using esp_brookesia::service::FunctionSchema;
using esp_brookesia::service::FunctionValue;
using esp_brookesia::service::RawBuffer;

template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

inline boost::json::value serialize_raw_buffer(const RawBuffer &buffer)
{
    return boost::json::parse(BROOKESIA_DESCRIBE_JSON_SERIALIZE(buffer));
}

inline boost::json::value serialize_function_value(const FunctionValue &value)
{
    return std::visit(
    Overloaded{
        [](bool current) -> boost::json::value { return current; },
        [](double current) -> boost::json::value { return current; },
        [](const std::string & current) -> boost::json::value { return boost::json::value(current); },
        [](const boost::json::object & current) -> boost::json::value { return current; },
        [](const boost::json::array & current) -> boost::json::value { return current; },
        [](const RawBuffer & current) -> boost::json::value { return serialize_raw_buffer(current); },
    },
    static_cast<const FunctionValue::Base &>(value)
           );
}

inline boost::json::value serialize_default_value(const std::optional<FunctionValue> &value)
{
    if (!value.has_value()) {
        return nullptr;
    }
    boost::json::value json_value = serialize_function_value(value.value());
    boost::json::object result;
    result["json"] = json_value;
    result["display"] = boost::json::serialize(json_value);
    return result;
}

inline boost::json::object serialize_parameter(const FunctionParameterSchema &parameter)
{
    boost::json::object result;
    result["name"] = parameter.name;
    result["description"] = parameter.description;
    result["type"] = BROOKESIA_DESCRIBE_ENUM_TO_STR(parameter.type);
    result["required"] = parameter.is_required();
    result["default_value"] = serialize_default_value(parameter.default_value);
    return result;
}

inline boost::json::array serialize_functions(std::span<const FunctionSchema> schemas)
{
    boost::json::array result;
    result.reserve(schemas.size());
    for (const auto &schema : schemas) {
        boost::json::object schema_obj;
        schema_obj["name"] = schema.name;
        schema_obj["description"] = schema.description;
        schema_obj["require_scheduler"] = schema.require_scheduler;

        boost::json::array parameters;
        parameters.reserve(schema.parameters.size());
        for (const auto &parameter : schema.parameters) {
            parameters.emplace_back(serialize_parameter(parameter));
        }
        schema_obj["parameters"] = std::move(parameters);
        result.emplace_back(std::move(schema_obj));
    }
    return result;
}

inline boost::json::object serialize_event_item(const EventItemSchema &item)
{
    boost::json::object result;
    result["name"] = item.name;
    result["description"] = item.description;
    result["type"] = BROOKESIA_DESCRIBE_ENUM_TO_STR(item.type);
    return result;
}

inline boost::json::array serialize_events(std::span<const EventSchema> schemas)
{
    boost::json::array result;
    result.reserve(schemas.size());
    for (const auto &schema : schemas) {
        boost::json::object schema_obj;
        schema_obj["name"] = schema.name;
        schema_obj["description"] = schema.description;
        schema_obj["require_scheduler"] = schema.require_scheduler;

        boost::json::array items;
        items.reserve(schema.items.size());
        for (const auto &item : schema.items) {
            items.emplace_back(serialize_event_item(item));
        }
        schema_obj["items"] = std::move(items);
        result.emplace_back(std::move(schema_obj));
    }
    return result;
}

inline void write_json_file(const std::filesystem::path &path, const boost::json::value &value)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path);
    if (!stream) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }
    stream << boost::json::serialize(value) << std::endl;
}

inline boost::json::array describe_to_json_array(std::span<const FunctionSchema> schemas)
{
    boost::json::array array;
    array.reserve(schemas.size());
    for (const auto &schema : schemas) {
        array.emplace_back(BROOKESIA_DESCRIBE_TO_JSON(schema));
    }
    return array;
}

inline boost::json::array describe_to_json_array(std::span<const EventSchema> schemas)
{
    boost::json::array array;
    array.reserve(schemas.size());
    for (const auto &schema : schemas) {
        array.emplace_back(BROOKESIA_DESCRIBE_TO_JSON(schema));
    }
    return array;
}

inline void append_helper_schema_dump(
    boost::json::array &helpers, std::string_view helper_name, std::span<const FunctionSchema> function_schemas,
    std::span<const EventSchema> event_schemas
)
{
    boost::json::object helper_object;
    helper_object["helper"] = helper_name;
    helper_object["function_count"] = static_cast<std::uint64_t>(function_schemas.size());
    helper_object["event_count"] = static_cast<std::uint64_t>(event_schemas.size());
    helper_object["functions"] = describe_to_json_array(function_schemas);
    helper_object["events"] = describe_to_json_array(event_schemas);
    helpers.emplace_back(std::move(helper_object));
}

} // namespace brookesia_host
