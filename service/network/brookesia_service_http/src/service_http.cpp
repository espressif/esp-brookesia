/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <exception>
#include <vector>
#include "boost/json.hpp"
#if !defined(__EMSCRIPTEN__)
#include "boost/thread.hpp"
#endif
#include "brookesia/service_http/macro_configs.h"
#if !BROOKESIA_SERVICE_HTTP_SERVICE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/thread_config.hpp"
#include "brookesia/service_helper/network/sntp.hpp"
#include "brookesia/service_http/service_http.hpp"

namespace esp_brookesia::service::http {

using SNTPHelper = helper::SNTP;

namespace {

#if !defined(__EMSCRIPTEN__)
constexpr const char *FILE_IO_THREAD_NAME = "SvcHttpFileIO";
#endif

bool is_http_url(const std::string &url)
{
    return url.rfind("http://", 0) == 0;
}

bool is_https_url(const std::string &url)
{
    return url.rfind("https://", 0) == 0;
}

uint32_t get_effective_limit(uint32_t request_limit, uint32_t default_limit)
{
    return request_limit > 0 ? request_limit : default_limit;
}

boost::json::object http_response_to_json(const helper::Http::HttpResponse &response)
{
    return {
        {"request_id", response.request_id},
        {"status_code", response.status_code},
        {"headers", response.headers},
        {"body", response.body},
        {"file_path", response.file_path},
        {"error", BROOKESIA_DESCRIBE_TO_STR(response.error)},
        {"error_message", response.error_message},
    };
}

boost::json::object request_progress_to_json(const helper::Http::RequestProgress &progress)
{
    return {
        {"request_id", progress.request_id},
        {"downloaded", progress.downloaded},
        {"total", progress.total},
    };
}

boost::json::object request_state_info_to_json(const helper::Http::RequestStateInfo &info)
{
    return {
        {"request_id", info.request_id},
        {"state", BROOKESIA_DESCRIBE_TO_STR(info.state)},
        {"error", BROOKESIA_DESCRIBE_TO_STR(info.error)},
        {"status_code", info.status_code},
        {"downloaded", info.downloaded},
        {"total", info.total},
    };
}

bool should_retry_status(const helper::Http::HttpRequest &request, int status_code)
{
    return std::find(
               request.retry_on_status_codes.begin(), request.retry_on_status_codes.end(), status_code
           ) != request.retry_on_status_codes.end();
}

} // namespace

bool Http::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_SERVICE_HTTP_VER_MAJOR, BROOKESIA_SERVICE_HTTP_VER_MINOR,
        BROOKESIA_SERVICE_HTTP_VER_PATCH
    );

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        state_machine_ = std::make_unique<StateMachine>(), false, "Failed to create state machine"
    );
    BROOKESIA_CHECK_FALSE_RETURN(state_machine_->init(), false, "Failed to initialize state machine");
    state_machine_->set_time_sync_checker([this]() {
        return is_time_synced();
    });
    state_machine_->set_state_changed_callback([this](GeneralState state) {
        set_general_state_from_state_machine(state);
    });

    // Directly assign the field instead of going through set_general_state. The
    // service base only registers the GeneralStateChanged signal *after* on_init
    // returns, so publishing here would always log a spurious "signal not found".
    // Subscribers see the first published transition when on_start fires Started.
    general_state_ = GeneralState::Inited;
    BROOKESIA_LOGI("HTTP service initialized");

    return true;
}

void Http::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    cancel_all_requests();
    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        // Service instance is cached by PluginRegistry across init/deinit cycles, so retained
        // async-request contexts (retain_after_finish=true) would accumulate without explicit
        // teardown. Drop them here so they don't outlive the deinit boundary.
        contexts_.clear();
        running_request_count_ = 0;
    }
    sntp_binding_.release();
    http_iface_.reset();
    state_machine_.reset();
    // Mirror on_init: the signal is unregistered before on_deinit completes, so
    // skip publishing and just record the final state.
    general_state_ = GeneralState::Idle;
}

bool Http::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    scheduler_ = get_task_scheduler();
    BROOKESIA_CHECK_NULL_RETURN(scheduler_, false, "Failed to get task scheduler");
    http_iface_ = hal::acquire_first_interface<hal::network::HttpClientIface>();
    BROOKESIA_CHECK_FALSE_RETURN(http_iface_, false, "Failed to acquire ProtocolHttp HAL interface");
    lib_utils::FunctionGuard stop_guard([this]() {
        if (state_machine_) {
            state_machine_->stop();
        }
        scheduler_.reset();
        sntp_binding_.release();
        http_iface_.reset();
    });
    if (state_machine_) {
        BROOKESIA_CHECK_FALSE_RETURN(
            state_machine_->start(scheduler_, get_state_task_group()), false, "Failed to start state machine"
        );
    }

    bool time_sync_started = false;
    BROOKESIA_CHECK_FALSE_RETURN(start_time_sync_if_available(time_sync_started), false, "Failed to start time sync");
    if (time_sync_started) {
        set_general_state(GeneralState::TimeSyncing);
        BROOKESIA_LOGI("HTTP service waiting for time sync");
    } else {
        set_general_state(GeneralState::Started);
        BROOKESIA_LOGI("HTTP service started");
    }

    BROOKESIA_CHECK_FALSE_RETURN(
    scheduler_->post_periodic([this]() -> bool {
        emit_pending_progress();
        return true;
    }, BROOKESIA_SERVICE_HTTP_PROGRESS_PUBLISH_INTERVAL_MS, &progress_emitter_task_id_, get_call_task_group()),
    false, "Failed to post progress emitter task"
    );

    stop_guard.release();

    return true;
}

void Http::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_general_state(GeneralState::Stopping);
    cancel_all_requests();
    if (scheduler_ && progress_emitter_task_id_ != 0) {
        scheduler_->cancel(progress_emitter_task_id_);
        progress_emitter_task_id_ = 0;
    }
    {
        std::lock_guard<std::mutex> lock(pending_progress_mutex_);
        pending_progress_.reset();
    }
    // Keep this order so active workers can clean up transactions before the scheduler is torn down.
    if (state_machine_) {
        state_machine_->stop();
    }
    scheduler_.reset();
    set_general_state(GeneralState::Stopped);
    sntp_binding_.release();
    http_iface_.reset();
    BROOKESIA_LOGI("HTTP service stopped");
}

std::expected<boost::json::object, std::string> Http::function_request(const boost::json::object &request)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: request(%1%)", BROOKESIA_DESCRIBE_TO_STR(request));

    HttpRequest http_request;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(request, http_request)) {
        return std::unexpected("Invalid HTTP request object");
    }

    auto response_promise = std::make_shared<std::promise<HttpResponse>>();
    auto response_future = response_promise->get_future();
    auto submit_result = submit_request(std::move(http_request), response_promise);
    if (!submit_result) {
        return std::unexpected(submit_result.error());
    }

    auto wait_timeout_ms = submit_result.value()->request.timeout_ms *
                           (submit_result.value()->request.retry_count + 1) +
                           BROOKESIA_SERVICE_HTTP_SYNC_WAIT_EXTRA_MS;
    if (response_future.wait_for(std::chrono::milliseconds(wait_timeout_ms)) != std::future_status::ready) {
        submit_result.value()->cancel_requested.store(true);
        close_context_transaction(*submit_result.value());
        return std::unexpected("HTTP request timeout: id(" + std::to_string(submit_result.value()->request_id) + ")");
    }

    return http_response_to_json(response_future.get());
}

std::expected<double, std::string> Http::function_request_async(const boost::json::object &request)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: request(%1%)", BROOKESIA_DESCRIBE_TO_STR(request));

    HttpRequest http_request;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(request, http_request)) {
        return std::unexpected("Invalid HTTP request object");
    }

    auto submit_result = submit_request(std::move(http_request));
    if (!submit_result) {
        return std::unexpected(submit_result.error());
    }

    BROOKESIA_LOGI(
        "Submitted async HTTP request: id(%1%), url(%2%)",
        submit_result.value()->request_id, submit_result.value()->request.url
    );
    return static_cast<double>(submit_result.value()->request_id);
}

std::expected<void, std::string> Http::function_cancel_request(double request_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: request_id(%1%)", request_id);

    auto id = static_cast<uint64_t>(request_id);
    std::shared_ptr<RequestContext> context;
    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        auto it = contexts_.find(id);
        if (it == contexts_.end()) {
            return std::unexpected("Request not found");
        }
        context = it->second;
    }

    context->cancel_requested.store(true);
    close_context_transaction(*context);
    BROOKESIA_LOGI("Canceled HTTP request: id(%1%)", id);

    return {};
}

std::expected<boost::json::object, std::string> Http::function_get_request_state(double request_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: request_id(%1%)", request_id);

    auto info = get_request_state_info(static_cast<uint64_t>(request_id));
    if (info.request_id == 0) {
        return std::unexpected("Request not found");
    }

    return request_state_info_to_json(info);
}

std::expected<void, std::string> Http::function_reset_statistics()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        statistics_ = {};
    }
    BROOKESIA_LOGI("HTTP statistics reset");

    return {};
}

std::expected<std::shared_ptr<Http::RequestContext>, std::string> Http::submit_request(
    HttpRequest request, std::shared_ptr<std::promise<HttpResponse>> response_promise
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (scheduler_ == nullptr) {
        return std::unexpected("HTTP service is not started");
    }
    if (general_state_ != GeneralState::Started) {
        return std::unexpected(
                   "HTTP service is not ready: state(" + BROOKESIA_DESCRIBE_TO_STR(general_state_) + ")"
               );
    }

    std::string error_message;
    if (!validate_request(request, error_message)) {
        return std::unexpected(error_message);
    }

    auto context = std::make_shared<RequestContext>();
    context->request_id = next_request_id_.fetch_add(1);
    context->request = std::move(request);
    context->response_promise = std::move(response_promise);
    context->retain_after_finish = context->response_promise == nullptr;
    context->response.request_id = context->request_id;

    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        if (running_request_count_ >= BROOKESIA_SERVICE_HTTP_MAX_CONCURRENT_REQUESTS) {
            return std::unexpected("Too many concurrent HTTP requests");
        }
        running_request_count_++;
        contexts_[context->request_id] = context;
    }

    auto task = [this, context]() {
        execute_request(context);
    };
#if defined(__EMSCRIPTEN__)
    // Wasm `post()` is intentionally inline for sync service helpers; defer HTTP so UI render callbacks unwind first.
    const bool post_result = scheduler_->post_delayed(std::move(task), 16, &context->task_id);
#else
    const bool post_result = scheduler_->post(std::move(task), &context->task_id);
#endif
    if (!post_result) {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        running_request_count_--;
        contexts_.erase(context->request_id);
        return std::unexpected("Failed to post HTTP request task");
    }

    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        statistics_.submitted_count++;
    }
    BROOKESIA_LOGI("Submitted HTTP request: id(%1%), url(%2%)", context->request_id, context->request.url);

    return context;
}

void Http::execute_request(std::shared_ptr<RequestContext> context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    set_context_state(context, RequestState::Running);
    publish_request_started(*context);

    auto response = perform_request(context);
    finish_context(context, std::move(response));
}

Http::HttpResponse Http::perform_request(std::shared_ptr<RequestContext> context)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    HttpResponse response;
    response.request_id = context->request_id;

    auto retry_count = context->request.retry_count;
    for (uint32_t attempt = 0; attempt <= retry_count; attempt++) {
        if (context->cancel_requested.load()) {
            response.error = ErrorCode::Canceled;
            response.error_message = "Request canceled";
            return response;
        }

        response = {};
        response.request_id = context->request_id;
        auto error = perform_once(context, response);
        response.error = error;
        if (error == ErrorCode::Canceled) {
            return response;
        }

        if (error == ErrorCode::Ok) {
            if (!should_retry_status(context->request, response.status_code) || attempt >= retry_count) {
                return response;
            }
            BROOKESIA_LOGW(
                "HTTP request %1% got retryable status %2% on attempt %3%, retrying",
                context->request_id, response.status_code, attempt + 1
            );
            continue;
        }

        if (attempt < retry_count) {
            BROOKESIA_LOGW(
                "HTTP request %1% failed on attempt %2%: error(%3%), message(%4%), retrying",
                context->request_id, attempt + 1, BROOKESIA_DESCRIBE_TO_STR(response.error),
                response.error_message
            );
        }
    }

    return response;
}

Http::ErrorCode Http::perform_once(std::shared_ptr<RequestContext> context, HttpResponse &response)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    const auto &request = context->request;

    if (!http_iface_) {
        response.error_message = "ProtocolHttp HAL interface is not available";
        return ErrorCode::ClientInitFailed;
    }

    auto transaction = http_iface_->create_transaction();
    if (transaction == nullptr) {
        response.error_message = "Failed to create ProtocolHttp HAL transaction";
        return ErrorCode::ClientInitFailed;
    }
    set_context_transaction(*context, transaction);
    lib_utils::FunctionGuard transaction_guard([&]() {
        clear_context_transaction(*context);
    });

    uint32_t content_length = 0;
    auto error = transaction->open(request, response, content_length);
    if (error != ErrorCode::Ok) {
        return context->cancel_requested.load() ? ErrorCode::Canceled : error;
    }

    context->total.store(content_length);

    // HEAD responses carry no body even when the server advertises a non-zero
    // Content-Length (it reports the GET-equivalent body size). Returning here
    // avoids a long backend read timeout waiting for data that will
    // never arrive.
    if (request.method == Helper::HttpMethod::Head) {
        return ErrorCode::Ok;
    }

    if (!request.download_path.empty()) {
        return perform_file_response_body_with_safe_stack(context, response, transaction);
    }

    std::vector<char> buffer(BROOKESIA_SERVICE_HTTP_BUFFER_SIZE);
    const auto max_memory_size = get_effective_limit(
                                     request.max_response_size, BROOKESIA_SERVICE_HTTP_MAX_MEMORY_BODY_SIZE
                                 );

    while (!context->cancel_requested.load()) {
        std::string error_message;
        int read_len = transaction->read(buffer.data(), buffer.size(), error_message);
        if (read_len < 0) {
            response.error_message = error_message.empty() ? "Failed to read HTTP response body" : error_message;
            return ErrorCode::RequestFailed;
        }
        if (read_len == 0) {
            break;
        }

        auto downloaded = context->downloaded.fetch_add(read_len) + read_len;
        if (downloaded > max_memory_size) {
            response.error_message = "HTTP response body size exceeds memory limit";
            return ErrorCode::BodyTooLarge;
        }
        response.body.append(buffer.data(), read_len);

        publish_request_progress(*context);
    }

    if (context->cancel_requested.load()) {
        response.error_message = "Request canceled";
        return ErrorCode::Canceled;
    }

    if (!transaction->is_complete()) {
        response.error_message = "HTTP response body is incomplete";
        return ErrorCode::RequestFailed;
    }

    return ErrorCode::Ok;
}

Http::ErrorCode Http::perform_file_response_body_with_safe_stack(
    std::shared_ptr<RequestContext> context, HttpResponse &response, std::shared_ptr<HttpTransaction> transaction
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (lib_utils::ThreadConfig::check_stack_cache_safe()) {
        BROOKESIA_LOGD("Downloading HTTP response to file in current thread");
        return perform_file_response_body(context, response, transaction);
    }

#if defined(__EMSCRIPTEN__)
    // The browser build is single-threaded, and MEMFS writes are safe on the
    // current stack. Avoid Boost.Thread, which has no library backend in emsdk.
    BROOKESIA_LOGD("Downloading HTTP response to wasm MEMFS in current thread");
    return perform_file_response_body(context, response, transaction);
#else
    ErrorCode result = ErrorCode::RequestFailed;
    auto file_io_func = [&]() {
        BROOKESIA_LOG_TRACE_GUARD();
        result = perform_file_response_body(context, response, transaction);
    };

    BROOKESIA_LOGD("Downloading HTTP response to file in internal-stack thread");
    BROOKESIA_THREAD_CONFIG_GUARD({
        .name = FILE_IO_THREAD_NAME,
        .stack_size = static_cast<size_t>(BROOKESIA_SERVICE_HTTP_FILE_IO_THREAD_STACK_SIZE),
        .stack_in_ext = false,
    });
    try {
        boost::thread(file_io_func).join();
    } catch (const std::exception &e) {
        response.error_message = std::string("Failed to download HTTP response to file in new thread: ") + e.what();
        BROOKESIA_LOGE("%1%", response.error_message);
        return ErrorCode::RequestFailed;
    } catch (...) {
        response.error_message = "Failed to download HTTP response to file in new thread";
        BROOKESIA_LOGE("%1%", response.error_message);
        return ErrorCode::RequestFailed;
    }

    return result;
#endif
}

Http::ErrorCode Http::perform_file_response_body(
    std::shared_ptr<RequestContext> context, HttpResponse &response, std::shared_ptr<HttpTransaction> transaction
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(transaction, ErrorCode::ClientInitFailed, "Invalid HTTP transaction handle");

    const auto &request = context->request;
    FILE *output_file = std::fopen(request.download_path.c_str(), "wb");
    if (output_file == nullptr) {
        response.error_message = "Failed to open download file";
        return ErrorCode::FileOpenFailed;
    }
    response.file_path = request.download_path;
    lib_utils::FunctionGuard file_guard([&]() {
        std::fclose(output_file);
    });

    std::vector<char> buffer(BROOKESIA_SERVICE_HTTP_BUFFER_SIZE);
    const auto max_file_size = get_effective_limit(
                                   request.max_file_size, BROOKESIA_SERVICE_HTTP_MAX_DOWNLOAD_FILE_SIZE
                               );

    while (!context->cancel_requested.load()) {
        std::string error_message;
        int read_len = transaction->read(buffer.data(), buffer.size(), error_message);
        if (read_len < 0) {
            response.error_message = error_message.empty() ? "Failed to read HTTP response body" : error_message;
            return context->cancel_requested.load() ? ErrorCode::Canceled : ErrorCode::RequestFailed;
        }
        if (read_len == 0) {
            break;
        }

        auto downloaded = context->downloaded.fetch_add(read_len) + read_len;
        if (downloaded > max_file_size) {
            response.error_message = "HTTP download file size exceeds limit";
            return ErrorCode::FileTooLarge;
        }
        if (std::fwrite(buffer.data(), 1, read_len, output_file) != static_cast<size_t>(read_len)) {
            response.error_message = "Failed to write HTTP response body to file";
            return ErrorCode::FileOpenFailed;
        }

        publish_request_progress(*context);
    }

    if (context->cancel_requested.load()) {
        response.error_message = "Request canceled";
        return ErrorCode::Canceled;
    }

    if (!transaction->is_complete()) {
        response.error_message = "HTTP response body is incomplete";
        return ErrorCode::RequestFailed;
    }

    return ErrorCode::Ok;
}

bool Http::validate_request(HttpRequest &request, std::string &error_message) const
{
    if (request.url.empty()) {
        error_message = "URL is empty";
        return false;
    }
    if (is_http_url(request.url)) {
        if (!BROOKESIA_SERVICE_HTTP_ENABLE_HTTP) {
            error_message = "HTTP transport is disabled";
            return false;
        }
    } else if (is_https_url(request.url)) {
        if (!BROOKESIA_SERVICE_HTTP_ENABLE_HTTPS) {
            error_message = "HTTPS transport is disabled";
            return false;
        }
    } else {
        error_message = "Unsupported URL scheme";
        return false;
    }

    if (request.timeout_ms == 0) {
        request.timeout_ms = BROOKESIA_SERVICE_HTTP_DEFAULT_TIMEOUT_MS;
    }
    if (request.tls_verify == Helper::TlsVerifyMode::Default) {
        request.tls_verify = BROOKESIA_SERVICE_HTTP_REQUIRE_TLS_VERIFY ?
                             Helper::TlsVerifyMode::Verify : Helper::TlsVerifyMode::Skip;
    }

    return true;
}

Http::RequestStateInfo Http::get_request_state_info(uint64_t request_id)
{
    std::lock_guard<std::mutex> lock(contexts_mutex_);
    auto it = contexts_.find(request_id);
    if (it == contexts_.end()) {
        return {};
    }

    return {
        .request_id = request_id,
        .state = it->second->state,
        .error = it->second->response.error,
        .status_code = it->second->response.status_code,
        .downloaded = it->second->downloaded.load(),
        .total = it->second->total.load(),
    };
}

void Http::set_general_state(GeneralState state)
{
    general_state_ = state;
    if (state_machine_ && state_machine_->is_running()) {
        if (!state_machine_->trigger_state(state)) {
            BROOKESIA_LOGW(
                "Failed to trigger state machine transition to %1%", BROOKESIA_DESCRIBE_TO_STR(state)
            );
        }
    }

    publish_general_state(state);
}

void Http::set_general_state_from_state_machine(GeneralState state)
{
    general_state_ = state;
    publish_general_state(state);
    if (state == GeneralState::Started) {
        BROOKESIA_LOGI("HTTP service started");
    }
}

void Http::publish_general_state(GeneralState state)
{
    // publish_event already logs at warning level when its preconditions fail
    // (e.g. service not running during shutdown). Drop our own duplicate error
    // so noisy but expected lifecycle phases don't surface as ERROR-level logs.
    BROOKESIA_CHECK_FALSE_EXECUTE(
    publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::GeneralStateChanged), EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(Helper::EventGeneralStateChangedParam::State), BROOKESIA_DESCRIBE_TO_STR(state)}
    }), {}, {
        BROOKESIA_LOGD("Skipped general state change publication (state=%1%)", BROOKESIA_DESCRIBE_TO_STR(state));
    }
    );
}

bool Http::start_time_sync_if_available(bool &started)
{
    started = false;
#if BROOKESIA_SERVICE_HTTP_REQUIRE_TIME_SYNC
    if (!SNTPHelper::is_available()) {
        BROOKESIA_LOGW("SNTP service is not available, skip HTTP startup time sync");
        return true;
    }

    auto binding = ServiceManager::get_instance().bind(SNTPHelper::get_name().data());
    BROOKESIA_CHECK_FALSE_RETURN(binding.is_valid(), false, "Failed to bind SNTP service");
    sntp_binding_ = std::move(binding);

    auto start_result = SNTPHelper::call_function_sync<void>(SNTPHelper::FunctionId::Start);
    BROOKESIA_CHECK_FALSE_RETURN(start_result, false, "Failed to start SNTP: %1%", start_result.error());
    started = true;
#endif
    return true;
}

bool Http::is_time_synced()
{
#if BROOKESIA_SERVICE_HTTP_REQUIRE_TIME_SYNC
    auto result = SNTPHelper::call_function_sync<bool>(SNTPHelper::FunctionId::IsTimeSynced);
    if (!result) {
        BROOKESIA_LOGW("Failed to check SNTP sync state: %1%", result.error());
        return false;
    }
    return result.value();
#else
    return true;
#endif
}

void Http::set_context_state(std::shared_ptr<RequestContext> context, RequestState state)
{
    std::lock_guard<std::mutex> lock(contexts_mutex_);
    context->state = state;
}

void Http::publish_request_started(const RequestContext &context)
{
    const bool published = publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::RequestStarted), EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(Helper::EventRequestParam::RequestId), context.request_id},
        {BROOKESIA_DESCRIBE_TO_STR(Helper::EventRequestParam::State), BROOKESIA_DESCRIBE_TO_STR(context.state)}
    });
    BROOKESIA_CHECK_FALSE_EXECUTE(published, {}, {
        BROOKESIA_LOGE("Failed to publish request started event");
    }
                                 );
}

void Http::publish_request_progress(const RequestContext &context)
{
    // Single-slot mailbox: latest snapshot wins. The actual event is published
    // by emit_pending_progress() running on a periodic scheduler tick. This
    // decouples a tight read loop (which can call this thousands of times per
    // download) from the much heavier publish_event() path that allocates a
    // boost::promise<bool> + EventItemMap copy per post.
    std::lock_guard<std::mutex> lock(pending_progress_mutex_);
    pending_progress_ = RequestProgress{
        .request_id = context.request_id,
        .downloaded = context.downloaded.load(),
        .total = context.total.load(),
    };
}

void Http::emit_pending_progress()
{
    std::optional<RequestProgress> snapshot;
    {
        std::lock_guard<std::mutex> lock(pending_progress_mutex_);
        snapshot.swap(pending_progress_);
    }
    if (!snapshot) {
        return;
    }
    BROOKESIA_CHECK_FALSE_EXECUTE(
    publish_event(BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::RequestProgress), EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(Helper::EventRequestParam::RequestId), snapshot->request_id},
        {
            BROOKESIA_DESCRIBE_TO_STR(Helper::EventRequestParam::Progress),
            request_progress_to_json(*snapshot)
        }
    }), {}, {
        BROOKESIA_LOGE("Failed to publish request progress event");
    }
    );
}

void Http::publish_request_finished(const RequestContext &context)
{
    Helper::EventId event_id = Helper::EventId::RequestFailed;
    if (context.state == RequestState::Completed) {
        event_id = Helper::EventId::RequestCompleted;
    } else if (context.state == RequestState::Canceled) {
        event_id = Helper::EventId::RequestCanceled;
    }

    BROOKESIA_CHECK_FALSE_EXECUTE(
    publish_event(BROOKESIA_DESCRIBE_TO_STR(event_id), EventItemMap{
        {BROOKESIA_DESCRIBE_TO_STR(Helper::EventRequestParam::RequestId), context.request_id},
        {
            BROOKESIA_DESCRIBE_TO_STR(Helper::EventRequestParam::Response),
            http_response_to_json(context.response)
        }
    }), {}, {
        BROOKESIA_LOGE("Failed to publish request finished event");
    }
    );
}

void Http::finish_context(std::shared_ptr<RequestContext> context, HttpResponse response)
{
    RequestState state = RequestState::Failed;
    if (response.error == ErrorCode::Ok) {
        state = RequestState::Completed;
    } else if (response.error == ErrorCode::Canceled) {
        state = RequestState::Canceled;
    }

    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        context->response = response;
        context->state = state;
        if (state == RequestState::Completed) {
            statistics_.completed_count++;
        } else if (state == RequestState::Canceled) {
            statistics_.canceled_count++;
        } else {
            statistics_.failed_count++;
        }
    }

    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        running_request_count_--;
    }
    publish_request_finished(*context);
    if (context->response_promise) {
        context->response_promise->set_value(context->response);
        context->response_promise.reset();
    }
    cleanup_finished_requests();
}

void Http::cleanup_finished_requests()
{
    std::lock_guard<std::mutex> lock(contexts_mutex_);
    for (auto it = contexts_.begin(); it != contexts_.end();) {
        if (!it->second->retain_after_finish &&
                (it->second->state == RequestState::Completed || it->second->state == RequestState::Failed ||
                 it->second->state == RequestState::Canceled)) {
            it = contexts_.erase(it);
        } else {
            ++it;
        }
    }
}

void Http::set_context_transaction(RequestContext &context, std::shared_ptr<HttpTransaction> transaction)
{
    std::lock_guard<std::mutex> lock(context.transaction_mutex);
    context.transaction = std::move(transaction);
}

void Http::clear_context_transaction(RequestContext &context)
{
    std::lock_guard<std::mutex> lock(context.transaction_mutex);
    context.transaction.reset();
}

void Http::cancel_all_requests()
{
    std::vector<std::shared_ptr<RequestContext>> contexts;
    {
        std::lock_guard<std::mutex> lock(contexts_mutex_);
        for (auto &context_pair : contexts_) {
            contexts.push_back(context_pair.second);
        }
    }

    for (auto &context : contexts) {
        context->cancel_requested.store(true);
        close_context_transaction(*context);
    }
}

void Http::close_context_transaction(RequestContext &context)
{
    std::shared_ptr<HttpTransaction> transaction;
    {
        std::lock_guard<std::mutex> lock(context.transaction_mutex);
        transaction = context.transaction;
    }
    if (transaction != nullptr) {
        transaction->cancel();
    }
}

#if BROOKESIA_SERVICE_HTTP_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Http, Http::get_instance().get_attributes().name, Http::get_instance(),
    BROOKESIA_SERVICE_HTTP_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service::http
