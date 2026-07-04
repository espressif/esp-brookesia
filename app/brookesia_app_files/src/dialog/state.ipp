/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void FileManagerApp::reset_message_dialog(system::core::AppContext &context)
{
    if (message_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        return;
    }
    (void)context.system_service().hide_message_dialog(message_dialog_request_id_);
    message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
}
