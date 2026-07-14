/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <mutex>

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
 * @brief Service wrapper for one video-encoder device instance.
 */
class VideoEncoder : public ServiceBase {
public:
    /**
     * @brief Construct an encoder service bound to one device path.
     *
     * @param id Encoder instance index appended to the service name.
     */
    explicit VideoEncoder(int id)
        : ServiceBase({
        .name = std::string(helper::Video::ENCODER_NAME_PREFIX) + std::to_string(id),
        .description = "Encode video frames for one configured output instance.",
        .version = get_component_version(),
    })
    , id_(id)
    {}
    /**
     * @brief Destroy the encoder service wrapper.
     */
    ~VideoEncoder() = default;

private:
    static std::string get_component_version();

    using BaseHelper = helper::Video;
    // Use a fixed id to represent the encoder
    using Helper = helper::VideoEncoder<0>;

    bool on_init() override;
    void on_stop() override;

    std::expected<void, std::string> function_open(const boost::json::object &config);
    std::expected<void, std::string> function_close();
    std::expected<void, std::string> function_start();
    std::expected<void, std::string> function_stop();
    std::expected<void, std::string> function_fetch_frame(double sink_index);

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
                Helper, Helper::FunctionId::FetchFrame, double,
                function_fetch_frame(PARAM)
            ),
        };
    }

    std::expected<void, std::string> close_internal(uint32_t timeout_ms);
    std::expected<void, std::string> stop_internal(uint32_t timeout_ms);
    void cancel_pending_calls();
    void close_locked();
    bool stop_locked();
    std::expected<void, std::string> setup_display_output(BaseHelper::EncoderConfig &encoder_cfg);
    void clear_display_output();
    void on_encoder_frame(
        Helper::EventId event_id, size_t sink_index, const BaseHelper::EncoderSinkInfo &sink_info,
        const uint8_t *data, size_t size
    );
    hal::video::EncoderConfig to_hal_config(const BaseHelper::EncoderConfig &encoder_cfg) const;
    std::expected<BaseHelper::EncoderSinkFormat, std::string> to_encoder_sink_format(
        Display::PixelFormat pixel_format
    ) const;

    bool is_opened()
    {
        return encoder_iface_ && encoder_iface_->is_opened();
    }

    bool is_started()
    {
        return encoder_iface_ && encoder_iface_->is_started();
    }

    int id_ = 0;
    std::timed_mutex operation_mutex_;
    std::atomic<bool> closing_requested_ = false;
    hal::InterfaceHandle<hal::video::EncoderIface> encoder_iface_;
    BaseHelper::EncoderConfig encoder_cfg_;
    ServiceBinding display_binding_;
    uint32_t display_source_id_ = 0;
    std::string display_output_name_;
    Display::PixelFormat display_pixel_format_ = Display::PixelFormat::Max;
    uint32_t display_x_ = 0;
    uint32_t display_y_ = 0;
    uint32_t display_draw_timeout_ms_ = 0;
    uint32_t display_sink_index_ = 0;
    uint32_t display_present_warning_count_ = 0;
    bool publish_sink_event_ = true;
};

} // namespace esp_brookesia::service
