/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void FileManagerApp::clear_entry_views(system::core::AppContext &context)
{
    for (size_t i = 0; i < entry_view_count_; ++i) {
        (void)context.gui().destroy_view(make_entry_path(i));
    }
    entry_view_count_ = 0;
    entries_.clear();
}

std::expected<void, std::string> FileManagerApp::ensure_entry_view_capacity(
    system::core::AppContext &context,
    size_t count
)
{
    while (entry_view_count_ < count) {
        auto instance_id = make_entry_instance_id(entry_view_count_);
        auto view = context.gui().create_view(ENTRY_TEMPLATE_ID, LIST_PATH, instance_id);
        if (!view) {
            return std::unexpected(view.error());
        }
        ++entry_view_count_;
    }
    return {};
}

void FileManagerApp::trim_entry_view_pool(system::core::AppContext &context, size_t visible_count)
{
    if (entry_view_count_ <= ENTRY_POOL_TRIM_MIN_CAPACITY) {
        return;
    }

    const auto target_count = std::max(visible_count * 2, visible_count + ENTRY_POOL_TRIM_SPARE);
    if (entry_view_count_ <= target_count) {
        return;
    }

    for (size_t i = entry_view_count_; i > target_count; --i) {
        const auto path = make_entry_path(i - 1);
        if (!context.gui().destroy_view(path)) {
            BROOKESIA_LOGW("Failed to trim File Manager hidden entry view: path(%1%)", path);
        }
    }
    entry_view_count_ = target_count;
}

std::expected<void, std::string> FileManagerApp::refresh_entries(system::core::AppContext &context)
{
    auto next_entries = build_entries_for_current_location();
    if (!next_entries) {
        return std::unexpected(next_entries.error());
    }

    auto capacity_result = ensure_entry_view_capacity(context, next_entries->size());
    if (!capacity_result) {
        return capacity_result;
    }

    for (size_t i = 0; i < next_entries->size(); ++i) {
        (*next_entries)[i].instance_id = make_entry_instance_id(i);
        (*next_entries)[i].view_path = make_entry_path(i);
    }

    auto previous_entries = std::move(entries_);
    entries_ = std::move(*next_entries);
    auto result = refresh_ui(context);
    if (!result) {
        entries_ = std::move(previous_entries);
        return result;
    }
    trim_entry_view_pool(context, entries_.size());
    if (auto scroll_result = scroll_first_entry_to_top(context); !scroll_result) {
        return scroll_result;
    }
    return {};
}

std::expected<void, std::string> FileManagerApp::scroll_first_entry_to_top(system::core::AppContext &context)
{
    if (entries_.empty()) {
        return {};
    }

    system::core::GuiBatchCommand command;
    command.type = system::core::GuiBatchCommand::Type::ScrollToView;
    command.path = entries_.front().view_path;
    command.animated = false;

    auto batch_result = context.gui().execute_batch({command});
    if (!batch_result) {
        return std::unexpected(batch_result.error());
    }
    if (batch_result->success) {
        return {};
    }
    for (const auto &command_result : batch_result->results) {
        if (!command_result.success) {
            return std::unexpected(
                       command_result.error_message.empty() ?
                       "Failed to scroll File Manager list to first entry" :
                       command_result.error_message
                   );
        }
    }
    return std::unexpected("Failed to scroll File Manager list to first entry");
}

std::expected<void, std::string> FileManagerApp::refresh_ui(system::core::AppContext &context)
{
    if (auto i18n_result = refresh_i18n(context); !i18n_result) {
        BROOKESIA_LOGW("Failed to refresh Files i18n: %1%", i18n_result.error());
    }
    std::vector<gui::BindingValueUpdate> updates;
    add_binding(
        updates,
        HEADER_TITLE_PATH,
        "labelProps.text",
        current_volume_index_ ? volumes_[*current_volume_index_].display_name : tr("storage")
    );
    add_binding(
        updates,
        HEADER_SUBTITLE_PATH,
        "labelProps.text",
        current_volume_index_ ? ("/" + current_directory_.generic_string()) : tr("volumes_subtitle")
    );
    add_binding(
        updates,
        HEADER_COUNT_PATH,
        "labelProps.text",
        item_count_text(entries_.size(), tr("empty"), tr("items_suffix"))
    );

    for (const auto &entry : entries_) {
        add_binding(updates, entry.view_path, "commonProps.hidden", "false");
        add_binding(updates, join_path(entry.view_path, "body/icon"), "imageProps.src", entry.icon);
        add_binding(updates, join_path(entry.view_path, "body/content/title"), "labelProps.text", entry.name);
        add_binding(updates, join_path(entry.view_path, "body/content/detail"), "labelProps.text", entry.detail);
    }
    for (size_t i = entries_.size(); i < entry_view_count_; ++i) {
        add_binding(updates, make_entry_path(i), "commonProps.hidden", "true");
    }

    auto result = context.gui().set_binding_values(updates);
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

std::expected<void, std::string> FileManagerApp::navigate_back(system::core::AppContext &context)
{
    if (!current_volume_index_) {
        return context.system_service().request_close_app(context.app_id());
    }
    if (current_directory_.empty() || current_directory_ == ".") {
        current_volume_index_.reset();
        current_directory_.clear();
    } else {
        current_directory_ = current_directory_.parent_path();
        if (current_directory_ == ".") {
            current_directory_.clear();
        }
    }
    return refresh_entries(context);
}
