/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <expected>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/hal_interface.hpp"
#include "brookesia/service_helper/network/http.hpp"
#include "brookesia/service_http/macro_configs.h"
#include "brookesia/service_http/state_machine.hpp"
#include "brookesia/service_manager/service/base.hpp"
#include "brookesia/service_manager/service/manager.hpp"

namespace esp_brookesia::service::http {

class Http : public ServiceBase {
public:
    static Http &get_instance()
    {
        static Http instance;
        return instance;
    }

private:
    static std::string get_component_version();

    using Helper = helper::Http;
    using HttpRequest = Helper::HttpRequest;
    using HttpResponse = Helper::HttpResponse;
    using RequestState = Helper::RequestState;
    using RequestStateInfo = Helper::RequestStateInfo;
    using RequestProgress = Helper::RequestProgress;
    using ErrorCode = Helper::ErrorCode;
    using Statistics = Helper::Statistics;
    using HttpTransaction = hal::network::HttpClientIface::Transaction;

    struct RequestContext {
        uint64_t request_id = 0;
        HttpRequest request;
        HttpResponse response;
        RequestState state = RequestState::Queued;
        std::atomic<uint32_t> downloaded{0};
        std::atomic<uint32_t> total{0};
        lib_utils::TaskScheduler::TaskId task_id = 0;
        std::mutex transaction_mutex;
        std::shared_ptr<HttpTransaction> transaction;
        std::atomic<bool> cancel_requested{false};
        std::shared_ptr<std::promise<HttpResponse>> response_promise;
        bool retain_after_finish = true;
    };

#if BROOKESIA_SERVICE_HTTP_ENABLE_WORKER
    /**
     * @brief Default worker configuration used by the service.
     *
     * Mirrors the multi-worker pattern in `service_manager` so that long
     * blocking operations (e.g. file downloads that join the I/O thread on
     * a worker) cannot starve cooperative tasks such as the periodic
     * progress emitter.
     */
    static lib_utils::TaskScheduler::StartConfig make_default_task_scheduler_start_config()
    {
        lib_utils::TaskScheduler::StartConfig config{
            .worker_configs = {},
        };
        lib_utils::ThreadConfig worker0;
        worker0.name = std::string(BROOKESIA_SERVICE_HTTP_WORKER_NAME) + "0";
        worker0.core_id = BROOKESIA_SERVICE_HTTP_WORKER_0_CORE_ID;
        worker0.priority = BROOKESIA_SERVICE_HTTP_WORKER_PRIORITY;
        worker0.stack_size = BROOKESIA_SERVICE_HTTP_WORKER_STACK_SIZE;
        worker0.stack_in_ext = BROOKESIA_SERVICE_HTTP_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker0);
#if BROOKESIA_SERVICE_HTTP_WORKER_NUM >= 2
        lib_utils::ThreadConfig worker1;
        worker1.name = std::string(BROOKESIA_SERVICE_HTTP_WORKER_NAME) + "1";
        worker1.core_id = BROOKESIA_SERVICE_HTTP_WORKER_1_CORE_ID;
        worker1.priority = BROOKESIA_SERVICE_HTTP_WORKER_PRIORITY;
        worker1.stack_size = BROOKESIA_SERVICE_HTTP_WORKER_STACK_SIZE;
        worker1.stack_in_ext = BROOKESIA_SERVICE_HTTP_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker1);
#endif
#if BROOKESIA_SERVICE_HTTP_WORKER_NUM >= 3
        lib_utils::ThreadConfig worker2;
        worker2.name = std::string(BROOKESIA_SERVICE_HTTP_WORKER_NAME) + "2";
        worker2.core_id = BROOKESIA_SERVICE_HTTP_WORKER_2_CORE_ID;
        worker2.priority = BROOKESIA_SERVICE_HTTP_WORKER_PRIORITY;
        worker2.stack_size = BROOKESIA_SERVICE_HTTP_WORKER_STACK_SIZE;
        worker2.stack_in_ext = BROOKESIA_SERVICE_HTTP_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker2);
#endif
#if BROOKESIA_SERVICE_HTTP_WORKER_NUM >= 4
        lib_utils::ThreadConfig worker3;
        worker3.name = std::string(BROOKESIA_SERVICE_HTTP_WORKER_NAME) + "3";
        worker3.core_id = BROOKESIA_SERVICE_HTTP_WORKER_3_CORE_ID;
        worker3.priority = BROOKESIA_SERVICE_HTTP_WORKER_PRIORITY;
        worker3.stack_size = BROOKESIA_SERVICE_HTTP_WORKER_STACK_SIZE;
        worker3.stack_in_ext = BROOKESIA_SERVICE_HTTP_WORKER_STACK_IN_EXT;
        config.worker_configs.push_back(worker3);
#endif
        config.worker_poll_interval_ms = BROOKESIA_SERVICE_HTTP_WORKER_POLL_INTERVAL_MS;
        return config;
    }
#endif

    Http()
        : ServiceBase({
        .name = Helper::get_name().data(),
        .description = "Manage HTTP requests and transfer progress.",
        .version = get_component_version(),
#if BROOKESIA_SERVICE_HTTP_ENABLE_WORKER
        .task_scheduler_config = make_default_task_scheduler_start_config(),
#endif
    })
    {}
    ~Http() = default;

    bool on_init() override;
    void on_deinit() override;
    bool on_start() override;
    void on_stop() override;

    std::expected<boost::json::object, std::string> function_request(const boost::json::object &request);
    std::expected<double, std::string> function_request_async(const boost::json::object &request);
    std::expected<void, std::string> function_cancel_request(double request_id);
    std::expected<boost::json::object, std::string> function_get_request_state(double request_id);
    std::expected<void, std::string> function_reset_statistics();

    std::vector<FunctionSchema> get_function_schemas() override
    {
        auto function_schemas = Helper::get_function_schemas();
        return std::vector<FunctionSchema>(function_schemas.begin(), function_schemas.end());
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        auto event_schemas = Helper::get_event_schemas();
        return std::vector<EventSchema>(event_schemas.begin(), event_schemas.end());
    }

    ServiceBase::FunctionHandlerMap get_function_handlers() override
    {
        return {
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::Request, boost::json::object,
                function_request(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::RequestAsync, boost::json::object,
                function_request_async(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::CancelRequest, double,
                function_cancel_request(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
                Helper, Helper::FunctionId::GetRequestState, double,
                function_get_request_state(PARAM)
            ),
            BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
                Helper, Helper::FunctionId::ResetStatistics,
                function_reset_statistics()
            ),
        };
    }

    std::expected<std::shared_ptr<RequestContext>, std::string> submit_request(
        HttpRequest request, std::shared_ptr<std::promise<HttpResponse>> response_promise = nullptr
    );
    lib_utils::TaskScheduler::Group get_state_task_group() const
    {
        return get_call_task_group();
    }
    void execute_request(std::shared_ptr<RequestContext> context);
    HttpResponse perform_request(std::shared_ptr<RequestContext> context);
    ErrorCode perform_once(std::shared_ptr<RequestContext> context, HttpResponse &response);
    ErrorCode perform_file_response_body_with_safe_stack(
        std::shared_ptr<RequestContext> context, HttpResponse &response, std::shared_ptr<HttpTransaction> transaction
    );
    ErrorCode perform_file_response_body(
        std::shared_ptr<RequestContext> context, HttpResponse &response, std::shared_ptr<HttpTransaction> transaction
    );

    bool validate_request(HttpRequest &request, std::string &error_message) const;
    RequestStateInfo get_request_state_info(uint64_t request_id);

    void set_general_state(GeneralState state);
    void set_general_state_from_state_machine(GeneralState state);
    void publish_general_state(GeneralState state);
    bool start_time_sync_if_available(bool &started);
    bool is_time_synced();
    void set_context_state(std::shared_ptr<RequestContext> context, RequestState state);
    void publish_request_started(const RequestContext &context);
    // Coalesces in-flight download progress: producers (read loops) only stamp
    // the latest snapshot into a single-slot mailbox, and a periodic emitter
    // task (started in on_start) publishes at most one RequestProgress event
    // per BROOKESIA_SERVICE_HTTP_PROGRESS_PUBLISH_INTERVAL_MS, regardless of
    // how fast the loop reads.
    void publish_request_progress(const RequestContext &context);
    void emit_pending_progress();
    void publish_request_finished(const RequestContext &context);
    void finish_context(std::shared_ptr<RequestContext> context, HttpResponse response);
    void cleanup_finished_requests();
    void set_context_transaction(RequestContext &context, std::shared_ptr<HttpTransaction> transaction);
    void clear_context_transaction(RequestContext &context);
    void cancel_all_requests();
    void close_context_transaction(RequestContext &context);

    std::shared_ptr<lib_utils::TaskScheduler> scheduler_;
    std::unique_ptr<StateMachine> state_machine_;
    hal::InterfaceHandle<hal::network::HttpClientIface> http_iface_;
    ServiceBinding sntp_binding_;

    mutable std::mutex contexts_mutex_;
    std::map<uint64_t, std::shared_ptr<RequestContext>> contexts_;
    std::atomic<uint64_t> next_request_id_{1};
    size_t running_request_count_ = 0;
    Statistics statistics_;
    GeneralState general_state_ = GeneralState::Idle;

    mutable std::mutex pending_progress_mutex_;
    std::optional<RequestProgress> pending_progress_;
    lib_utils::TaskScheduler::TaskId progress_emitter_task_id_ = 0;
};

} // namespace esp_brookesia::service::http
