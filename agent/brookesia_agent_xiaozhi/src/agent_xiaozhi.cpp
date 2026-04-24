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
#include "esp_mcp_engine.h"
#include "esp_xiaozhi_camera.h"
#include "esp_xiaozhi_chat.h"
#include "esp_xiaozhi_info.h"
#include "../src/priv_include/esp_xiaozhi_keystore.h"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/agent_xiaozhi/agent_xiaozhi.hpp"

namespace esp_brookesia::agent {

using AudioHelper = service::helper::Audio;
using XiaoZhiHelper = helper::XiaoZhi;
using NVS_Helper = service::helper::NVS;

constexpr uint32_t INTERRUPTED_SPEAKING_RESET_DELAY_MS = 1000;
constexpr uint32_t OPEN_AUDIO_CHANNEL_DELAY_MS = 5000;

namespace {
static uint32_t keystore_nvs_handle_counter = 0;
static std::map<uint32_t, std::string> keystore_nvs_nspace_map;
static std::string keystore_nvs_nspace;

nvs_handle_t get_keystore_nvs_handle_by_nspace(std::string nspace)
{
    // Try to find the handle by name space
    for (auto &[counter, _nspace] : keystore_nvs_nspace_map) {
        if (_nspace == nspace) {
            BROOKESIA_LOGD("Found NVS handle '%1%' for namespace '%2%'", counter, nspace);
            return reinterpret_cast<nvs_handle_t>(counter);
        }
    }
    // If not found, create a new handle
    auto new_counter = ++keystore_nvs_handle_counter;
    keystore_nvs_nspace_map[new_counter] = nspace;

    BROOKESIA_LOGD("Created new NVS handle '%1%' for namespace '%2%'", new_counter, nspace);

    return reinterpret_cast<nvs_handle_t>(new_counter);
}

std::string get_keystore_nspace_by_handle(nvs_handle_t handle)
{
    auto counter = reinterpret_cast<uint32_t>(handle);
    auto nspace = keystore_nvs_nspace_map.find(counter);
    if (nspace == keystore_nvs_nspace_map.end()) {
        BROOKESIA_LOGW("NVS handle not found, returning empty string");
        return std::string();
    }
    BROOKESIA_LOGD("Found namespace '%1%' for NVS handle '%2%'", nspace->second, handle);

    return nspace->second;
};

esp_err_t on_keystore_nvs_open(const char *name_space, bool read_write, nvs_handle_t *out_handle)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: name_space(%1%), read_write(%2%)", name_space, read_write);

    BROOKESIA_CHECK_NULL_RETURN(name_space, ESP_FAIL, "Invalid NVS namespace");
    BROOKESIA_CHECK_NULL_RETURN(out_handle, ESP_FAIL, "Invalid NVS handle");

    *out_handle = get_keystore_nvs_handle_by_nspace(std::string(name_space));

    return ESP_OK;
}

void on_keystore_nvs_close(nvs_handle_t handle)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handle(%1%)", handle);
}

esp_err_t on_keystore_nvs_commit(nvs_handle_t handle)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    return ESP_OK;
}

esp_err_t on_keystore_nvs_get_str(nvs_handle_t handle, const char *key, char *out_value, size_t *length)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD(
        "Params: handle(%1%), key(%2%), out_value(%3%), length(%4%)", handle, key, reinterpret_cast<void *>(out_value),
        length
    );

    auto nspace = get_keystore_nspace_by_handle(handle);
    BROOKESIA_CHECK_FALSE_RETURN(!nspace.empty(), ESP_FAIL, "NVS handle not found");
    BROOKESIA_CHECK_NULL_RETURN(key, ESP_FAIL, "Invalid key");
    BROOKESIA_CHECK_NULL_RETURN(out_value, ESP_FAIL, "Invalid out value");
    BROOKESIA_CHECK_NULL_RETURN(length, ESP_FAIL, "Invalid length");

    auto result = NVS_Helper::get_key_value<std::string>(nspace, key);
    if (!result && result.error().find("ESP_ERR_NVS_NOT_FOUND")) {
        BROOKESIA_LOGW("Key '%1%' not found in NVS, returning empty string", key);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        result, ESP_FAIL, "Failed to get (key: %1%) string from NVS: %2%", key, result.error()
    );

    auto out_len = std::min(*length, result.value().size()) + 1;
    strncpy(out_value, result.value().c_str(), out_len - 1);
    out_value[out_len - 1] = '\0';
    *length = out_len;

    BROOKESIA_LOGD("Keystore get '%1%':'%2%'", key, out_value);

    return ESP_OK;
}

esp_err_t on_keystore_nvs_set_str(nvs_handle_t handle, const char *key, const char *value)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handle(%1%), key(%2%), value(%3%)", handle, key, value);

    auto nspace = get_keystore_nspace_by_handle(handle);
    BROOKESIA_CHECK_FALSE_RETURN(!nspace.empty(), ESP_FAIL, "NVS handle not found");
    BROOKESIA_CHECK_NULL_RETURN(key, ESP_FAIL, "Invalid key");
    BROOKESIA_CHECK_NULL_RETURN(value, ESP_FAIL, "Invalid value");

    auto result = NVS_Helper::save_key_value(nspace, key, value);
    BROOKESIA_CHECK_FALSE_RETURN(
        result, ESP_FAIL, "Failed to set (key: %1%) string to NVS: %2%", key, result.error()
    );

    return ESP_OK;
}

esp_err_t on_keystore_nvs_get_i32(nvs_handle_t handle, const char *key, int32_t *out_value)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD(
        "Params: handle(%1%), key(%2%), out_value(%3%)", handle, key, reinterpret_cast<void *>(out_value)
    );

    auto nspace = get_keystore_nspace_by_handle(handle);
    BROOKESIA_CHECK_FALSE_RETURN(!nspace.empty(), ESP_FAIL, "NVS handle not found");
    BROOKESIA_CHECK_NULL_RETURN(key, ESP_FAIL, "Invalid key");
    BROOKESIA_CHECK_NULL_RETURN(out_value, ESP_FAIL, "Invalid out value");

    auto result = NVS_Helper::get_key_value<int32_t>(nspace, key);
    if (!result && result.error().find("ESP_ERR_NVS_NOT_FOUND")) {
        BROOKESIA_LOGW("Key '%1%' not found in NVS, returning empty string", key);
        return ESP_ERR_NVS_NOT_FOUND;
    }

    *out_value = result.value();

    BROOKESIA_LOGD("Keystore get '%1%':'%2%'", key, *out_value);

    return ESP_OK;
}

esp_err_t on_keystore_nvs_set_i32(nvs_handle_t handle, const char *key, int32_t value)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handle(%1%), key(%2%), value(%3%)", handle, key, value);

    auto nspace = get_keystore_nspace_by_handle(handle);
    BROOKESIA_CHECK_FALSE_RETURN(!nspace.empty(), ESP_FAIL, "NVS handle not found");
    BROOKESIA_CHECK_NULL_RETURN(key, ESP_FAIL, "Invalid key");

    auto result = NVS_Helper::save_key_value(nspace, key, value);
    BROOKESIA_CHECK_FALSE_RETURN(
        result, ESP_FAIL, "Failed to set (key: %1%) int to NVS: %2%", key, result.error()
    );

    return ESP_OK;
}

esp_err_t on_keystore_nvs_erase_key(nvs_handle_t handle, const char *key)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handle(%1%), key(%2%)", handle, key);

    auto nspace = get_keystore_nspace_by_handle(handle);
    BROOKESIA_CHECK_FALSE_RETURN(!nspace.empty(), ESP_FAIL, "NVS handle not found");
    BROOKESIA_CHECK_NULL_RETURN(key, ESP_FAIL, "Invalid key");

    auto result = NVS_Helper::erase_keys(nspace, {key});
    BROOKESIA_CHECK_FALSE_RETURN(
        result, ESP_FAIL, "Failed to erase (key: %1%) from NVS: %2%", key, result.error()
    );

    return ESP_OK;
}

esp_err_t on_keystore_nvs_erase_all(nvs_handle_t handle)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: handle(%1%)", handle);

    auto nspace = get_keystore_nspace_by_handle(handle);
    BROOKESIA_CHECK_FALSE_RETURN(!nspace.empty(), ESP_FAIL, "NVS handle not found");

    auto result = NVS_Helper::erase_keys(nspace, {});
    BROOKESIA_CHECK_FALSE_RETURN(
        result, ESP_FAIL, "Failed to erase all keys from NVS: %2%", result.error()
    );

    return ESP_OK;
}

} // anonymous namespace

bool XiaoZhi::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_AGENT_XIAOZHI_VER_MAJOR, BROOKESIA_AGENT_XIAOZHI_VER_MINOR,
        BROOKESIA_AGENT_XIAOZHI_VER_PATCH
    );

    static const esp_xiaozhi_keystore_nvs_ops_t keystore_nvs_callbacks{
        .nvs_open = on_keystore_nvs_open,
        .nvs_close = on_keystore_nvs_close,
        .nvs_commit = on_keystore_nvs_commit,
        .nvs_get_str = on_keystore_nvs_get_str,
        .nvs_set_str = on_keystore_nvs_set_str,
        .nvs_get_i32 = on_keystore_nvs_get_i32,
        .nvs_set_i32 = on_keystore_nvs_set_i32,
        .nvs_erase_key = on_keystore_nvs_erase_key,
        .nvs_erase_all = on_keystore_nvs_erase_all,
    };
    esp_xiaozhi_keystore_register_nvs_ops(&keystore_nvs_callbacks);

    return true;
}

void XiaoZhi::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    mcp_tool_registry_.remove_all_tools();

    esp_xiaozhi_keystore_register_nvs_ops(nullptr);
}

std::expected<boost::json::array, std::string> XiaoZhi::function_add_mcp_tools_with_service_function(
    const std::string &service_name, const boost::json::array &function_names
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: service_name(%1%), function_names(%2%)", service_name, function_names);

    std::vector<std::string> func_names;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(function_names, func_names)) {
        return std::unexpected(std::string("Failed to deserialize function names"));
    }

    auto names = mcp_tool_registry_.add_service_tools(service_name, std::move(func_names));

    return BROOKESIA_DESCRIBE_TO_JSON(names).as_array();
}

std::expected<boost::json::array, std::string> XiaoZhi::function_add_mcp_tools_with_custom_function(
    const boost::json::array &tools
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: tools(%1%)", BROOKESIA_DESCRIBE_TO_STR(tools));

    std::vector<mcp_utils::CustomTool> custom_tools;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(tools, custom_tools)) {
        return std::unexpected(std::string("Failed to deserialize custom tools"));
    }

    std::vector<std::string> added_tools;
    for (auto &tool : custom_tools) {
        auto name = mcp_tool_registry_.add_custom_tool(std::move(tool));
        added_tools.push_back(name);
    }

    return BROOKESIA_DESCRIBE_TO_JSON(added_tools).as_array();
}

std::expected<void, std::string> XiaoZhi::function_remove_mcp_tools(const boost::json::array &tools)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: tools(%1%)", BROOKESIA_DESCRIBE_TO_STR(tools));

    std::vector<std::string> tool_names;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(tools, tool_names)) {
        return std::unexpected(std::string("Failed to deserialize tool names"));
    }

    for (const auto &tool_name : tool_names) {
        mcp_tool_registry_.remove_tool(tool_name);
    }

    return {};
}

std::expected<std::string, std::string> XiaoZhi::function_explain_image(
    const service::RawBuffer &image, const std::string &question
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: image(%1%), question(%2%)", image, question);

    if (!image_explain_handle_) {
        esp_xiaozhi_camera_config_t camera_config = {
            .explain_url = BROOKESIA_AGENT_XIAOZHI_IMAGE_EXPLAIN_URL,
            .explain_token = BROOKESIA_AGENT_XIAOZHI_IMAGE_EXPLAIN_TOKEN,
        };
        auto create_ret = esp_xiaozhi_camera_create(
                              &camera_config, reinterpret_cast<esp_xiaozhi_camera_handle_t **>(&image_explain_handle_)
                          );
        if (create_ret != ESP_OK) {
            return std::unexpected(
                       std::string("Failed to create image explain: ") + std::string(esp_err_to_name(create_ret))
                   );
        }
    }

    esp_xiaozhi_camera_frame_t camera_frame = {
        .data = image.data_ptr,
        .len = static_cast<size_t>(image.data_size),
    };
    std::vector<uint8_t> response_buffer(BROOKESIA_AGENT_XIAOZHI_IMAGE_EXPLAIN_RESPONSE_BUFFER_SIZE);
    auto response_ptr = reinterpret_cast<char *>(response_buffer.data());
    size_t response_len = 0;
    auto explain_ret = esp_xiaozhi_camera_explain(
                           reinterpret_cast<esp_xiaozhi_camera_handle_t *>(image_explain_handle_), &camera_frame,
                           question.c_str(), response_ptr, response_buffer.size(), &response_len
                       );
    if (explain_ret != ESP_OK) {
        return std::unexpected(
                   std::string("Failed to explain image: ") + std::string(esp_err_to_name(explain_ret))
               );
    }

    auto response_string = std::string(response_ptr, response_len);

    BROOKESIA_LOGD("Response: %1%", response_string);

    return response_string;
}

bool XiaoZhi::on_activate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto check_activation_task = [this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGI("Checking activation info...");

        esp_xiaozhi_chat_info_t info{};
        auto ret = esp_xiaozhi_chat_get_info(&info);
        BROOKESIA_CHECK_ESP_ERR_RETURN(
            ret, true, "Failed to get info, retry in %1% ms", BROOKESIA_AGENT_XIAOZHI_GET_HTTP_INFO_INTERVAL_MS
        );

        lib_utils::FunctionGuard free_info_guard([this, &info]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            esp_xiaozhi_chat_free_info(&info);
        });

        if (!info.has_activation_code && !info.has_activation_challenge) {
            BROOKESIA_LOGI("No activation code or challenge found, activate successfully");
            set_chat_info(ChatInfo{
                .has_mqtt_config = info.has_mqtt_config,
                .has_websocket_config = info.has_websocket_config,
            });
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
                               &get_chat_info_task_, get_state_task_group()
                           );
        BROOKESIA_CHECK_FALSE_RETURN(task_result, false, "Failed to post task function");
    }

    return true;
}

bool XiaoZhi::on_startup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    esp_mcp_t *mcp_engine = nullptr;
    auto ret_create = esp_mcp_create(&mcp_engine);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret_create, false, "Failed to create MCP engine");
    lib_utils::FunctionGuard destroy_mcp_engine_guard([this, mcp_engine]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_mcp_destroy(mcp_engine), {}, {
            BROOKESIA_LOGE("Failed to destroy MCP engine");
        });
    });

    auto mcp_handles = mcp_tool_registry_.generate_tools();
    for (auto handle : mcp_handles) {
        auto ret_add = esp_mcp_add_tool(mcp_engine, reinterpret_cast<esp_mcp_tool_t *>(handle));
        BROOKESIA_CHECK_ESP_ERR_RETURN(ret_add, false, "Failed to add MCP tool");
    }

    // Register agent event handler
    auto agent_event_handler = +[](void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<XiaoZhi *>(arg);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        BROOKESIA_CHECK_FALSE_EXIT(self->on_agent_event(event_id), "Failed to on agent event");
    };
    auto ret_register = esp_event_handler_instance_register(
                            ESP_XIAOZHI_CHAT_EVENTS, ESP_EVENT_ANY_ID, agent_event_handler, this, &agent_event_instance_
                        );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret_register, false, "Failed to register agent event handler");

    // Initialize chat
    esp_xiaozhi_chat_config_t chat_config = ESP_XIAOZHI_CHAT_DEFAULT_CONFIG();
    chat_config.audio_callback = +[](const uint8_t *data, int len, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<XiaoZhi *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        BROOKESIA_CHECK_FALSE_EXIT(self->on_chat_data(data, static_cast<size_t>(len)), "Failed to on chat data");
    };
    chat_config.audio_callback_ctx = this;
    chat_config.event_callback = +[](esp_xiaozhi_chat_event_t event, void *event_data, void *ctx) {
        // BROOKESIA_LOG_TRACE_GUARD();
        auto self = static_cast<XiaoZhi *>(ctx);
        BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");
        BROOKESIA_CHECK_FALSE_EXIT(self->on_chat_event(
                                       static_cast<uint8_t>(event), event_data
                                   ), "Failed to on chat event");
    };
    chat_config.audio_type = ESP_XIAOZHI_CHAT_AUDIO_TYPE_OPUS;
    chat_config.event_callback_ctx = this;
    chat_config.has_mqtt_config = get_chat_info().has_mqtt_config;
    chat_config.has_websocket_config = get_chat_info().has_websocket_config;
    chat_config.mcp_engine = mcp_engine;
    auto ret_init = esp_xiaozhi_chat_init(&chat_config, &chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret_init, false, "Failed to init chat");

    is_xiaozhi_initialized_ = true;

    auto ret = esp_xiaozhi_chat_start(chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to start chat");

    is_xiaozhi_started_ = true;

    destroy_mcp_engine_guard.release();

    return true;
}

void XiaoZhi::on_shutdown()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto task_scheduler = get_task_scheduler();
    // Cancel periodic tasks first to prevent use-after-free
    if (get_chat_info_task_ != 0) {
        if (task_scheduler) {
            task_scheduler->cancel(get_chat_info_task_);
        }
        get_chat_info_task_ = 0;
    }
    if (open_audio_channel_task_ != 0) {
        if (task_scheduler) {
            task_scheduler->cancel(open_audio_channel_task_);
        }
        open_audio_channel_task_ = 0;
    }

    if (agent_event_instance_) {
        esp_event_handler_instance_unregister(ESP_XIAOZHI_CHAT_EVENTS, ESP_EVENT_ANY_ID, agent_event_instance_);
        agent_event_instance_ = nullptr;
    }
    if (is_xiaozhi_started()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_xiaozhi_chat_stop(chat_handle_), {}, {
            BROOKESIA_LOGE("Failed to stop xiaozhi");
        });
        is_xiaozhi_started_ = false;
    }
    if (is_xiaozhi_initialized()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_xiaozhi_chat_deinit(chat_handle_), {}, {
            BROOKESIA_LOGE("Failed to deinit xiaozhi");
        });
        is_xiaozhi_initialized_ = false;
    }
    if (image_explain_handle_) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_xiaozhi_camera_destroy(
                                            reinterpret_cast<esp_xiaozhi_camera_handle_t *>(image_explain_handle_)
        ), {}, {
            BROOKESIA_LOGE("Failed to destroy image explain");
        });
        image_explain_handle_ = nullptr;
    }
    chat_handle_ = 0;
    is_xiaozhi_audio_channel_opened_ = false;

    trigger_general_event(GeneralEvent::Stopped);
}

bool XiaoZhi::on_interrupt_speaking()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_xiaozhi_audio_channel_opened(), false, "Audio channel is not opened");

    auto ret = esp_xiaozhi_chat_send_abort_speaking(
                   chat_handle_, ESP_XIAOZHI_CHAT_ABORT_SPEAKING_REASON_WAKE_WORD_DETECTED
               );
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to abort speaking");

    reset_interrupted_speaking();

    return true;
}

bool XiaoZhi::on_manual_start_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_xiaozhi_audio_channel_opened(), false, "Audio channel is not opened");

    auto ret = esp_xiaozhi_chat_send_start_listening(chat_handle_, ESP_XIAOZHI_CHAT_LISTENING_MODE_MANUAL);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to start listening");

    return true;
}

bool XiaoZhi::on_manual_stop_listening()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_xiaozhi_audio_channel_opened(), false, "Audio channel is not opened");

    auto ret = esp_xiaozhi_chat_send_stop_listening(chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to stop listening");

    return true;
}

bool XiaoZhi::on_sleep()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_xiaozhi_chat_close_audio_channel(chat_handle_);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to close audio channel");

    return true;
}

bool XiaoZhi::on_wakeup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto ret = esp_xiaozhi_chat_open_audio_channel(chat_handle_, nullptr, nullptr, 0);
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

    auto ret = esp_xiaozhi_chat_send_audio_data(chat_handle_, reinterpret_cast<const char *>(data), data_size);
    BROOKESIA_CHECK_ESP_ERR_RETURN(ret, false, "Failed to send audio data");

    return true;
}

bool XiaoZhi::on_agent_event(int32_t event_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_id(%1%)", event_id);

    std::function<void(void)> task_func;

    // Will be executed when the function exits
    lib_utils::FunctionGuard post_task_guard([this, &task_func]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (task_func) {
            auto scheduler = get_task_scheduler();
            BROOKESIA_CHECK_NULL_EXIT(scheduler, "Scheduler is not available");
            auto group = get_state_task_group();
            auto result = scheduler->post(task_func, nullptr, group);
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post task function");
        }
    });

    switch (event_id) {
    case ESP_XIAOZHI_CHAT_EVENT_CONNECTED: {
        BROOKESIA_LOGI("connected");
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            // Skip if XiaoZhi is stopped
            if (!is_xiaozhi_started()) {
                BROOKESIA_LOGW("XiaoZhi is not started, skip");
                return;
            }

            // Trigger general event to start agent
            trigger_general_event(GeneralEvent::Started);

            if (open_audio_channel_task_ != 0) {
                BROOKESIA_LOGW("Open audio channel task is already running, skip");
                return;
            }

            // Open audio channel
            // Delay to ensure the audio channel is opened after the agent is started
            // Otherwise, the MCP tools may not be available in first chat
            auto open_audio_channel_func = [this]() {
                BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

                // Skip if XiaoZhi is stopped
                if (!is_xiaozhi_started()) {
                    BROOKESIA_LOGW("XiaoZhi is not started, skip");
                    return;
                }

                open_audio_channel_task_ = 0;

                auto get_format_str = [](AudioHelper::CodecFormat format) {
                    switch (format) {
                    case AudioHelper::CodecFormat::OPUS:
                        return "opus";
                    case AudioHelper::CodecFormat::G711A:
                        return "g711a";
                    default:
                        return "pcm";
                    }
                };
                auto &audio_config = get_audio_config();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
                esp_xiaozhi_chat_audio_t chat_audio = {
                    .format = get_format_str(audio_config.decoder.type),
                    .sample_rate = static_cast<int>(audio_config.decoder.general.sample_rate),
                    .channels = static_cast<int>(audio_config.decoder.general.channels),
                    .frame_duration = static_cast<int>(audio_config.decoder.general.frame_duration),
                };
#pragma GCC diagnostic pop
                auto ret = esp_xiaozhi_chat_open_audio_channel(chat_handle_, &chat_audio, nullptr, 0);
                BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {
                    trigger_general_event(GeneralEvent::Stopped);
                }, {
                    BROOKESIA_LOGE("Failed to open audio channel");
                });
            };
            auto scheduler = get_task_scheduler();
            BROOKESIA_CHECK_NULL_EXIT(scheduler, "Scheduler is not available");
            auto group = get_state_task_group();
            auto result = scheduler->post_delayed(
                              open_audio_channel_func, OPEN_AUDIO_CHANNEL_DELAY_MS, &open_audio_channel_task_, group
                          );
            BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post task function");
        };
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_DISCONNECTED:
        BROOKESIA_LOGI("disconnected");
        is_xiaozhi_audio_channel_opened_.store(false);
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Stopped);
        };
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_OPENED: {
        BROOKESIA_LOGI("audio channel opened");
        is_xiaozhi_audio_channel_opened_.store(true);
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Awake);
        };
        int mode = -1;
        switch (get_chat_mode()) {
        case ChatMode::HalfDuplex:
            mode = ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME;
            break;
        case ChatMode::RealTime:
            mode = ESP_XIAOZHI_CHAT_LISTENING_MODE_REALTIME;
            break;
        default:
            break;
        }
        if (mode != -1) {
            auto ret = esp_xiaozhi_chat_send_start_listening(chat_handle_, mode);
            BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {
                task_func = [this]()
                {
                    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
                    trigger_general_event(GeneralEvent::Stopped);
                };
                return false;
            }, {
                BROOKESIA_LOGE("Failed to start listening");
            });
        }
        std::string wake_word = BROOKESIA_AGENT_XIAOZHI_WAKE_WORD;
        if (wake_word.empty()) {
            auto wake_words = get_wake_words();
            if (!wake_words.empty()) {
                wake_word = wake_words[0];
            }
        }
        if (!wake_word.empty()) {
            auto ret = esp_xiaozhi_chat_send_wake_word(chat_handle_, wake_word.c_str());
            BROOKESIA_CHECK_ESP_ERR_EXECUTE(ret, {}, {
                BROOKESIA_LOGE("Failed to send wake word");
            });
        }
        break;
    }
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_CHANNEL_CLOSED:
        BROOKESIA_LOGI("audio channel closed");
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            if (is_general_action_running(GeneralAction::Sleep)) {
                trigger_general_event(GeneralEvent::Slept);
            }
        };
        is_xiaozhi_audio_channel_opened_.store(false);
        break;
    case ESP_XIAOZHI_CHAT_EVENT_AUDIO_DATA_INCOMING:
        BROOKESIA_LOGI("audio data incoming");
        break;
    case ESP_XIAOZHI_CHAT_EVENT_SERVER_GOODBYE:
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

    return true;
}

bool XiaoZhi::on_chat_data(const uint8_t *data, size_t len)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), len(%2%)", data, len);

    if (!is_xiaozhi_audio_channel_opened()) {
        BROOKESIA_LOGD("XiaoZhi is not started, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(feed_audio_decoder_data(data, len), false, "Failed to feed audio data");

    return true;
}

bool XiaoZhi::on_chat_event(uint8_t event, void *event_data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event(%1%), event_data(%2%)", event, event_data);

    std::function<void(void)> task_func;

    auto chat_event = static_cast<esp_xiaozhi_chat_event_t>(event);
    switch (chat_event) {
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STARTED: {
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
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_SPEECH_STOPPED: {
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
    }
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_TEXT: {
        BROOKESIA_CHECK_NULL_RETURN(event_data, false, "Invalid event data");
        auto text_data = *static_cast<esp_xiaozhi_chat_text_data_t *>(event_data);
        BROOKESIA_LOGD("Chat role: %1%, text: %2%", static_cast<int>(text_data.role), text_data.text);
        task_func = [this, role = text_data.role, text = std::string(text_data.text)]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            std::string role_str;
            bool result = false;
            if (role == ESP_XIAOZHI_CHAT_TEXT_ROLE_ASSISTANT) {
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
    case ESP_XIAOZHI_CHAT_EVENT_CHAT_EMOJI: {
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

void XiaoZhi::set_chat_info(const ChatInfo &chat_info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: chat_info(%1%)", BROOKESIA_DESCRIBE_TO_STR(chat_info));

    chat_info_ = chat_info;
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
