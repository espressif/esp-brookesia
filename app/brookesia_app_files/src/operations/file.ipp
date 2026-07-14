/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void FileManagerApp::request_rename(system::core::AppContext &context)
{
    if (!can_operate_selected()) {
        show_message(
            context,
            tr("rename_unavailable"),
            tr("select_first"),
            system::core::MessageDialogIcon::Warning
        );
        return;
    }
    auto selected = *selected_entry_;
    auto keyboard_result_handler = [this, selected](const system::core::KeyboardResult & result) {
        if (!context_ || !result.confirmed) {
            return;
        }
        const auto new_name = result.text;
        if (new_name.empty() || new_name.find('/') != std::string::npos || new_name == "." || new_name == "..") {
            show_message(*context_, tr("rename_failed"), tr("invalid_name"), system::core::MessageDialogIcon::Warning);
            return;
        }
        const auto old_path = selected.path.lexically_normal();
        auto destination = selected.path.parent_path() / new_name;
        destination = destination.lexically_normal();
        if (!is_inside_current_root(destination)) {
            show_message(
                *context_,
                tr("rename_failed"),
                tr("destination_escapes"),
                system::core::MessageDialogIcon::Critical
            );
            return;
        }

        auto operation = PendingFileOperation{
            .kind = FileOperationKind::Rename,
            .entry_kind = selected.kind,
            .source_path = old_path,
            .destination_path = destination,
        };
        auto schedule_result = schedule_file_operation(*context_, std::move(operation));
        if (!schedule_result) {
            show_message(
                *context_,
                tr("rename_failed"),
                schedule_result.error(),
                system::core::MessageDialogIcon::Warning
            );
        }
    };
    auto keyboard_options = system::core::KeyboardRequestOptions{
        .title = tr("rename_title"),
        .placeholder = tr("rename_placeholder"),
        .initial_text = selected.name,
        .max_length = 96,
        .password = false,
        .mode = "text",
        .allowed_modes = {},
    };
    auto keyboard = context.system_service().show_keyboard(
                        std::move(keyboard_options),
                        std::move(keyboard_result_handler)
                    );
    if (!keyboard) {
        show_message(context, tr("rename_failed"), keyboard.error(), system::core::MessageDialogIcon::Warning);
    }
}

void FileManagerApp::request_delete(system::core::AppContext &context)
{
    if (!can_operate_selected()) {
        show_message(
            context,
            tr("delete_unavailable"),
            tr("select_first"),
            system::core::MessageDialogIcon::Warning
        );
        return;
    }
    auto selected = *selected_entry_;
    const auto title = tr("delete_title_prefix") + selected.name + tr("delete_title_suffix");
    auto dialog_result_handler = [this, selected](const system::core::MessageDialogResult & result) {
        if (!context_ || result.button_role != system::core::MessageDialogButtonRole::Destructive) {
            return;
        }

        auto operation = PendingFileOperation{
            .kind = FileOperationKind::Delete,
            .entry_kind = selected.kind,
            .source_path = selected.path.lexically_normal(),
            .destination_path = {},
        };
        auto schedule_result = schedule_file_operation(*context_, std::move(operation));
        if (!schedule_result) {
            show_message(
                *context_,
                tr("delete_failed"),
                schedule_result.error(),
                system::core::MessageDialogIcon::Warning
            );
        }
    };
    auto dialog_options = system::core::MessageDialogOptions{
        .text = title,
        .informative_text = selected.path.generic_string(),
        .icon = system::core::MessageDialogIcon::Warning,
        .buttons = {
            system::core::MessageDialogButton{
                .text = tr("delete"),
                .role = system::core::MessageDialogButtonRole::Destructive,
            },
            system::core::MessageDialogButton{
                .text = tr("cancel"),
                .role = system::core::MessageDialogButtonRole::Reject,
            },
        },
        .auto_close_ms = 0,
    };
    auto dialog = context.system_service().show_message_dialog(
                      std::move(dialog_options),
                      std::move(dialog_result_handler)
                  );
    if (!dialog) {
        show_message(context, tr("delete_failed"), dialog.error(), system::core::MessageDialogIcon::Warning);
    }
}

std::expected<void, std::string> FileManagerApp::schedule_file_operation(
    system::core::AppContext &context,
    PendingFileOperation operation
)
{
    if (pending_file_operation_ || file_operation_dialog_timer_id_ != system::core::INVALID_TIMER_ID ||
            file_operation_commit_timer_id_ != system::core::INVALID_TIMER_ID ||
            file_operation_dialog_request_id_ != system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        BROOKESIA_LOGW("Ignore File Manager operation while another operation is running");
        return {};
    }

    reset_message_dialog(context);
    pending_file_operation_ = std::move(operation);
    auto timer = context.timer().start_delayed(FILE_OPERATION_DIALOG_TIMER_NAME, FILE_OPERATION_DEFER_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule File Manager operation dialog: %1%", timer.error());
        auto dialog_result = show_file_operation_dialog(context);
        if (!dialog_result) {
            BROOKESIA_LOGW("Failed to show File Manager operation dialog: %1%", dialog_result.error());
        }
        return schedule_file_operation_commit(context);
    }

    file_operation_dialog_timer_id_ = *timer;
    return {};
}

std::expected<void, std::string> FileManagerApp::schedule_file_operation_commit(
    system::core::AppContext &context
)
{
    if (!pending_file_operation_) {
        hide_file_operation_dialog(context);
        return {};
    }

    auto timer = context.timer().start_delayed(FILE_OPERATION_COMMIT_TIMER_NAME, FILE_OPERATION_DEFER_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule File Manager operation commit: %1%", timer.error());
        return commit_file_operation(context);
    }

    file_operation_commit_timer_id_ = *timer;
    return {};
}

std::expected<void, std::string> FileManagerApp::commit_file_operation(system::core::AppContext &context)
{
    if (!pending_file_operation_) {
        hide_file_operation_dialog(context);
        return {};
    }

    if (file_operation_dialog_timer_id_ != system::core::INVALID_TIMER_ID) {
        (void)context.timer().stop(file_operation_dialog_timer_id_);
        file_operation_dialog_timer_id_ = system::core::INVALID_TIMER_ID;
    }

    auto operation = std::move(*pending_file_operation_);
    pending_file_operation_.reset();

    auto hide_guard = lib_utils::FunctionGuard([this, &context]() {
        hide_file_operation_dialog(context);
    });
    auto show_operation_message = [this, &context](
                                      std::string text,
                                      std::string informative_text,
                                      system::core::MessageDialogIcon icon
                                  ) {
        hide_file_operation_dialog(context);
        show_message(context, std::move(text), std::move(informative_text), icon);
    };

    if (operation.kind == FileOperationKind::Rename) {
        BROOKESIA_LOGI(
            "Rename File Manager item: from(%1%) to(%2%)",
            operation.source_path.generic_string(),
            operation.destination_path.generic_string()
        );
        auto rename_result = StorageHelper::fs_rename(
                                 operation.source_path.generic_string(),
                                 operation.destination_path.generic_string()
                             );
        if (!rename_result) {
            hide_guard.release();
            show_operation_message(
                tr("rename_failed"),
                rename_result.error(),
                system::core::MessageDialogIcon::Warning
            );
            return {};
        }
        if (operation.entry_kind == EntryKind::Directory) {
            rebase_current_directory_after_move(operation.source_path, operation.destination_path);
        }
        BROOKESIA_LOGI("Renamed File Manager item: path(%1%)", operation.destination_path.generic_string());
        (void)hide_operations(context);
        auto refresh_result = refresh_entries(context);
        if (!refresh_result) {
            hide_guard.release();
            show_operation_message(
                tr("refresh_failed"),
                refresh_result.error(),
                system::core::MessageDialogIcon::Warning
            );
        }
        return {};
    }

    BROOKESIA_LOGI("Delete File Manager item: path(%1%)", operation.source_path.generic_string());
    auto remove_result = remove_path_tree(operation.source_path);
    if (!remove_result) {
        BROOKESIA_LOGW(
            "Failed to delete File Manager item: path(%1%), error(%2%)",
            operation.source_path.generic_string(),
            remove_result.error()
        );
        hide_guard.release();
        show_operation_message(
            tr("delete_failed"),
            remove_result.error(),
            system::core::MessageDialogIcon::Warning
        );
        return {};
    }
    if (operation.entry_kind == EntryKind::Directory) {
        recover_current_directory_after_remove(operation.source_path);
    }
    BROOKESIA_LOGI(
        "Deleted File Manager item: path(%1%), entries(%2%)",
        operation.source_path.generic_string(),
        static_cast<unsigned long long>(*remove_result)
    );
    (void)hide_operations(context);
    auto refresh_result = refresh_entries(context);
    if (!refresh_result) {
        hide_guard.release();
        show_operation_message(
            tr("refresh_failed"),
            refresh_result.error(),
            system::core::MessageDialogIcon::Warning
        );
    }
    return {};
}
