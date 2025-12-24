/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <future>
#include "boost/format.hpp"
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_RPC_CLIENT_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/rpc/client.hpp"

namespace esp_brookesia::service::rpc {

Client::~Client()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        deinit();
    }
}

bool Client::connect(const std::string &host, uint16_t port, uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: host(%1%), port(%2%), timeout_ms(%3%)", host, port, timeout_ms);

    BROOKESIA_CHECK_FALSE_RETURN(is_initialized(), false, "Not initialized");

    boost::lock_guard lock(operations_mutex_);

    if (is_connected()) {
        if ((host == host_) && (port == port_)) {
            BROOKESIA_LOGD("Already connected to the same server");
            return true;
        } else {
            BROOKESIA_LOGD("Already connected to other server(%1%:%2%), disconnect first", host_, port_);
            disconnect();
        }
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        data_link_->connect(host, port, timeout_ms), false, "Failed to connect to server(%1%:%2%)", host, port
    );

    host_ = host;
    port_ = port;

    return true;
}

void Client::disconnect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_EXIT(is_initialized(), "Not initialized");

    boost::lock_guard lock(operations_mutex_);

    if (!is_connected()) {
        BROOKESIA_LOGD("Not connected to server");
        return;
    }

    data_link_->disconnect();

    {
        FunctionResult result{
            .success = false,
            .error_message = "Connection closed"
        };
        boost::lock_guard req_lock(pending_requests_mutex_);
        for (auto& [request_id, promise] : pending_requests_) {
            promise->set_value(result);
        }
        pending_requests_.clear();
    }
}

std::future<FunctionResult> Client::call_function_async(
    const std::string &target, const std::string &method, boost::json::object &&params
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: target(%1%), method(%2%), params(%3%)", target, method, BROOKESIA_DESCRIBE_TO_STR(params));

    std::shared_ptr<std::promise<FunctionResult>> result_promise = std::make_shared<std::promise<FunctionResult>>();
    std::future<FunctionResult> result_future = result_promise->get_future();

    FunctionResult error_result{
        .success = false,
    };
    lib_utils::FunctionGuard set_error_result_guard(
    [this, result_promise, error_result = std::move(error_result)]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        result_promise->set_value(std::move(error_result));
    });

    if (!is_connected()) {
        error_result.error_message = "Client not connected to server";
        return result_future;
    }

    std::string request_id = utils_generate_uuid();

    {
        boost::lock_guard lock(pending_requests_mutex_);
        pending_requests_[request_id] = result_promise;
    }
    lib_utils::FunctionGuard clear_pending_requests_guard([this, request_id]() {
        boost::lock_guard lock(pending_requests_mutex_);
        pending_requests_.erase(request_id);
    });

    Request request;
    request.id = request_id;
    request.service = target;
    request.method = method;
    request.params = std::move(params);
    if (!data_link_->send_data(std::move(BROOKESIA_DESCRIBE_JSON_SERIALIZE(request)))) {
        error_result.error_message = "Failed to send request";
        return result_future;
    }

    set_error_result_guard.release();
    clear_pending_requests_guard.release();

    return result_future;
}

FunctionResult Client::call_function_sync(
    const std::string &target, const std::string &method, boost::json::object &&params, size_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: target(%1%), method(%2%), params(%3%), timeout_ms(%4%)", target, method,
        BROOKESIA_DESCRIBE_TO_STR(params), timeout_ms
    );

    FunctionResult result{
        .success = false,
    };
    auto &error_message = result.error_message;
    auto result_future = call_function_async(target, method, std::move(params));

    if (result_future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
        error_message = (boost::format("Timeout after %1%ms") % timeout_ms).str();
        BROOKESIA_CHECK_FALSE_RETURN(false, result, "%1%", error_message);
    }

    return result_future.get();
}

std::string Client::subscribe_event(
    const std::string &target, const std::string &event_name, EventDispatcher::NotifyCallback callback,
    size_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: target(%1%), event_name(%2%), callback(%3%), timeout_ms(%4%)", target, event_name,
        BROOKESIA_DESCRIBE_TO_STR(callback), timeout_ms
    );

    BROOKESIA_CHECK_FALSE_RETURN(is_connected(), "", "Client not connected to server");

    auto response = call_function_sync(target, SUBSCRIBE_EVENT_FUNC_NAME, boost::json::object{
        {SUBSCRIBE_EVENT_FUNC_PARAM_NAME, event_name}
    }, timeout_ms);
    BROOKESIA_CHECK_FALSE_RETURN(
        response.success, "", "Failed to call function, error: %1%", response.error_message
    );

    auto subscription_id = std::get_if<std::string>(&response.data.value());
    BROOKESIA_CHECK_NULL_RETURN(subscription_id, "", "Failed to parse subscription id");

    BROOKESIA_CHECK_FALSE_RETURN(
        event_dispatcher_->subscribe(*subscription_id, callback), "", "Failed to subscribe to event"
    );

    return *subscription_id;
}

bool Client::unsubscribe_events(
    const std::string &target, const std::vector<std::string> &subscription_ids, size_t timeout_ms
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: target(%1%), subscription_ids(%2%)", target, BROOKESIA_DESCRIBE_TO_STR(subscription_ids));

    BROOKESIA_CHECK_FALSE_RETURN(is_connected(), false, "Client not connected to server");

    auto subscription_ids_json = boost::json::array{};
    for (const auto &subscription_id : subscription_ids) {
        event_dispatcher_->unsubscribe(subscription_id);
        subscription_ids_json.emplace_back(subscription_id);
    }

    auto response = call_function_sync(target, UNSUBSCRIBE_EVENT_FUNC_NAME, boost::json::object{
        {UNSUBSCRIBE_EVENT_FUNC_PARAM_NAME, subscription_ids_json}
    }, timeout_ms);
    BROOKESIA_CHECK_FALSE_RETURN(
        response.success, false, "Failed to call function, error: %1%", response.error_message
    );

    return true;
}

bool Client::init(boost::asio::io_context::executor_type executor, DisconnectCallback on_disconnect_callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operations_mutex_);

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit();
    });

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        data_link_ = std::make_unique<DataLinkClient>(executor), false, "Failed to create DataLinkClient"
    );
    data_link_->set_on_data_received([this](const std::string & data, size_t /* connection_id */) {
        on_data_received(data);
    });
    data_link_->set_on_connection_closed([this](size_t connection_id) {
        BROOKESIA_LOGD("Connection(%1%) closed by server", connection_id);
        if (on_disconnect_callback_) {
            on_disconnect_callback_();
        }
    });

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        event_dispatcher_ = std::make_unique<EventDispatcher>(), false, "Failed to create EventDispatcher"
    );

    on_disconnect_callback_ = std::move(on_disconnect_callback);

    deinit_guard.release();

    return true;
}

void Client::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(operations_mutex_);

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized");
        return;
    }

    if (is_connected()) {
        disconnect();
    }

    data_link_.reset();
    on_disconnect_callback_ = nullptr;

    if (on_deinit_callback_) {
        on_deinit_callback_();
    }
}

void Client::on_data_received(const std::string &data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: data(%1%)", data);

    Response response;
    if (BROOKESIA_DESCRIBE_JSON_DESERIALIZE(data, response) && response.is_valid()) {
        BROOKESIA_LOGD("Got response");
        BROOKESIA_CHECK_FALSE_EXIT(on_response(std::move(response)), "Failed to handle response");
        return;
    }

    Notify notify;
    if (BROOKESIA_DESCRIBE_JSON_DESERIALIZE(data, notify) && notify.is_valid()) {
        BROOKESIA_LOGD("Got notify");
        BROOKESIA_CHECK_FALSE_EXIT(on_notify(std::move(notify)), "Failed to handle notify");
        return;
    }

    BROOKESIA_LOGW("Unknown data received: %1%", data);
}

bool Client::on_response(const Response &response)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: response(%1%)", BROOKESIA_DESCRIBE_TO_STR(response));

    FunctionResult result{
        .success = response.is_success(),
    };
    if (!response.is_success()) {
        result.error_message = response.error.value().message;
    } else if (response.has_result()) {
        auto &result_json = response.result.value();
        if (!BROOKESIA_DESCRIBE_FROM_JSON(result_json, result)) {
            result.success = false;
            result.error_message = "Failed to parse result";
        }
    }

    std::shared_ptr<std::promise<FunctionResult>> result_promise;
    {
        boost::lock_guard lock(pending_requests_mutex_);
        auto it = pending_requests_.find(response.id);
        BROOKESIA_CHECK_FALSE_RETURN(it != pending_requests_.end(), false, "Request not found");
        result_promise = it->second;
        pending_requests_.erase(it);
    }
    result_promise->set_value(std::move(result));

    return true;
}

bool Client::on_notify(const Notify &notify)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: notify(%1%)", BROOKESIA_DESCRIBE_TO_STR(notify));

    BROOKESIA_CHECK_NULL_RETURN(event_dispatcher_, false, "Event dispatcher is invalid");

    EventItemMap notify_event_items;
    BROOKESIA_CHECK_FALSE_RETURN(
        BROOKESIA_DESCRIBE_FROM_JSON(notify.data, notify_event_items), false, "Failed to parse data"
    );

    event_dispatcher_->on_notify(notify.subscription_ids, notify_event_items);

    return true;
}

} // namespace esp_brookesia::service::rpc
