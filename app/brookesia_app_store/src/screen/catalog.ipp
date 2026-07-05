/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::optional<uint64_t> AppStoreApp::store_entry_package_size(const StoreEntry &entry) const
{
    if (auto size = display_size_from_file_size(entry.cached_package_size)) {
        return size;
    }
    if (entry.download_size > 0) {
        return entry.download_size;
    }
    return std::nullopt;
}

std::optional<uint64_t> AppStoreApp::find_installed_package_size(std::string_view manifest_id) const
{
    if (manifest_id.empty()) {
        return std::nullopt;
    }

    const auto local_package_it = local_package_by_manifest_id_.find(std::string(manifest_id));
    if (local_package_it != local_package_by_manifest_id_.end() && local_package_it->second < local_packages_.size()) {
        const auto &package = local_packages_[local_package_it->second];
        if (package.schema_error.empty()) {
            if (auto size = display_size_from_file_size(package.file_size)) {
                return size;
            }
        }
    }

    for (const auto &entry : entries_) {
        if (entry.package_name == manifest_id) {
            if (auto size = store_entry_package_size(entry)) {
                return size;
            }
        }
    }
    return std::nullopt;
}

std::string AppStoreApp::make_size_detail(uint64_t bytes) const
{
    if (bytes == 0) {
        return {};
    }
    return tr("detail.size_prefix") + format_bytes(bytes);
}

void AppStoreApp::clear_entry_views(system::core::AppContext &context)
{
    cancel_refresh_icon_update(context);
    stop_size_metadata_timer(context);
    cancel_size_metadata_requests();
    for (auto it = entry_paths_.rbegin(); it != entry_paths_.rend(); ++it) {
        (void)context.gui().destroy_view(*it);
    }
    entry_paths_.clear();
    visible_items_.clear();
    instance_to_entry_.clear();
    list_slot_count_ = 0;
    list_page_ = 0;
    for (auto &entry : entries_) {
        entry.item_path.clear();
    }
    for (auto &entry : local_packages_) {
        entry.item_path.clear();
    }
    for (auto &entry : installed_runtime_apps_) {
        entry.item_path.clear();
    }
}
std::expected<void, std::string> AppStoreApp::populate_entries(system::core::AppContext &context)
{
    visible_items_ = build_visible_items();
    clamp_list_page();

    const auto page_size = get_list_page_size();
    for (size_t i = list_slot_count_; i < page_size; ++i) {
        const auto instance_id = "app_" + std::to_string(i);
        auto created = context.gui().create_view(ITEM_TEMPLATE_ID, LIST_PATH, instance_id);
        if (!created) {
            clear_entry_views(context);
            return std::unexpected(created.error());
        }
        entry_paths_.push_back(join_path(LIST_PATH, instance_id));
        ++list_slot_count_;
    }

    instance_to_entry_.clear();
    for (auto &entry : entries_) {
        entry.item_path.clear();
    }
    for (auto &entry : local_packages_) {
        entry.item_path.clear();
    }
    for (auto &entry : installed_runtime_apps_) {
        entry.item_path.clear();
    }

    const auto page_start = get_page_start(list_page_);
    const auto visible_count = get_page_visible_count(visible_items_.size(), list_page_);
    for (size_t i = 0; i < list_slot_count_ && i < visible_count; ++i) {
        const auto instance_id = "app_" + std::to_string(i);
        const auto path = join_path(LIST_PATH, instance_id);
        auto ref = visible_items_[page_start + i];
        if (ref.kind == VisibleItemKind::Store) {
            entries_[ref.index].item_path = path;
        } else if (ref.kind == VisibleItemKind::Local) {
            local_packages_[ref.index].item_path = path;
        } else {
            installed_runtime_apps_[ref.index].item_path = path;
        }
        instance_to_entry_.emplace(instance_id, ref);
    }
    auto result = refresh_ui(context);
    if (!result) {
        return result;
    }
    if (auto scroll_result = scroll_list_to_top(context); !scroll_result) {
        BROOKESIA_LOGW("Failed to scroll App Store list to top: %1%", scroll_result.error());
    }
    start_visible_size_metadata_requests(context);
    start_visible_icon_update(context);
    return {};
}
std::expected<void, std::string> AppStoreApp::refresh_ui(system::core::AppContext &context)
{
    if (auto i18n_result = refresh_i18n(context); !i18n_result) {
        BROOKESIA_LOGW("Failed to refresh App Store i18n: %1%", i18n_result.error());
    }

    std::vector<gui::BindingValueUpdate> updates;
    const auto visible_count = visible_items_.size();
    add_binding(
        updates,
        TITLE_PATH,
        "title",
        tr("screen.title") + " · " + std::to_string(visible_count) +
        (visible_count == 1 ? tr("title.app_suffix") : tr("title.apps_suffix"))
    );
    std::string status = status_text_;
    if (visible_count == 0) {
        if (view_mode_ == ViewMode::Store) {
            status = tr("status.all_apps_installed");
        } else if (view_mode_ == ViewMode::Local) {
            status = tr("status.no_local_packages");
        } else {
            status = tr("status.no_runtime_apps");
        }
    }
    add_binding(updates, STATUS_PATH, "status", status);
    add_binding(updates, STORAGE_PATH, "storage", storage_text_);
    add_binding(updates, REFRESH_LABEL_PATH, "label", tr("action.refresh"));
    add_binding(updates, REFRESH_BUTTON_PATH, "commonProps.disabled", "false");
    add_binding(updates, REFRESH_BUTTON_PATH, "style.opacity", std::to_string(ENABLED_OPACITY));

    const auto add_tab = [&updates](std::string_view path, std::string_view label_path, std::string label, bool active) {
        add_binding(updates, std::string(label_path), "label", std::move(label));
        add_binding(updates, std::string(path), "style.opacity", std::to_string(active ? ENABLED_OPACITY : INACTIVE_OPACITY));
    };
    add_tab(TAB_STORE_PATH, TAB_STORE_LABEL_PATH, tr("tab.store"), view_mode_ == ViewMode::Store);
    add_tab(
        TAB_INSTALLED_PATH,
        TAB_INSTALLED_LABEL_PATH,
        tr("tab.installed"),
        view_mode_ == ViewMode::Installed
    );
    add_tab(TAB_LOCAL_PATH, TAB_LOCAL_LABEL_PATH, tr("tab.local"), view_mode_ == ViewMode::Local);

    const bool hide_store_list = (view_mode_ == ViewMode::Store) && !network_ready_;
    add_binding(updates, "/app_store/page/list", "commonProps.hidden", bool_value(hide_store_list));

    const auto page_count = get_page_count(visible_items_.size());
    const auto show_pager = visible_items_.size() > get_list_page_size();
    const auto can_prev = show_pager && list_page_ > 0;
    const auto can_next = show_pager && list_page_ + 1 < page_count;
    add_binding(updates, PAGER_PATH, "commonProps.hidden", bool_value(!show_pager));
    add_binding(updates, PAGER_PREV_PATH, "commonProps.disabled", bool_value(!can_prev));
    add_binding(
        updates,
        PAGER_PREV_PATH,
        "style.opacity",
        std::to_string(can_prev ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    add_binding(updates, PAGER_LABEL_PATH, "labelProps.text", make_page_text(list_page_, page_count));
    add_binding(updates, PAGER_NEXT_PATH, "commonProps.disabled", bool_value(!can_next));
    add_binding(
        updates,
        PAGER_NEXT_PATH,
        "style.opacity",
        std::to_string(can_next ? ENABLED_OPACITY : DISABLED_OPACITY)
    );

    for (size_t i = 0; i < list_slot_count_; ++i) {
        add_binding(updates, join_path(LIST_PATH, "app_" + std::to_string(i)), "commonProps.hidden", "true");
    }

    const auto add_item = [&updates](
                              const std::string & item_path,
                              const std::string & title,
                              const std::string & description,
                              const std::string & detail,
                              const std::string & state,
                              const std::string & action_label,
                              bool action_enabled,
                              const std::string & icon_resource_id,
                              const std::string & fallback_label
    ) {
        const auto icon_box_path = item_path + "/body/icon_box";
        const auto content_path = item_path + "/body/content";
        const auto actions_path = item_path + "/body/actions";
        add_binding(updates, item_path, "commonProps.hidden", "false");
        add_binding(updates, content_path + "/title", "title", title);
        add_binding(updates, content_path + "/description", "description", description);
        add_binding(updates, content_path + "/detail", "detail", detail);
        add_binding(updates, content_path + "/state", "state", state);
        add_binding(updates, actions_path + "/primary/label", "label", action_label);
        add_binding(updates, actions_path + "/primary", "commonProps.disabled", bool_value(!action_enabled));
        add_binding(
            updates,
            actions_path + "/primary",
            "style.opacity",
            std::to_string(action_enabled ? ENABLED_OPACITY : DISABLED_OPACITY)
        );
        if (!icon_resource_id.empty()) {
            add_binding(updates, icon_box_path + "/icon", "src", icon_resource_id);
            add_binding(updates, icon_box_path + "/icon", "hidden", "false");
            add_binding(updates, icon_box_path + "/fallback", "hidden", "true");
        } else {
            add_binding(updates, icon_box_path + "/icon", "src", "");
            add_binding(updates, icon_box_path + "/icon", "hidden", "true");
            add_binding(updates, icon_box_path + "/fallback", "hidden", "false");
            add_binding(updates, icon_box_path + "/fallback", "label", first_utf8_character(fallback_label));
        }
    };

    for (const auto &[_, ref] : instance_to_entry_) {
        if (ref.kind == VisibleItemKind::Store && ref.index < entries_.size()) {
            const auto &entry = entries_[ref.index];
            if (entry.item_path.empty()) {
                continue;
            }
            const auto can_install = entry.cached && !entry.cached_package_path.empty();
            const auto can_download = http_available_ && (!entry.installed || entry.update_available) &&
                                      !entry.package_name.empty() && !entry.latest_version.empty() &&
                                      (!entry.metadata_url.empty() || !entry.download_url.empty()) &&
                                      entry.schema_error != "manifest_id must match package_name";
            const auto action_enabled = !entry.metadata_loading && (entry.downloading || can_install || can_download);
            int progress = 0;
            if (entry.downloading) {
                progress = entry.total > 0 ?
                           static_cast<int>((static_cast<uint64_t>(entry.downloaded) * 100U) / entry.total) :
                           1;
                progress = std::clamp(progress, 1, 99);
            }
            std::string detail;
            if (entry.update_available) {
                append_detail(detail, "Installed: " + entry.installed_version);
                append_detail(detail, "Latest: " + entry.latest_version);
            } else if (!entry.latest_version.empty()) {
                append_detail(detail, "Latest: " + entry.latest_version);
            }
            if (!entry.updated_at.empty()) {
                append_detail(detail, "Updated: " + entry.updated_at);
            }
            if (!entry.categories.empty()) {
                append_detail(detail, "Categories: " + string_array_join(entry.categories));
            }
            if (auto size = store_entry_package_size(entry)) {
                append_detail(detail, make_size_detail(*size));
            }
            std::string state = entry.schema_error.empty() ? tr("state.remote") : entry.schema_error;
            std::string action_label = entry.update_available ? tr("action.update") : tr("action.install");
            if (entry.downloading) {
                state = tr("state.downloading");
                action_label = tr("status.downloading_prefix") + std::to_string(progress) + "%";
            } else if (can_install) {
                state = tr("state.local_bpk");
            }
            const auto title = localized(entry.app_names);
            add_item(
                entry.item_path,
                title,
                localized(entry.descriptions),
                detail,
                state,
                action_label,
                action_enabled,
                entry.icon_resource_id,
                title
            );
        } else if (ref.kind == VisibleItemKind::Local && ref.index < local_packages_.size()) {
            const auto &entry = local_packages_[ref.index];
            if (entry.item_path.empty()) {
                continue;
            }
            const auto title = localized(entry.app_names);
            std::string detail = "Manifest: " + entry.manifest_id;
            if (!entry.version.empty()) {
                append_detail(detail, "Version: " + entry.version);
            }
            if (auto size = display_size_from_file_size(entry.file_size)) {
                append_detail(detail, make_size_detail(*size));
            }
            const auto invalid = !entry.schema_error.empty();
            const auto local_action = get_local_package_action(entry);
            auto state = invalid ? entry.schema_error : tr("state.local_bpk");
            auto action_label = tr("action.install");
            if (entry.installed && local_action == LocalPackageAction::None) {
                state = tr("state.installed");
                action_label = tr("state.installed");
            } else if (local_action == LocalPackageAction::Update) {
                state = tr("state.update_available");
                action_label = tr("action.update");
            }
            add_item(
                entry.item_path,
                title,
                tr("state.local_package"),
                detail,
                state,
                action_label,
                local_action != LocalPackageAction::None,
                entry.icon_resource_id,
                title
            );
        } else if (ref.kind == VisibleItemKind::Installed && ref.index < installed_runtime_apps_.size()) {
            const auto &entry = installed_runtime_apps_[ref.index];
            if (entry.item_path.empty()) {
                continue;
            }
            const auto title = system::core::resolve_app_display_name(entry.app.manifest, current_language());
            std::string detail = "Manifest: " + entry.app.manifest.id;
            if (!entry.app.manifest.version.empty()) {
                append_detail(detail, "Version: " + entry.app.manifest.version);
            }
            if (auto size = find_installed_package_size(entry.app.manifest.id)) {
                append_detail(detail, make_size_detail(*size));
            }
            std::string state = tr("state.installed");
            for (const auto &remote_entry : entries_) {
                if (remote_entry.package_name == entry.app.manifest.id && remote_entry.update_available) {
                    append_detail(detail, "Update available: " + remote_entry.latest_version);
                    state = tr("state.update_available");
                    break;
                }
            }
            append_detail(detail, "State: " + BROOKESIA_DESCRIBE_TO_STR(entry.app.state));
            add_item(
                entry.item_path,
                title,
                tr("state.runtime_app"),
                detail,
                tr("state.installed"),
                tr("action.uninstall"),
                true,
                entry.icon_resource_id,
                title
            );
        }
    }

    if (updates.empty()) {
        return {};
    }

    system::core::GuiBatchCommand command;
    command.type = system::core::GuiBatchCommand::Type::SetBindings;
    command.binding_updates = std::move(updates);
    auto result = context.gui().execute_batch({command});
    if (!result) {
        return std::unexpected(result.error());
    }
    if (!result->success) {
        for (const auto &command_result : result->results) {
            if (!command_result.success) {
                return std::unexpected(
                           command_result.error_message.empty() ?
                           "Failed to refresh App Store GUI bindings" :
                           command_result.error_message
                       );
            }
        }
        return std::unexpected("Failed to refresh App Store GUI bindings");
    }
    return {};
}
std::expected<void, std::string> AppStoreApp::scroll_list_to_top(system::core::AppContext &context)
{
    system::core::GuiBatchCommand command;
    command.type = system::core::GuiBatchCommand::Type::ScrollToView;
    command.path = LIST_TOP_ANCHOR_PATH;
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
                       "Failed to scroll App Store list to top" :
                       command_result.error_message
                   );
        }
    }
    return std::unexpected("Failed to scroll App Store list to top");
}
std::expected<void, std::string> AppStoreApp::refresh_entry(system::core::AppContext &context, size_t index)
{
    if (index >= entries_.size()) {
        return {};
    }
    return refresh_ui(context);
}

bool AppStoreApp::should_fetch_size_metadata(const StoreEntry &entry) const
{
    return !store_entry_package_size(entry) && !entry.size_metadata_loading && !entry.size_metadata_failed &&
           !entry.downloading && !entry.metadata_loading && (!entry.installed || entry.update_available) &&
           !entry.package_name.empty() && !entry.latest_version.empty() && !entry.metadata_url.empty() &&
           entry.schema_error != "manifest_id must match package_name";
}

bool AppStoreApp::should_fetch_icon(const StoreEntry &entry) const
{
    return entry.icon_request_id == 0 && entry.icon_resource_id.empty() && !entry.icon_url.empty() &&
           !entry.package_name.empty();
}

bool AppStoreApp::has_size_metadata_request_in_flight() const
{
    for (const auto &entry : entries_) {
        if (entry.size_metadata_loading) {
            return true;
        }
    }
    return false;
}

std::optional<size_t> AppStoreApp::find_next_visible_size_metadata_index() const
{
    if (view_mode_ != ViewMode::Store) {
        return std::nullopt;
    }

    const auto page_start = get_page_start(list_page_);
    const auto visible_count = get_page_visible_count(visible_items_.size(), list_page_);
    for (size_t i = 0; i < visible_count; ++i) {
        const auto &ref = visible_items_[page_start + i];
        if (ref.kind == VisibleItemKind::Store && ref.index < entries_.size() &&
                should_fetch_size_metadata(entries_[ref.index])) {
            return ref.index;
        }
    }
    return std::nullopt;
}

void AppStoreApp::process_size_metadata_step(system::core::AppContext &context)
{
    if (!http_available_ || view_mode_ != ViewMode::Store) {
        stop_size_metadata_timer(context);
        cancel_size_metadata_requests();
        return;
    }
    if (has_size_metadata_request_in_flight()) {
        return;
    }

    while (auto index = find_next_visible_size_metadata_index()) {
        if (start_size_metadata_request(context, *index)) {
            return;
        }
    }
}

void AppStoreApp::schedule_size_metadata_step(system::core::AppContext &context, int delay_ms)
{
    if (size_metadata_timer_id_ != system::core::INVALID_TIMER_ID) {
        return;
    }

    auto timer = context.timer().start_delayed(SIZE_METADATA_TIMER_NAME, delay_ms);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store size metadata step: %1%", timer.error());
        if (delay_ms <= SIZE_METADATA_STEP_DELAY_MS) {
            process_size_metadata_step(context);
        }
        return;
    }
    size_metadata_timer_id_ = *timer;
}

void AppStoreApp::stop_size_metadata_timer(system::core::AppContext &context)
{
    if (size_metadata_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (!context.timer().stop(size_metadata_timer_id_)) {
        BROOKESIA_LOGW("Failed to stop App Store size metadata timer: %1%", size_metadata_timer_id_);
    }
    size_metadata_timer_id_ = system::core::INVALID_TIMER_ID;
}

void AppStoreApp::start_visible_size_metadata_requests(system::core::AppContext &context)
{
    if (!http_available_) {
        stop_size_metadata_timer(context);
        cancel_size_metadata_requests();
        return;
    }
    if (view_mode_ != ViewMode::Store) {
        stop_size_metadata_timer(context);
        cancel_size_metadata_requests();
        return;
    }

    std::unordered_set<size_t> visible_store_indices;
    for (const auto &[_, ref] : instance_to_entry_) {
        if (ref.kind == VisibleItemKind::Store && ref.index < entries_.size()) {
            visible_store_indices.insert(ref.index);
        }
    }

    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].size_metadata_loading && !visible_store_indices.contains(i)) {
            cancel_size_metadata_request(entries_[i]);
        }
    }

    process_size_metadata_step(context);
}

void AppStoreApp::handle_primary_action(const gui::Event &event)
{
    if (context_ == nullptr) {
        return;
    }
    auto ref = find_visible_item_by_path(event.path);
    if (!ref) {
        return;
    }
    if (ref->kind == VisibleItemKind::Store) {
        start_or_cancel_download(*context_, ref->index);
    } else if (ref->kind == VisibleItemKind::Local) {
        install_local_package(*context_, ref->index);
    } else {
        uninstall_installed_app(*context_, ref->index);
    }
}
