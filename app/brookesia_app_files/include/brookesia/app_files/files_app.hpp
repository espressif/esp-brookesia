/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "brookesia/lib_utils/signal.hpp"

#include "brookesia/service_manager.hpp"
#include "brookesia/system_core.hpp"

namespace esp_brookesia::app::files {

/**
 * @brief Built-in file manager application for browsing storage volumes and basic file actions.
 */
class FileManagerApp final: public system::core::IApp {
public:
    /**
     * @brief Create a file manager application instance.
     */
    FileManagerApp();

    /**
     * @brief Destroy the file manager application instance.
     */
    ~FileManagerApp() override;

    /**
     * @brief File browser entry category used for rendering icons and supported actions.
     */
    enum class EntryKind {
        /** Storage volume root entry. */
        Volume,
        /** Directory entry. */
        Directory,
        /** Regular file entry. */
        File,
        /** Unknown or unsupported filesystem entry. */
        Other,
    };

    /**
     * @brief Internal page currently shown by the file manager.
     */
    enum class Page {
        /** Main browser page. */
        Browser,
        /** Per-entry operation page. */
        Operations,
    };

    system::core::AppManifest get_manifest() const override;
    system::core::AppGuiDescriptor get_gui_descriptor() const override;
    std::expected<void, std::string> on_start(system::core::AppContext &context) override;
    std::expected<void, std::string> on_stop(system::core::AppContext &context) override;
    std::expected<void, std::string> on_action(
        system::core::AppContext &context,
        std::string_view action
    ) override;
    std::expected<void, std::string> on_timer(
        system::core::AppContext &context,
        system::core::TimerId timer_id,
        std::string_view name
    ) override;

private:
    struct VolumeEntry {
        system::core::StorageVolume volume;
        std::string display_name;
        std::string capacity_text;
    };

    struct Entry {
        EntryKind kind = EntryKind::Other;
        std::string instance_id;
        std::string view_path;
        std::string name;
        std::string detail;
        std::string icon;
        std::filesystem::path path;
        size_t volume_index = 0;
    };

    std::expected<void, std::string> subscribe_actions(system::core::AppContext &context);
    void bind_storage_service();
    void release_storage_service();
    void refresh_volume_capacities(system::core::AppContext &context);
    void handle_storage_filesystems_result(uint64_t generation, service::FunctionResult &&result);
    void handle_storage_capacity_result(uint64_t generation, size_t volume_index, service::FunctionResult &&result);
    void clear_entry_views(system::core::AppContext &context);
    std::expected<std::vector<Entry>, std::string> build_entries_for_current_location() const;
    std::expected<void, std::string> ensure_entry_view_capacity(system::core::AppContext &context, size_t count);
    void trim_entry_view_pool(system::core::AppContext &context, size_t visible_count);
    std::expected<void, std::string> refresh_entries(system::core::AppContext &context);
    std::expected<void, std::string> scroll_first_entry_to_top(system::core::AppContext &context);
    std::expected<void, std::string> refresh_ui(system::core::AppContext &context);
    std::expected<void, std::string> navigate_back(system::core::AppContext &context);
    std::expected<void, std::string> open_entry(system::core::AppContext &context, const Entry &entry);
    std::expected<void, std::string> show_operations(system::core::AppContext &context, const Entry &entry);
    std::expected<void, std::string> hide_operations(system::core::AppContext &context);
    void request_rename(system::core::AppContext &context);
    void request_delete(system::core::AppContext &context);
    void show_message(
        system::core::AppContext &context,
        std::string text,
        std::string informative_text,
        system::core::MessageDialogIcon icon
    );

    std::vector<VolumeEntry> build_volumes() const;
    std::optional<size_t> find_current_volume() const;
    std::filesystem::path current_root() const;
    bool is_inside_current_root(const std::filesystem::path &path) const;
    bool can_operate_selected() const;
    std::optional<Entry> find_entry_by_path(std::string_view event_path) const;
    void rebase_current_directory_after_move(
        const std::filesystem::path &old_path,
        const std::filesystem::path &new_path
    );
    void recover_current_directory_after_remove(const std::filesystem::path &removed_path);
    void set_current_directory_from_absolute_path(const std::filesystem::path &path);
    void reset_message_dialog(system::core::AppContext &context);
    std::expected<void, std::string> refresh_i18n(system::core::AppContext &context);
    std::string tr(std::string_view key) const;
    std::string current_language() const;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

class FileManagerAppProvider final: public system::core::IAppProvider {
public:
    /**
     * @brief Get the manifest for the built-in file manager app.
     *
     * @return File manager application manifest.
     */
    system::core::AppManifest get_manifest() const override;

    /**
     * @brief Create a file manager application instance.
     *
     * @return Shared application instance.
     */
    std::shared_ptr<system::core::IApp> create_app() override;
};

} // namespace esp_brookesia::app::files
