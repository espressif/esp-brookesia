/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::vector<AppStoreApp::VisibleItemRef> AppStoreApp::build_visible_items() const
{
    std::vector<VisibleItemRef> visible_items;
    if (view_mode_ == ViewMode::Store) {
        std::unordered_set<std::string> remote_manifest_ids;
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (!entries_[i].package_name.empty()) {
                remote_manifest_ids.insert(entries_[i].package_name);
            }
            if (!entries_[i].installed || entries_[i].update_available) {
                visible_items.push_back(VisibleItemRef{
                    .kind = VisibleItemKind::Store,
                    .index = i,
                });
            }
        }
        for (size_t i = 0; i < local_packages_.size(); ++i) {
            const auto &entry = local_packages_[i];
            if (!entry.schema_error.empty() || entry.manifest_id.empty() || entry.installed ||
                    remote_manifest_ids.contains(entry.manifest_id)) {
                continue;
            }
            visible_items.push_back(VisibleItemRef{
                .kind = VisibleItemKind::StoreLocal,
                .index = i,
            });
        }
    } else if (view_mode_ == ViewMode::Local) {
        for (size_t i = 0; i < local_packages_.size(); ++i) {
            visible_items.push_back(VisibleItemRef{
                .kind = VisibleItemKind::Local,
                .index = i,
            });
        }
    } else {
        for (size_t i = 0; i < installed_runtime_apps_.size(); ++i) {
            visible_items.push_back(VisibleItemRef{
                .kind = VisibleItemKind::Installed,
                .index = i,
            });
        }
    }

    return visible_items;
}
void AppStoreApp::clamp_list_page()
{
    const auto page_count = get_page_count(visible_items_.size());
    if (list_page_ >= page_count) {
        list_page_ = page_count - 1;
    }
}
std::optional<size_t> AppStoreApp::find_store_entry_index_by_package(std::string_view package_name) const
{
    if (package_name.empty()) {
        return std::nullopt;
    }
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].package_name == package_name) {
            return i;
        }
    }
    return std::nullopt;
}
AppStoreApp::LocalPackageAction AppStoreApp::get_local_package_action(const LocalPackageEntry &entry) const
{
    if (entry.manifest_id.empty() || !entry.schema_error.empty()) {
        return LocalPackageAction::None;
    }

    const auto store_index = find_store_entry_index_by_package(entry.manifest_id);
    if (!store_index) {
        return LocalPackageAction::Install;
    }

    const auto installed_version_it = installed_version_by_manifest_id_.find(entry.manifest_id);
    if (installed_version_it == installed_version_by_manifest_id_.end()) {
        return LocalPackageAction::Install;
    }

    if (!entry.version.empty() && compare_versions(entry.version, installed_version_it->second) > 0) {
        return LocalPackageAction::Update;
    }

    if (!entries_[*store_index].latest_version.empty() &&
            entry.version == entries_[*store_index].latest_version &&
            compare_versions(entries_[*store_index].latest_version, installed_version_it->second) > 0) {
        return LocalPackageAction::Update;
    }

    return LocalPackageAction::None;
}
