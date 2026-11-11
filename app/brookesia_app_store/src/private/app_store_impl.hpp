/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <deque>
#include <mutex>

#include "brookesia/app_store.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::app::app_store {

struct AppStoreApp::Impl {
    struct HttpEventMailbox {
        std::mutex mutex;
        bool open = true;
        std::vector<PendingHttpEvent> events;
    };

    system::core::AppContext *context = nullptr;
    service::ServiceBinding http_binding;
    service::ServiceBinding device_binding;
    service::ServiceBinding storage_binding;
    std::vector<esp_brookesia::lib_utils::scoped_connection> http_event_connections;
    std::shared_ptr<HttpEventMailbox> http_event_mailbox;
    gui::ScopedConnection primary_action_connection;
    gui::ScopedConnection local_delete_action_connection;
    std::vector<StoreEntry> entries;
    std::vector<LocalPackageEntry> local_packages;
    std::vector<InstalledAppEntry> installed_runtime_apps;
    std::vector<std::string> entry_paths;
    std::vector<VisibleItemRef> visible_items;
    std::vector<VisibleItemRef> refresh_icon_indices;
    std::vector<std::filesystem::path> startup_cache_candidates;
    std::vector<std::filesystem::path> local_package_scan_dirs;
    std::vector<LocalPackageScanCandidate> local_package_scan_candidates;
    std::vector<LocalPackageEntry> local_package_scan_results;
    std::unordered_map<std::string, VisibleItemRef> instance_to_entry;
    std::unordered_map<std::string, size_t> local_package_by_manifest_id;
    std::unordered_set<std::string> installed_manifest_ids;
    std::unordered_map<std::string, std::string> installed_version_by_manifest_id;
    std::unordered_set<std::string> registered_icon_resource_ids;
    std::unordered_set<std::string> local_package_scan_seen_keys;
    std::unordered_set<uint64_t> terminal_download_request_ids;
    std::string applied_i18n_locale;
    std::unordered_map<std::string, std::string> i18n_strings;
    std::string status_text;
    std::string storage_text;
    std::string refresh_dialog_message;
    std::string pending_delete_local_package_name;
    std::string pending_delete_success_package_name;
    std::filesystem::path pending_delete_local_package_path;
    std::filesystem::path pending_delete_success_package_path;
    std::string startup_cache_last_error;
    ViewMode view_mode = ViewMode::Store;
    StartupLoadPhase startup_load_phase = StartupLoadPhase::None;
    LocalPackageScanPhase local_package_scan_phase = LocalPackageScanPhase::None;
    size_t list_slot_count = 0;
    size_t list_page = 0;
    size_t startup_cache_cursor = 0;
    size_t local_package_scan_dir_cursor = 0;
    size_t local_package_scan_candidate_cursor = 0;
    system::core::MessageDialogRequestId message_dialog_request_id =
        system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    system::core::TimerId refresh_icon_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId size_metadata_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId startup_load_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId view_mode_load_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId deferred_operation_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId local_package_scan_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId refresh_request_timeout_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId refresh_result_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId http_event_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId time_sync_timeout_timer_id = system::core::INVALID_TIMER_ID;
    system::core::TimerId time_sync_success_close_timer_id = system::core::INVALID_TIMER_ID;
    std::optional<service::helper::Http::GeneralState> http_general_state;
    std::optional<ViewMode> pending_view_mode;
    std::deque<DeferredOperation> pending_deferred_operations;
    MessageDialogPurpose message_dialog_purpose = MessageDialogPurpose::None;
    PendingRefreshResultType pending_refresh_result_type = PendingRefreshResultType::None;
    IconUpdatePurpose refresh_icon_purpose = IconUpdatePurpose::None;
    std::optional<boost::json::object> pending_refresh_result_response;
    uint64_t async_generation = 0;
    uint64_t storage_free_bytes = 0;
    bool storage_capacity_known = false;
    uint64_t refresh_request_id = 0;
    uint64_t refresh_icon_request_id = 0;
    uint64_t download_dialog_request_id = 0;
    size_t refresh_icon_cursor = 0;
    bool refresh_in_progress = false;
    bool http_available = false;
    bool device_available = false;
    bool storage_available = false;
    bool network_ready = true;
    bool storage_match_warning_logged = false;
    bool time_sync_waiting = false;
    bool processing_pending_refresh_result = false;
    bool startup_cache_load_in_progress = false;
    bool local_package_scan_in_progress = false;
    bool local_package_scan_refresh_view = false;
};

} // namespace esp_brookesia::app::app_store
