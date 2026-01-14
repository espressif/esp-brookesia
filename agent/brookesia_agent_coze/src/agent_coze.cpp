/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <random>
#include <string>
#include <string_view>
#include <sstream>
#include <memory>
#include <vector>
#include <cstring>
#include "esp_mac.h"
#include "esp_coze_utils.h"
#include "http_client_request.h"
#include "boost/json.hpp"
#include "boost/system/error_code.hpp"
#include "brookesia/agent_coze/macro_configs.h"
#if !BROOKESIA_AGENT_COZE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_helper/nvs.hpp"
#include "brookesia/agent_manager/manager.hpp"
#include "brookesia/agent_coze/agent_coze.hpp"

namespace esp_brookesia::agent {

using AudioHelper = service::helper::Audio;
using AgentManagerHelper = service::helper::AgentManager;
using AgentCozeHelper = service::helper::AgentCoze;
using NVSHelper = service::helper::NVS;

constexpr std::string_view AUTHORIZATION_URL = "https://api.coze.cn/api/permission/oauth2/token";
static const std::map<int, AgentCozeHelper::CozeEvent> ERROR_CODE_TYPE_MAP = {
    {4027, AgentCozeHelper::CozeEvent::InsufficientCreditsBalance},
    {4028, AgentCozeHelper::CozeEvent::InsufficientCreditsBalance},
};

constexpr uint32_t NVS_SAVE_DATA_TIMEOUT_MS = 20;
constexpr uint32_t NVS_ERASE_DATA_TIMEOUT_MS = 20;

bool Coze::on_activate()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    try_load_data();

    return true;
}

bool Coze::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_AGENT_COZE_VER_MAJOR, BROOKESIA_AGENT_COZE_VER_MINOR,
        BROOKESIA_AGENT_COZE_VER_PATCH
    );

    return true;
}

bool Coze::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &info = get_data<DataType::Info>();
    BROOKESIA_LOGD("Start with info: %1%", BROOKESIA_DESCRIBE_TO_STR(info));

    auto &auth_info = info.authorization;
    std::string access_token = get_access_token(auth_info);
    if (access_token.empty()) {
        BROOKESIA_LOGE("Failed to get access token");
        return false;
    }

    auto &robot = info.robots[get_data<DataType::BotIndex>()];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wwrite-strings"
    // Use a static array to ensure lifetime and prevent temporary array from being overwritten
    static const char *subscribe_events[] = {
        "conversation.chat.requires_action", NULL
    };
    esp_coze_chat_config_t chat_config = ESP_COZE_CHAT_DEFAULT_CONFIG();
    chat_config.pull_task_stack_size = 5 * 1024;
    chat_config.push_task_core = 1;
    chat_config.enable_subtitle = true;
    chat_config.subscribe_event = subscribe_events;
    chat_config.user_id = const_cast<char *>(auth_info.user_id.c_str());
    chat_config.bot_id = const_cast<char *>(robot.bot_id.c_str());
    chat_config.voice_id = const_cast<char *>(robot.voice_id.c_str());
    chat_config.access_token = const_cast<char *>(access_token.c_str());
    chat_config.uplink_audio_type = get_uplink_audio_type(get_audio_config().encoder.type);
    chat_config.downlink_audio_type = get_downlink_audio_type(get_audio_config().decoder.type);
    chat_config.audio_callback = audio_data_callback;
    chat_config.audio_callback_ctx = this;
    chat_config.event_callback = audio_event_callback;
    chat_config.event_callback_ctx = this;
    chat_config.ws_event_callback = websocket_event_callback;
#pragma GCC diagnostic pop

    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_coze_chat_init(&chat_config, &chat_handle_), false, "Failed to init chat");
    BROOKESIA_CHECK_ESP_ERR_RETURN(esp_coze_chat_start(chat_handle_), false, "Failed to start chat");
    is_chat_started_ = true;

    return true;
}

void Coze::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_chat_initialized()) {
        BROOKESIA_LOGD("Chat is not initialized, skip");
        return;
    }

    is_chat_connected_ = false;

    if (is_chat_started()) {
        BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_coze_chat_stop(chat_handle_), {}, {
            BROOKESIA_LOGE("Failed to stop chat");
        });
        is_chat_started_ = false;
    }
    BROOKESIA_CHECK_ESP_ERR_EXECUTE(esp_coze_chat_deinit(chat_handle_), {}, {
        BROOKESIA_LOGE("Failed to deinit chat");
    });
    chat_handle_ = nullptr;

    trigger_general_event(GeneralEvent::Stopped);
}

bool Coze::on_sleep()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    trigger_general_event(GeneralEvent::Slept);

    return true;
}

void Coze::on_wakeup()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    trigger_general_event(GeneralEvent::Awake);
}

bool Coze::on_encoder_data_ready(const uint8_t *data, size_t data_size)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), data_size(%2%)", data, data_size);

    if (!is_chat_started() || !is_chat_connected()) {
        // BROOKESIA_LOGD("Chat is not started, skip");
        return true;
    }

    auto result = esp_coze_chat_send_audio_data(
                      chat_handle_, const_cast<char *>(reinterpret_cast<const char *>(data)),
                      static_cast<int>(data_size)
                  );
    BROOKESIA_CHECK_ESP_ERR_RETURN(result, false, "Failed to send audio data");

    return true;
}

bool Coze::set_info(const boost::json::object &info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: info(%1%)", BROOKESIA_DESCRIBE_TO_STR(info));

    try_load_data();

    CozeInfo coze_info;

    auto success = BROOKESIA_DESCRIBE_FROM_JSON(info, coze_info);
    BROOKESIA_CHECK_FALSE_RETURN(
        success, false, "Failed to deserialize coze info: %1%", BROOKESIA_DESCRIBE_TO_STR(info)
    );

    BROOKESIA_CHECK_FALSE_RETURN(validate_info(coze_info), false, "Failed to validate coze info");

    auto current_info_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(get_data<DataType::Info>());
    auto new_info_str = BROOKESIA_DESCRIBE_JSON_SERIALIZE(coze_info);
    if (current_info_str == new_info_str) {
        BROOKESIA_LOGI("Info is the same, skip setting");
        return true;
    }

    set_data<DataType::Info>(std::move(coze_info));

    try_save_data(DataType::Info);

    return true;
}

bool Coze::reset_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    is_data_loaded_ = false;
    set_data<DataType::BotIndex>(0);
    set_data<DataType::Info>(CozeInfo{});

    try_erase_data();

    BROOKESIA_LOGI("Reset all data");

    return true;
}

std::expected<void, std::string> Coze::function_set_active_robot_index(double index)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto &robots = get_data<DataType::Info>().robots;
    if (index < 0 || index >= robots.size()) {
        return std::unexpected(
                   (boost::format("Invalid robot index: %1% (size: %2%)") % index % robots.size()).str()
               );
    }

    if (get_data<DataType::BotIndex>() == static_cast<uint8_t>(index)) {
        BROOKESIA_LOGD("Active robot index is the same, skip");
        return {};
    }

    set_data<DataType::BotIndex>(static_cast<uint8_t>(index));

    try_save_data(DataType::BotIndex);

    return {};
}

std::expected<double, std::string> Coze::function_get_active_robot_index()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return static_cast<double>(get_data<DataType::BotIndex>());
}

std::expected<boost::json::array, std::string> Coze::function_get_robot_infos()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    return BROOKESIA_DESCRIBE_TO_JSON(get_data<DataType::Info>().robots).as_array();
}

void Coze::try_load_data()
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

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::Info);
        auto result = NVSHelper::get_key_value<CozeInfo>(get_attributes().name, key);
        if (!result) {
            BROOKESIA_LOGD("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::Info>(std::move(result.value()));
            BROOKESIA_LOGD("Loaded '%1%' from NVS", key);
        }
    }

    {
        auto key = BROOKESIA_DESCRIBE_TO_STR(DataType::BotIndex);
        auto result = NVSHelper::get_key_value<uint8_t>(get_attributes().name, key);
        if (!result) {
            BROOKESIA_LOGD("Failed to load '%1%' from NVS: %2%", key, result.error());
        } else {
            set_data<DataType::BotIndex>(result.value());
            BROOKESIA_LOGD("Loaded '%1%' from NVS", key);
        }
    }

    is_data_loaded_ = true;

    BROOKESIA_LOGI("Loaded all data from NVS");
}

void Coze::try_save_data(DataType type)
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
        auto result = NVSHelper::save_key_value(get_attributes().name, key, data_value, NVS_SAVE_DATA_TIMEOUT_MS);
        if (!result) {
            BROOKESIA_LOGE("Failed to save '%1%' to NVS: %2%", key, result.error());
        } else {
            BROOKESIA_LOGI("Saved '%1%' to NVS", key);
        }
    };

    if (type == DataType::Info) {
        save_function(get_data<DataType::Info>());
    } else if (type == DataType::BotIndex) {
        save_function(get_data<DataType::BotIndex>());
    } else {
        BROOKESIA_LOGE("Invalid data type for saving to NVS");
    }
}

void Coze::try_erase_data()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!NVSHelper::is_available()) {
        BROOKESIA_LOGD("NVS is not available, skip");
        return;
    }

    auto result = NVSHelper::erase_keys(get_attributes().name, {}, NVS_ERASE_DATA_TIMEOUT_MS);
    if (!result) {
        BROOKESIA_LOGE("Failed to erase NVS data: %1%", result.error());
    } else {
        BROOKESIA_LOGI("Erased NVS data");
    }
}

bool Coze::validate_info(CozeInfo &info)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Process authorization info
    auto &auth_info = info.authorization;
    BROOKESIA_CHECK_FALSE_RETURN(!auth_info.app_id.empty(), false, "app_id is empty");
    BROOKESIA_CHECK_FALSE_RETURN(!auth_info.public_key.empty(), false, "public_key is empty");
    BROOKESIA_CHECK_FALSE_RETURN(!auth_info.private_key.empty(), false, "private_key is empty");

    std::string mac_str;
    BROOKESIA_CHECK_FALSE_RETURN(get_mac_str(mac_str), false, "Failed to get MAC address");
    BROOKESIA_LOGD("Get MAC address: %s", mac_str.c_str());

    if (auth_info.session_name.empty()) {
        auth_info.session_name = mac_str;
    }
    if (auth_info.device_id.empty()) {
        auth_info.device_id = mac_str;
    }
    if (auth_info.user_id.empty()) {
        auth_info.user_id = mac_str;
    }

    // Process robot infos
    auto &robots = info.robots;
    robots.erase(std::remove_if(robots.begin(), robots.end(), [](const CozeRobotInfo & robot) {
        if (robot.name.empty() || robot.bot_id.empty() || robot.voice_id.empty()) {
            BROOKESIA_LOGW("Remove invalid robot: %s", BROOKESIA_DESCRIBE_TO_STR(robot));
            return true;
        }
        return false;
    }), robots.end());

    return true;
}

bool Coze::get_mac_str(std::string &mac_str)
{
    uint8_t mac[6] = {0};
    esp_err_t err = esp_efuse_mac_get_default(mac);
    BROOKESIA_CHECK_FALSE_RETURN(err == ESP_OK, false, "Failed to get MAC address(%s)", esp_err_to_name(err));

    char mac_hex[13];
    snprintf(mac_hex, sizeof(mac_hex), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    mac_str = "ESP_" + std::string(mac_hex);

    return true;
}

bool Coze::on_audio_data(char *data, int len)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // BROOKESIA_LOGD("Params: data(%1%), len(%2%)", data, len);

    if (!is_chat_connected() || !is_chat_started()) {
        // BROOKESIA_LOGD("Chat is not connected or started, skip");
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        feed_audio_decoder_data(reinterpret_cast<const uint8_t *>(data), len), false, "Failed to feed audio data"
    );

    return true;
}

bool Coze::on_audio_event(esp_coze_chat_event_t event, char *data)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::function<void(void)> task_func;

    if (event == ESP_COZE_CHAT_EVENT_CHAT_ERROR) {
        BROOKESIA_LOGE("chat error: %s", data);

        int code = parse_error_code(std::string_view(data));
        BROOKESIA_CHECK_FALSE_RETURN(code != -1, false, "Failed to parse chat error code");

        auto error_it = ERROR_CODE_TYPE_MAP.find(code);
        if (error_it == ERROR_CODE_TYPE_MAP.end()) {
            BROOKESIA_LOGD("Unknown error code: %d", code);
            return true;
        }

        auto result = publish_service_event(BROOKESIA_DESCRIBE_TO_STR(AgentCozeHelper::EventId::CozeEventHappened),
        service::EventItemMap{
            {
                BROOKESIA_DESCRIBE_ENUM_TO_STR(AgentCozeHelper::EventCozeEventHappenedParam::CozeEvent),
                BROOKESIA_DESCRIBE_ENUM_TO_STR(error_it->second)
            }
        }, true);
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish error occurred event");

        // Set task function to stop agent
        task_func = [this]() {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
            trigger_general_event(GeneralEvent::Stopped);
        };
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STARTED) {
        BROOKESIA_LOGI("chat start");
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SPEECH_STOPED) {
        BROOKESIA_LOGI("chat stop");
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_COMPLETED) {
        BROOKESIA_LOGI("chat complete");
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_CUSTOMER_DATA) {
        BROOKESIA_LOGD("Customer data: %s", data);
    } else if (event == ESP_COZE_CHAT_EVENT_CHAT_SUBTITLE_EVENT) {
        // BROOKESIA_LOGD("chat subtitle event: %s", data);
        BROOKESIA_CHECK_NULL_RETURN(data, false, "Invalid data");

        auto emote = get_emote(std::string_view(data));
        if (emote.empty()) {
            // BROOKESIA_LOGD("No emoji found, skip");
            return true;
        }

        BROOKESIA_LOGI("Got emote: %1%", emote);
        auto result = publish_service_event(BROOKESIA_DESCRIBE_TO_STR(AgentManagerHelper::EventId::EmoteGot),
        service::EventItemMap{
            {
                BROOKESIA_DESCRIBE_ENUM_TO_STR(AgentManagerHelper::EventEmoteGotParam::Emote), emote
            }
        });
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to publish emote got event");
    }

    if (task_func) {
        auto group = Manager::get_instance().get_state_task_group();
        auto scheduler = get_service_scheduler();
        BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Scheduler is not available");
        auto result = scheduler->post(std::move(task_func), nullptr, group);
        BROOKESIA_CHECK_FALSE_RETURN(result, false, "Failed to post task function");
    }

    return true;
}

bool Coze::on_websocket_event(esp_coze_ws_event_t event)
{
    // BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    auto event_id = event.event_id;
    // BROOKESIA_LOGD("Params: event_id(%1%)", static_cast<int>(event_id));

    if (!is_chat_started()) {
        BROOKESIA_LOGD("Chat is not started, ignore websocket event");
        return true;
    }

    GeneralEvent target_event = GeneralEvent::Max;

    switch (event_id) {
    case WEBSOCKET_EVENT_CONNECTED:
        BROOKESIA_LOGI("Websocket connected");
        is_chat_connected_ = true;
        target_event = GeneralEvent::Started;
        break;
    case WEBSOCKET_EVENT_CLOSED:
        BROOKESIA_LOGI("Websocket closed");
        is_chat_connected_ = false;
        // Stop agent
        target_event = GeneralEvent::Stopped;
        break;
    case WEBSOCKET_EVENT_DISCONNECTED: {
        BROOKESIA_LOGE("Websocket disconnected");
        // Stop agent
        target_event = GeneralEvent::Stopped;
        break;
    }
    case WEBSOCKET_EVENT_ERROR: {
        BROOKESIA_LOGE("Websocket error");
        // Stop agent
        target_event = GeneralEvent::Stopped;
        break;
    }
    default:
        break;
    }

    if (target_event != GeneralEvent::Max) {
        trigger_general_event(target_event);
    }

    return true;
}

esp_coze_chat_audio_type_t Coze::get_uplink_audio_type(service::helper::Audio::CodecFormat codec_format)
{
    switch (codec_format) {
    case AudioHelper::CodecFormat::OPUS:
        return ESP_COZE_CHAT_AUDIO_TYPE_OPUS;
    case AudioHelper::CodecFormat::G711A:
        return ESP_COZE_CHAT_AUDIO_TYPE_G711A;
    default:
        return ESP_COZE_CHAT_AUDIO_TYPE_PCM;
    }
}

esp_coze_chat_audio_type_t Coze::get_downlink_audio_type(service::helper::Audio::CodecFormat codec_format)
{
    switch (codec_format) {
    case AudioHelper::CodecFormat::OPUS:
        return ESP_COZE_CHAT_AUDIO_TYPE_OPUS;
    case AudioHelper::CodecFormat::G711A:
        return ESP_COZE_CHAT_AUDIO_TYPE_G711A;
    default:
        return ESP_COZE_CHAT_AUDIO_TYPE_PCM;
    }
}

void Coze::audio_data_callback(char *data, int len, void *ctx)
{
    // BROOKESIA_LOG_TRACE_GUARD();

    auto self = static_cast<Coze *>(ctx);
    BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");

    BROOKESIA_CHECK_FALSE_EXIT(self->on_audio_data(data, len), "Failed to on audio data");
}

void Coze::audio_event_callback(esp_coze_chat_event_t event, char *data, void *ctx)
{
    // BROOKESIA_LOG_TRACE_GUARD();

    auto self = static_cast<Coze *>(ctx);
    BROOKESIA_CHECK_NULL_EXIT(self, "Invalid context");

    BROOKESIA_CHECK_FALSE_EXIT(self->on_audio_event(event, data), "Failed to on audio event");
}

void Coze::websocket_event_callback(esp_coze_ws_event_t *event)
{
    // BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_CHECK_NULL_EXIT(event, "Invalid event");

    auto scheduler = get_instance().get_service_scheduler();
    BROOKESIA_CHECK_NULL_EXIT(scheduler, "Scheduler is not available");

    auto task = [event = *event]() {
        // BROOKESIA_LOG_TRACE_GUARD();
        BROOKESIA_CHECK_FALSE_EXIT(get_instance().on_websocket_event(event), "Failed to on websocket event");
    };
    auto result = scheduler->post(task, nullptr, Manager::get_instance().get_state_task_group());
    BROOKESIA_CHECK_FALSE_EXIT(result, "Failed to post websocket event task");
}

int Coze::parse_error_code(std::string_view data)
{
    BROOKESIA_LOG_TRACE_GUARD();

    boost::system::error_code ec;
    boost::json::value json_root = boost::json::parse(data.data(), ec);
    BROOKESIA_CHECK_FALSE_RETURN(!ec, -1, "Failed to parse JSON data: %1%", ec.message());
    BROOKESIA_CHECK_FALSE_RETURN(!json_root.is_object(), -1, "JSON root is not an object");

    const auto &json_obj = json_root.as_object();
    auto data_it = json_obj.find("data");
    if (data_it == json_obj.end() || !data_it->value().is_object()) {
        BROOKESIA_CHECK_FALSE_RETURN(false, -1, "No data found in JSON or data is not an object");
    }

    const auto &data_obj = data_it->value().as_object();
    auto code_it = data_obj.find("code");
    if (code_it == data_obj.end() || !code_it->value().is_number()) {
        BROOKESIA_CHECK_FALSE_RETURN(false, -1, "No code found in JSON or code is not a number");
    }

    return static_cast<int>(code_it->value().as_int64());
}

std::string Coze::generate_random_string(size_t length)
{
    constexpr std::string_view charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    constexpr size_t charset_size = charset.size();

    std::string result;
    result.reserve(length);

    for (size_t i = 0; i < length; i++) {
        int key = esp_random() % charset_size;
        result += charset[key];
    }

    return result;
}

std::string Coze::get_access_token(const CozeAuthInfo &auth_info)
{
    BROOKESIA_LOG_TRACE_GUARD();

    // Construct JWT payload
    std::string random_str = generate_random_string(32);

    time_t now = time(nullptr);
    boost::json::object device_info;
    device_info["device_id"] = auth_info.device_id;
    device_info["custom_consumer"] = auth_info.custom_consumer;

    boost::json::object session_context;
    session_context["device_info"] = std::move(device_info);

    boost::json::object payload;
    payload["iss"] = auth_info.app_id;
    payload["aud"] = "api.coze.cn";
    payload["iat"] = static_cast<std::int64_t>(now);
    payload["exp"] = static_cast<std::int64_t>(now + 6000);
    payload["jti"] = random_str;
    payload["session_name"] = auth_info.session_name;
    payload["session_context"] = std::move(session_context);

    // Serialize to JSON string
    std::string payload_str = boost::json::serialize(payload);
    BROOKESIA_LOGD("payload_str: %1%", payload_str);
    BROOKESIA_LOGD("formatted_payload_str: %1%", boost::json::serialize(boost::json::value(payload)));

    // Create JWT
    std::unique_ptr<char, decltype(&free)> jwt(
        coze_jwt_create_handler(
            auth_info.public_key.c_str(),
            payload_str.c_str(),
            reinterpret_cast<const uint8_t *>(auth_info.private_key.c_str()),
            auth_info.private_key.length()
        ),
        &free
    );

    if (!jwt) {
        BROOKESIA_LOGE("Failed to create JWT");
        return std::string();
    }

    // Construct Authorization header
    std::string authorization = "Bearer ";
    authorization += jwt.get();

    // Construct HTTP request JSON
    boost::json::object http_req_json;
    http_req_json["duration_seconds"] = 86399;
    http_req_json["grant_type"] = "urn:ietf:params:oauth:grant-type:jwt-bearer";
    std::string http_req_json_str = boost::json::serialize(http_req_json);

    // Prepare HTTP headers (need to be modified strings)
    std::vector<char> authorization_buf(authorization.begin(), authorization.end());
    authorization_buf.push_back('\0');
    std::vector<char> http_req_json_buf(http_req_json_str.begin(), http_req_json_str.end());
    http_req_json_buf.push_back('\0');

    http_req_header_t header[] = {
        {"Content-Type", "application/json"},
        {"Authorization", authorization_buf.data()},
        {nullptr, nullptr}
    };

    // Send HTTP POST request
    http_response_t response = {0};
    esp_err_t ret = http_client_post(AUTHORIZATION_URL.data(), header, http_req_json_buf.data(), &response);

    if (ret != ESP_OK) {
        BROOKESIA_LOGE("HTTP POST failed");
        return std::string();
    }

    // Use RAII to manage response.body
    std::unique_ptr<char, decltype(&free)> response_body_guard(
        response.body,
        &free
    );

    if (!response.body) {
        BROOKESIA_LOGE("Response body is null");
        return std::string();
    }

    BROOKESIA_LOGD("response: %1%", response.body);

    // Parse response JSON
    boost::system::error_code ec;
    boost::json::value response_json = boost::json::parse(response.body, ec);
    if (ec) {
        BROOKESIA_LOGE("Failed to parse JSON response: %1%", ec.message());
        return std::string();
    }

    if (!response_json.is_object()) {
        BROOKESIA_LOGE("Response JSON is not an object");
        return std::string();
    }

    const auto &response_obj = response_json.as_object();
    std::string access_token;

    // Extract access_token
    auto access_token_it = response_obj.find("access_token");
    if (access_token_it != response_obj.end() && access_token_it->value().is_string()) {
        const auto &token_str = access_token_it->value().as_string();
        access_token = std::string(token_str.c_str(), token_str.size());
        BROOKESIA_LOGD("access_token: %1%", access_token);
    } else {
        BROOKESIA_LOGE("access_token is invalid or not exist");
    }

    // Record other fields (for debugging)
    auto expires_in_it = response_obj.find("expires_in");
    if (expires_in_it != response_obj.end() && expires_in_it->value().is_number()) {
        BROOKESIA_LOGD("expires_in: %1%", expires_in_it->value().as_int64());
    }

    auto token_type_it = response_obj.find("token_type");
    if (token_type_it != response_obj.end() && token_type_it->value().is_string()) {
        // const auto &token_type_str = token_type_it->value().as_string();
        // BROOKESIA_LOGD("token_type: %1%", std::string(token_type_str.c_str(), token_type_str.size()));
    }

    return access_token;
}

std::string Coze::get_emote(std::string_view data)
{
    constexpr std::string_view prefix = "（";
    constexpr std::string_view suffix = "）";
    constexpr size_t prefix_len = prefix.length();
    constexpr size_t suffix_len = suffix.length();

    // Check if string starts with prefix and ends with suffix
    if (data.length() >= prefix_len + suffix_len && data.substr(0, prefix_len) == prefix &&
            data.substr(data.length() - suffix_len) == suffix) {

        // Extract the middle part (remove prefix and suffix)
        std::string emoji_str(data.substr(prefix_len, data.length() - prefix_len - suffix_len));

        // Check if emoji_str is wrapped with colons
        if (emoji_str.length() >= 2 && emoji_str.front() == ':' && emoji_str.back() == ':') {
            // Remove colons
            emoji_str = emoji_str.substr(1, emoji_str.length() - 2);
            return emoji_str;
        }
    }

    return std::string();
}

BROOKESIA_PLUGIN_REGISTER_SINGLETON(Base, Coze, Coze::DEFAULT_AGENT_ATTRIBUTES.name, Coze::get_instance());

} // namespace esp_brookesia::agent
