/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_helper.hpp"

using namespace esp_brookesia::service;
using namespace esp_brookesia::service::helper;

namespace {

struct HelperSchemaStats {
    size_t helper_count = 0;
    size_t function_count = 0;
    size_t event_count = 0;
};

void assert_function_schemas(std::string_view helper_name, std::span<const FunctionSchema> schemas)
{
    TEST_ASSERT_FALSE_MESSAGE(helper_name.empty(), "helper name must not be empty");
    TEST_ASSERT_FALSE_MESSAGE(schemas.empty(), "helper must expose at least one function schema");
    for (const auto &schema : schemas) {
        TEST_ASSERT_FALSE_MESSAGE(schema.name.empty(), "function schema name must not be empty");
        for (const auto &parameter : schema.parameters) {
            TEST_ASSERT_FALSE_MESSAGE(parameter.name.empty(), "function parameter name must not be empty");
        }
    }
}

void assert_event_schemas(std::span<const EventSchema> schemas)
{
    for (const auto &schema : schemas) {
        TEST_ASSERT_FALSE_MESSAGE(schema.name.empty(), "event schema name must not be empty");
        for (const auto &item : schema.items) {
            TEST_ASSERT_FALSE_MESSAGE(item.name.empty(), "event item name must not be empty");
        }
    }
}

template <typename Helper>
void accumulate_helper(HelperSchemaStats &stats)
{
    const auto name = Helper::get_name();
    const auto function_schemas = Helper::get_function_schemas();
    const auto event_schemas = Helper::get_event_schemas();
    assert_function_schemas(name, function_schemas);
    assert_event_schemas(event_schemas);
    stats.helper_count++;
    stats.function_count += function_schemas.size();
    stats.event_count += event_schemas.size();
}

boost::json::value serialize_and_parse(const auto &value)
{
    return BROOKESIA_DESCRIBE_TO_JSON(value);
}

} // namespace

BROOKESIA_TEST_CASE(
    test_service_helper_all_public_helpers_expose_valid_schemas,
    "Service helper schemas are generated for every public helper",
    "[service][helper][schema]"
)
{
    HelperSchemaStats stats;

    accumulate_helper<AudioPlayback>(stats);
    accumulate_helper<AudioEncoder<0>>(stats);
    accumulate_helper<AudioDecoder<0>>(stats);
    accumulate_helper<Device>(stats);
    accumulate_helper<Display>(stats);
    accumulate_helper<Http>(stats);
    accumulate_helper<Nes>(stats);
    accumulate_helper<SNTP>(stats);
    accumulate_helper<Storage>(stats);
    accumulate_helper<Wifi>(stats);
    accumulate_helper<ExpressionEmote>(stats);
    accumulate_helper<VideoEncoder<0>>(stats);
    accumulate_helper<VideoDecoder<0>>(stats);
    accumulate_helper<Manager>(stats);
    accumulate_helper<Coze>(stats);
    accumulate_helper<Openai>(stats);
    accumulate_helper<XiaoZhi>(stats);

    TEST_ASSERT_EQUAL_size_t(17, stats.helper_count);
    TEST_ASSERT_GREATER_THAN(60, stats.function_count);
    TEST_ASSERT_GREATER_THAN(20, stats.event_count);
}

BROOKESIA_TEST_CASE(
    test_service_helper_names_and_enum_descriptions_are_stable,
    "Service helper public names and described enums are stable",
    "[service][helper][describe]"
)
{
    TEST_ASSERT_EQUAL_STRING("AudioPlayback", AudioPlayback::get_name().data());
    TEST_ASSERT_EQUAL_STRING("AudioEncoder0", AudioEncoder<0>::get_name().data());
    TEST_ASSERT_EQUAL_STRING("AudioDecoder0", AudioDecoder<0>::get_name().data());
    TEST_ASSERT_EQUAL_STRING("VideoEncoder0", VideoEncoder<0>::get_name().data());
    TEST_ASSERT_EQUAL_STRING("VideoDecoder0", VideoDecoder<0>::get_name().data());
    TEST_ASSERT_EQUAL_STRING("Wifi", Wifi::get_name().data());
    TEST_ASSERT_EQUAL_STRING("AgentManager", Manager::get_name().data());
    TEST_ASSERT_EQUAL_STRING("Coze", Coze::get_name().data());
    TEST_ASSERT_EQUAL_STRING("OpenAI", Openai::get_name().data());
    TEST_ASSERT_EQUAL_STRING("XiaoZhi", XiaoZhi::get_name().data());

    TEST_ASSERT_EQUAL_STRING("Connect", BROOKESIA_DESCRIBE_ENUM_TO_STR(Wifi::GeneralAction::Connect).c_str());
    TEST_ASSERT_EQUAL_STRING("Request", BROOKESIA_DESCRIBE_ENUM_TO_STR(Http::FunctionId::Request).c_str());
    TEST_ASSERT_EQUAL_STRING("Open", BROOKESIA_DESCRIBE_ENUM_TO_STR(Video::EncoderFunctionId::Open).c_str());
    TEST_ASSERT_EQUAL_STRING("Activate", BROOKESIA_DESCRIBE_ENUM_TO_STR(Manager::GeneralAction::Activate).c_str());
}

BROOKESIA_TEST_CASE(
    test_service_helper_described_payload_roundtrips,
    "Service helper described payloads serialize and deserialize",
    "[service][helper][payload]"
)
{
    Video::EncoderConfig encoder_config = {
        .sinks = {
            {
                .format = Video::EncoderSinkFormat::MJPEG,
                .width = 320,
                .height = 240,
                .fps = 15,
            },
        },
        .enable_stream_mode = true,
        .display = Video::EncoderDisplayConfig{
            .output_name = "main",
            .source_name = "video",
            .source_role = "preview",
            .x = 4,
            .y = 8,
            .draw_timeout_ms = 100,
            .publish_sink_event = true,
            .activate_source = true,
            .sink_index = 0,
        },
    };
    auto encoder_json = serialize_and_parse(encoder_config);
    Video::EncoderConfig parsed_encoder_config;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(encoder_json, parsed_encoder_config));
    TEST_ASSERT_EQUAL_size_t(1, parsed_encoder_config.sinks.size());
    TEST_ASSERT_TRUE(parsed_encoder_config.enable_stream_mode);
    TEST_ASSERT_TRUE(parsed_encoder_config.display.has_value());
    TEST_ASSERT_EQUAL_STRING("main", parsed_encoder_config.display->output_name.c_str());

    Storage::KvNameResult kv_name = {
        .name = "app.theme",
        .original_name = "theme",
        .hashed = false,
        .warning = "",
    };
    auto kv_json = serialize_and_parse(kv_name);
    Storage::KvNameResult parsed_kv_name;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(kv_json, parsed_kv_name));
    TEST_ASSERT_EQUAL_STRING(kv_name.name.c_str(), parsed_kv_name.name.c_str());
    TEST_ASSERT_EQUAL_STRING(kv_name.original_name.c_str(), parsed_kv_name.original_name.c_str());
}
