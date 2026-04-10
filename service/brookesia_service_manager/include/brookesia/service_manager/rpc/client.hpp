/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <future>
#include <memory>
#include <map>
#include "boost/asio/io_context.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/event/dispatcher.hpp"
#include "brookesia/service_manager/function/definition.hpp"
#include "brookesia/service_manager/rpc/data_link_client.hpp"
#include "brookesia/service_manager/rpc/protocol.hpp"

namespace esp_brookesia::service::rpc {

/**
 * @brief RPC client used to call service functions and subscribe to service events.
 */
class Client {
public:
    /**
     * @brief Callback invoked when the client is deinitialized.
     */
    using DeinitCallback = std::function<void()>;
    /**
     * @brief Callback invoked after the transport disconnects.
     */
    using DisconnectCallback = std::function<void()>;

    /**
     * @brief Construct a client with an optional deinitialization callback.
     *
     * @param[in] on_deinit_callback Callback invoked by `deinit()`.
     */
    Client(DeinitCallback on_deinit_callback = nullptr)
        : on_deinit_callback_(on_deinit_callback)
    {}
    /**
     * @brief Destructor.
     */
    ~Client();

    // Delete copy constructor and assignment operators
    Client(const Client &) = delete;
    Client(Client &&) = delete;
    Client &operator=(const Client &) = delete;
    Client &operator=(Client &&) = delete;

    /**
     * @brief Initialize the client transport on an executor.
     *
     * @param[in] executor Executor used for async socket operations.
     * @param[in] on_disconnect_callback Callback invoked if the transport disconnects.
     * @return true if initialization succeeds.
     */
    bool init(boost::asio::io_context::executor_type executor, DisconnectCallback on_disconnect_callback);
    /**
     * @brief Deinitialize the client and release its transport resources.
     */
    void deinit();

    /**
     * @brief Connect to an RPC server.
     *
     * @param[in] host Remote host name or address.
     * @param[in] port Remote port.
     * @param[in] timeout_ms Connection timeout in milliseconds.
     * @return true if the connection succeeds.
     */
    bool connect(const std::string &host, uint16_t port, uint32_t timeout_ms);
    /**
     * @brief Disconnect from the current RPC server.
     */
    void disconnect();

    bool is_initialized() const
    {
        return (data_link_ != nullptr);
    }
    bool is_connected() const
    {
        return (data_link_ != nullptr) && data_link_->is_connected();
    }

    /**
     * @brief Call a remote service function asynchronously.
     *
     * @param[in] target Target service name.
     * @param[in] method Remote function name.
     * @param[in] params JSON object containing function parameters.
     * @return std::future<FunctionResult> Future resolved with the call result.
     */
    std::future<FunctionResult> call_function_async(
        const std::string &target, const std::string &method, boost::json::object &&params
    );
    /**
     * @brief Call a remote service function synchronously.
     *
     * @param[in] target Target service name.
     * @param[in] method Remote function name.
     * @param[in] params JSON object containing function parameters.
     * @param[in] timeout_ms Wait timeout in milliseconds.
     * @return FunctionResult Call result or timeout/error information.
     */
    FunctionResult call_function_sync(
        const std::string &target, const std::string &method, boost::json::object &&params, size_t timeout_ms
    );

    /**
     * @brief Subscribe to a remote service event.
     *
     * @param[in] target Target service name.
     * @param[in] event_name Event name to subscribe to.
     * @param[in] callback Callback invoked when notifications arrive.
     * @param[in] timeout_ms RPC timeout in milliseconds.
     * @return std::string Subscription id, or an empty string on failure.
     */
    std::string subscribe_event(
        const std::string &target, const std::string &event_name, EventDispatcher::NotifyCallback callback,
        size_t timeout_ms
    );
    /**
     * @brief Unsubscribe from multiple remote event subscriptions.
     *
     * @param[in] target Target service name.
     * @param[in] subscription_ids Subscription ids to cancel.
     * @param[in] timeout_ms RPC timeout in milliseconds.
     * @return true if the unsubscribe request succeeds.
     */
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
