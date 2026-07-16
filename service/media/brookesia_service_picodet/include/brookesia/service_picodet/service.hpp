/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/service_helper/media/picodet.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_picodet/detector.hpp"
#include "brookesia/service_picodet/macro_configs.h"

namespace esp_brookesia::service {

/**
 * @brief Service wrapper around one resident PicoDet detector.
 *
 * Maps the helper functions Open/Detect/Close/GetInfo onto a `picodet::PicoDetDetector`.
 * The model is loaded once on Open and reused across Detect calls until Close.
 */
class PicoDet : public ServiceBase {
public:
    PicoDet();
    ~PicoDet() override;

private:
    using Helper = helper::PicoDet;

    bool on_init() override;
    void on_stop() override;

    std::expected<boost::json::object, std::string> function_open(const boost::json::object &config);
    std::expected<boost::json::array, std::string> function_detect(const std::string &image_path);
    std::expected<void, std::string> function_close();
    std::expected<boost::json::object, std::string> function_get_info();
    std::expected<void, std::string> function_attach(const boost::json::object &config);
    std::expected<void, std::string> function_detach();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(schemas.begin(), schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(schemas.begin(), schemas.end());
    }

    FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::Open, boost::json::object,
                function_open(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::Detect, std::string,
                function_detect(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Close,
                function_close()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::GetInfo,
                function_get_info()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::Attach, boost::json::object,
                function_attach(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::Detach,
                function_detach()
            ),
        };
    }

    boost::json::object build_info_locked() const;
    void publish_detection_boxes(const std::vector<picodet::Box> &boxes);

    /// Pipeline wiring state (frame subscription + optional display presenting); see service.cpp.
    struct Pipeline;

    /// Per-frame body of the pipeline (ESP builds only); runs with mutex_ held.
    void on_pipeline_frame(Pipeline &pipeline, const EventItemMap &items);

    std::mutex mutex_;
    std::unique_ptr<picodet::PicoDetDetector> detector_;
    std::unique_ptr<Pipeline> pipeline_;
};

} // namespace esp_brookesia::service
