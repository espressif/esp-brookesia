/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::vector<AppStoreApp::VisibleItemRef> AppStoreApp::build_visible_items() const
{
    std::vector<VisibleItemRef> visible_items;
    if (view_mode_ == ViewMode::Store) {
        for (size_t i = 0; i < entries_.size(); ++i) {
            if (!entries_[i].installed || entries_[i].update_available) {
                visible_items.push_back(VisibleItemRef{
                    .kind = VisibleItemKind::Store,
                    .index = i,
                });
            }
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
AppStoreApp::LocalPackageAction AppStoreApp::get_local_package_action(const LocalPackageEntry &entry) const
{
    if (entry.manifest_id.empty() || !entry.schema_error.empty()) {
        return LocalPackageAction::None;
    }

    const auto installed_version_it = installed_version_by_manifest_id_.find(entry.manifest_id);
    if (installed_version_it == installed_version_by_manifest_id_.end()) {
        return LocalPackageAction::Install;
    }

    if (!entry.version.empty() && compare_versions(entry.version, installed_version_it->second) > 0) {
        return LocalPackageAction::Update;
    }

    return LocalPackageAction::None;
}
