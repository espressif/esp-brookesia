/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "private/esp_brookesia_ai_expression_utils.hpp"
#include "esp_brookesia_ai_expression.hpp"

namespace esp_brookesia::ai_framework {

Expression::~Expression()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_CHECK_FALSE_EXIT(!_flags.is_begun || del(), "Del failed");
}

bool Expression::begin(const ExpressionData &data, const EmojiMap *emoji_map, const SystemIconMap *system_icon_map)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    if (_flags.is_begun) {
        ESP_UTILS_LOGD("Already begun");
        return true;
    }

    esp_utils::function_guard del_guard([this]() {
        ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

        ESP_UTILS_CHECK_FALSE_EXIT(del(), "Del failed");
    });

    if (data.flags.enable_emotion) {
        auto animation_num = data.emotion.data.getAnimationNum();
        ESP_UTILS_CHECK_FALSE_RETURN(animation_num > 0, false, "Invalid emotion animation num");
        ESP_UTILS_CHECK_NULL_RETURN(emoji_map, false, "Invalid emoji map");

        auto &emoji_map_tmp = *emoji_map;
        for (auto &[emoji, emotion_icon] : emoji_map_tmp) {
            ESP_UTILS_CHECK_VALUE_RETURN(
                emotion_icon.first, EMOTION_TYPE_NONE, animation_num - 1, false, "Emotion index out of data range"
            );
            ESP_UTILS_CHECK_VALUE_RETURN(
                emotion_icon.second, EMOTION_TYPE_NONE, animation_num - 1, false, "Icon index out of data range"
            );
        }

        _emoji_map = emoji_map_tmp;
        _emotion_player = std::make_unique<gui::AnimPlayer>();
        ESP_UTILS_CHECK_NULL_RETURN(_emotion_player, false, "Invalid emotion player");
        ESP_UTILS_CHECK_FALSE_RETURN(_emotion_player->begin(data.emotion.data), false, "Emotion player begin failed");
    }
    if (data.flags.enable_icon) {
        auto animation_num = data.icon.data.getAnimationNum();
        ESP_UTILS_CHECK_FALSE_RETURN(animation_num > 0, false, "Invalid icon animation num");
        ESP_UTILS_CHECK_NULL_RETURN(system_icon_map, false, "Invalid system icon map");

        auto &system_icon_map_tmp = *system_icon_map;
        for (auto &[icon, icon_type] : system_icon_map_tmp) {
            ESP_UTILS_CHECK_VALUE_RETURN(
                icon_type, ICON_TYPE_NONE, animation_num - 1, false, "Icon index out of data range"
            );
        }

        _system_icon_map = system_icon_map_tmp;
        _icon_player = std::make_unique<gui::AnimPlayer>();
        ESP_UTILS_CHECK_NULL_RETURN(_icon_player, false, "Invalid icon player");
        ESP_UTILS_CHECK_FALSE_RETURN(_icon_player->begin(data.icon.data), false, "Icon player begin failed");
    }

    del_guard.release();

    _flags.is_begun = true;
    return true;
}

bool Expression::del()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    _flags = {};
    _emotion_player = nullptr;
    _icon_player = nullptr;
    _emotion_operation_before_pause = gui::AnimPlayer::Operation::PlayOnceStop;
    _icon_operation_before_pause = gui::AnimPlayer::Operation::PlayOnceStop;
    _emotion_type_before_pause = EMOTION_TYPE_NONE;
    _icon_type_before_pause = ICON_TYPE_NONE;

    return true;
}

bool Expression::pause()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    if (_flags.is_paused) {
        ESP_UTILS_LOGW("Already paused");
        return true;
    }

    if (_emotion_player != nullptr) {
        ESP_UTILS_CHECK_FALSE_RETURN(_emotion_player->sendEvent({
            .index = EMOTION_TYPE_NONE,
            .operation = gui::AnimPlayer::Operation::Pause,
            .flags = {
                .enable_interrupt = true,
                .force = false,
            },
        }, true), false, "Send emotion event failed");
    }
    if (_icon_player != nullptr) {
        ESP_UTILS_CHECK_FALSE_RETURN(_icon_player->sendEvent( {
            .index = ICON_TYPE_NONE,
            .operation = gui::AnimPlayer::Operation::Pause,
            .flags = {
                .enable_interrupt = true,
                .force = false,
            },
        }, true), false, "Send icon event failed");
    }

    _flags.is_paused = true;
    return true;
}

bool Expression::resume(bool update_emotion, bool update_icon)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    ESP_UTILS_LOGD(
        "Param: update_emotion(%d), update_icon(%d)", update_emotion, update_icon
    );
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    if (!_flags.is_paused) {
        ESP_UTILS_LOGD("Not paused");
        return true;
    }

    _flags.is_paused = false;
    if ((_emotion_player != nullptr) && update_emotion) {
        ESP_UTILS_LOGD(
            "Emotion before pause: type(%d), operation(%d)", _emotion_type_before_pause,
            static_cast<int>(_emotion_operation_before_pause)
        );
        ESP_UTILS_CHECK_FALSE_RETURN(_emotion_player->sendEvent({
            .index = _emotion_type_before_pause,
            .operation = _emotion_operation_before_pause,
            .flags = {
                .enable_interrupt = true,
                .force = true,
            },
        }, true), false, "Send emotion event failed");
    }
    if ((_icon_player != nullptr) && update_icon) {
        ESP_UTILS_LOGD(
            "Icon before pause: type(%d), operation(%d)", _icon_type_before_pause,
            static_cast<int>(_icon_operation_before_pause)
        );
        ESP_UTILS_CHECK_FALSE_RETURN(_icon_player->sendEvent({
            .index = _icon_type_before_pause,
            .operation = _icon_operation_before_pause,
            .flags = {
                .enable_interrupt = true,
                .force = true,
            },
        }, true), false, "Send icon event failed");
    }

    return true;
}

void Expression::emojiTimerCallback(TimerHandle_t timer)
{
    auto expression = static_cast<Expression *>(pvTimerGetTimerID(timer));
    if (expression != nullptr && !expression->_flags.is_paused) {
        expression->setEmoji(expression->_last_emoji, expression->_last_emotion_config, expression->_last_icon_config);
        ESP_UTILS_LOGI("Emoji timer callback: set emoji to %s", expression->_last_emoji.c_str());
    }
    xTimerDelete(timer, 0);
    expression->timer = nullptr;
}

bool Expression::insertEmojiTemporary(const std::string &emoji, uint32_t duration_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (timer) {
        ESP_UTILS_LOGW("Emoji timer already exists");
        return true;
    }
    std::string emoji_tmp = this->_last_emoji;
    AnimOperationConfig emotion_config = this->_last_emotion_config;
    AnimOperationConfig icon_config = this->_last_icon_config;
    setEmoji(emoji, AnimOperationConfig{}, AnimOperationConfig{.en = false});
    this->_last_emoji = emoji_tmp;
    this->_last_emotion_config = emotion_config;
    this->_last_icon_config = icon_config;
    timer = xTimerCreate("insertEmoji", pdMS_TO_TICKS(duration_ms), pdFALSE, this, emojiTimerCallback);
    ESP_UTILS_CHECK_FALSE_RETURN(timer != nullptr, false, "Failed to create emoji timer");
    xTimerStart(timer, 0);
    return true;
}

bool Expression::setEmoji(
    const std::string &emoji, const AnimOperationConfig &emotion_config, const AnimOperationConfig &icon_config
)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    std::lock_guard lock(_mutex);

    ESP_UTILS_LOGD(
        "Param: emoji(%s), "
        "emotion_config(repeat(%d), keep_when_stop(%d), immediate(%d)), "
        "icon_config(repeat(%d), keep_when_stop(%d), immediate(%d))",
        emoji.c_str(),
        emotion_config.repeat, emotion_config.keep_when_stop, emotion_config.immediate,
        icon_config.repeat, icon_config.keep_when_stop, icon_config.immediate
    );
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    ESP_UTILS_CHECK_FALSE_RETURN(_emoji_map.size() > 0, false, "Emoji map not enabled");

    auto it = _emoji_map.find(emoji);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _emoji_map.end(), false, "Unknown emoji");
    _last_emoji = emoji;
    _last_emotion_config = emotion_config;
    _last_icon_config = icon_config;

    if (_emotion_player != nullptr && emotion_config.en) {
        auto emotion_type = it->second.first;
        gui::AnimPlayer::Operation emotion_operation = gui::AnimPlayer::Operation::Stop;
        if (emotion_type != EMOTION_TYPE_NONE) {
            emotion_operation = emotion_config.repeat ? gui::AnimPlayer::Operation::PlayLoop :
                                (emotion_config.keep_when_stop ? gui::AnimPlayer::Operation::PlayOncePause :
                                 gui::AnimPlayer::Operation::PlayOnceStop);
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            setEmotion(emotion_type, emotion_operation, emotion_config.immediate), false, "Set emoji emotion failed"
        );
    }

    if (_icon_player != nullptr && icon_config.en) {
        auto icon_type = it->second.second;
        gui::AnimPlayer::Operation icon_operation = gui::AnimPlayer::Operation::Stop;
        if (icon_type != ICON_TYPE_NONE) {
            icon_operation = icon_config.repeat ? gui::AnimPlayer::Operation::PlayLoop :
                             (icon_config.keep_when_stop ? gui::AnimPlayer::Operation::PlayOncePause :
                              gui::AnimPlayer::Operation::PlayOnceStop);
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            setIcon(icon_type, icon_operation, icon_config.immediate), false, "Set emoji icon failed"
        );
    }

    return true;
}

bool Expression::setSystemIcon(const std::string &icon, const AnimOperationConfig &config)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: icon(%s), config(repeat(%d), keep_when_stop(%d), immediate(%d))", icon.c_str(),
        config.repeat, config.keep_when_stop, config.immediate
    );
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    ESP_UTILS_CHECK_FALSE_RETURN(_system_icon_map.size() > 0, false, "System icon map not enabled");

    auto it = _system_icon_map.find(icon);
    ESP_UTILS_CHECK_FALSE_RETURN(it != _system_icon_map.end(), false, "Unknown icon");

    if (_icon_player != nullptr) {
        auto icon_type = it->second;
        gui::AnimPlayer::Operation operation = gui::AnimPlayer::Operation::Stop;
        if (icon_type != ICON_TYPE_NONE) {
            operation = config.repeat ? gui::AnimPlayer::Operation::PlayLoop :
                        (config.keep_when_stop ? gui::AnimPlayer::Operation::PlayOncePause :
                         gui::AnimPlayer::Operation::PlayOnceStop);
        }
        ESP_UTILS_CHECK_FALSE_RETURN(
            setIcon(icon_type, operation, config.immediate), false, "Set system icon failed"
        );
    }

    return true;
}

bool Expression::setEmotion(EmotionType type, gui::AnimPlayer::Operation operation, bool immediate)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: type(%d), operation(%d), immediate(%d)", type, static_cast<int>(operation), immediate
    );
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    ESP_UTILS_CHECK_NULL_RETURN(_emotion_player, false, "Emotion player not enabled");

    if (_flags.is_paused) {
        ESP_UTILS_LOGW("Already paused");
        goto end;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(_emotion_player->sendEvent({
        .index = type,
        .operation = operation,
        .flags = {
            .enable_interrupt = immediate,
        },
    }, true), false, "Send emotion event failed");

end:
    _emotion_type_before_pause = type;
    _emotion_operation_before_pause = operation;
    return true;
}

bool Expression::setIcon(IconType type, gui::AnimPlayer::Operation operation, bool immediate)
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    ESP_UTILS_LOGD(
        "Param: type(%d), operation(%d), immediate(%d)", type, static_cast<int>(operation), immediate
    );
    ESP_UTILS_CHECK_FALSE_RETURN(_flags.is_begun, false, "Not begun");
    ESP_UTILS_CHECK_NULL_RETURN(_icon_player, false, "Icon player not enabled");

    if (_flags.is_paused) {
        ESP_UTILS_LOGW("Already paused");
        goto end;
    }

    ESP_UTILS_CHECK_FALSE_RETURN(_icon_player->sendEvent({
        .index = type,
        .operation = operation,
        .flags = {
            .enable_interrupt = immediate,
        },
    }, true), false, "Send icon event failed");

end:
    _icon_type_before_pause = type;
    _icon_operation_before_pause = operation;
    return true;
}

} // namespace esp_brookesia::ai_framework
