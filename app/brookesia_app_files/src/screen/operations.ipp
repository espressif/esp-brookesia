/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> FileManagerApp::show_operations(
    system::core::AppContext &context,
    const Entry &entry
)
{
    selected_entry_ = entry;
    BROOKESIA_LOGI("Show File Manager operations: path(%1%)", entry.path.generic_string());
    std::vector<gui::BindingValueUpdate> updates;
    add_binding(updates, OP_INFO_ICON_PATH, "imageProps.src", entry.icon);
    add_binding(updates, OP_INFO_TITLE_PATH, "labelProps.text", entry.name);
    add_binding(updates, OP_INFO_DETAIL_PATH, "labelProps.text", entry.path.generic_string());
    add_binding(updates, OP_INFO_CHEVRON_PATH, "labelProps.text", "");
    add_binding(updates, OP_RENAME_PATH, "commonProps.disabled", bool_text(!can_operate_selected()));
    add_binding(updates, OP_DELETE_PATH, "commonProps.disabled", bool_text(!can_operate_selected()));
    add_binding(
        updates,
        OP_RENAME_PATH,
        "style.opacity",
        std::to_string(can_operate_selected() ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    add_binding(
        updates,
        OP_DELETE_PATH,
        "style.opacity",
        std::to_string(can_operate_selected() ? ENABLED_OPACITY : DISABLED_OPACITY)
    );
    auto set_result = context.gui().set_binding_values(updates);
    if (!set_result) {
        return std::unexpected(set_result.error());
    }
    auto flow_result = context.gui().trigger_screen_flow(FLOW_ID, ACTION_OPEN_OPERATION);
    if (flow_result) {
        current_page_ = Page::Operations;
    }
    return flow_result;
}

std::expected<void, std::string> FileManagerApp::hide_operations(system::core::AppContext &context)
{
    selected_entry_.reset();
    auto flow_result = context.gui().trigger_screen_flow(FLOW_ID, ACTION_OPEN_BROWSER);
    if (flow_result) {
        current_page_ = Page::Browser;
    }
    return flow_result;
}
