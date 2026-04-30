/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdlib>
#include <cstdint>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>
#include "boost/json.hpp"
#include "schema_json_serializer.hpp"
#include "brookesia/service_helper.hpp"

using namespace esp_brookesia::service;
using namespace esp_brookesia::service::helper;
using brookesia_host::append_helper_schema_dump;

int main()
{
    boost::json::array helpers;
    helpers.reserve(8);
    std::size_t total_function_count = 0;
    std::size_t total_event_count = 0;
    std::size_t helper_error_count = 0;

    auto append_dump_with_guard = [&](std::string_view helper_name, auto function_getter, auto event_getter) {
        try {
            const auto function_schemas = function_getter();
            const auto event_schemas = event_getter();
            total_function_count += function_schemas.size();
            total_event_count += event_schemas.size();
            append_helper_schema_dump(helpers, helper_name, function_schemas, event_schemas);
        } catch (const std::exception &e) {
            boost::json::object helper_object;
            helper_object["helper"] = helper_name;
            helper_object["error"] = std::string(e.what());
            helpers.emplace_back(std::move(helper_object));
            helper_error_count++;
        }
    };

    append_dump_with_guard("Audio", Audio::get_function_schemas, Audio::get_event_schemas);
    append_dump_with_guard("Device", Device::get_function_schemas, Device::get_event_schemas);
    append_dump_with_guard("NVS", NVS::get_function_schemas, NVS::get_event_schemas);
    append_dump_with_guard("SNTP", SNTP::get_function_schemas, SNTP::get_event_schemas);
    append_dump_with_guard("Wifi", Wifi::get_function_schemas, Wifi::get_event_schemas);
    append_dump_with_guard("ExpressionEmote", ExpressionEmote::get_function_schemas, ExpressionEmote::get_event_schemas);
    append_dump_with_guard("VideoEncoder", Video::get_encoder_function_schemas, Video::get_encoder_event_schemas);
    append_dump_with_guard("VideoDecoder", Video::get_decoder_function_schemas, Video::get_decoder_event_schemas);

    boost::json::object root;
    root["helper_count"] = static_cast<std::uint64_t>(helpers.size());
    root["total_function_count"] = static_cast<std::uint64_t>(total_function_count);
    root["total_event_count"] = static_cast<std::uint64_t>(total_event_count);
    root["helper_error_count"] = static_cast<std::uint64_t>(helper_error_count);
    root["helpers"] = std::move(helpers);

    std::cout << boost::json::serialize(root) << std::endl;
    return (helper_error_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
