/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

system::core::AppManifest FileManagerApp::get_manifest() const
{
    return {
        .id = APP_ID,
        .name = localized_app_name(LOCALE_EN),
        .localized_names = {
            {LOCALE_EN, localized_app_name(LOCALE_EN)},
            {LOCALE_ZH_CN, localized_app_name(LOCALE_ZH_CN)},
        },
        .version = make_app_version(),
        .kind = system::core::AppKind::Native,
        .visible = true,
        .icon_id = APP_ICON_ID,
        .supported_systems = {},
        .icon_path = APP_ICON_PATH,
        .runtime_type = runtime::BackendType::Unknown,
        .app_path = {},
        .entry = {},
        .resource_dir = BROOKESIA_APP_FILES_RESOURCE_DIR,
        .arguments = {},
    };
}

system::core::AppGuiDescriptor FileManagerApp::get_gui_descriptor() const
{
    return {
        .root_kind = system::core::GuiRootKind::File,
        .root = GUI_ROOT,
        .resources = {},
        .screen_flows = {
            system::core::GuiScreenFlowEntry{
                .screen_flow = FLOW_ID,
                .layer = system::core::GuiAppLayer::AppDefault,
            },
            system::core::GuiScreenFlowEntry{
                .screen_flow = HEADER_FLOW_ID,
                .layer = system::core::GuiAppLayer::AppTop,
            },
        },
    };
}

std::expected<void, std::string> FileManagerApp::on_start(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    context_ = &context;
    resource_dir_ = resolve_app_resource_dir(context);
    if (auto i18n_result = refresh_i18n(context); !i18n_result) {
        BROOKESIA_LOGW("Failed to load Files i18n: %1%", i18n_result.error());
    }
    storage_layout_ = context.system_service().get_storage_layout();
    volumes_ = build_volumes();
    bind_storage_service();
    refresh_volume_capacities(context);
    current_volume_index_.reset();
    current_directory_.clear();
    suppressed_entry_click_path_.reset();
    current_page_ = Page::Browser;

    auto result = subscribe_actions(context);
    if (!result) {
        context_ = nullptr;
        return result;
    }
    auto timer = context.timer().start_delayed(INITIAL_REFRESH_TIMER_NAME, INITIAL_REFRESH_DELAY_MS);
    if (timer) {
        initial_refresh_timer_id_ = *timer;
    } else {
        BROOKESIA_LOGW("Failed to start File Manager initial refresh timer: %1%", timer.error());
        return refresh_entries(context);
    }
    return {};
}

std::expected<void, std::string> FileManagerApp::on_stop(system::core::AppContext &context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    ++storage_capacity_generation_;
    if (initial_refresh_timer_id_ != system::core::INVALID_TIMER_ID) {
        (void)context.timer().stop(initial_refresh_timer_id_);
        initial_refresh_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    clear_entry_views(context);
    reset_message_dialog(context);
    entry_click_connection_.disconnect();
    entry_long_press_connection_.disconnect();
    release_storage_service();
    entries_.clear();
    i18n_strings_.clear();
    applied_i18n_locale_.clear();
    resource_dir_.clear();
    selected_entry_.reset();
    suppressed_entry_click_path_.reset();
    current_page_ = Page::Browser;
    context_ = nullptr;
    return {};
}

std::expected<void, std::string> FileManagerApp::on_action(
    system::core::AppContext &context,
    std::string_view action
)
{
    if (action == ACTION_BACK) {
        if (current_page_ == Page::Operations) {
            return hide_operations(context);
        }
        return navigate_back(context);
    }
    if (action == ACTION_RENAME) {
        request_rename(context);
        return {};
    }
    if (action == ACTION_DELETE) {
        request_delete(context);
        return {};
    }
    return {};
}

std::expected<void, std::string> FileManagerApp::on_timer(
    system::core::AppContext &context,
    system::core::TimerId timer_id,
    std::string_view name
)
{
    if (timer_id == initial_refresh_timer_id_ && name == INITIAL_REFRESH_TIMER_NAME) {
        initial_refresh_timer_id_ = system::core::INVALID_TIMER_ID;
        return refresh_entries(context);
    }
    return {};
}

std::expected<void, std::string> FileManagerApp::subscribe_actions(system::core::AppContext &context)
{
    for (const auto *action : {
                ACTION_BACK, ACTION_RENAME, ACTION_DELETE
            }) {
        auto result = context.gui().subscribe_action(action);
        if (!result) {
            return result;
        }
    }

    entry_click_connection_ = context.gui().subscribe_action(ACTION_ENTRY_CLICK, [this](const gui::Event & event) {
        if (!context_) {
            return;
        }
        if (suppressed_entry_click_path_ &&
                (event.path == *suppressed_entry_click_path_ ||
                 std::string(event.path).starts_with(*suppressed_entry_click_path_ + "/"))) {
            BROOKESIA_LOGI("Ignore File Manager click after long press: path(%1%)", event.path);
            suppressed_entry_click_path_.reset();
            return;
        }
        auto entry = find_entry_by_path(event.path);
        if (!entry) {
            return;
        }
        auto result = open_entry(*context_, *entry);
        if (!result) {
            show_message(*context_, tr("open_failed"), result.error(), system::core::MessageDialogIcon::Warning);
        }
    });
    entry_long_press_connection_ = context.gui().subscribe_action(ACTION_ENTRY_LONG, [this](const gui::Event & event) {
        if (!context_) {
            return;
        }
        auto entry = find_entry_by_path(event.path);
        if (!entry || entry->kind == EntryKind::Volume) {
            return;
        }
        suppressed_entry_click_path_ = entry->view_path;
        auto result = show_operations(*context_, *entry);
        if (!result) {
            show_message(*context_, tr("operation_unavailable"), result.error(), system::core::MessageDialogIcon::Warning);
        }
    });
    if (!entry_click_connection_.connected() || !entry_long_press_connection_.connected()) {
        return std::unexpected("Failed to subscribe File Manager entry actions");
    }
    return {};
}
