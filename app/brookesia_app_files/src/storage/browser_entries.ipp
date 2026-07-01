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
    std::error_code error_code;
    if (!std::filesystem::exists(directory, error_code) || !std::filesystem::is_directory(directory, error_code)) {
        return std::unexpected("Current directory is unavailable: " + directory.generic_string());
    }

    for (const auto &item : std::filesystem::directory_iterator(directory, error_code)) {
        if (error_code) {
            return std::unexpected("Failed to read directory: " + error_code.message());
        }
        const auto item_path = item.path();
        const auto name = item_path.filename().generic_string();
        if (name.empty()) {
            continue;
        }
        EntryKind kind = EntryKind::Other;
        std::string detail;
        std::error_code stat_error;
        if (item.is_directory(stat_error)) {
            kind = EntryKind::Directory;
            detail = tr("folder");
        } else if (item.is_regular_file(stat_error)) {
            kind = EntryKind::File;
            detail = format_size(item.file_size(stat_error));
            if (stat_error) {
                detail = tr("file");
            }
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
    if (error_code) {
        return std::unexpected("Failed to list directory: " + error_code.message());
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
    std::error_code error_code;
    const auto relative = std::filesystem::relative(entry.path, current_root(), error_code);
    if (error_code || relative.empty()) {
        return std::unexpected(
                   "Failed to resolve directory: " + (error_code ? error_code.message() : entry.path.generic_string())
               );
    }
    current_directory_ = relative.lexically_normal();
    BROOKESIA_LOGI("Open File Manager directory: path(%1%)", entry.path.generic_string());
    return refresh_entries(context);
}
