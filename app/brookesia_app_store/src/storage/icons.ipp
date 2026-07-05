/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<void, std::string> AppStoreApp::register_icon(system::core::AppContext &context, size_t index)
{
    if (context_ == nullptr || index >= entries_.size() || entries_[index].icon_file_path.empty()) {
        return {};
    }
    const auto icon_path = std::filesystem::path(entries_[index].icon_file_path);
    auto size = read_png_size(entries_[index].icon_file_path);
    if (!size) {
        remove_invalid_icon_file(icon_path, "PNG validation failed before register");
        entries_[index].icon_file_path.clear();
        return std::unexpected("Failed to read icon PNG size");
    }
    const auto image_id = "app_store.icon." + safe_name(entries_[index].package_name);
    if (registered_icon_resource_ids_.contains(image_id)) {
        entries_[index].icon_resource_id = image_id;
        return refresh_entry(context, index);
    }
    auto result = context.gui().register_image(gui::RuntimeImageResource{
        .id = image_id,
        .primary_src = entries_[index].icon_file_path,
        .native_src = 0,
        .width = size->first,
        .height = size->second,
    });
    if (!result) {
        return result;
    }
    entries_[index].icon_resource_id = image_id;
    registered_icon_resource_ids_.insert(image_id);
    return refresh_entry(context, index);
}

std::string AppStoreApp::register_cached_icon(system::core::AppContext &context, const std::vector<std::string> &keys)
{
    for (const auto &key : keys) {
        if (key.empty()) {
            continue;
        }
        const auto safe_key = safe_name(key);
        const auto image_id = "app_store.icon." + safe_key;
        if (registered_icon_resource_ids_.contains(image_id)) {
            return image_id;
        }
        for (const auto &icon_path : cached_icon_file_candidates(context, {key})) {
            std::error_code error_code;
            if (!std::filesystem::is_regular_file(icon_path, error_code) || error_code) {
                error_code.clear();
                continue;
            }
            auto size = read_png_size(icon_path.generic_string());
            if (!size) {
                remove_invalid_icon_file(icon_path, "cached PNG validation failed before register");
                continue;
            }
            auto result = context.gui().register_image(gui::RuntimeImageResource{
                .id = image_id,
                .primary_src = icon_path.generic_string(),
                .native_src = 0,
                .width = size->first,
                .height = size->second,
            });
            if (!result) {
                BROOKESIA_LOGW("Failed to register cached App Store icon: %1%", result.error());
                continue;
            }
            registered_icon_resource_ids_.insert(image_id);
            return image_id;
        }
    }
    return {};
}

void AppStoreApp::unregister_icons(system::core::AppContext &context)
{
    for (const auto &image_id : registered_icon_resource_ids_) {
        (void)context.gui().unregister_image(image_id);
    }
    registered_icon_resource_ids_.clear();
    for (auto &entry : entries_) {
        entry.icon_resource_id.clear();
    }
    for (auto &entry : local_packages_) {
        entry.icon_resource_id.clear();
    }
    for (auto &entry : installed_runtime_apps_) {
        entry.icon_resource_id.clear();
    }
}
