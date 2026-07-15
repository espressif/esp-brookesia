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

std::expected<void, std::string> FileManagerApp::show_file_operation_dialog(system::core::AppContext &context)
{
    if (!pending_file_operation_) {
        return {};
    }

    const auto text_key = pending_file_operation_->kind == FileOperationKind::Rename ?
                          "rename_in_progress" : "delete_in_progress";
    auto options = system::core::MessageDialogOptions{
        .text = tr(text_key),
        .informative_text = pending_file_operation_->source_path.generic_string(),
        .icon = system::core::MessageDialogIcon::Information,
        .buttons = {},
        .auto_close_ms = 0,
    };

    if (file_operation_dialog_request_id_ != system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        auto update_result = context.system_service().update_message_dialog(
                                 file_operation_dialog_request_id_,
                                 options
                             );
        if (update_result) {
            return {};
        }
        BROOKESIA_LOGW("Failed to update File Manager operation dialog: %1%", update_result.error());
        file_operation_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    }

    auto dialog_result_handler = [this](const system::core::MessageDialogResult & result) {
        if (file_operation_dialog_request_id_ == result.request_id) {
            file_operation_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
        }
    };
    auto request = context.system_service().show_message_dialog(
                       std::move(options),
                       std::move(dialog_result_handler)
                   );
    if (!request) {
        return std::unexpected(request.error());
    }

    file_operation_dialog_request_id_ = *request;
    return {};
}
