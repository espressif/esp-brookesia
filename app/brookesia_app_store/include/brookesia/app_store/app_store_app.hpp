/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "brookesia/lib_utils/signal.hpp"

#include "brookesia/service_helper/network/http.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/system_core.hpp"

namespace esp_brookesia::app::app_store {

/**
 * @brief Built-in app store application for browsing, downloading, installing, and removing apps.
 */
class AppStoreApp final: public system::core::IApp {
public:
    /**
     * @brief Create an app store application instance.
     */
    AppStoreApp();

    /**
     * @brief Destroy the app store application instance.
     */
    ~AppStoreApp() override;

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
    enum class ViewMode {
        Store,
        Installed,
        Local,
    };

    enum class VisibleItemKind {
        Store,
        StoreLocal,
        Installed,
        Local,
    };

    enum class IndexLoadMode {
        CacheOnly,
        RemoteRefresh,
    };

    enum class NetworkCheckPurpose {
        StartupCache,
        StartupRemoteRefresh,
        ManualRemoteRefresh,
    };

    enum class MessageDialogPurpose {
        None,
        Download,
        Install,
        Uninstall,
        TimeSyncRetry,
    };

    enum class LocalPackageAction {
        None,
        Install,
        Update,
    };

    enum class PendingRefreshResultType {
        None,
        Completed,
        Failed,
        Canceled,
    };

    struct VisibleItemRef {
        VisibleItemKind kind = VisibleItemKind::Store;
        size_t index = 0;
    };

    struct StoreEntry {
        std::string package_name;
        std::string manifest_id;
        std::map<std::string, std::string> app_names;
        std::map<std::string, std::string> descriptions;
        std::string latest_version;
        std::vector<std::string> categories;
        std::string icon_url;
        std::string metadata_url;
        std::string download_url;
        std::string sha256;
        std::string updated_at;
        std::string schema_error;
        std::string item_path;
        std::string icon_resource_id;
        std::string icon_file_path;
        std::filesystem::path cached_package_path;
        std::filesystem::path download_path;
        std::filesystem::path icon_download_path;
        uintmax_t cached_package_size = 0;
        uint64_t download_request_id = 0;
        uint64_t icon_request_id = 0;
        uint64_t metadata_request_id = 0;
        uint64_t download_size = 0;
        uint32_t downloaded = 0;
        uint32_t total = 0;
        std::string installed_version;
        bool downloading = false;
        bool metadata_loading = false;
        bool installed = false;
        bool update_available = false;
        bool cached = false;
    };

    struct LocalPackageEntry {
        std::filesystem::path package_path;
        std::string file_name;
        std::string manifest_id;
        std::map<std::string, std::string> app_names;
        std::string version;
        std::string schema_error;
        std::string item_path;
        std::string icon_resource_id;
        uintmax_t file_size = 0;
        bool installed = false;
    };

    struct InstalledAppEntry {
        system::core::AppInfo app;
        std::string item_path;
        std::string icon_resource_id;
    };

    std::expected<void, std::string> subscribe_actions(system::core::AppContext &context);
    void bind_http_service();
    void release_http_service();
    void bind_device_service();
    void release_device_service();
    void bind_storage_service();
    void release_storage_service();
    void disconnect_http_events();
    void subscribe_http_events();
    std::filesystem::path resolve_resource_dir(system::core::AppContext &context) const;
    std::vector<system::core::AppStorageDirs> app_storage_dirs(system::core::AppContext &context) const;
    std::filesystem::path cache_dir(system::core::AppContext &context) const;
    std::vector<std::filesystem::path> cache_dirs(system::core::AppContext &context) const;
    std::filesystem::path download_dir(system::core::AppContext &context) const;
    std::vector<std::filesystem::path> download_dirs(system::core::AppContext &context) const;
    std::vector<std::filesystem::path> scan_download_dirs(system::core::AppContext &context) const;
    std::vector<std::filesystem::path> cache_file_candidates(
        system::core::AppContext &context,
        const std::filesystem::path &relative_path
    ) const;
    std::optional<std::filesystem::path> writable_cache_file(
        system::core::AppContext &context,
        const std::filesystem::path &relative_path
    ) const;
    bool write_cache_text_file(
        system::core::AppContext &context,
        const std::filesystem::path &relative_path,
        std::string_view text
    ) const;
    void prefer_external_install_storage(system::core::AppContext &context) const;
    void refresh_storage_capacity(system::core::AppContext &context);
    void handle_storage_filesystems_result(uint64_t generation, service::FunctionResult &&result);
    void handle_storage_capacity_result(uint64_t generation, service::FunctionResult &&result);
    void refresh_installed_apps(system::core::AppContext &context);
    void apply_installed_state();
    void scan_local_packages(system::core::AppContext &context);
    std::expected<void, std::string> load_cached_index(system::core::AppContext &context);
    std::expected<void, std::string> start_remote_refresh(system::core::AppContext &context);
    std::expected<void, std::string> start_remote_refresh_request(system::core::AppContext &context);
    std::expected<void, std::string> parse_index_json(
        system::core::AppContext &context,
        std::string_view json,
        std::string_view index_url,
        IndexLoadMode mode
    );
    std::expected<void, std::string> populate_entries(system::core::AppContext &context);
    void clear_entry_views(system::core::AppContext &context);
    std::expected<void, std::string> refresh_i18n(system::core::AppContext &context);
    std::expected<void, std::string> refresh_ui(system::core::AppContext &context);
    std::expected<void, std::string> scroll_list_to_top(system::core::AppContext &context);
    std::expected<void, std::string> refresh_entry(system::core::AppContext &context, size_t index);
    std::vector<VisibleItemRef> build_visible_items() const;
    void clamp_list_page();
    std::optional<size_t> find_store_entry_index_by_package(std::string_view package_name) const;
    LocalPackageAction get_local_package_action(const LocalPackageEntry &entry) const;
    void handle_primary_action(const gui::Event &event);
    void start_view_mode_load(system::core::AppContext &context, ViewMode mode);
    std::expected<void, std::string> process_view_mode_load(system::core::AppContext &context);
    void stop_view_mode_load_timer(system::core::AppContext &context);
    void set_view_mode(system::core::AppContext &context, ViewMode mode);
    void start_or_cancel_download(system::core::AppContext &context, size_t index);
    void start_metadata_request(system::core::AppContext &context, size_t index);
    void start_download(system::core::AppContext &context, size_t index);
    std::expected<void, std::string> parse_metadata_json(
        system::core::AppContext &context,
        size_t index,
        std::string_view json
    );
    std::expected<void, std::string> install_package(
        system::core::AppContext &context,
        const std::filesystem::path &path,
        std::string_view name
    );
    void install_local_package(system::core::AppContext &context, size_t index);
    void uninstall_installed_app(system::core::AppContext &context, size_t index);
    void cancel_download(StoreEntry &entry);
    void show_download_preparing_dialog(system::core::AppContext &context, const StoreEntry &entry);
    void ensure_download_progress_dialog(system::core::AppContext &context, const StoreEntry &entry);
    void update_download_progress_dialog_if_current(system::core::AppContext &context, const StoreEntry &entry);
    void show_download_failed_dialog(
        system::core::AppContext &context,
        const StoreEntry &entry,
        std::string error_message
    );
    std::string make_download_progress_detail(const StoreEntry &entry) const;
    void cancel_remote_refresh();
    void start_refresh_request_watchdog(system::core::AppContext &context, uint32_t timeout_ms, uint32_t retry_count);
    void stop_refresh_request_watchdog(system::core::AppContext &context);
    void process_refresh_request_timeout(system::core::AppContext &context);
    void schedule_refresh_result_processing(
        system::core::AppContext &context,
        PendingRefreshResultType type,
        const boost::json::object &response_json = {}
    );
    void stop_refresh_result_timer(system::core::AppContext &context);
    void process_pending_refresh_result(system::core::AppContext &context);
    void handle_request_progress(double request_id, const boost::json::object &progress_json);
    void handle_request_completed(double request_id, const boost::json::object &response_json);
    void handle_request_failed(double request_id, const boost::json::object &response_json);
    void handle_request_canceled(double request_id, const boost::json::object &response_json);
    void handle_http_general_state_changed(std::string_view state_text);
    void handle_metadata_completed(size_t index, const boost::json::object &response_json);
    void handle_metadata_failed(size_t index, const boost::json::object &response_json);
    void handle_metadata_canceled(size_t index);
    void handle_refresh_progress(const boost::json::object &progress_json);
    void handle_refresh_completed(const boost::json::object &response_json);
    void handle_refresh_failed(const boost::json::object &response_json);
    void handle_refresh_canceled();
    void handle_refresh_icon_progress(const boost::json::object &progress_json);
    void handle_refresh_icon_completed(const boost::json::object &response_json);
    void handle_refresh_icon_failed(const boost::json::object &response_json);
    void handle_refresh_icon_canceled();
    std::optional<size_t> find_entry_by_request(uint64_t request_id) const;
    std::optional<size_t> find_entry_by_metadata_request(uint64_t request_id) const;
    std::optional<VisibleItemRef> find_visible_item_by_path(std::string_view path) const;
    void start_refresh_icon_update(system::core::AppContext &context);
    std::expected<void, std::string> process_refresh_icon_step(system::core::AppContext &context);
    void schedule_refresh_icon_step(system::core::AppContext &context);
    void stop_refresh_icon_timer(system::core::AppContext &context);
    void begin_time_sync_wait(system::core::AppContext &context);
    void stop_time_sync_timers(system::core::AppContext &context);
    void finish_time_sync_wait_success(system::core::AppContext &context);
    void finish_time_sync_wait_timeout(system::core::AppContext &context);
    void process_time_sync_success_close(system::core::AppContext &context);
    bool should_wait_for_http_time_sync() const;
    bool submit_network_check(system::core::AppContext &context, NetworkCheckPurpose purpose);
    void handle_network_check_result(
        uint64_t generation,
        NetworkCheckPurpose purpose,
        service::FunctionResult &&result
    );
    bool parse_network_ready(const boost::json::array &json, std::string &error_message) const;
    void show_network_unavailable_dialog(system::core::AppContext &context);
    system::core::MessageDialogOptions make_message_dialog_options(
        std::string text,
        std::string informative_text,
        system::core::MessageDialogIcon icon,
        int32_t auto_close_ms,
        std::vector<system::core::MessageDialogButton> buttons = {}
    ) const;
    void ensure_message_dialog(
        system::core::AppContext &context,
        std::string text,
        std::string informative_text,
        system::core::MessageDialogIcon icon,
        int32_t auto_close_ms,
        MessageDialogPurpose purpose = MessageDialogPurpose::None,
        std::vector<system::core::MessageDialogButton> buttons = {}
    );
    void update_message_dialog_if_visible(
        system::core::AppContext &context,
        std::string text,
        std::string informative_text,
        system::core::MessageDialogIcon icon,
        int32_t auto_close_ms,
        MessageDialogPurpose purpose = MessageDialogPurpose::None,
        std::vector<system::core::MessageDialogButton> buttons = {}
    );
    void hide_message_dialog_if_visible(system::core::AppContext &context);
    void handle_message_dialog_result(const system::core::MessageDialogResult &result);
    void finish_remote_refresh_with_error(system::core::AppContext &context, std::string error_message);
    void finish_remote_refresh_success(system::core::AppContext &context);
    std::expected<void, std::string> register_icon(system::core::AppContext &context, size_t index);
    std::string register_cached_icon(system::core::AppContext &context, const std::vector<std::string> &keys);
    void unregister_icons(system::core::AppContext &context);
    std::string localized(const std::map<std::string, std::string> &values) const;
    std::string tr(std::string_view key) const;
    std::string current_language() const;

    class Impl;
    std::unique_ptr<Impl> impl_;
};

class AppStoreAppProvider final: public system::core::IAppProvider {
public:
    /**
     * @brief Get the manifest for the built-in app store app.
     *
     * @return App store application manifest.
     */
    system::core::AppManifest get_manifest() const override;

    /**
     * @brief Create an app store application instance.
     *
     * @return Shared application instance.
     */
    std::shared_ptr<system::core::IApp> create_app() override;
};

} // namespace esp_brookesia::app::app_store
