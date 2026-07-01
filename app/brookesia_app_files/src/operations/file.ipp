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
    auto keyboard = context.system_service().show_keyboard(
    system::core::KeyboardRequestOptions{
        .title = tr("rename_title"),
        .placeholder = tr("rename_placeholder"),
        .initial_text = selected.name,
        .max_length = 96,
        .password = false,
        .mode = "text",
        .allowed_modes = {},
    },
    [this](const system::core::KeyboardResult & result) {
        if (!context_ || !result.confirmed || !selected_entry_) {
            return;
        }
        const auto new_name = result.text;
        if (new_name.empty() || new_name.find('/') != std::string::npos || new_name == "." || new_name == "..") {
            show_message(*context_, tr("rename_failed"), tr("invalid_name"), system::core::MessageDialogIcon::Warning);
            return;
        }
        const auto old_path = selected_entry_->path.lexically_normal();
        const auto old_kind = selected_entry_->kind;
        auto destination = selected_entry_->path.parent_path() / new_name;
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
        BROOKESIA_LOGI(
            "Rename File Manager item: from(%1%) to(%2%)",
            old_path.generic_string(),
            destination.generic_string()
        );
        std::error_code error_code;
        std::filesystem::rename(selected_entry_->path, destination, error_code);
        if (error_code) {
            show_message(*context_, tr("rename_failed"), error_code.message(), system::core::MessageDialogIcon::Warning);
            return;
        }
        if (old_kind == EntryKind::Directory) {
            rebase_current_directory_after_move(old_path, destination);
        }
        BROOKESIA_LOGI("Renamed File Manager item: path(%1%)", destination.generic_string());
        (void)hide_operations(*context_);
        auto refresh_result = refresh_entries(*context_);
        if (!refresh_result) {
            show_message(*context_, tr("refresh_failed"), refresh_result.error(), system::core::MessageDialogIcon::Warning);
        }
    }
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
    const auto title = tr("delete_title_prefix") + selected_entry_->name + tr("delete_title_suffix");
    auto dialog = context.system_service().show_message_dialog(
    system::core::MessageDialogOptions{
        .text = title,
        .informative_text = selected_entry_->path.generic_string(),
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
    },
    [this](const system::core::MessageDialogResult & result) {
        if (!context_ || !selected_entry_ ||
                result.button_role != system::core::MessageDialogButtonRole::Destructive) {
            return;
        }
        const auto removed_path = selected_entry_->path.lexically_normal();
        const auto removed_kind = selected_entry_->kind;
        BROOKESIA_LOGI("Delete File Manager item: path(%1%)", removed_path.generic_string());
        std::error_code error_code;
        if (selected_entry_->kind == EntryKind::Directory) {
            std::filesystem::remove_all(selected_entry_->path, error_code);
        } else {
            std::filesystem::remove(selected_entry_->path, error_code);
        }
        if (error_code) {
            show_message(*context_, tr("delete_failed"), error_code.message(), system::core::MessageDialogIcon::Warning);
            return;
        }
        if (removed_kind == EntryKind::Directory) {
            recover_current_directory_after_remove(removed_path);
        }
        BROOKESIA_LOGI("Deleted File Manager item: path(%1%)", removed_path.generic_string());
        (void)hide_operations(*context_);
        auto refresh_result = refresh_entries(*context_);
        if (!refresh_result) {
            show_message(*context_, tr("refresh_failed"), refresh_result.error(), system::core::MessageDialogIcon::Warning);
        }
    }
                  );
    if (!dialog) {
        show_message(context, tr("delete_failed"), dialog.error(), system::core::MessageDialogIcon::Warning);
    }
}
