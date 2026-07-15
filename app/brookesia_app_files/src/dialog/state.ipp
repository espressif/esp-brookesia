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

void FileManagerApp::hide_file_operation_dialog(system::core::AppContext &context)
{
    if (file_operation_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        return;
    }

    auto hide_result = context.system_service().hide_message_dialog(file_operation_dialog_request_id_);
    if (!hide_result) {
        BROOKESIA_LOGW("Failed to hide File Manager operation dialog: %1%", hide_result.error());
    }
    file_operation_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
}

void FileManagerApp::cancel_file_operation(system::core::AppContext &context)
{
    if (file_operation_dialog_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(file_operation_dialog_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop File Manager operation dialog timer: %1%",
                file_operation_dialog_timer_id_
            );
        }
        file_operation_dialog_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    if (file_operation_commit_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(file_operation_commit_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop File Manager operation commit timer: %1%",
                file_operation_commit_timer_id_
            );
        }
        file_operation_commit_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    pending_file_operation_.reset();
    hide_file_operation_dialog(context);
}
