/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

void AppStoreApp::start_refresh_request_watchdog(
    system::core::AppContext &context,
    uint32_t timeout_ms,
    uint32_t retry_count
)
{
    stop_refresh_request_watchdog(context);
    const uint64_t delay_ms = static_cast<uint64_t>(timeout_ms) * (static_cast<uint64_t>(retry_count) + 1) +
                              REFRESH_REQUEST_TIMEOUT_EXTRA_MS;
    const int timer_delay_ms = static_cast<int>(
                                   std::min<uint64_t>(delay_ms, static_cast<uint64_t>(std::numeric_limits<int>::max()))
                               );
    auto timer = context.timer().start_delayed(REFRESH_REQUEST_TIMEOUT_TIMER_NAME, timer_delay_ms);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store refresh request watchdog: %1%", timer.error());
        return;
    }
    refresh_request_timeout_timer_id_ = *timer;
}

void AppStoreApp::stop_refresh_request_watchdog(system::core::AppContext &context)
{
    if (refresh_request_timeout_timer_id_ == system::core::INVALID_TIMER_ID) {
        return;
    }
    if (!context.timer().stop(refresh_request_timeout_timer_id_)) {
        BROOKESIA_LOGW(
            "Failed to stop App Store refresh request watchdog: %1%",
            refresh_request_timeout_timer_id_
        );
    }
    refresh_request_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
}

void AppStoreApp::process_refresh_request_timeout(system::core::AppContext &context)
{
    if (refresh_request_id_ == 0) {
        return;
    }
    const auto request_id = refresh_request_id_;
    BROOKESIA_LOGW("App Store refresh request timeout: request_id(%1%)", request_id);
    if (http_available_) {
        if (!HttpHelper::call_function_async(
                    HttpHelper::FunctionId::CancelRequest,
                    static_cast<double>(request_id)
                )) {
            BROOKESIA_LOGW("Failed to submit cancel for timed out App Store refresh request %1%", request_id);
        }
    }
    refresh_request_id_ = 0;
    finish_remote_refresh_with_error(context, "HTTP request timeout");
}

void AppStoreApp::schedule_refresh_result_processing(
    system::core::AppContext &context,
    PendingRefreshResultType type,
    const boost::json::object &response_json
)
{
    stop_refresh_result_timer(context);
    pending_refresh_result_type_ = type;
    pending_refresh_result_response_ = response_json;
    auto timer = context.timer().start_delayed(REFRESH_RESULT_TIMER_NAME, 500);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store refresh result processing: %1%", timer.error());
        process_pending_refresh_result(context);
        return;
    }
    refresh_result_timer_id_ = *timer;
}

void AppStoreApp::stop_refresh_result_timer(system::core::AppContext &context)
{
    if (refresh_result_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(refresh_result_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop App Store refresh result timer: %1%", refresh_result_timer_id_);
        }
        refresh_result_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    pending_refresh_result_type_ = PendingRefreshResultType::None;
    pending_refresh_result_response_.reset();
}

void AppStoreApp::process_pending_refresh_result(system::core::AppContext &context)
{
    (void)context;
    const auto type = pending_refresh_result_type_;
    const auto response = pending_refresh_result_response_.value_or(boost::json::object{});
    pending_refresh_result_type_ = PendingRefreshResultType::None;
    pending_refresh_result_response_.reset();

    processing_pending_refresh_result_ = true;
    switch (type) {
    case PendingRefreshResultType::Completed:
        handle_refresh_completed(response);
        break;
    case PendingRefreshResultType::Failed:
        handle_refresh_failed(response);
        break;
    case PendingRefreshResultType::Canceled:
        handle_refresh_canceled();
        break;
    case PendingRefreshResultType::None:
    default:
        break;
    }
    processing_pending_refresh_result_ = false;
}

void AppStoreApp::cancel_remote_refresh()
{
    if (context_ != nullptr) {
        stop_refresh_request_watchdog(*context_);
        stop_refresh_result_timer(*context_);
        stop_time_sync_timers(*context_);
    } else {
        refresh_request_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
        refresh_result_timer_id_ = system::core::INVALID_TIMER_ID;
        pending_refresh_result_type_ = PendingRefreshResultType::None;
        pending_refresh_result_response_.reset();
        time_sync_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
        time_sync_success_close_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    time_sync_waiting_ = false;

    if (refresh_request_id_ != 0 && http_available_) {
        const auto request_id = refresh_request_id_;
        if (!HttpHelper::call_function_async(
                    HttpHelper::FunctionId::CancelRequest,
                    static_cast<double>(request_id)
                )) {
            BROOKESIA_LOGW("Failed to submit cancel for App Store refresh request %1%", request_id);
        }
    }
    if (context_ != nullptr) {
        cancel_refresh_icon_update(*context_);
    } else {
        reset_refresh_icon_state();
    }
    refresh_request_id_ = 0;
    refresh_in_progress_ = false;
}

void AppStoreApp::handle_http_general_state_changed(std::string_view state_text)
{
    HttpHelper::GeneralState state = HttpHelper::GeneralState::Max;
    if (!BROOKESIA_DESCRIBE_STR_TO_ENUM(std::string(state_text), state)) {
        BROOKESIA_LOGW("Ignore unknown App Store HTTP general state: %1%", state_text);
        return;
    }

    http_general_state_ = state;
    BROOKESIA_LOGI("App Store HTTP general state changed: %1%", BROOKESIA_DESCRIBE_TO_STR(state));
    if (state == HttpHelper::GeneralState::Started && time_sync_waiting_ && context_ != nullptr) {
        finish_time_sync_wait_success(*context_);
    }
}

void AppStoreApp::handle_request_progress(double request_id, const boost::json::object &progress_json)
{
    if (refresh_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_request_id_) {
        handle_refresh_progress(progress_json);
        return;
    }
    if (refresh_icon_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_icon_request_id_) {
        handle_refresh_icon_progress(progress_json);
        return;
    }

    if (find_entry_by_size_metadata_request(static_cast<uint64_t>(request_id))) {
        return;
    }

    if (find_entry_by_metadata_request(static_cast<uint64_t>(request_id))) {
        return;
    }

    auto index = find_entry_by_request(static_cast<uint64_t>(request_id));
    if (!index || context_ == nullptr) {
        return;
    }
    HttpHelper::RequestProgress progress;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(progress_json, progress)) {
        return;
    }
    auto &entry = entries_[*index];
    entry.downloaded = progress.downloaded;
    entry.total = progress.total;
    update_download_progress_dialog_if_current(*context_, entry);
    (void)refresh_entry(*context_, *index);
}

void AppStoreApp::handle_request_completed(double request_id, const boost::json::object &response_json)
{
    if (refresh_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_request_id_) {
        handle_refresh_completed(response_json);
        return;
    }
    if (refresh_icon_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_icon_request_id_) {
        handle_refresh_icon_completed(response_json);
        return;
    }

    if (auto size_metadata_index = find_entry_by_size_metadata_request(static_cast<uint64_t>(request_id))) {
        handle_size_metadata_completed(*size_metadata_index, response_json);
        return;
    }

    if (auto metadata_index = find_entry_by_metadata_request(static_cast<uint64_t>(request_id))) {
        handle_metadata_completed(*metadata_index, response_json);
        return;
    }

    auto index = find_entry_by_request(static_cast<uint64_t>(request_id));
    if (!index || context_ == nullptr) {
        return;
    }
    HttpHelper::HttpResponse response;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(response_json, response)) {
        auto &entry = entries_[*index];
        entry.schema_error = tr("error.download_failed");
        entry.downloading = false;
        entry.download_request_id = 0;
        entry.download_path.clear();
        entry.downloaded = 0;
        entry.total = 0;
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        (void)refresh_entry(*context_, *index);
        return;
    }
    auto &entry = entries_[*index];
    entry.downloading = false;
    const auto completed_request_id = entry.download_request_id;
    if (response.error != HttpHelper::ErrorCode::Ok || response.status_code < 200 || response.status_code >= 300) {
        entry.schema_error = tr("error.download_failed");
        if (!response.error_message.empty()) {
            entry.schema_error = response.error_message;
        }
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        entry.download_request_id = 0;
        entry.download_path.clear();
        entry.downloaded = 0;
        entry.total = 0;
        (void)refresh_entry(*context_, *index);
        return;
    }
    if (entry.total == 0) {
        entry.total = entry.downloaded == 0 ? 100 : entry.downloaded;
    }
    entry.downloaded = entry.total;
    const auto package_path = entry.download_path.empty() ?
                              (download_dir(*context_) / (
                                   safe_name(entry.package_name) + "-" +
                                   safe_name(entry.latest_version.empty() ? "latest" : entry.latest_version) + ".bpk"
                               )) :
                              entry.download_path;
    entry.download_path.clear();
    entry.download_request_id = 0;
    if (message_dialog_purpose_ == MessageDialogPurpose::Download &&
            download_dialog_request_id_ == completed_request_id) {
        download_dialog_request_id_ = 0;
    }
    scan_local_packages(*context_);
    refresh_storage_capacity(*context_);
    auto result = install_package(*context_, package_path, localized(entry.app_names));
    if (!result) {
        entry.schema_error = result.error();
        (void)refresh_entry(*context_, *index);
    }
}

void AppStoreApp::handle_request_failed(double request_id, const boost::json::object &response_json)
{
    if (refresh_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_request_id_) {
        handle_refresh_failed(response_json);
        return;
    }
    if (refresh_icon_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_icon_request_id_) {
        handle_refresh_icon_failed(response_json);
        return;
    }

    if (auto size_metadata_index = find_entry_by_size_metadata_request(static_cast<uint64_t>(request_id))) {
        handle_size_metadata_failed(*size_metadata_index, response_json);
        return;
    }

    if (auto metadata_index = find_entry_by_metadata_request(static_cast<uint64_t>(request_id))) {
        handle_metadata_failed(*metadata_index, response_json);
        return;
    }

    auto index = find_entry_by_request(static_cast<uint64_t>(request_id));
    if (!index || context_ == nullptr) {
        return;
    }
    HttpHelper::HttpResponse response;
    if (BROOKESIA_DESCRIBE_FROM_JSON(response_json, response) && !response.error_message.empty()) {
        entries_[*index].schema_error = response.error_message;
    } else {
        entries_[*index].schema_error = tr("error.download_failed");
    }
    show_download_failed_dialog(*context_, entries_[*index], entries_[*index].schema_error);
    entries_[*index].downloading = false;
    entries_[*index].download_request_id = 0;
    entries_[*index].download_path.clear();
    entries_[*index].downloaded = 0;
    entries_[*index].total = 0;
    (void)refresh_entry(*context_, *index);
}

void AppStoreApp::handle_request_canceled(double request_id, const boost::json::object &)
{
    if (refresh_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_request_id_) {
        handle_refresh_canceled();
        return;
    }
    if (refresh_icon_request_id_ != 0 && static_cast<uint64_t>(request_id) == refresh_icon_request_id_) {
        handle_refresh_icon_canceled();
        return;
    }

    if (auto size_metadata_index = find_entry_by_size_metadata_request(static_cast<uint64_t>(request_id))) {
        handle_size_metadata_canceled(*size_metadata_index);
        return;
    }

    if (auto metadata_index = find_entry_by_metadata_request(static_cast<uint64_t>(request_id))) {
        handle_metadata_canceled(*metadata_index);
        return;
    }

    auto index = find_entry_by_request(static_cast<uint64_t>(request_id));
    if (!index || context_ == nullptr) {
        return;
    }
    const auto request_id_uint = static_cast<uint64_t>(request_id);
    if (message_dialog_purpose_ == MessageDialogPurpose::Download &&
            download_dialog_request_id_ == request_id_uint) {
        hide_message_dialog_if_visible(*context_);
    }
    entries_[*index].downloading = false;
    entries_[*index].download_request_id = 0;
    entries_[*index].download_path.clear();
    entries_[*index].downloaded = 0;
    entries_[*index].total = 0;
    (void)refresh_entry(*context_, *index);
}

void AppStoreApp::handle_metadata_completed(size_t index, const boost::json::object &response_json)
{
    if (context_ == nullptr || index >= entries_.size()) {
        return;
    }

    HttpHelper::HttpResponse response;
    auto &entry = entries_[index];
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(response_json, response)) {
        entry.schema_error = "Failed to parse metadata HTTP response";
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        (void)refresh_entry(*context_, index);
        return;
    }
    if (response.error != HttpHelper::ErrorCode::Ok || response.status_code < 200 || response.status_code >= 300) {
        entry.schema_error = "Metadata request failed: status(" + std::to_string(response.status_code) + ")";
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        (void)refresh_entry(*context_, index);
        return;
    }
    if (response.body.empty()) {
        entry.schema_error = "Metadata response body is empty";
        show_download_failed_dialog(*context_, entry, entry.schema_error);
        (void)refresh_entry(*context_, index);
        return;
    }

    auto result = parse_metadata_json(*context_, index, response.body);
    if (!result) {
        entries_[index].schema_error = result.error();
        entries_[index].metadata_loading = false;
        entries_[index].metadata_request_id = 0;
        show_download_failed_dialog(*context_, entries_[index], entries_[index].schema_error);
        (void)refresh_entry(*context_, index);
    }
}

void AppStoreApp::handle_metadata_failed(size_t index, const boost::json::object &response_json)
{
    if (context_ == nullptr || index >= entries_.size()) {
        return;
    }

    HttpHelper::HttpResponse response;
    auto &entry = entries_[index];
    if (BROOKESIA_DESCRIBE_FROM_JSON(response_json, response) && !response.error_message.empty()) {
        entry.schema_error = response.error_message;
    } else {
        entry.schema_error = tr("error.metadata_request_failed");
    }
    show_download_failed_dialog(*context_, entry, entry.schema_error);
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;
    (void)refresh_entry(*context_, index);
}

void AppStoreApp::handle_metadata_canceled(size_t index)
{
    if (context_ == nullptr || index >= entries_.size()) {
        return;
    }

    auto &entry = entries_[index];
    entry.metadata_loading = false;
    entry.metadata_request_id = 0;
    (void)refresh_entry(*context_, index);
}

void AppStoreApp::handle_size_metadata_completed(size_t index, const boost::json::object &response_json)
{
    if (index >= entries_.size()) {
        return;
    }

    HttpHelper::HttpResponse response;
    auto &entry = entries_[index];
    entry.size_metadata_loading = false;
    entry.size_metadata_request_id = 0;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(response_json, response)) {
        entry.size_metadata_failed = true;
        BROOKESIA_LOGW("Failed to parse App Store size metadata HTTP response: package(%1%)", entry.package_name);
        if (context_ != nullptr) {
            schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
        }
        return;
    }
    if (response.error != HttpHelper::ErrorCode::Ok || response.status_code < 200 || response.status_code >= 300) {
        entry.size_metadata_failed = true;
        BROOKESIA_LOGW(
            "App Store size metadata request failed: package(%1%), status(%2%), error(%3%)",
            entry.package_name, response.status_code, response.error_message
        );
        if (context_ != nullptr) {
            schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
        }
        return;
    }
    if (response.body.empty()) {
        entry.size_metadata_failed = true;
        BROOKESIA_LOGW("App Store size metadata response is empty: package(%1%)", entry.package_name);
        if (context_ != nullptr) {
            schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
        }
        return;
    }

    auto result = parse_size_metadata_json(index, response.body);
    if (!result) {
        entry.size_metadata_failed = true;
        BROOKESIA_LOGW(
            "Failed to parse App Store size metadata: package(%1%), error(%2%)",
            entry.package_name, result.error()
        );
        if (context_ != nullptr) {
            schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
        }
        return;
    }

    entry.size_metadata_failed = false;
    if (context_ != nullptr) {
        (void)refresh_entry(*context_, index);
        schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
    }
}

void AppStoreApp::handle_size_metadata_failed(size_t index, const boost::json::object &response_json)
{
    if (index >= entries_.size()) {
        return;
    }

    HttpHelper::HttpResponse response;
    auto &entry = entries_[index];
    if (BROOKESIA_DESCRIBE_FROM_JSON(response_json, response) && !response.error_message.empty()) {
        BROOKESIA_LOGW(
            "App Store size metadata request failed: package(%1%), error(%2%)",
            entry.package_name, response.error_message
        );
    } else {
        BROOKESIA_LOGW("App Store size metadata request failed: package(%1%)", entry.package_name);
    }
    entry.size_metadata_failed = true;
    entry.size_metadata_loading = false;
    entry.size_metadata_request_id = 0;
    if (context_ != nullptr) {
        schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
    }
}

void AppStoreApp::handle_size_metadata_canceled(size_t index)
{
    if (index >= entries_.size()) {
        return;
    }

    auto &entry = entries_[index];
    entry.size_metadata_loading = false;
    entry.size_metadata_request_id = 0;
    if (context_ != nullptr) {
        schedule_size_metadata_step(*context_, SIZE_METADATA_STEP_DELAY_MS);
    }
}

void AppStoreApp::handle_refresh_progress(const boost::json::object &progress_json)
{
    if (context_ == nullptr) {
        return;
    }

    HttpHelper::RequestProgress progress;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(progress_json, progress)) {
        return;
    }
    std::string label = tr("status.downloading_app_list_prefix") +
                        format_bytes(progress.downloaded);
    if (progress.total > 0) {
        label += " / " + format_bytes(progress.total);
    }
    update_message_dialog_if_visible(
        *context_,
        std::move(label),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
}

void AppStoreApp::handle_refresh_completed(const boost::json::object &response_json)
{
    if (context_ == nullptr) {
        return;
    }
    if (!processing_pending_refresh_result_) {
        schedule_refresh_result_processing(*context_, PendingRefreshResultType::Completed, response_json);
        return;
    }

    stop_refresh_request_watchdog(*context_);
    refresh_request_id_ = 0;
    HttpHelper::HttpResponse response;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(response_json, response)) {
        finish_remote_refresh_with_error(*context_, "Failed to parse HTTP response");
        return;
    }
    if (response.error != HttpHelper::ErrorCode::Ok || response.status_code < 200 || response.status_code >= 300) {
        finish_remote_refresh_with_error(
            *context_,
            "Index request failed: status(" + std::to_string(response.status_code) + "), error(" +
            std::string(BROOKESIA_DESCRIBE_TO_STR(response.error)) + ")"
        );
        return;
    }
    if (response.body.empty()) {
        finish_remote_refresh_with_error(*context_, "Index response body is empty");
        return;
    }

    update_message_dialog_if_visible(
        *context_,
        tr("dialog.parsing_app_list"),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
    if (!write_cache_text_file(*context_, INDEX_CACHE_FILE, response.body)) {
        BROOKESIA_LOGW("Failed to write App Store index cache");
    }

    auto parse_result = parse_index_json(*context_, response.body, configured_index_url(), IndexLoadMode::RemoteRefresh);
    if (!parse_result) {
        finish_remote_refresh_with_error(*context_, parse_result.error());
        return;
    }
    start_refresh_icon_update(*context_);
}

void AppStoreApp::handle_refresh_failed(const boost::json::object &response_json)
{
    if (context_ == nullptr) {
        return;
    }
    if (!processing_pending_refresh_result_) {
        schedule_refresh_result_processing(*context_, PendingRefreshResultType::Failed, response_json);
        return;
    }

    stop_refresh_request_watchdog(*context_);
    refresh_request_id_ = 0;
    HttpHelper::HttpResponse response;
    if (BROOKESIA_DESCRIBE_FROM_JSON(response_json, response) && !response.error_message.empty()) {
        finish_remote_refresh_with_error(*context_, response.error_message);
        return;
    }
    finish_remote_refresh_with_error(*context_, tr("error.http_request_failed"));
}

void AppStoreApp::handle_refresh_canceled()
{
    if (context_ == nullptr) {
        return;
    }
    if (!processing_pending_refresh_result_) {
        schedule_refresh_result_processing(*context_, PendingRefreshResultType::Canceled);
        return;
    }

    stop_refresh_request_watchdog(*context_);
    refresh_request_id_ = 0;
    finish_remote_refresh_with_error(*context_, tr("error.refresh_canceled"));
}

void AppStoreApp::handle_refresh_icon_progress(const boost::json::object &progress_json)
{
    if (context_ == nullptr || refresh_icon_purpose_ != IconUpdatePurpose::RefreshVisiblePage) {
        return;
    }
    const auto current_index = current_refresh_icon_index();
    if (!current_index) {
        return;
    }

    HttpHelper::RequestProgress progress;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(progress_json, progress)) {
        return;
    }
    std::string label = tr("dialog.updating_icons_prefix") +
                        std::to_string(refresh_icon_cursor_ + 1) + "/" +
                        std::to_string(refresh_icon_indices_.size()) +
                        ": " + localized(entries_[*current_index].app_names);
    if (progress.total > 0) {
        label += " (" + format_bytes(progress.downloaded) + " / " + format_bytes(progress.total) + ")";
    } else if (progress.downloaded > 0) {
        label += " (" + format_bytes(progress.downloaded) + ")";
    }
    update_message_dialog_if_visible(
        *context_,
        std::move(label),
        {},
        system::core::MessageDialogIcon::Information,
        0
    );
}

void AppStoreApp::handle_refresh_icon_completed(const boost::json::object &response_json)
{
    if (context_ == nullptr) {
        return;
    }

    const auto request_id = refresh_icon_request_id_;
    refresh_icon_request_id_ = 0;
    const auto current_index = current_refresh_icon_index();
    if (!current_index) {
        schedule_refresh_icon_step(*context_);
        return;
    }

    auto &entry = entries_[*current_index];
    entry.icon_request_id = 0;
    HttpHelper::HttpResponse response;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(response_json, response) ||
            response.error != HttpHelper::ErrorCode::Ok ||
            response.status_code < 200 ||
            response.status_code >= 300) {
        BROOKESIA_LOGW(
            "Failed to download App Store icon: url(%1%), request_id(%2%), status(%3%), error(%4%), message(%5%)",
            entry.icon_url,
            request_id,
            response.status_code,
            BROOKESIA_DESCRIBE_TO_STR(response.error),
            response.error_message
        );
        if (!entry.icon_download_path.empty()) {
            remove_invalid_icon_file(entry.icon_download_path, "icon download failed");
        }
    } else if (entry.icon_download_path.empty() || !read_png_size(entry.icon_download_path)) {
        if (!entry.icon_download_path.empty()) {
            remove_invalid_icon_file(entry.icon_download_path, "downloaded PNG validation failed");
        }
    } else {
        entry.icon_file_path = entry.icon_download_path.generic_string();
        auto register_result = register_icon(*context_, *current_index);
        if (!register_result) {
            BROOKESIA_LOGW("Failed to register App Store icon: %1%", register_result.error());
        }
    }
    entry.icon_download_path.clear();
    advance_refresh_icon_step(*context_);
}

void AppStoreApp::handle_refresh_icon_failed(const boost::json::object &response_json)
{
    if (context_ == nullptr) {
        return;
    }

    refresh_icon_request_id_ = 0;
    if (auto current_index = current_refresh_icon_index()) {
        auto &entry = entries_[*current_index];
        HttpHelper::HttpResponse response;
        if (BROOKESIA_DESCRIBE_FROM_JSON(response_json, response) && !response.error_message.empty()) {
            BROOKESIA_LOGW(
                "Failed to request App Store icon: url(%1%), error(%2%)",
                entry.icon_url, response.error_message
            );
        } else {
            BROOKESIA_LOGW("Failed to request App Store icon: url(%1%)", entry.icon_url);
        }
        if (!entry.icon_download_path.empty()) {
            remove_invalid_icon_file(entry.icon_download_path, "icon request failed");
        }
        entry.icon_request_id = 0;
        entry.icon_download_path.clear();
    }
    advance_refresh_icon_step(*context_);
}

void AppStoreApp::handle_refresh_icon_canceled()
{
    if (context_ == nullptr) {
        return;
    }

    refresh_icon_request_id_ = 0;
    if (auto current_index = current_refresh_icon_index()) {
        auto &entry = entries_[*current_index];
        entry.icon_request_id = 0;
        if (!entry.icon_download_path.empty()) {
            remove_invalid_icon_file(entry.icon_download_path, "icon request canceled");
        }
        entry.icon_download_path.clear();
    }
    advance_refresh_icon_step(*context_);
}

std::optional<size_t> AppStoreApp::find_entry_by_request(uint64_t request_id) const
{
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].download_request_id == request_id) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> AppStoreApp::find_entry_by_metadata_request(uint64_t request_id) const
{
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].metadata_request_id == request_id) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> AppStoreApp::find_entry_by_size_metadata_request(uint64_t request_id) const
{
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].size_metadata_request_id == request_id) {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<AppStoreApp::VisibleItemRef> AppStoreApp::find_visible_item_by_path(std::string_view path) const
{
    if (!path.starts_with(LIST_PATH)) {
        return std::nullopt;
    }
    std::string_view suffix(path);
    suffix.remove_prefix(std::string_view(LIST_PATH).size());
    if (!suffix.empty() && suffix.front() == '/') {
        suffix.remove_prefix(1);
    }
    const auto slash = suffix.find('/');
    const auto instance_id = std::string(suffix.substr(0, slash));
    auto it = instance_to_entry_.find(instance_id);
    if (it == instance_to_entry_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void AppStoreApp::start_refresh_icon_update(system::core::AppContext &context)
{
    restart_visible_icon_update(context, IconUpdatePurpose::RefreshVisiblePage);
}

void AppStoreApp::start_visible_icon_update(system::core::AppContext &context)
{
    if (refresh_in_progress_) {
        if (refresh_icon_purpose_ == IconUpdatePurpose::RefreshVisiblePage) {
            restart_visible_icon_update(context, IconUpdatePurpose::RefreshVisiblePage);
        }
        return;
    }
    restart_visible_icon_update(context, IconUpdatePurpose::LazyVisiblePage);
}

void AppStoreApp::restart_visible_icon_update(system::core::AppContext &context, IconUpdatePurpose purpose)
{
    cancel_refresh_icon_update(context);
    if (view_mode_ != ViewMode::Store || !http_available_) {
        if (purpose == IconUpdatePurpose::RefreshVisiblePage) {
            finish_remote_refresh_success(context);
        }
        return;
    }

    refresh_icon_indices_ = build_visible_icon_indices();
    refresh_icon_cursor_ = 0;
    refresh_icon_purpose_ = purpose;
    if (refresh_icon_indices_.empty()) {
        finish_refresh_icon_update(context);
        return;
    }

    if (refresh_icon_purpose_ == IconUpdatePurpose::RefreshVisiblePage) {
        update_message_dialog_if_visible(
            context,
            tr("status.loaded_icons_prefix") + std::to_string(entries_.size()) +
            tr("status.loaded_icons_suffix"),
            {},
            system::core::MessageDialogIcon::Information,
            0
        );
    }
    schedule_refresh_icon_step(context);
}

void AppStoreApp::cancel_refresh_icon_update(system::core::AppContext &context)
{
    stop_refresh_icon_timer(context);
    const auto current_index = current_refresh_icon_index();
    if (refresh_icon_request_id_ != 0 && http_available_) {
        const auto request_id = refresh_icon_request_id_;
        if (!HttpHelper::call_function_async(
                    HttpHelper::FunctionId::CancelRequest,
                    static_cast<double>(request_id)
                )) {
            BROOKESIA_LOGW("Failed to submit cancel for App Store icon request %1%", request_id);
        }
    }
    if (current_index) {
        auto &entry = entries_[*current_index];
        entry.icon_request_id = 0;
        if (!entry.icon_download_path.empty()) {
            remove_invalid_icon_file(entry.icon_download_path, "icon request canceled");
        }
        entry.icon_download_path.clear();
    }
    reset_refresh_icon_state();
}

void AppStoreApp::reset_refresh_icon_state()
{
    refresh_icon_request_id_ = 0;
    refresh_icon_indices_.clear();
    refresh_icon_cursor_ = 0;
    refresh_icon_purpose_ = IconUpdatePurpose::None;
}

std::vector<size_t> AppStoreApp::build_visible_icon_indices() const
{
    std::vector<size_t> indices;
    if (view_mode_ != ViewMode::Store) {
        return indices;
    }

    const auto page_start = get_page_start(list_page_);
    const auto visible_count = get_page_visible_count(visible_items_.size(), list_page_);
    for (size_t i = 0; i < visible_count; ++i) {
        const auto &ref = visible_items_[page_start + i];
        if (ref.kind == VisibleItemKind::Store && ref.index < entries_.size() &&
                should_fetch_icon(entries_[ref.index])) {
            indices.push_back(ref.index);
        }
    }
    return indices;
}

std::optional<size_t> AppStoreApp::current_refresh_icon_index() const
{
    if (refresh_icon_cursor_ >= refresh_icon_indices_.size()) {
        return std::nullopt;
    }
    const auto index = refresh_icon_indices_[refresh_icon_cursor_];
    if (index >= entries_.size()) {
        return std::nullopt;
    }
    return index;
}

void AppStoreApp::advance_refresh_icon_step(system::core::AppContext &context)
{
    if (refresh_icon_cursor_ < refresh_icon_indices_.size()) {
        ++refresh_icon_cursor_;
    }
    schedule_refresh_icon_step(context);
}

void AppStoreApp::finish_refresh_icon_update(system::core::AppContext &context)
{
    const auto purpose = refresh_icon_purpose_;
    reset_refresh_icon_state();
    if (purpose == IconUpdatePurpose::RefreshVisiblePage) {
        finish_remote_refresh_success(context);
        return;
    }
    if (purpose == IconUpdatePurpose::LazyVisiblePage) {
        (void)refresh_ui(context);
    }
}

std::expected<void, std::string> AppStoreApp::process_refresh_icon_step(system::core::AppContext &context)
{
    if (refresh_icon_request_id_ != 0 || refresh_icon_purpose_ == IconUpdatePurpose::None) {
        return {};
    }
    if (refresh_icon_purpose_ == IconUpdatePurpose::RefreshVisiblePage && !refresh_in_progress_) {
        reset_refresh_icon_state();
        return {};
    }
    if (refresh_icon_purpose_ == IconUpdatePurpose::LazyVisiblePage &&
            (view_mode_ != ViewMode::Store || !http_available_)) {
        reset_refresh_icon_state();
        return {};
    }
    if (refresh_icon_cursor_ >= refresh_icon_indices_.size()) {
        finish_refresh_icon_update(context);
        return {};
    }

    const auto current_index = current_refresh_icon_index();
    if (!current_index) {
        advance_refresh_icon_step(context);
        return {};
    }

    auto &entry = entries_[*current_index];
    if (!should_fetch_icon(entry)) {
        advance_refresh_icon_step(context);
        return {};
    }

    if (refresh_icon_purpose_ == IconUpdatePurpose::RefreshVisiblePage) {
        const auto label = tr("dialog.updating_icons_prefix") +
                           std::to_string(refresh_icon_cursor_ + 1) + "/" +
                           std::to_string(refresh_icon_indices_.size()) +
                           ": " + localized(entry.app_names);
        update_message_dialog_if_visible(
            context,
            label,
            {},
            system::core::MessageDialogIcon::Information,
            0
        );
    }

    const auto safe_package = safe_name(entry.package_name);
    const auto relative_icon_path = std::filesystem::path(ICON_DIR) / (safe_package + ".png");
    for (const auto &cached_path : cache_file_candidates(context, relative_icon_path)) {
        std::error_code error_code;
        if (!std::filesystem::is_regular_file(cached_path, error_code) || error_code) {
            error_code.clear();
            continue;
        }
        if (!read_png_size(cached_path)) {
            remove_invalid_icon_file(cached_path, "cached PNG validation failed before refresh");
            continue;
        }
        entry.icon_file_path = cached_path.generic_string();
        auto register_result = register_icon(context, *current_index);
        if (!register_result) {
            BROOKESIA_LOGW("Failed to register cached App Store icon: %1%", register_result.error());
        }
        advance_refresh_icon_step(context);
        return {};
    }

    auto output = writable_cache_file(context, relative_icon_path);
    if (!output) {
        BROOKESIA_LOGW("Failed to create App Store icon cache directory");
        advance_refresh_icon_step(context);
        return {};
    }
    remove_invalid_icon_file(*output, "cached PNG validation failed before refresh download");

    auto request = make_get_request(entry.icon_url);
    request.download_path = output->generic_string();
    request.max_file_size = ICON_MAX_FILE_SIZE;
    auto request_id = HttpHelper::call_function_sync<double>(
                          HttpHelper::FunctionId::RequestAsync,
                          BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                      );
    if (!request_id) {
        BROOKESIA_LOGW(
            "Failed to submit App Store icon request: url(%1%), error(%2%)",
            entry.icon_url, request_id.error()
        );
        remove_invalid_icon_file(*output, "icon request submit failed");
        advance_refresh_icon_step(context);
        return {};
    }
    refresh_icon_request_id_ = static_cast<uint64_t>(*request_id);
    entry.icon_request_id = refresh_icon_request_id_;
    entry.icon_download_path = *output;
    return {};
}

void AppStoreApp::schedule_refresh_icon_step(system::core::AppContext &context)
{
    if (refresh_icon_purpose_ == IconUpdatePurpose::None ||
            refresh_icon_timer_id_ != system::core::INVALID_TIMER_ID) {
        return;
    }

    auto timer = context.timer().start_delayed(REFRESH_ICON_TIMER_NAME, REFRESH_ICON_STEP_DELAY_MS);
    if (!timer) {
        if (refresh_icon_purpose_ == IconUpdatePurpose::RefreshVisiblePage) {
            finish_remote_refresh_with_error(context, "Failed to schedule icon update: " + timer.error());
        } else {
            BROOKESIA_LOGW("Failed to schedule App Store visible icon update: %1%", timer.error());
            reset_refresh_icon_state();
        }
        return;
    }
    refresh_icon_timer_id_ = *timer;
}

void AppStoreApp::stop_refresh_icon_timer(system::core::AppContext &context)
{
    if (refresh_icon_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(refresh_icon_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop App Store refresh icon timer: %1%", refresh_icon_timer_id_);
        }
        refresh_icon_timer_id_ = system::core::INVALID_TIMER_ID;
    }
}

void AppStoreApp::begin_time_sync_wait(system::core::AppContext &context)
{
    if (time_sync_success_close_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(time_sync_success_close_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop App Store time sync success close timer: %1%",
                time_sync_success_close_timer_id_
            );
        }
        time_sync_success_close_timer_id_ = system::core::INVALID_TIMER_ID;
    }

    refresh_request_id_ = 0;
    reset_refresh_icon_state();
    time_sync_waiting_ = true;
    status_text_ = tr("status.waiting_time_sync");
    ensure_message_dialog(
        context,
        tr("dialog.waiting_time_sync"),
        tr("dialog.waiting_time_sync_body"),
        system::core::MessageDialogIcon::Warning,
        0
    );
    (void)refresh_ui(context);

    if (time_sync_timeout_timer_id_ != system::core::INVALID_TIMER_ID) {
        return;
    }
    auto timer = context.timer().start_delayed(TIME_SYNC_TIMEOUT_TIMER_NAME, TIME_SYNC_WAIT_TIMEOUT_MS);
    if (!timer) {
        time_sync_waiting_ = false;
        finish_remote_refresh_with_error(context, "Failed to schedule time sync wait: " + timer.error());
        return;
    }
    time_sync_timeout_timer_id_ = *timer;
    BROOKESIA_LOGI("App Store waiting for HTTP time sync: timeout_ms(%1%)", TIME_SYNC_WAIT_TIMEOUT_MS);
}

void AppStoreApp::stop_time_sync_timers(system::core::AppContext &context)
{
    if (time_sync_timeout_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(time_sync_timeout_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop App Store time sync timeout timer: %1%", time_sync_timeout_timer_id_);
        }
        time_sync_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    if (time_sync_success_close_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(time_sync_success_close_timer_id_)) {
            BROOKESIA_LOGW(
                "Failed to stop App Store time sync success close timer: %1%",
                time_sync_success_close_timer_id_
            );
        }
        time_sync_success_close_timer_id_ = system::core::INVALID_TIMER_ID;
    }
}

void AppStoreApp::finish_time_sync_wait_success(system::core::AppContext &context)
{
    if (!time_sync_waiting_) {
        return;
    }

    if (time_sync_timeout_timer_id_ != system::core::INVALID_TIMER_ID) {
        if (!context.timer().stop(time_sync_timeout_timer_id_)) {
            BROOKESIA_LOGW("Failed to stop App Store time sync timeout timer: %1%", time_sync_timeout_timer_id_);
        }
        time_sync_timeout_timer_id_ = system::core::INVALID_TIMER_ID;
    }
    time_sync_waiting_ = false;
    status_text_ = tr("status.time_synchronized");
    ensure_message_dialog(
        context,
        tr("dialog.time_synchronized"),
        tr("dialog.time_synchronized_body"),
        system::core::MessageDialogIcon::Information,
        0
    );
    (void)refresh_ui(context);

    if (time_sync_success_close_timer_id_ != system::core::INVALID_TIMER_ID) {
        return;
    }
    auto timer = context.timer().start_delayed(TIME_SYNC_SUCCESS_CLOSE_TIMER_NAME, TIME_SYNC_SUCCESS_CLOSE_MS);
    if (!timer) {
        BROOKESIA_LOGW("Failed to schedule App Store time sync success close timer: %1%", timer.error());
        process_time_sync_success_close(context);
        return;
    }
    time_sync_success_close_timer_id_ = *timer;
}

void AppStoreApp::finish_time_sync_wait_timeout(system::core::AppContext &context)
{
    if (!time_sync_waiting_) {
        return;
    }

    time_sync_waiting_ = false;
    refresh_request_id_ = 0;
    reset_refresh_icon_state();
    refresh_in_progress_ = false;
    status_text_ = tr("status.time_sync_timeout");
    ensure_message_dialog(
        context,
        tr("dialog.time_sync_timeout"),
        tr("dialog.time_sync_timeout_body"),
        system::core::MessageDialogIcon::Critical,
        0,
    MessageDialogPurpose::TimeSyncRetry, {
        system::core::MessageDialogButton{
            .text = tr("action.refresh"),
            .role = system::core::MessageDialogButtonRole::Action,
        },
        system::core::MessageDialogButton{
            .text = tr("action.close"),
            .role = system::core::MessageDialogButtonRole::Reject,
        },
    }
    );
    (void)populate_entries(context);
}

void AppStoreApp::process_time_sync_success_close(system::core::AppContext &context)
{
    hide_message_dialog_if_visible(context);
    if (!refresh_in_progress_) {
        return;
    }
    (void)start_remote_refresh_request(context);
}

bool AppStoreApp::should_wait_for_http_time_sync() const
{
    return http_general_state_.has_value() && *http_general_state_ == HttpHelper::GeneralState::TimeSyncing;
}

bool AppStoreApp::submit_network_check(system::core::AppContext &context, NetworkCheckPurpose purpose)
{
    bind_device_service();
    if (!device_available_) {
        if (purpose == NetworkCheckPurpose::StartupCache) {
            show_network_unavailable_dialog(context);
        }
        return false;
    }

    const auto generation = async_generation_;
    auto handler = [this, generation, purpose](service::FunctionResult && result) {
        handle_network_check_result(generation, purpose, std::move(result));
    };
    if (!DeviceHelper::call_function_async(DeviceHelper::FunctionId::GetNetworkConnectivityInfo, handler)) {
        BROOKESIA_LOGW("Failed to submit App Store network connectivity request");
        return false;
    }
    return true;
}

void AppStoreApp::handle_network_check_result(
    uint64_t generation,
    NetworkCheckPurpose purpose,
    service::FunctionResult &&result
)
{
    if (generation != async_generation_ || context_ == nullptr) {
        return;
    }

    auto info_result = DeviceHelper::process_function_result<boost::json::array>(result);
    std::string network_error;
    if (!info_result) {
        network_error = info_result.error();
    } else if (!parse_network_ready(*info_result, network_error)) {
        // parse_network_ready fills network_error.
    } else {
        network_ready_ = true;
        (void)refresh_ui(*context_);
        if (purpose == NetworkCheckPurpose::StartupCache) {
            return;
        }
        (void)start_remote_refresh_request(*context_);
        return;
    }

    BROOKESIA_LOGI("App Store network unavailable: %1%", network_error);
    network_ready_ = false;
    show_network_unavailable_dialog(*context_);
    if (purpose == NetworkCheckPurpose::StartupCache) {
        (void)refresh_ui(*context_);
        return;
    }

    refresh_request_id_ = 0;
    reset_refresh_icon_state();
    refresh_in_progress_ = false;
    status_text_ = tr("status.network_unavailable");
    (void)populate_entries(*context_);
}

bool AppStoreApp::parse_network_ready(const boost::json::array &json, std::string &error_message) const
{
    DeviceHelper::NetworkConnectivityInfos infos;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(json, infos)) {
        error_message = "Failed to parse network connectivity info";
        return false;
    }
    if (infos.empty()) {
        error_message = "No network connectivity interface is available";
        return false;
    }

    std::ostringstream summary;
    bool has_ready_network = false;
    for (size_t i = 0; i < infos.size(); ++i) {
        const auto &info = infos[i];
        BROOKESIA_LOGI(
            "App Store network status: instance(%1%), state(%2%), network_ready(%3%), internet_ready(%4%)",
            info.instance_name,
            BROOKESIA_DESCRIBE_TO_STR(info.state),
            info.network_ready,
            info.internet_ready
        );
        if (info.network_ready) {
            has_ready_network = true;
        }
        if (i > 0) {
            summary << "; ";
        }
        summary << (info.instance_name.empty() ? "<unknown>" : info.instance_name)
                << "=" << BROOKESIA_DESCRIBE_TO_STR(info.state);
    }
    if (!has_ready_network) {
        error_message = "Network is not ready: " + summary.str();
        return false;
    }
    error_message.clear();
    return true;
}

void AppStoreApp::show_network_unavailable_dialog(system::core::AppContext &context)
{
    ensure_message_dialog(
        context,
        tr("status.network_unavailable"),
        tr("dialog.network_unavailable_body"),
        system::core::MessageDialogIcon::Warning,
        NETWORK_UNAVAILABLE_DIALOG_AUTO_CLOSE_MS
    );
}

system::core::MessageDialogOptions AppStoreApp::make_message_dialog_options(
    std::string text,
    std::string informative_text,
    system::core::MessageDialogIcon icon,
    int32_t auto_close_ms,
    std::vector<system::core::MessageDialogButton> buttons
) const
{
    return system::core::MessageDialogOptions{
        .text = std::move(text),
        .informative_text = std::move(informative_text),
        .icon = icon,
        .buttons = std::move(buttons),
        .auto_close_ms = auto_close_ms,
    };
}

void AppStoreApp::ensure_message_dialog(
    system::core::AppContext &context,
    std::string text,
    std::string informative_text,
    system::core::MessageDialogIcon icon,
    int32_t auto_close_ms,
    MessageDialogPurpose purpose,
    std::vector<system::core::MessageDialogButton> buttons
)
{
    refresh_dialog_message_ = text;
    message_dialog_purpose_ = purpose;
    if (purpose != MessageDialogPurpose::Download) {
        download_dialog_request_id_ = 0;
    }
    auto options = make_message_dialog_options(
                       std::move(text),
                       std::move(informative_text),
                       icon,
                       auto_close_ms,
                       std::move(buttons)
                   );
    if (message_dialog_request_id_ != system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        auto result = context.system_service().update_message_dialog(message_dialog_request_id_, options);
        if (result) {
            return;
        }
        BROOKESIA_LOGW("Failed to update App Store message dialog: %1%", result.error());
        message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    }

    auto request_id = context.system_service().show_message_dialog(
                          std::move(options),
    [this](const system::core::MessageDialogResult & result) {
        handle_message_dialog_result(result);
    }
                      );
    if (!request_id) {
        BROOKESIA_LOGW("Failed to show App Store message dialog: %1%", request_id.error());
        message_dialog_purpose_ = MessageDialogPurpose::None;
        download_dialog_request_id_ = 0;
        return;
    }
    message_dialog_request_id_ = *request_id;
}

void AppStoreApp::update_message_dialog_if_visible(
    system::core::AppContext &context,
    std::string text,
    std::string informative_text,
    system::core::MessageDialogIcon icon,
    int32_t auto_close_ms,
    MessageDialogPurpose purpose,
    std::vector<system::core::MessageDialogButton> buttons
)
{
    refresh_dialog_message_ = text;
    if (message_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        return;
    }

    message_dialog_purpose_ = purpose;
    if (purpose != MessageDialogPurpose::Download) {
        download_dialog_request_id_ = 0;
    }
    auto options = make_message_dialog_options(
                       std::move(text),
                       std::move(informative_text),
                       icon,
                       auto_close_ms,
                       std::move(buttons)
                   );
    auto result = context.system_service().update_message_dialog(message_dialog_request_id_, std::move(options));
    if (!result) {
        BROOKESIA_LOGW("Failed to update App Store message dialog: %1%", result.error());
        message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
        message_dialog_purpose_ = MessageDialogPurpose::None;
        download_dialog_request_id_ = 0;
    }
}

void AppStoreApp::hide_message_dialog_if_visible(system::core::AppContext &context)
{
    if (message_dialog_request_id_ == system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID) {
        message_dialog_purpose_ = MessageDialogPurpose::None;
        download_dialog_request_id_ = 0;
        return;
    }

    auto result = context.system_service().hide_message_dialog(message_dialog_request_id_);
    if (!result) {
        BROOKESIA_LOGW("Failed to hide App Store message dialog: %1%", result.error());
    }
    message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    message_dialog_purpose_ = MessageDialogPurpose::None;
    download_dialog_request_id_ = 0;
}

void AppStoreApp::handle_message_dialog_result(const system::core::MessageDialogResult &result)
{
    if (message_dialog_request_id_ != result.request_id) {
        return;
    }

    const auto purpose = message_dialog_purpose_;
    message_dialog_request_id_ = system::core::INVALID_MESSAGE_DIALOG_REQUEST_ID;
    message_dialog_purpose_ = MessageDialogPurpose::None;
    download_dialog_request_id_ = 0;
    if (purpose != MessageDialogPurpose::TimeSyncRetry ||
            result.button_role != system::core::MessageDialogButtonRole::Action ||
            context_ == nullptr) {
        return;
    }

    BROOKESIA_LOGI("Retry App Store refresh from time sync timeout dialog");
    (void)start_remote_refresh(*context_);
}

void AppStoreApp::finish_remote_refresh_with_error(
    system::core::AppContext &context,
    std::string error_message
)
{
    stop_refresh_request_watchdog(context);
    refresh_request_id_ = 0;
    refresh_in_progress_ = false;
    reset_refresh_icon_state();
    status_text_ = error_message;
    update_message_dialog_if_visible(
        context,
        tr("dialog.refresh_failed_prefix") + error_message,
        {},
        system::core::MessageDialogIcon::Warning,
        REFRESH_FAILED_DIALOG_AUTO_CLOSE_MS
    );
    (void)populate_entries(context);
}

void AppStoreApp::finish_remote_refresh_success(system::core::AppContext &context)
{
    stop_refresh_request_watchdog(context);
    refresh_request_id_ = 0;
    refresh_in_progress_ = false;
    reset_refresh_icon_state();
    status_text_ = tr("status.ready");
    update_message_dialog_if_visible(
        context,
        tr("status.refresh_complete_prefix") + std::to_string(entries_.size()) +
        tr("status.refresh_complete_suffix"),
        {},
        system::core::MessageDialogIcon::Information,
        REFRESH_SUCCESS_DIALOG_AUTO_CLOSE_MS
    );
    (void)refresh_ui(context);
}
