/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::expected<std::vector<FileManagerApp::Entry>, std::string>
FileManagerApp::build_entries_for_current_location() const
{
    std::vector<Entry> next_entries;
    if (!current_volume_index_) {
        for (size_t i = 0; i < volumes_.size(); ++i) {
            next_entries.push_back(Entry{
                .kind = EntryKind::Volume,
                .instance_id = {},
                .view_path = {},
                .name = volumes_[i].display_name,
                .detail = describe_volume(volumes_[i].volume, volumes_[i].capacity_text),
                .icon = ICON_DRIVER,
                .path = volumes_[i].volume.root_path,
                .volume_index = i,
            });
        }
        return next_entries;
    }

    const auto root = current_root();
    const auto directory = (root / current_directory_).lexically_normal();
    auto dir_info = StorageHelper::fs_stat(directory.generic_string());
    if (!dir_info || dir_info->type != StorageHelper::FileType::Directory) {
        return std::unexpected("Current directory is unavailable: " + directory.generic_string());
    }

    auto entries = StorageHelper::fs_list(directory.generic_string());
    if (!entries) {
        return std::unexpected("Failed to read directory: " + entries.error());
    }
    for (const auto &item : entries.value()) {
        const auto item_path = directory / item.name;
        const auto name = item.name;
        if (name.empty()) {
            continue;
        }
        EntryKind kind = EntryKind::Other;
        std::string detail;
        if (item.info.type == StorageHelper::FileType::Directory) {
            kind = EntryKind::Directory;
            detail = tr("folder");
        } else if (item.info.type == StorageHelper::FileType::File) {
            kind = EntryKind::File;
            detail = format_size(item.info.size);
        } else {
            detail = tr("other");
        }
        next_entries.push_back(Entry{
            .kind = kind,
            .instance_id = {},
            .view_path = {},
            .name = name,
            .detail = detail,
            .icon = file_icon_for(item_path, kind),
            .path = item_path,
            .volume_index = *current_volume_index_,
        });
    }

    std::sort(next_entries.begin(), next_entries.end(), [](const Entry & lhs, const Entry & rhs) {
        if (lhs.kind == EntryKind::Directory && rhs.kind != EntryKind::Directory) {
            return true;
        }
        if (lhs.kind != EntryKind::Directory && rhs.kind == EntryKind::Directory) {
            return false;
        }
        return lower_copy(lhs.name) < lower_copy(rhs.name);
    });
    return next_entries;
}

std::expected<void, std::string> FileManagerApp::open_entry(system::core::AppContext &context, const Entry &entry)
{
    if (entry.kind == EntryKind::Volume) {
        BROOKESIA_LOGI("Open File Manager volume: name(%1%), root(%2%)", entry.name, entry.path.generic_string());
        current_volume_index_ = entry.volume_index;
        current_directory_.clear();
        return refresh_entries(context);
    }
    if (entry.kind != EntryKind::Directory) {
        BROOKESIA_LOGI("Open File Manager item operations: path(%1%)", entry.path.generic_string());
        return show_operations(context, entry);
    }
    const auto relative = make_relative_path_inside_root(entry.path, current_root());
    if (!relative || relative->empty()) {
        return std::unexpected("Failed to resolve directory: " + entry.path.generic_string());
    }
    current_directory_ = relative->lexically_normal();
    BROOKESIA_LOGI("Open File Manager directory: path(%1%)", entry.path.generic_string());
    return refresh_entries(context);
}
