/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <unordered_set>

#include "brookesia/app_store.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::app::app_store {

struct AppStoreApp::Impl {
    struct HttpEventMailbox {
        struct RequestState {
            bool response_lost = false;
            std::chrono::milliseconds idle_timeout{};
            std::chrono::steady_clock::time_point missing_result_deadline{};
        };
        static constexpr size_t MAX_REQUESTS = 16;
        static constexpr size_t MAX_EVENTS = 2 * MAX_REQUESTS + 1;
        static constexpr size_t MAX_BYTES = 768 * 1024;
        std::mutex mutex;
        bool open = true;
        std::vector<PendingHttpEvent> events;
        std::unordered_map<uint64_t, RequestState> requests;
        std::unordered_set<uint64_t> lost_early_requests;
        size_t pending_submissions = 0;
        size_t pending_bytes = 0;
        bool early_loss_tracking_full = false;

        void record_early_loss(uint64_t id)
        {
            if (lost_early_requests.contains(id)) {
                return;
            }
            if (lost_early_requests.size() < MAX_EVENTS) {
                lost_early_requests.insert(id);
            } else {
                early_loss_tracking_full = true;
            }
        }

        static size_t json_bytes(const boost::json::value &value, unsigned depth = 0)
        {
            if (depth > 32) {
                return MAX_BYTES + 1;
            }
            size_t bytes = sizeof(value);
            if (value.is_string()) {
                return bytes + value.as_string().size();
            }
            if (value.is_object()) {
                for (const auto &item : value.as_object()) {
                    bytes += sizeof(item) + item.key().size() + json_bytes(item.value(), depth + 1);
                    if (bytes > MAX_BYTES) {
                        break;
                    }
                }
            } else if (value.is_array()) {
                for (const auto &item : value.as_array()) {
                    bytes += json_bytes(item, depth + 1);
                    if (bytes > MAX_BYTES) {
                        break;
                    }
                }
            }
            return bytes;
        }

        bool begin_submission()
        {
            std::lock_guard lock(mutex);
            if (!open || requests.size() + pending_submissions >= MAX_REQUESTS) {
                return false;
            }
            ++pending_submissions;
            return true;
        }

        bool finish_submission(std::optional<uint64_t> id,
                               std::chrono::milliseconds idle_timeout = std::chrono::seconds(60))
        {
            std::lock_guard lock(mutex);
            if (!open) {
                return false;
            }
            --pending_submissions;
            if (id) {
                // Early terminal events already carry ids; a loss must not fail a different submission.
                const auto is_terminal = [id](const auto & event) {
                    return event.request_id == *id && event.type != PendingHttpEventType::Progress;
                };
                const bool terminal_queued = std::any_of(events.begin(), events.end(), is_terminal);
                const bool response_lost = lost_early_requests.erase(*id) != 0 && !terminal_queued;
                RequestState state{.response_lost = response_lost, .idle_timeout = idle_timeout};
                if (early_loss_tracking_full && !terminal_queued && !response_lost) {
                    // If the bounded loss-id history filled, allow normal progress and completion. Only a
                    // missing result that stays inactive for the request's retry budget becomes a failure.
                    state.missing_result_deadline = std::chrono::steady_clock::now() + idle_timeout;
                }
                requests.emplace(*id, state);
            }
            if (pending_submissions == 0) {
                const auto discard_unowned = [this](const auto & event) {
                    if (event.type == PendingHttpEventType::GeneralStateChanged || requests.contains(event.request_id)) {
                        return false;
                    }
                    pending_bytes -= event.memory_bytes;
                    return true;
                };
                std::erase_if(events, discard_unowned);
                lost_early_requests.clear();
                early_loss_tracking_full = false;
            }
            return true;
        }

        void enqueue(PendingHttpEventType type, double id, const boost::json::object *response,
                     const std::string *state = nullptr)
        {
            std::lock_guard lock(mutex);
            if (!open) {
                return;
            }
            const bool general = type == PendingHttpEventType::GeneralStateChanged;
            const auto request = requests.find(id);
            if (!general && request == requests.end() && pending_submissions == 0) {
                return;
            }
            if (request != requests.end() && type == PendingHttpEventType::Progress &&
                    request->second.missing_result_deadline != std::chrono::steady_clock::time_point{}) {
                request->second.missing_result_deadline =
                    std::chrono::steady_clock::now() + request->second.idle_timeout;
            }
            for (auto it = events.begin(); it != events.end(); ++it) {
                if ((general && it->type == type) || (!general && it->type != PendingHttpEventType::GeneralStateChanged &&
                                                      it->request_id == id)) {
                    if (!general && it->type != PendingHttpEventType::Progress) {
                        return;
                    }
                    pending_bytes -= it->memory_bytes;
                    events.erase(it);
                    break;
                }
            }
            size_t bytes = sizeof(PendingHttpEvent);
            if (response != nullptr) {
                for (const auto &item : *response) {
                    bytes += sizeof(item) + item.key().size() + json_bytes(item.value());
                    if (bytes > MAX_BYTES) {
                        break;
                    }
                }
            }
            if (state != nullptr) {
                bytes += state->size();
            }
            const auto full = [&] { return events.size() >= MAX_EVENTS || bytes > MAX_BYTES - pending_bytes; };
            // Progress and unclaimed events must not displace a known request's final result.
            for (auto it = events.begin(); full() && it != events.end();) {
                if (it->type == PendingHttpEventType::Progress ||
                        (it->type != PendingHttpEventType::GeneralStateChanged && !requests.contains(it->request_id))) {
                    if (it->type != PendingHttpEventType::Progress) {
                        record_early_loss(static_cast<uint64_t>(it->request_id));
                    }
                    pending_bytes -= it->memory_bytes;
                    it = events.erase(it);
                } else {
                    ++it;
                }
            }
            if (full()) {
                if (!general && type != PendingHttpEventType::Progress) {
                    if (request != requests.end()) {
                        request->second.response_lost = true;
                    } else {
                        record_early_loss(static_cast<uint64_t>(id));
                    }
                }
                return;
            }
            PendingHttpEvent event{.type = type, .request_id = id, .memory_bytes = bytes};
            if (response != nullptr) {
                event.response = *response;
            }
            if (state != nullptr) {
                event.state = *state;
            }
            events.push_back(std::move(event));
            pending_bytes += bytes;
            if (request != requests.end() && type != PendingHttpEventType::Progress) {
                request->second.response_lost = false;
                request->second.missing_result_deadline = {};
            }
        }
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
