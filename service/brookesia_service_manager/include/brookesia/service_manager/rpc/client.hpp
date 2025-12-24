/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <future>
#include <memory>
#include <map>
#include "boost/thread.hpp"
#include "boost/asio.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/event/dispatcher.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/rpc/data_link_client.hpp"
#include "brookesia/service_manager/rpc/protocol.hpp"

namespace esp_brookesia::service::rpc {

class Client {
public:
    using DeinitCallback = std::function<void()>;
    using DisconnectCallback = std::function<void()>;

    Client(DeinitCallback on_deinit_callback = nullptr)
        : on_deinit_callback_(on_deinit_callback)
    {}
    ~Client();

    // Delete copy constructor and assignment operators
    Client(const Client &) = delete;
    Client(Client &&) = delete;
    Client &operator=(const Client &) = delete;
    Client &operator=(Client &&) = delete;

    bool init(boost::asio::io_context::executor_type executor, DisconnectCallback on_disconnect_callback);
    void deinit();

    bool connect(const std::string &host, uint16_t port, uint32_t timeout_ms);
    void disconnect();

    bool is_initialized() const
    {
        return (data_link_ != nullptr);
    }
    bool is_connected() const
    {
        return (data_link_ != nullptr) && data_link_->is_connected();
    }

    // Function APIs
    std::future<FunctionResult> call_function_async(
        const std::string &target, const std::string &method, boost::json::object &&params
    );
    FunctionResult call_function_sync(
        const std::string &target, const std::string &method, boost::json::object &&params, size_t timeout_ms
    );

    // Event APIs
    std::string subscribe_event(
        const std::string &target, const std::string &event_name, EventDispatcher::NotifyCallback callback,
        size_t timeout_ms
    );
    bool unsubscribe_events(
        const std::string &target, const std::vector<std::string> &subscription_ids, size_t timeout_ms
    );

private:
    void on_data_received(const std::string &data);
    bool on_response(const Response &response);
    bool on_notify(const Notify &notify);

    std::string host_;
    uint16_t port_;
    DeinitCallback on_deinit_callback_;
    DisconnectCallback on_disconnect_callback_;

    std::recursive_mutex operations_mutex_;
    std::unique_ptr<DataLinkClient> data_link_;

    boost::mutex pending_requests_mutex_;
    std::map<std::string, std::shared_ptr<std::promise<FunctionResult>>> pending_requests_;

    std::unique_ptr<EventDispatcher> event_dispatcher_;
};

} // namespace esp_brookesia::service::rpc
