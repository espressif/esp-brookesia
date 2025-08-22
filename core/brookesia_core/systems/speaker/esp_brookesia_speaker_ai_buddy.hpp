/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "esp_netif.h"
#include "boost/thread.hpp"
#include "agent/esp_brookesia_ai_agent.hpp"
#include "expression/esp_brookesia_ai_expression.hpp"
#include "assets/esp_brookesia_speaker_assets.h"

namespace esp_brookesia::systems::speaker {

class AI_Buddy {
public:
    struct Data {
        struct {
            ai_framework::ExpressionData data;
        } expression;
    };

    enum ExpressionEmotionType {
        ExpressionEmotionTypeNone = ai_framework::Expression::EMOTION_TYPE_NONE,
        ExpressionEmotionAngry = 0,
        ExpressionEmotionBlush,
        ExpressionEmotionFastBlink,
        ExpressionEmotionSlowBlink,
        ExpressionEmotionDizzy,
        ExpressionEmotionHappy,
        ExpressionEmotionSad,
        ExpressionEmotionSleep,
    };
    enum ExpressionIconType {
        ExpressionIconTypeNone = ai_framework::Expression::ICON_TYPE_NONE,
        ExpressionIconSystemBrightnessDown = 0,
        ExpressionIconSystemBrightnessUp,
        ExpressionIconEmotionConfused,
        ExpressionIconEmotionSleep,
        ExpressionIconEmotionThinking,
        ExpressionIconSystemInvalidFile,
        ExpressionIconSystemServerConnected,
        ExpressionIconSystemServerConnecting,
        ExpressionIconSystemVolumeDown,
        ExpressionIconSystemVolumeMute,
        ExpressionIconSystemVolumeUp,
        ExpressionIconSystemWifiDisconnected,
    };

    AI_Buddy(const AI_Buddy &) = delete;
    AI_Buddy(AI_Buddy &&) = delete;
    AI_Buddy &operator=(const AI_Buddy &) = delete;
    AI_Buddy &operator=(AI_Buddy &&) = delete;

    ~AI_Buddy();

    bool begin(const Data &data);
    bool resume();
    bool pause();
    bool del();

    bool isSpeaking() const
    {
        return _flags.is_speaking;
    }

    bool isPause() const
    {
        return _flags.is_pause;
    }

    bool isWiFiValid() const
    {
        return _flags.is_wifi_connected;
    }

    static std::shared_ptr<AI_Buddy> requestInstance();
    static void releaseInstance();

    ai_framework::Expression expression;

    enum class AudioType {
        WifiNeedConnect,
        WifiConnected,
        WifiDisconnected,
        ServerConnected,
        ServerDisconnected,
        ServerConnecting,
        MicOn,
        MicOff,
        WakeUp,
        ResponseLaiLo,
        ResponseWoZaiTingNe,
        ResponseWoZai,
        ResponseZaiNe,
        SleepBaiBaiLo,
        SleepHaoDe,
        SleepWoTuiXiaLe,
        SleepXianZheYangLo,
        InvalidConfig,
        CozeErrorInsufficientCreditsBalance,
        Max,
    };
    struct AudioEvent {
        AudioType type;
        int repeat_count;
        int repeat_interval_ms;
    };
    void sendAudioEvent(const AudioEvent &event);

private:
    struct AudioProcessInfo {
        AudioEvent event;
        int64_t create_time_ms;
        int64_t last_play_time_ms;
    };
    using AudioInfo = std::pair<std::string, int>;
    using RandomAudios = std::vector<std::pair<float, AudioType>>;

    AI_Buddy() = default;

    void stopAudio(AudioType type);
    bool processAudioEvent(AudioProcessInfo &info);
    void playWiFiNeedConnectAudio();
    bool playRandomAudio(const RandomAudios &audios);
    std::string getAudioName(AudioType type) const;

    bool processOnWiFiEvent(int32_t event_id, void *event_data);
    bool processOnIpEvent(int32_t event_id, void *event_data);

    struct {
        int is_begun: 1;
        int is_pause: 1;
        int is_wifi_connected: 1;
        int is_speaking: 1;
        int is_coze_error: 1;
    } _flags = {};
    std::recursive_mutex _mutex;

    std::shared_ptr<ai_framework::Agent> _agent;
    std::vector<boost::signals2::connection> _agent_connections;

    boost::thread _audio_event_thread;
    std::vector<AudioType> _audio_removed_process_infos;
    std::list<AudioProcessInfo> _audio_current_process_infos;
    std::list<AudioProcessInfo> _audio_next_process_infos;
    AudioType _audio_playing_type = AudioType::Max;
    std::recursive_mutex _audio_event_mutex;
    boost::condition_variable_any _audio_event_cv;

    esp_event_handler_instance_t _wifi_event_handler = nullptr;
    esp_event_handler_instance_t _ip_event_handler = nullptr;

    inline static std::mutex _instance_mutex;
    inline static std::shared_ptr<AI_Buddy> _instance;

    inline static ai_framework::Expression::EmojiMap _emoji_map = {
        {"neutral", std::make_pair(ExpressionEmotionSlowBlink, ExpressionIconTypeNone)},
        {"happy", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"laughing", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"funny", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"sad", std::make_pair(ExpressionEmotionSad, ExpressionIconTypeNone)},
        {"angry", std::make_pair(ExpressionEmotionAngry, ExpressionIconTypeNone)},
        {"crying", std::make_pair(ExpressionEmotionSad, ExpressionIconTypeNone)},
        {"loving", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"embarrassed", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconEmotionThinking)},
        {"surprised", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconTypeNone)},
        {"shocked", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconTypeNone)},
        {"thinking", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconEmotionThinking)},
        {"relaxed", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"delicious", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"kissy", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"confident", std::make_pair(ExpressionEmotionHappy, ExpressionIconTypeNone)},
        {"sleepy", std::make_pair(ExpressionEmotionSleep, ExpressionIconEmotionSleep)},
        {"silly", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconTypeNone)},
        {"confused", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconEmotionConfused)},
        {"curious", std::make_pair(ExpressionEmotionFastBlink, ExpressionIconEmotionConfused)},
        {"dizzy", std::make_pair(ExpressionEmotionDizzy, ExpressionIconTypeNone)},
        {"blink", std::make_pair(ExpressionEmotionBlush, ExpressionIconTypeNone)},
    };
    inline static ai_framework::Expression::SystemIconMap _system_icon_map = {
        {"brightness_down", ExpressionIconSystemBrightnessDown},
        {"brightness_up", ExpressionIconSystemBrightnessUp},
        {"invalid_config", ExpressionIconSystemInvalidFile},
        {"server_connected", ExpressionIconSystemServerConnected},
        {"server_connecting", ExpressionIconSystemServerConnecting},
        {"volume_down", ExpressionIconSystemVolumeDown},
        {"volume_mute", ExpressionIconSystemVolumeMute},
        {"volume_up", ExpressionIconSystemVolumeUp},
        {"wifi_disconnected", ExpressionIconSystemWifiDisconnected},
    };
    inline static std::map<AudioType, AudioInfo> _audio_file_map = {
        {AudioType::WifiNeedConnect,      {"file://spiffs/wifi_need_connect.mp3", 4 * 1000}},
        {AudioType::WifiConnected,        {"file://spiffs/wifi_connect_success.mp3", 2 * 1000}},
        {AudioType::WifiDisconnected,     {"file://spiffs/wifi_disconnect.mp3", 4 * 1000}},
        {AudioType::ServerConnected,      {"file://spiffs/server_connected.mp3", 2 * 1000}},
        {AudioType::ServerDisconnected,   {"file://spiffs/server_disconnect.mp3", 2 * 1000}},
        {AudioType::ServerConnecting,     {"file://spiffs/server_connecting.mp3", 3 * 1000}},
        {AudioType::MicOn,                {"file://spiffs/mic_open.mp3", 2 * 1000}},
        {AudioType::MicOff,               {"file://spiffs/mic_close.mp3", 5 * 1000}},
        {AudioType::WakeUp,               {"file://spiffs/wake_up.mp3", 3 * 1000}},
        {AudioType::ResponseLaiLo,        {"file://spiffs/response_lai_lo.mp3", 2 * 1000}},
        {AudioType::ResponseWoZaiTingNe,  {"file://spiffs/response_wo_zai_ting_ne.mp3", 2 * 1000}},
        {AudioType::ResponseWoZai,        {"file://spiffs/response_wo_zai.mp3", 2 * 1000}},
        {AudioType::ResponseZaiNe,        {"file://spiffs/response_zai_ne.mp3", 1 * 1000}},
        {AudioType::SleepBaiBaiLo,        {"file://spiffs/sleep_bai_bai_lo.mp3", 2 * 1000}},
        {AudioType::SleepHaoDe,           {"file://spiffs/sleep_hao_de.mp3", 3 * 1000}},
        {AudioType::SleepWoTuiXiaLe,      {"file://spiffs/sleep_wo_tui_xia_le.mp3", 2 * 1000}},
        {AudioType::SleepXianZheYangLo,   {"file://spiffs/sleep_xian_zhe_yang_lo.mp3", 3 * 1000}},
        {AudioType::InvalidConfig,        {"file://spiffs/invalid_config_file.mp3", 5 * 1000}},
        {AudioType::CozeErrorInsufficientCreditsBalance, {"file://spiffs/coze_error_credits.mp3", 7 * 1000}},
    };
    inline static RandomAudios _response_audios = {
        {0.25, AudioType::ResponseLaiLo},
        {0.25, AudioType::ResponseWoZaiTingNe},
        {0.25, AudioType::ResponseWoZai},
        {0.25, AudioType::ResponseZaiNe},
    };
    inline static RandomAudios _sleep_audios = {
        {0.25, AudioType::SleepBaiBaiLo},
        {0.25, AudioType::SleepHaoDe},
        {0.25, AudioType::SleepWoTuiXiaLe},
        {0.25, AudioType::SleepXianZheYangLo},
    };
};

/* Keep compatibility with old code */
using AI_BuddyData [[deprecated("Use `esp_brookesia::systems::speaker::AI_Buddy::Data` instead")]] =
    AI_Buddy::Data;

} // namespace esp_brookesia::systems::speaker
