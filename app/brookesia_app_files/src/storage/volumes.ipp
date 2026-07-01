/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

std::vector<FileManagerApp::VolumeEntry> FileManagerApp::build_volumes() const
{
    std::vector<VolumeEntry> result;
    if (!storage_layout_) {
        return result;
    }
    if (storage_layout_->internal.available) {
        result.push_back(VolumeEntry{
            .volume = storage_layout_->internal,
            .display_name = tr("internal"),
            .capacity_text = {},
        });
    }
    for (size_t i = 0; i < storage_layout_->external.size(); ++i) {
        const auto &volume = storage_layout_->external[i];
        if (!volume.available) {
            continue;
        }
        result.push_back(VolumeEntry{
            .volume = volume,
            .display_name = tr("external_prefix") + std::to_string(i + 1),
            .capacity_text = {},
        });
    }
    return result;
}

std::optional<size_t> FileManagerApp::find_current_volume() const
{
    if (!current_volume_index_ || *current_volume_index_ >= volumes_.size()) {
        return std::nullopt;
    }
    return current_volume_index_;
}

std::filesystem::path FileManagerApp::current_root() const
{
    const auto index = find_current_volume();
    if (!index) {
        return {};
    }
    return std::filesystem::path(volumes_[*index].volume.root_path).lexically_normal();
}

bool FileManagerApp::is_inside_current_root(const std::filesystem::path &path) const
{
    const auto root = current_root().lexically_normal();
    const auto target = path.lexically_normal();
    const auto root_text = root.generic_string();
    const auto target_text = target.generic_string();
    return target_text == root_text || target_text.starts_with(root_text + "/");
}

bool FileManagerApp::can_operate_selected() const
{
    return selected_entry_ &&
           selected_entry_->kind != EntryKind::Volume &&
           is_inside_current_root(selected_entry_->path);
}

void FileManagerApp::set_current_directory_from_absolute_path(const std::filesystem::path &path)
{
    const auto root = current_root().lexically_normal();
    if (root.empty()) {
        current_directory_.clear();
        return;
    }

    const auto target = path.lexically_normal();
    if (target == root) {
        current_directory_.clear();
        return;
    }
    if (!path_is_same_or_child(target, root)) {
        current_directory_.clear();
        return;
    }

    std::error_code error_code;
    auto relative = std::filesystem::relative(target, root, error_code);
    if (error_code || relative.empty() || relative == ".") {
        current_directory_.clear();
        return;
    }
    current_directory_ = relative.lexically_normal();
}

void FileManagerApp::rebase_current_directory_after_move(
    const std::filesystem::path &old_path,
    const std::filesystem::path &new_path
)
{
    const auto root = current_root();
    if (root.empty()) {
        return;
    }

    const auto current_absolute = (root / current_directory_).lexically_normal();
    const auto old_normalized = old_path.lexically_normal();
    if (!path_is_same_or_child(current_absolute, old_normalized)) {
        return;
    }

    auto rebased = new_path.lexically_normal();
    const auto current_text = current_absolute.generic_string();
    const auto old_text = old_normalized.generic_string();
    if (current_text.size() > old_text.size()) {
        rebased /= current_text.substr(old_text.size() + 1);
    }
    set_current_directory_from_absolute_path(rebased);
}

void FileManagerApp::recover_current_directory_after_remove(const std::filesystem::path &removed_path)
{
    const auto root = current_root();
    if (root.empty()) {
        return;
    }

    const auto current_absolute = (root / current_directory_).lexically_normal();
    const auto removed_normalized = removed_path.lexically_normal();
    if (!path_is_same_or_child(current_absolute, removed_normalized)) {
        return;
    }

    std::error_code error_code;
    auto fallback = removed_normalized.parent_path().lexically_normal();
    while (path_is_same_or_child(fallback, root) && fallback != root &&
            !std::filesystem::is_directory(fallback, error_code)) {
        error_code.clear();
        fallback = fallback.parent_path().lexically_normal();
    }
    if (!path_is_same_or_child(fallback, root) || !std::filesystem::is_directory(fallback, error_code)) {
        current_directory_.clear();
        return;
    }
    set_current_directory_from_absolute_path(fallback);
}

std::optional<FileManagerApp::Entry> FileManagerApp::find_entry_by_path(std::string_view event_path) const
{
    for (const auto &entry : entries_) {
        if (event_path == entry.view_path || std::string(event_path).starts_with(entry.view_path + "/")) {
            return entry;
        }
    }
    return std::nullopt;
}
