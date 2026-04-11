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

/**
 * @brief Expression service that renders emojis, animations, QR codes, and event messages.
 */
class Emote: public service::ServiceBase {
public:
    /**
     * @brief Event-message type alias re-exported from the helper schema.
     */
    using AssetMessageType = service::helper::ExpressionEmote::EventMessageType;
    /**
     * @brief Asset-source type alias re-exported from the helper schema.
     */
    using AssetSourceType = service::helper::ExpressionEmote::AssetSourceType;
    /**
     * @brief Asset-source descriptor alias re-exported from the helper schema.
     */
    using AssetSource = service::helper::ExpressionEmote::AssetSource;
    /**
     * @brief Configuration type alias re-exported from the helper schema.
     */
    using Config = service::helper::ExpressionEmote::Config;

    /**
     * @brief Native graphics object types managed by the emote service.
     */
    enum class GFX_ObjectType {
        Emoji,
        Animation,
        Qrcode,
        Max,
    };

    Emote(const Emote &) = delete;
    Emote(Emote &&) = delete;
    Emote &operator=(const Emote &) = delete;
    Emote &operator=(Emote &&) = delete;

    /**
     * @brief Notify the native backend that the previous flush operation finished.
     *
     * @return `true` on success, or `false` otherwise.
     */
    bool native_notify_flush_finished();

    /**
     * @brief Get the process-wide singleton instance.
     *
     * @return Reference to the singleton emote service.
     */
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
    std::expected<void, std::string> function_hide_emoji();
    std::expected<void, std::string> function_set_animation(const std::string &animation);
    std::expected<void, std::string> function_insert_animation(const std::string &animation, double duration_ms);
    std::expected<void, std::string> function_stop_animation();
    std::expected<void, std::string> function_wait_animation_frame_done(double timeout_ms);
    std::expected<void, std::string> function_set_event_msg(const std::string &event, const std::string &message);
    std::expected<void, std::string> function_hide_event_message();
    std::expected<void, std::string> function_set_qrcode(const std::string &qrcode);
    std::expected<void, std::string> function_hide_qrcode();
    std::expected<void, std::string> function_notify_flush_finished();
    std::expected<void, std::string> function_refresh_all();

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
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::HideEmoji,
                function_hide_emoji()
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
                Helper, Helper::FunctionId::HideEventMessage,
                function_hide_event_message()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::SetQrcode, std::string,
                function_set_qrcode(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::HideQrcode,
                function_hide_qrcode()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::NotifyFlushFinished,
                function_notify_flush_finished()
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::RefreshAll,
                function_refresh_all()
            ),
        };
    }

    bool set_obj_visible(GFX_ObjectType type, bool visible);

    bool is_configured() const
    {
        return is_configured_;
    }

    Config config_{};

    bool is_configured_ = false;

    void *native_handle_ = nullptr;
};

BROOKESIA_DESCRIBE_ENUM(Emote::GFX_ObjectType, Emoji, Animation, Qrcode, Max);

} // namespace esp_brookesia::expression
