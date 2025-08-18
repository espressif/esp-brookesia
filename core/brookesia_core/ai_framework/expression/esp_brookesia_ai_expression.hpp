/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <string>
#include "boost/thread.hpp"
#include "gui/anim_player/esp_brookesia_anim_player.hpp"

namespace esp_brookesia::ai_framework {

struct ExpressionData {
    struct {
        gui::AnimPlayerData data;
    } emotion;
    struct {
        gui::AnimPlayerData data;
    } icon;
    struct {
        int enable_emotion: 1;
        int enable_icon: 1;
    } flags;
};

class Expression {
public:
    struct AnimOperationConfig {
        bool en = true;
        bool repeat = true;
        bool keep_when_stop = false;
        bool immediate = true;
    };

    using EmotionType = int;
    using IconType = int;
    using EmojiMap = std::map<std::string, std::pair<EmotionType, IconType>>;
    using SystemIconMap = std::map<std::string, IconType>;

    static constexpr int EMOTION_TYPE_NONE = gui::AnimPlayer::INDEX_NONE;
    static constexpr int ICON_TYPE_NONE = gui::AnimPlayer::INDEX_NONE;

    Expression(const Expression &) = delete;
    Expression(Expression &&) = delete;
    Expression &operator=(const Expression &) = delete;
    Expression &operator=(Expression &&) = delete;

    Expression() = default;
    ~Expression();

    bool begin(const ExpressionData &data, const EmojiMap *emoji_map, const SystemIconMap *system_icon_map);
    bool del();
    bool pause();
    bool resume(bool update_emotion, bool update_icon);

    bool setEmoji(
        const std::string &emoji, const AnimOperationConfig &emotion_config, const AnimOperationConfig &icon_config
    );
    bool setEmoji(const std::string &emoji)
    {
        return setEmoji(emoji, AnimOperationConfig{}, AnimOperationConfig{});
    }
    bool insertEmojiTemporary(const std::string &emoji, uint32_t duration_ms = 1000);
    bool setSystemIcon(const std::string &icon, const AnimOperationConfig &config);
    bool setSystemIcon(const std::string &icon)
    {
        return setSystemIcon(icon, AnimOperationConfig{});
    }

private:
    bool setEmotion(EmotionType type, gui::AnimPlayer::Operation operation, bool immediate);
    bool setIcon(IconType type, gui::AnimPlayer::Operation operation, bool immediate);
    static void emojiTimerCallback(TimerHandle_t timer);

    struct {
        int is_begun: 1;
        int is_paused: 1;
    } _flags = {};
    std::mutex _mutex;

    EmojiMap _emoji_map;
    SystemIconMap _system_icon_map;
    std::string _last_emoji;
    AnimOperationConfig _last_emotion_config;
    AnimOperationConfig _last_icon_config;
    TimerHandle_t timer;

    EmotionType _emotion_type_before_pause = EMOTION_TYPE_NONE;
    gui::AnimPlayer::Operation _emotion_operation_before_pause = gui::AnimPlayer::Operation::PlayOnceStop;
    std::unique_ptr<gui::AnimPlayer> _emotion_player;

    IconType _icon_type_before_pause = ICON_TYPE_NONE;
    gui::AnimPlayer::Operation _icon_operation_before_pause = gui::AnimPlayer::Operation::PlayOnceStop;
    std::unique_ptr<gui::AnimPlayer> _icon_player;
};

} // namespace esp_brookesia::ai_framework
