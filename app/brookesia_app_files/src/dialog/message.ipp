/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void FileManagerApp::show_message(
    system::core::AppContext &context,
    std::string text,
    std::string informative_text,
    system::core::MessageDialogIcon icon
)
{
    reset_message_dialog(context);
    auto request = context.system_service().show_message_dialog(
    system::core::MessageDialogOptions{
        .text = std::move(text),
        .informative_text = std::move(informative_text),
        .icon = icon,
        .buttons = {
            system::core::MessageDialogButton{
                .text = tr("close"),
                .role = system::core::MessageDialogButtonRole::Reject,
            },
        },
        .auto_close_ms = MESSAGE_AUTO_CLOSE_MS,
    },
    [this](const system::core::MessageDialogResult & result) {
        if (message_dialog_request_id_ == result.request_id) {
            message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
        }
    }
                   );
    if (request) {
        message_dialog_request_id_ = *request;
    } else {
        BROOKESIA_LOGW("Failed to show File Manager message dialog: %1%", request.error());
    }
}
