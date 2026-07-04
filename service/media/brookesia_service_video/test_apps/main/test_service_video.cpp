/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <cstdint>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_video.hpp"

using namespace esp_brookesia;
using VideoHelper = service::helper::Video;
using EncoderHelper = service::helper::VideoEncoder<0>;
using DecoderHelper = service::helper::VideoDecoder<0>;

namespace {

service::ServiceBinding encoder_binding;
service::ServiceBinding decoder_binding;

bool startup()
{
    auto &manager = service::ServiceManager::get_instance();
    if (!manager.start()) {
        return false;
    }
    encoder_binding = manager.bind(EncoderHelper::get_name().data());
    decoder_binding = manager.bind(DecoderHelper::get_name().data());
    return encoder_binding.is_valid() && decoder_binding.is_valid();
}

void shutdown()
{
    decoder_binding.release();
    encoder_binding.release();
    service::ServiceManager::get_instance().deinit();
}

boost::json::object make_encoder_open_config()
{
    VideoHelper::EncoderConfig config = {
        .sinks = {
            {
                .format = VideoHelper::EncoderSinkFormat::MJPEG,
                .width = 320,
                .height = 240,
                .fps = 15,
            },
        },
        .enable_stream_mode = false,
    };
    return BROOKESIA_DESCRIBE_TO_JSON(config).as_object();
}

boost::json::object make_decoder_open_config()
{
    VideoHelper::DecoderConfig config = {
        .width = 320,
        .height = 240,
        .source_format = VideoHelper::DecoderSourceFormat::MJPEG,
        .sink_format = VideoHelper::DecoderSinkFormat::RGB565_LE,
        .enable_stream_mode = false,
        .enable_hw_acceleration = false,
    };
    return BROOKESIA_DESCRIBE_TO_JSON(config).as_object();
}

} // namespace

BROOKESIA_TEST_CASE(
    test_service_video_plugins_bind_and_expose_schemas,
    "Service video plugins bind and expose helper schemas",
    "[service][video][schema]"
)
{
    TEST_ASSERT_TRUE(startup());
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    TEST_ASSERT_TRUE(EncoderHelper::is_available());
    TEST_ASSERT_TRUE(DecoderHelper::is_available());
    TEST_ASSERT_TRUE(EncoderHelper::is_running());
    TEST_ASSERT_TRUE(DecoderHelper::is_running());

    TEST_ASSERT_EQUAL_size_t(
        static_cast<size_t>(VideoHelper::EncoderFunctionId::Max),
        EncoderHelper::get_function_schemas().size()
    );
    TEST_ASSERT_EQUAL_size_t(
        static_cast<size_t>(VideoHelper::DecoderFunctionId::Max),
        DecoderHelper::get_function_schemas().size()
    );
    TEST_ASSERT_NOT_NULL(EncoderHelper::get_function_schema(VideoHelper::EncoderFunctionId::Open));
    TEST_ASSERT_NOT_NULL(DecoderHelper::get_function_schema(VideoHelper::DecoderFunctionId::FeedFrame));
    TEST_ASSERT_NOT_NULL(EncoderHelper::get_event_schema(VideoHelper::EncoderEventId::FetchSinkFrameReady));
    TEST_ASSERT_NOT_NULL(DecoderHelper::get_event_schema(VideoHelper::DecoderEventId::SinkFrameReady));
}

BROOKESIA_TEST_CASE(
    test_service_video_encoder_lifecycle_errors_are_reported,
    "Service video encoder reports lifecycle and missing HAL errors",
    "[service][video][encoder]"
)
{
    TEST_ASSERT_TRUE(startup());
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto start_before_open = EncoderHelper::call_function_sync<void>(VideoHelper::EncoderFunctionId::Start);
    TEST_ASSERT_FALSE(start_before_open.has_value());

    auto stop_before_open = EncoderHelper::call_function_sync<void>(VideoHelper::EncoderFunctionId::Stop);
    TEST_ASSERT_TRUE(stop_before_open.has_value());

    auto fetch_before_start = EncoderHelper::call_function_sync<void>(
                                  VideoHelper::EncoderFunctionId::FetchFrame, 0.0
                              );
    TEST_ASSERT_FALSE(fetch_before_start.has_value());

    auto open_result = EncoderHelper::call_function_sync<void>(
                           VideoHelper::EncoderFunctionId::Open, make_encoder_open_config()
                       );
    TEST_ASSERT_FALSE(open_result.has_value());

    auto close_result = EncoderHelper::call_function_sync<void>(VideoHelper::EncoderFunctionId::Close);
    TEST_ASSERT_TRUE(close_result.has_value());
}

BROOKESIA_TEST_CASE(
    test_service_video_decoder_lifecycle_errors_are_reported,
    "Service video decoder reports lifecycle and missing HAL errors",
    "[service][video][decoder]"
)
{
    TEST_ASSERT_TRUE(startup());
    lib_utils::FunctionGuard shutdown_guard([]() {
        shutdown();
    });

    auto start_before_open = DecoderHelper::call_function_sync<void>(VideoHelper::DecoderFunctionId::Start);
    TEST_ASSERT_FALSE(start_before_open.has_value());

    auto stop_before_open = DecoderHelper::call_function_sync<void>(VideoHelper::DecoderFunctionId::Stop);
    TEST_ASSERT_TRUE(stop_before_open.has_value());

    std::vector<uint8_t> frame_data = {0x01, 0x02, 0x03, 0x04};
    service::RawBuffer frame(frame_data.data(), frame_data.size());
    auto feed_before_start = DecoderHelper::call_function_sync<void>(
                                 VideoHelper::DecoderFunctionId::FeedFrame, frame
                             );
    TEST_ASSERT_FALSE(feed_before_start.has_value());

    auto open_result = DecoderHelper::call_function_sync<void>(
                           VideoHelper::DecoderFunctionId::Open, make_decoder_open_config()
                       );
    TEST_ASSERT_FALSE(open_result.has_value());

    auto close_result = DecoderHelper::call_function_sync<void>(VideoHelper::DecoderFunctionId::Close);
    TEST_ASSERT_TRUE(close_result.has_value());
}
