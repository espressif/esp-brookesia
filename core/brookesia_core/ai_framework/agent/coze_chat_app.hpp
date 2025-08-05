/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <string>
#include "esp_err.h"
#include "boost/signals2/signal.hpp"

#define COZE_CHAT_ERROR_CODE_INSUFFICIENT_CREDITS_BALANCE_1 (4027)
#define COZE_CHAT_ERROR_CODE_INSUFFICIENT_CREDITS_BALANCE_2 (4028)

struct CozeChatAgentInfo {
    void dump() const;
    bool isValid() const;

    std::string session_name;
    std::string device_id;
    std::string custom_consumer;
    std::string app_id;
    std::string user_id;
    std::string public_key;
    std::string private_key;
};

struct CozeChatRobotInfo {
    void dump() const;
    bool isValid() const;

    std::string name;
    std::string bot_id;
    std::string voice_id;
    std::string description;
};

extern boost::signals2::signal<void(const std::string &emoji)> coze_chat_emoji_signal;
extern boost::signals2::signal<void(bool is_speaking)> coze_chat_speaking_signal;
extern boost::signals2::signal<void(void)> coze_chat_response_signal;
extern boost::signals2::signal<void(bool is_wake_up)> coze_chat_wake_up_signal;
extern boost::signals2::signal<void(void)> coze_chat_websocket_disconnected_signal;
extern boost::signals2::signal<void(int code)> coze_chat_error_signal;

/**
 * @brief  Initialize the Coze chat application
 *
 * @return
 *       - ESP_OK  On success
 *       - Other   Appropriate esp_err_t error code on failure
 */
esp_err_t coze_chat_app_init(void);

esp_err_t coze_chat_app_start(const CozeChatAgentInfo &agent_info, const CozeChatRobotInfo &robot_info);

esp_err_t coze_chat_app_stop(void);

void coze_chat_app_resume(void);

void coze_chat_app_pause(void);

void coze_chat_app_wakeup(void);

void coze_chat_app_sleep(void);

void coze_chat_app_interrupt(void);
