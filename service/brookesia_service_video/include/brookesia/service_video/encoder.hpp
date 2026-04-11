/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_helper/video.hpp"
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
     * @param device_path Device path used to open the encoder.
     */
    VideoEncoder(int id, std::string device_path)
        : ServiceBase({
        .name = std::string(helper::Video::ENCODER_NAME_PREFIX) + std::to_string(id),
    })
    , device_path_(std::move(device_path))
    {}
    /**
     * @brief Destroy the encoder service wrapper.
     */
    ~VideoEncoder() = default;

    /**
     * @brief Get the device path associated with this encoder instance.
     *
     * @return Encoder device path.
     */
    const std::string &get_device_path() const
    {
        return device_path_;
    }

private:
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

    bool close_internal();
    bool stop_internal();

    bool is_opened()
    {
        return capture_handle_ != nullptr;
    }

    bool is_started()
    {
        return is_started_;
    }

    std::string device_path_;

    BaseHelper::EncoderConfig encoder_cfg_;
    bool is_started_ = false;
    void *capture_handle_ = nullptr;
};

} // namespace esp_brookesia::service
