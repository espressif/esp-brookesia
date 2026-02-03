/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/agent_xiaozhi/macro_configs.h"
#if !BROOKESIA_AGENT_XIAOZHI_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/agent_manager/manager.hpp"
#include "esp_iot_board.h"
#include "esp_iot_chat.h"
#include "brookesia/agent_xiaozhi/agent_xiaozhi.hpp"

namespace esp_brookesia::agent {

using XiaoZhiHelper = helper::XiaoZhi;

constexpr uint32_t INTERRUPTED_SPEAKING_RESET_DELAY_MS = 1000;

bool XiaoZhi::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_AGENT_XIAOZHI_VER_MAJOR, BROOKESIA_AGENT_XIAOZHI_VER_MINOR,
        BROOKESIA_AGENT_XIAOZHI_VER_PATCH
    );

    return true;
}

bool XiaoZhi::on_activate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto agent_event_handler = +[](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<XiaoZhi *>(arg);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        BROOKESIA_CHECK_FALSE_EXIT(self->on_agent_event(event_id), "Failed to on agent event");
    };
    auto ret_register = esp_event_handler_instance_register(
                            ESP_IOT_CHAT_EVENTS, ESP_EVENT_ANY_ID, agent_event_handler, this, &agent_event_instance_
                        );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret_register, false, "Failed to register agent event handler");

    esp_iot_chat_config_t chat_config = ESP_IOT_CHAT_DEFAULT_CONFIG();
    chat_config.audio_callback = +[](uint8_t *data, int len, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<XiaoZhi *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        BROOKESIA_CHECK_FALSE_EXIT(self->on_chat_data(data, static_cast<size_t>(len)), "Failed to on chat data");
    };
    chat_config.audio_callback_ctx = this;
    chat_config.event_callback = +[](esp_iot_chat_event_t event, void *event_data, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<XiaoZhi *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        BROOKESIA_CHECK_FALSE_EXIT(self->on_chat_event(
                                       static_cast<uint8_t>(event), event_data
                                   ), "Failed to on chat event");
    };
    chat_config.event_callback_ctx = this;
    // chat_config.mcp_handle = mcp_handle;
    auto ret_init = esp_iot_chat_init(&chat_config, &chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret_init, false, "Failed to init chat");

    is_xiaozhi_initialized_ = true;

    auto check_activation_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGI("Checking activation info...");

        esp_iot_chat_http_info_t info{};
        auto ret = esp_iot_chat_get_http_info(chat_handle_, &info);
        BROOKESIA_CHECK_ESP_ERR_RETURN(
            ret, true, "Failed to get HTTP info, retry in %1% ms", BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS
        );

        lib_utils::FunctionGuard free_http_info_guard([this, &info]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            esp_iot_chat_free_http_info(&info);
        });

        if (!info.has_activation_code && !info.has_activation_challenge) {
            BROOKESIA_LOGI("No activation code or challenge found, activate successfully");
            trigger_general_event(GeneralEvent::Activated);
            return false;
        }

        BROOKESIA_LOGW("Not activated, retry in %1% ms", BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS);

        auto result = publish_event(BROOKESIA_DESCRIBE_TO_STR(XiaoZhiHelper::EventId::ActivationCodeReceived),
        service::EventItemMap{
            {
                BROOKESIA_DESCRIBE_TO_STR(XiaoZhiHelper::EventActivationCodeReceivedParam::Code),
                info.activation_code
            }
        });
        BROOKESIA_CHECK_FALSE_RETURN(result, true, "Failed to publish activation code received event, retry later");

        return true;
    };
    if (check_activation_task()) {
        auto task_scheduler = get_task_scheduler();
        BROOKESIA_CHECK_NULL_RETURN(task_scheduler, false, "Scheduler is not available");
        auto task_result = task_scheduler->post_periodic(
                               check_activation_task, BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS,
                               &http_info_task_id_, get_state_task_group()
                           );
        BROOKESIA_CHECK_FALSE_RETURN(task_result, false, "Failed to post task function");
    }

    return true;
}

bool XiaoZhi::on_startup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_iot_chat_start(chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to start chat");

    is_xiaozhi_started_ = true;

    return true;
}

void XiaoZhi::on_shutdown()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Cancel periodic http info task first to prevent use-after-free
    if (http_info_task_id_ != 0) {
        auto task_scheduler = get_task_scheduler();
        if (task_scheduler) {
            task_scheduler->cancel(http_info_task_id_);
        }
        http_info_task_id_ = 0;
    }

    if (agent_event_instance_) {
        esp_event_handler_instance_unregister(ESP_IOT_CHAT_EVENTS, ESP_EVENT_ANY_ID, agent_event_instance_);
        agent_event_instance_ = nullptr;
    }
    if (is_xiaozhi_started()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_iot_chat_stop(chat_handle_), {}, {
            BROOKESIA_LOGE("Failed to stop xiaozhi");
        });
        is_xiaozhi_started_ = false;
    }
    if (is_xiaozhi_initialized()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_iot_chat_deinit(chat_handle_), {}, {
            BROOKESIA_LOGE("Failed to deinit xiaozhi");
        });
        is_xiaozhi_initialized_ = false;
    }
    is_xiaozhi_audio_channel_opened_ = false;

    trigger_general_event(GeneralEvent::Stopped);
}

bool XiaoZhi::on_interrupt_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_xiaozhi_audio_channel_opened(), false, "Audio channel is not opened");

    auto ret = esp_iot_chat_send_abort_speaking(chat_handle_, ESP_IOT_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to abort speaking");

    auto scheduler = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not available");

    auto task_func = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        reset_interrupted_speaking();
    };
    auto result = scheduler->post_delayed(
                      std::move(task_func), INTERRUPTED_SPEAKING_RESET_DELAY_MS, nullptr, get_call_task_group()
                  );
    BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post task function");

    return true;
}

bool XiaoZhi::on_manual_start_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_xiaozhi_audio_channel_opened(), false, "Audio channel is not opened");

    auto ret = esp_iot_chat_send_start_listening(chat_handle_, ESP_IOT_CHAT_LISTENING_MODE_MANUAL);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to start listening");

    return true;
}

bool XiaoZhi::on_manual_stop_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_xiaozhi_audio_channel_opened(), false, "Audio channel is not opened");

    auto ret = esp_iot_chat_send_stop_listening(chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to stop listening");

    return true;
}

bool XiaoZhi::on_sleep()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_iot_chat_close_audio_channel(chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to close audio channel");

    return true;
}

bool XiaoZhi::on_wakeup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_iot_chat_open_audio_channel(chat_handle_, NULL, 0);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to open audio channel");

    return true;
}

bool XiaoZhi::on_encoder_data_ready(const uint8_t *data, size_t data_size)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), data_size(%2%)", data, data_size);

    if (!is_xiaozhi_audio_channel_opened()) {
        // BROOKESIA_LOGD("XiaoZhi is not started, skip");
        return true;
    }

    auto ret = esp_iot_chat_send_audio_data(chat_handle_, reinterpret_cast<const char *>(data), data_size);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to send audio data");

    return true;
}

bool XiaoZhi::on_agent_event(int32_t event_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_id(%1%)", event_id
    );

    std::function<void(void)> task_func;

    switch (event_id) {
    case ESP_IOT_CHAT_EVENT_CONNECTED: {
        BROOKESIA_LOGI("connected");
        auto ret = esp_iot_chat_open_audio_channel(chat_handle_, NULL, 0);
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to open audio channel");
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Started);
        };
        break;
    }
    case ESP_IOT_CHAT_EVENT_DISCONNECTED:
        BROOKESIA_LOGI("disconnected");
        is_xiaozhi_audio_channel_opened_.store(false);
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Stopped);
        };
        break;
    case ESP_IOT_CHAT_EVENT_AUDIO_CHANNEL_OPENED:
        BROOKESIA_LOGI("audio channel opened");
        is_xiaozhi_audio_channel_opened_.store(true);
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Awake);
        };
        if (get_chat_mode() == ChatMode::RealTime) {
            esp_iot_chat_send_start_listening(chat_handle_, ESP_IOT_CHAT_LISTENING_MODE_REALTIME);
        }
        esp_iot_chat_send_wake_word_detected(chat_handle_, BROOKESIA_AGENT_XIAOZHI_WAKE_WORD);
        break;
    case ESP_IOT_CHAT_EVENT_AUDIO_CHANNEL_CLOSED:
        BROOKESIA_LOGI("audio channel closed");
        is_xiaozhi_audio_channel_opened_.store(false);
        break;
    case ESP_IOT_CHAT_EVENT_AUDIO_DATA_INCOMING:
        BROOKESIA_LOGI("audio data incoming");
        break;
    case ESP_IOT_CHAT_EVENT_SERVER_HELLO:
        BROOKESIA_LOGI("server hello");
        break;
    case ESP_IOT_CHAT_EVENT_SERVER_GOODBYE:
        BROOKESIA_LOGI("server goodbye");
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Slept);
        };
        break;
    default:
        BROOKESIA_LOGW("unknown event: %d", event_id);
        break;
    }

    if (task_func) {
        auto group = get_state_task_group();
        auto scheduler = get_task_scheduler();
        BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not available");
        auto result = scheduler->post(std::move(task_func), nullptr, group);
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post task function");
    }

    return true;
}

bool XiaoZhi::on_chat_data(uint8_t *data, size_t len)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), len(%2%)", data, len);

    if (!is_xiaozhi_audio_channel_opened()) {
        BROOKESIA_LOGD("XiaoZhi is not started, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        feed_audio_decoder_data(data, len), false, "Failed to feed audio data"
    );

    return true;
}

bool XiaoZhi::on_chat_event(uint8_t event, void *event_data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%), event_data(%2%)", event, event_data);

    std::function<void(void)> task_func;

    auto chat_event = static_cast<esp_iot_chat_event_t>(event);
    switch (chat_event) {
    case ESP_IOT_CHAT_EVENT_CHAT_STATE: {
        BROOKESIA_CHECK_NULL_RETURN(event_data, false, "Invalid event data");
        auto state = *static_cast<esp_iot_chat_device_state_t *>(event_data);
        BROOKESIA_LOGI("Chat state: %1%", static_cast<int>(state));
        switch (state) {
        case ESP_IOT_CHAT_DEVICE_STATE_LISTENING:
            task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                if (!set_speaking(false)) {
                    BROOKESIA_LOGW("Failed to set speaking to false");
                }
                if (!set_listening(true)) {
                    BROOKESIA_LOGW("Failed to set listening to true");
                }
            };
            break;
        case ESP_IOT_CHAT_DEVICE_STATE_SPEAKING:
            task_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                if (!set_listening(false)) {
                    BROOKESIA_LOGW("Failed to set listening to false");
                }
                if (!set_speaking(true)) {
                    BROOKESIA_LOGW("Failed to set speaking to true");
                }
            };
            break;
        default:
            break;
        }
        break;
    }
    case ESP_IOT_CHAT_EVENT_CHAT_TEXT: {
        BROOKESIA_CHECK_NULL_RETURN(event_data, false, "Invalid event data");
        auto text_data = *static_cast<esp_iot_chat_text_data_t *>(event_data);
        BROOKESIA_LOGD("Chat role: %1%, text: %2%", static_cast<int>(text_data.role), text_data.text);
        task_func = [this, role = text_data.role, text = std::string(text_data.text)]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            std::string role_str;
            bool result = false;
            if (role == ESP_IOT_CHAT_TEXT_ROLE_ASSISTANT) {
                role_str = "Agent";
                result = set_agent_speaking_text(text);
            } else {
                role_str = "User";
                result = set_user_speaking_text(text);
            }
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to set %1% speaking text: %2%", role_str, text);
        };
        break;
    }
    case ESP_IOT_CHAT_EVENT_CHAT_EMOJI: {
        BROOKESIA_CHECK_NULL_RETURN(event_data, false, "Invalid emoji data");
        auto emoji = static_cast<char *>(event_data);
        BROOKESIA_LOGI("Chat emoji: %1%", emoji);
        task_func = [this, emoji = std::string(emoji)]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            auto result = set_emote(emoji);
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to set emote");
        };
        break;
    }
    default:
        BROOKESIA_LOGI("Unhandled chat event: %1%", chat_event);
        break;
    }

    if (task_func) {
        auto scheduler = get_task_scheduler();
        BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not available");
        auto result = scheduler->post(std::move(task_func), nullptr, get_event_task_group());
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post task function");
    }

    return true;
}

#if BROOKESIA_AGENT_XIAOZHI_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON(
    Base, XiaoZhi, XiaoZhi::get_instance().get_attributes().get_name(), XiaoZhi::get_instance()
);
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    service::ServiceBase, XiaoZhi, XiaoZhi::get_instance().get_attributes().get_name(), XiaoZhi::get_instance(),
    BROOKESIA_AGENT_XIAOZHI_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::agent
