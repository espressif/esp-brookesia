/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "gfx.h"
#include "expression_emote.h"
#include "brookesia/expression_emote/macro_configs.h"
#if !BROOKESIA_EXPRESSION_EMOTE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/expression_emote/emote.hpp"

namespace esp_brookesia::expression {

using EmoteHelper = service::helper::ExpressionEmote;

static bool get_native_source_data(const Emote::AssetSource &source, emote_data_t &native_data);
static bool get_native_message_event(const std::string &event, std::string &native_event);

inline static emote_handle_t get_native_handle(void *handle)
{
    return static_cast<emote_handle_t>(handle);
}

bool Emote::native_notify_flush_finished()
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return false;
    }

    emote_notify_flush_finished(get_native_handle(native_handle_));

    return true;
}

bool Emote::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_EXPRESSION_EMOTE_VER_MAJOR, BROOKESIA_EXPRESSION_EMOTE_VER_MINOR,
        BROOKESIA_EXPRESSION_EMOTE_VER_PATCH
    );

    return true;
}

bool Emote::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_configured(), false, "Emote is not configured");

    BROOKESIA_LOGI("Emote start with config: %1%", BROOKESIA_DESCRIBE_TO_STR(config_));

    auto flush_cb = +[](int x_start, int y_start, int x_end, int y_end, const void *data, emote_handle_t handle) {
        // BROOKESIA_LOG_TRACE_GUARD();

        auto *self = static_cast<Emote *>(emote_get_user_data(handle));
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid user data");

        EmoteHelper::FlushReadyEventParam param = {
            .x_start = x_start,
            .y_start = y_start,
            .x_end = x_end,
            .y_end = y_end,
            .data = data,
        };
        self->publish_event(BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventId::FlushReady), service::EventItemMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(EmoteHelper::EventFlushReadyParam::Param),
                BROOKESIA_DESCRIBE_TO_JSON(param).as_object(),
            },
        });
    };
    emote_config_t native_config = {
        .flags = {
            .swap = config_.flag_swap_color_bytes,
            .double_buffer = config_.flag_double_buffer,
            .buff_dma = config_.flag_buff_dma,
            .buff_spiram = config_.flag_buff_spiram,
        },
        .gfx_emote = {
            .h_res = static_cast<int>(config_.h_res),
            .v_res = static_cast<int>(config_.v_res),
            .fps = static_cast<int>(config_.fps),
        },
        .buffers = {
            .buf_pixels = config_.buf_pixels,
        },
        .task = {
            .task_priority = config_.task_priority,
            .task_stack = config_.task_stack,
            .task_affinity = config_.task_affinity,
            .task_stack_in_ext = config_.task_stack_in_ext,
        },
        .flush_cb = flush_cb,
        .update_cb = nullptr,
        .user_data = this,
    };
    native_handle_ = emote_init(&native_config);
    BROOKESIA_CHECK_NULL_RETURN(native_handle_, false, "Failed to initialize native emote");

    return true;
}

void Emote::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (native_handle_) {
        auto result = emote_deinit(get_native_handle(native_handle_));
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(result, {}, {
            BROOKESIA_LOGE("Failed to deinitialize native emote");
        });
        native_handle_ = nullptr;
    }

    return;
}

std::expected<void, std::string> Emote::function_set_config(const boost::json::object &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto result = BROOKESIA_DESCRIBE_FROM_JSON(config, config_);
    if (!result) {
        return std::unexpected("Invalid config: " + BROOKESIA_DESCRIBE_TO_STR(config));
    }

    BROOKESIA_LOGI("Set config: %1%", BROOKESIA_DESCRIBE_TO_STR(config_));

    is_configured_ = true;

    return {};
}

std::expected<void, std::string> Emote::function_load_assets_source(const boost::json::object &source)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    AssetSource source_data;
    emote_data_t native_data;
    auto result = BROOKESIA_DESCRIBE_FROM_JSON(source, source_data);
    if (!result || !get_native_source_data(source_data, native_data)) {
        return std::unexpected("Invalid source: " + BROOKESIA_DESCRIBE_TO_STR(source));
    }

    auto load_result = emote_mount_and_load_assets(get_native_handle(native_handle_), &native_data);
    if (load_result != ESP_OK) {
        return std::unexpected(
                   "Failed to load assets from source: " + BROOKESIA_DESCRIBE_TO_STR(source) + ", error: " +
                   std::string(esp_err_to_name(load_result))
               );
    }
    return {};
}

std::expected<void, std::string> Emote::function_set_emoji(const std::string &emoji)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto set_result = emote_set_anim_emoji(get_native_handle(native_handle_), emoji.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set emoji: " + BROOKESIA_DESCRIBE_TO_STR(emoji) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_set_animation(const std::string &animation)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto set_result = emote_set_dialog_anim(get_native_handle(native_handle_), animation.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set animation: " + BROOKESIA_DESCRIBE_TO_STR(animation) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_insert_animation(const std::string &animation, double duration_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto insert_result = emote_insert_anim_dialog(
                             get_native_handle(native_handle_), animation.c_str(), static_cast<uint32_t>(duration_ms)
                         );
    if (insert_result != ESP_OK) {
        return std::unexpected(
                   "Failed to insert animation: " + BROOKESIA_DESCRIBE_TO_STR(animation) + ", error: " +
                   std::string(esp_err_to_name(insert_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_stop_animation()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto stop_result = emote_stop_anim_dialog(get_native_handle(native_handle_));
    if (stop_result != ESP_OK) {
        return std::unexpected(
                   "Failed to stop animation, error: " + std::string(esp_err_to_name(stop_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_wait_animation_frame_done(double timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto wait_result = emote_wait_emerg_dlg_done(get_native_handle(native_handle_), static_cast<uint32_t>(timeout_ms));
    if (wait_result != ESP_OK) {
        return std::unexpected(
                   "Failed to wait animation frame done, error: " + std::string(esp_err_to_name(wait_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_set_event_msg(const std::string &event, const std::string &message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    std::string native_event;
    if (!get_native_message_event(event, native_event)) {
        return std::unexpected("Invalid event: " + BROOKESIA_DESCRIBE_TO_STR(event));
    }

    auto set_result = emote_set_event_msg(get_native_handle(native_handle_), native_event.c_str(), message.c_str());
    if (set_result != ESP_OK) {
        return std::unexpected(
                   "Failed to set event message: " + BROOKESIA_DESCRIBE_TO_STR(event) + ", error: " +
                   std::string(esp_err_to_name(set_result))
               );
    }

    return {};
}

std::expected<void, std::string> Emote::function_notify_flush_finished()
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return std::unexpected("Emote is not started");
    }

    auto notify_result = emote_notify_flush_finished(get_native_handle(native_handle_));
    if (notify_result != ESP_OK) {
        return std::unexpected(
                   "Failed to notify flush finished, error: " + std::string(esp_err_to_name(notify_result))
               );
    }

    return {};
}

static bool get_native_source_data(const Emote::AssetSource &source, emote_data_t &native_data)
{
    switch (source.type) {
    case Emote::AssetSourceType::Path:
        native_data.type = EMOTE_SOURCE_PATH;
        native_data.source.path = source.source.c_str();
        native_data.flags.mmap_enable = source.flag_enable_mmap;
        break;
    case Emote::AssetSourceType::PartitionLabel:
        native_data.type = EMOTE_SOURCE_PARTITION;
        native_data.source.partition_label = source.source.c_str();
        native_data.flags.mmap_enable = source.flag_enable_mmap;
        break;
    default:
        return false;
    }
    return true;
}

static bool get_native_message_event(const std::string &event, std::string &native_event)
{
    Emote::AssetMessageType event_enum;
    auto event_result = BROOKESIA_DESCRIBE_STR_TO_ENUM(event, event_enum);
    if (!event_result) {
        return false;
    }

    switch (event_enum) {
    case Emote::AssetMessageType::Idle:
        native_event = EMOTE_MGR_EVT_IDLE;
        break;
    case Emote::AssetMessageType::Speak:
        native_event = EMOTE_MGR_EVT_SPEAK;
        break;
    case Emote::AssetMessageType::Listen:
        native_event = EMOTE_MGR_EVT_LISTEN;
        break;
    case Emote::AssetMessageType::System:
        native_event = EMOTE_MGR_EVT_SYS;
        break;
    case Emote::AssetMessageType::User:
        native_event = EMOTE_MGR_EVT_SET;
        break;
    case Emote::AssetMessageType::Battery:
        native_event = EMOTE_MGR_EVT_BAT;
        break;
    case Emote::AssetMessageType::QRCode:
        native_event = EMOTE_MGR_EVT_QRCODE;
        break;
    default:
        return false;
    }
    return true;
}

BROOKESIA_PLUGIN_REGISTER_SINGLETON(
    service::ServiceBase, Emote, Emote::get_instance().get_attributes().name, Emote::get_instance()
);

} // namespace esp_brookesia::expression
