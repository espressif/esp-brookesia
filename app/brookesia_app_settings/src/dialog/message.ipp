/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::MessageDialogOptions SettingsApp::make_message_dialog_options(
    std::string text,
    system::core::MessageDialogIcon icon,
    int32_t auto_close_ms
) const
{
    return system::core::MessageDialogOptions{
        .text = std::move(text),
        .informative_text = {},
        .icon = icon,
        .buttons = {},
        .auto_close_ms = auto_close_ms,
    };
}

void SettingsApp::ensure_message_dialog(
    system::core::AppContext &context,
    std::string text,
    system::core::MessageDialogIcon icon,
    int32_t auto_close_ms
)
{
    auto options = make_message_dialog_options(std::move(text), icon, auto_close_ms);
    if (message_dialog_request_id_ != system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        auto update_result = context.system_service().update_message_dialog(message_dialog_request_id_, options);
        if (update_result) {
            return;
        }
        BROOKESIA_LOGW("Failed to update Settings message dialog: %1%", update_result.error());
        message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    }

    auto request_id = context.system_service().show_message_dialog(
                          std::move(options),
    [this](const system::core::MessageDialogResult & result) {
        if (message_dialog_request_id_ == result.request_id) {
            message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
        }
    }
                      );
    if (!request_id) {
        BROOKESIA_LOGW("Failed to show Settings message dialog: %1%", request_id.error());
        return;
    }
    message_dialog_request_id_ = *request_id;
}

void SettingsApp::update_message_dialog_if_visible(
    system::core::AppContext &context,
    std::string text,
    system::core::MessageDialogIcon icon,
    int32_t auto_close_ms
)
{
    if (message_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        return;
    }

    auto options = make_message_dialog_options(std::move(text), icon, auto_close_ms);
    auto update_result = context.system_service().update_message_dialog(message_dialog_request_id_, std::move(options));
    if (!update_result) {
        BROOKESIA_LOGW("Failed to update Settings message dialog: %1%", update_result.error());
        message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    }
}

void SettingsApp::hide_message_dialog_if_visible(system::core::AppContext &context)
{
    if (message_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        return;
    }

    auto hide_result = context.system_service().hide_message_dialog(message_dialog_request_id_);
    if (!hide_result) {
        BROOKESIA_LOGW("Failed to hide Settings message dialog: %1%", hide_result.error());
    }
    message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
}
