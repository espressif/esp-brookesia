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
#include "brookesia/service_helper.hpp"
#include "schema_json_serializer.hpp"

using namespace esp_brookesia::service;
using namespace esp_brookesia::service::helper;
using brookesia_host::append_helper_schema_dump;

int main()
{
    boost::json::array helpers;
    helpers.reserve(17);
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

    append_dump_with_guard(
        AudioPlayback::get_name(), AudioPlayback::get_function_schemas, AudioPlayback::get_event_schemas
    );
    append_dump_with_guard(
        AudioEncoder<0>::get_name(), AudioEncoder<0>::get_function_schemas, AudioEncoder<0>::get_event_schemas
    );
    append_dump_with_guard(
        AudioDecoder<0>::get_name(), AudioDecoder<0>::get_function_schemas, AudioDecoder<0>::get_event_schemas
    );
    append_dump_with_guard(Device::get_name(), Device::get_function_schemas, Device::get_event_schemas);
    append_dump_with_guard(Display::get_name(), Display::get_function_schemas, Display::get_event_schemas);
    append_dump_with_guard(Http::get_name(), Http::get_function_schemas, Http::get_event_schemas);
    append_dump_with_guard(Nes::get_name(), Nes::get_function_schemas, Nes::get_event_schemas);
    append_dump_with_guard(SNTP::get_name(), SNTP::get_function_schemas, SNTP::get_event_schemas);
    append_dump_with_guard(Storage::get_name(), Storage::get_function_schemas, Storage::get_event_schemas);
    append_dump_with_guard(Wifi::get_name(), Wifi::get_function_schemas, Wifi::get_event_schemas);
    append_dump_with_guard(
        ExpressionEmote::get_name(), ExpressionEmote::get_function_schemas, ExpressionEmote::get_event_schemas
    );
    append_dump_with_guard(
        VideoEncoder<0>::get_name(), VideoEncoder<0>::get_function_schemas, VideoEncoder<0>::get_event_schemas
    );
    append_dump_with_guard(
        VideoDecoder<0>::get_name(), VideoDecoder<0>::get_function_schemas, VideoDecoder<0>::get_event_schemas
    );
    append_dump_with_guard(Manager::get_name(), Manager::get_function_schemas, Manager::get_event_schemas);
    append_dump_with_guard(Coze::get_name(), Coze::get_function_schemas, Coze::get_event_schemas);
    append_dump_with_guard(Openai::get_name(), Openai::get_function_schemas, Openai::get_event_schemas);
    append_dump_with_guard(XiaoZhi::get_name(), XiaoZhi::get_function_schemas, XiaoZhi::get_event_schemas);

    boost::json::object root;
    root["helper_count"] = static_cast<std::uint64_t>(helpers.size());
    root["total_function_count"] = static_cast<std::uint64_t>(total_function_count);
    root["total_event_count"] = static_cast<std::uint64_t>(total_event_count);
    root["helper_error_count"] = static_cast<std::uint64_t>(helper_error_count);
    root["helpers"] = std::move(helpers);

    std::cout << boost::json::serialize(root) << std::endl;
    return (helper_error_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
