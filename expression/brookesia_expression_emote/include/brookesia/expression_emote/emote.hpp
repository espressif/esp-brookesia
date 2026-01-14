/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/service_helper/expression/emote.hpp"
#include "brookesia/service_manager/service/base.hpp"

namespace esp_brookesia::expression {

class Emote: public service::ServiceBase {
public:
    using AssetMessageType = service::helper::ExpressionEmote::EventMessageType;
    using AssetSourceType = service::helper::ExpressionEmote::AssetSourceType;
    using AssetSource = service::helper::ExpressionEmote::AssetSource;
    using Config = service::helper::ExpressionEmote::Config;

    Emote(const Emote &) = delete;
    Emote(Emote &&) = delete;
    Emote &operator=(const Emote &) = delete;
    Emote &operator=(Emote &&) = delete;

    bool native_notify_flush_finished();

    static Emote &get_instance()
    {
        static Emote instance;
        return instance;
    }

private:
    using Helper = service::helper::ExpressionEmote;

    Emote()
        : service::ServiceBase({
        .name = Helper::get_name().data(),
    })
    {}
    ~Emote() = default;

    bool on_init() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<void, std::string> function_set_config(const boost::json::object &config);
    std::expected<void, std::string> function_load_assets_source(const boost::json::object &source);
    std::expected<void, std::string> function_set_emoji(const std::string &emoji);
    std::expected<void, std::string> function_set_animation(const std::string &animation);
    std::expected<void, std::string> function_insert_animation(const std::string &animation, double duration_ms);
    std::expected<void, std::string> function_stop_animation();
    std::expected<void, std::string> function_wait_animation_frame_done(double timeout_ms);
    std::expected<void, std::string> function_set_event_msg(const std::string &event, const std::string &message);
    std::expected<void, std::string> function_notify_flush_finished();

    std::vector<service::FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<service::FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<service::EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<service::EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    service::ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetConfig, boost::json::object,
                function_set_config(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::LoadAssetsSource, boost::json::object,
                function_load_assets_source(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetEmoji, std::string,
                function_set_emoji(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetAnimation, std::string,
                function_set_animation(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::InsertAnimation, std::string, double,
                function_insert_animation(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::StopAnimation,
                function_stop_animation()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::WaitAnimationFrameDone, double,
                function_wait_animation_frame_done(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_2(
                Helper, Helper::FunctionId::SetEventMessage, std::string, std::string,
                function_set_event_msg(PARAM1, PARAM2)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::NotifyFlushFinished,
                function_notify_flush_finished()
            ),
        };
    }

    bool is_configured() const
    {
        return is_configured_;
    }

    bool is_configured_ = false;
    Config config_{};

    void *native_handle_ = nullptr;
};

} // namespace esp_brookesia::expression
