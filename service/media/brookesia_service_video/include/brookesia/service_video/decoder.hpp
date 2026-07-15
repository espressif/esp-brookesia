/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"
#include "brookesia/hal_interface/interfaces/video/processor.hpp"
#include "brookesia/service_display.hpp"
#include "brookesia/service_helper/media/video.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/macro_configs.h"
#include "brookesia/service_video/macro_configs.h"

namespace esp_brookesia::service {

/**
 * @brief Service wrapper for one video-decoder device instance.
 */
class VideoDecoder : public ServiceBase {
public:
    /**
     * @brief Construct a decoder service bound to one logical decoder slot.
     *
     * @param id Decoder instance index appended to the service name.
     */
    VideoDecoder(size_t id)
        : ServiceBase({
        .name = std::string(helper::Video::DECODER_NAME_PREFIX) + std::to_string(id),
        .description = "Decode video streams for one configured output instance.",
        .version = get_component_version(),
    })
    , id_(id)
    {}
    /**
     * @brief Destroy the decoder service wrapper.
     */
    ~VideoDecoder() = default;

private:
    static std::string get_component_version();

    using BaseHelper = helper::Video;
    // Use a fixed id to represent the decoder
    using Helper = helper::VideoDecoder<0>;

    bool on_init() override;
    void on_stop() override;

    std::expected<void, std::string> function_open(const boost::json::object &config);
    std::expected<void, std::string> function_close();
    std::expected<void, std::string> function_start();
    std::expected<void, std::string> function_stop();
    std::expected<void, std::string> function_feed_frame(const RawBuffer &frame);

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::Open, boost::json::object,
                function_open(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Close,
                function_close()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Start,
                function_start()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Stop,
                function_stop()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::FeedFrame, RawBuffer,
                function_feed_frame(PARAM)
            ),
        };
    }

    bool close_internal();
    bool stop_internal();
    std::expected<void, std::string> setup_display_output(BaseHelper::DecoderConfig &decoder_cfg);
    void clear_display_output();
    void on_decoded_frame(uint16_t width, uint16_t height, const uint8_t *data, size_t size);
    hal::video::DecoderConfig to_hal_config(const BaseHelper::DecoderConfig &decoder_cfg) const;
    std::expected<BaseHelper::DecoderSinkFormat, std::string> to_decoder_sink_format(
        Display::PixelFormat pixel_format
    ) const;

    bool is_opened()
    {
        return decoder_iface_ && decoder_iface_->is_opened();
    }

    bool is_started()
    {
        return decoder_iface_ && decoder_iface_->is_started();
    }

    size_t id_ = 0;
    hal::InterfaceHandle<hal::video::DecoderIface> decoder_iface_;
    BaseHelper::DecoderConfig decoder_cfg_;
    ServiceBinding display_binding_;
    uint32_t display_source_id_ = 0;
    std::string display_output_name_;
    Display::PixelFormat display_pixel_format_ = Display::PixelFormat::Max;
    uint32_t display_x_ = 0;
    uint32_t display_y_ = 0;
    uint32_t display_draw_timeout_ms_ = 0;
    bool publish_sink_event_ = true;
};

} // namespace esp_brookesia::service
