/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <random>
#include "cJSON.h"
#include "esp_peer.h"
#include "boost/thread.hpp"
#include "brookesia/agent_openai/macro_configs.h"
#if !BROOKESIA_AGENT_OPENAI_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/agent_manager/manager.hpp"
#include "brookesia/agent_openai/agent_openai.hpp"
#include "openai.h"
#include "openai_datachannel.hpp"

namespace esp_brookesia::agent {

using NVSHelper = service::helper::NVS;

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool Openai::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_AGENT_OPENAI_VER_MAJOR, BROOKESIA_AGENT_OPENAI_VER_MINOR,
        BROOKESIA_AGENT_OPENAI_VER_PATCH
    );

    return true;
}

bool Openai::on_activate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    try_load_data();

    auto &info = get_data<DataType::Info>();
    BROOKESIA_LOGD("Start with info: %1%", BROOKESIA_DESCRIBE_TO_STR(info));

    BROOKESIA_CHECK_FALSE_RETURN(validate_info(info), false, "Invalid info");

    openai_config_t config = {
        .audio_data_handler = +[](uint8_t *data, int len, void *ctx)
        {
            auto self = static_cast<Openai *>(ctx);
            BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
            BROOKESIA_CHECK_FALSE_EXIT(self->on_audio_data(data, len), "Failed to on audio data");
        },
        .audio_event_handler = +[](int event, uint8_t *data, void *ctx)
        {
            auto self = static_cast<Openai *>(ctx);
            BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
            BROOKESIA_CHECK_FALSE_EXIT(self->on_audio_event(event, data), "Failed to on audio event");
        },
        .model = info.model.c_str(),
        .api_key = info.api_key.c_str(),
        .connect_timeout_ms = OPENAI_DEFAULT_CONNECT_TIMEOUT_MS,
        .ctx = this,
    };
    BROOKESIA_CHECK_ESP_ERR_RETURN(openai_init(&config), false, "Failed to init openai");
    is_openai_initialized_ = true;

    trigger_general_event(GeneralEvent::Activated);

    return true;
}

bool Openai::on_startup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_ESP_ERR_RETURN(openai_start(), false, "Failed to start openai");
    is_openai_started_ = true;

    return true;
}

void Openai::on_shutdown()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_openai_started()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(openai_stop(), {}, {
            BROOKESIA_LOGE("Failed to stop openai");
        });
        is_openai_started_ = false;
    }
    if (is_openai_initialized()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(openai_deinit(), {}, {
            BROOKESIA_LOGE("Failed to deinit openai");
        });
        is_openai_initialized_ = false;
    }
}

bool Openai::on_sleep()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    trigger_general_event(GeneralEvent::Slept);

    return true;
}

bool Openai::on_wakeup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    trigger_general_event(GeneralEvent::Awake);

    return true;
}

bool Openai::on_encoder_data_ready(const uint8_t *data, size_t data_size)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), data_size(%2%)", data, data_size);

    if (!is_openai_started()) {
        // BROOKESIA_LOGD("Openai is not started, skip");
        return true;
    }

    openai_send_audio(const_cast<uint8_t *>(data), data_size);

    return true;
}

bool Openai::set_info(const boost::json::object &info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", BROOKESIA_DESCRIBE_TO_STR(info));

    try_load_data();

    OpenaiInfo openai_info;

    auto success = BROOKESIA_DESCRIBE_FROM_JSON(info, openai_info);
    BROOKESIA_CHECK_FALSE_RETURN(
        success, false, "Failed to deserialize openai info: %1%", BROOKESIA_DESCRIBE_TO_STR(info)
    );

    auto current_info_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(get_data<DataType::Info>());
    auto new_info_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(openai_info);
    if (current_info_str == new_info_str) {
        BROOKESIA_LOGI("Info is the same, skip setting");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(validate_info(openai_info), false, "Invalid info");

    set_data<DataType::Info>(std::move(openai_info));

    try_save_data(DataType::Info);

    return true;
}

bool Openai::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_data_loaded_ = false;
    set_data<DataType::Info>(OpenaiInfo{});

    try_erase_data();

    BROOKESIA_LOGI("Reset all data");

    return true;
}

void Openai::try_load_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_data_loaded_) {
        BROOKESIA_LOGD("Data is already loaded, skip");
        return;
    }

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto binding = service::ServiceManager::get_instance().bind(NVSHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_EXIT(binding.is_valid(), "Failed to bind NVS service");

    auto nvs_namespace = get_attributes().get_name();

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Info);
        auto result = NVSHelper::get_key_value<OpenaiInfo>(nvs_namespace, key);
        if (!result) {
            BROOKESIA_LOGW("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::Info>(std::move(result.value()));
            BROOKESIA_LOGI("Loaded '%1%' from NVS", key);
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Openai::try_save_data(DataType type)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto key = BROOKESIA_DESCRIBE_TO_STR(type);
    BROOKESIA_LOGD("Params: type(%1%)", key);

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto save_function = [this, key](const auto & data_value) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        auto result = NVSHelper::save_key_value(get_attributes().get_name(), key, data_value, NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };

    if (type == DataType::Info) {
        save_function(get_data<DataType::Info>());
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
    }
}

void Openai::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(get_attributes().get_name(), {}, NVS_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS data");
    }
}

bool Openai::validate_info(OpenaiInfo &info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!info.model.empty(), false, "Model is empty");
    BROOKESIA_CHECK_FALSE_RETURN(!info.api_key.empty(), false, "API key is empty");

    return true;
}

bool Openai::on_audio_data(uint8_t *data, int len)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), len(%2%)", data, len);

    if (!is_openai_started()) {
        // BROOKESIA_LOGD("Openai is not started, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        feed_audio_decoder_data(data, len), false, "Failed to feed audio data"
    );

    return true;
}

bool Openai::on_audio_event(int event, uint8_t *data)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: event(%1%), data(%2%)", event, data);

    std::function<void(void)> task_func;

    esp_peer_state_t state = static_cast<esp_peer_state_t>(event);
    switch (state) {
    case ESP_PEER_STATE_CONNECTED:
        BROOKESIA_LOGI("peer connected");
        break;
    case ESP_PEER_STATE_DISCONNECTED:
        BROOKESIA_LOGE( "peer disconnected");
        // Set task function to stop agent
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Stopped);
        };
        break;
    case ESP_PEER_STATE_DATA_CHANNEL_CONNECTED:
        BROOKESIA_LOGI("peer data channel connected");
        // Set task function to start agent
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Started);
        };
        break;
    default:
        // int evt = (int)event;
        // if (evt == ESP_PEER_MSG_EVENT) {
        //     cJSON *json = cJSON_Parse((char *)data);
        //     if (json) {
        //         openai_datachannel_handle_message(json, (char *)data);
        //         cJSON_Delete(json);
        //     } else {
        //         BROOKESIA_LOGE("cJSON parse failed: %s", (char *)data);
        //     }
        // }
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

#if BROOKESIA_AGENT_OPENAI_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON(
    Base, Openai, Openai::get_instance().get_attributes().get_name(), Openai::get_instance()
);
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    service::ServiceBase, Openai, Openai::get_instance().get_attributes().get_name(), Openai::get_instance(), BROOKESIA_AGENT_OPENAI_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::agent
