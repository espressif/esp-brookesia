/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <expected>
#include <memory>
#include <string>
#include "brookesia/service_manager/function/registry.hpp"
#include "brookesia/service_manager/event/registry.hpp"
#include "brookesia/service_manager/rpc/protocol.hpp"

namespace esp_brookesia::service::rpc {

/**
 * @brief Bridges one service's registries to the RPC server transport.
 */
class ServerConnection {
public:
    /**
     * @brief Callback used to send an RPC response back to a client connection.
     */
    using Responder = std::function < bool(size_t /*connection_id*/, Response && /*response*/) >;
    /**
     * @brief Callback used to push an RPC notification to a client connection.
     */
    using Notifier = std::function < bool(std::size_t /*connection_id*/, Notify && /*notify*/) >;
    /**
     * @brief Optional callback that overrides request handling for custom routing.
     */
    using RequestHandler = std::function < bool(
                               size_t /*connection_id*/, std::string && /*request_id*/, std::string && /*method*/,
                               FunctionParameterMap && /*parameters*/
                           ) >;

    /**
     * @brief Construct a server-side connection wrapper for one service.
     *
     * @param[in] name Service name exposed to RPC clients.
     * @param[in] function_registry Registry used for function calls.
     * @param[in] event_registry Registry used for event subscriptions and notifications.
     */
    ServerConnection(std::string name, FunctionRegistry &function_registry, EventRegistry &event_registry)
        : name_(name)
        , function_registry_(function_registry)
        , event_registry_(event_registry)
    {}
    ~ServerConnection() = default;

    ServerConnection(const ServerConnection &) = delete;
    ServerConnection(ServerConnection &&) = delete;
    ServerConnection &operator=(const ServerConnection &) = delete;
    ServerConnection &operator=(ServerConnection &&) = delete;

    /**
     * @brief Set the responder used for sending RPC responses.
     *
     * @param[in] responder Transport callback.
     */
    void set_responder(Responder responder)
    {
        responder_ = responder;
    }
    /**
     * @brief Set the notifier used for sending RPC notifications.
     *
     * @param[in] notifier Transport callback.
     */
    void set_notifier(Notifier notifier)
    {
        notifier_ = notifier;
    }
    /**
     * @brief Set a custom request handler.
     *
     * @param[in] request_handler Custom transport-aware request handler.
     */
    void set_request_handler(RequestHandler request_handler)
    {
        request_handler_ = request_handler;
    }
    /**
     * @brief Enable or disable request handling on this connection.
     *
     * @param[in] active `true` to accept RPC traffic, `false` to reject it.
     */
    void activate(bool active)
    {
        is_active_.store(active);
    }

    /**
     * @brief Process an incoming RPC request for this service.
     *
     * @param[in] request_id RPC request id.
     * @param[in] connection_id Transport connection id.
     * @param[in] method Requested method name.
     * @param[in] parameters Method parameters.
     * @return std::expected<std::shared_ptr<FunctionResult>, std::string> Function result on success, or an error.
     */
    std::expected<std::shared_ptr<FunctionResult>, std::string> on_request(
        std::string &&request_id, size_t connection_id, std::string &&method,
        FunctionParameterMap &&parameters
    );
    /**
     * @brief Clear per-connection subscription state when a client disconnects.
     *
     * @param[in] connection_id Closed transport connection id.
     */
    void on_connection_closed(size_t connection_id);

    /**
     * @brief Publish a service event to subscribed RPC clients.
     *
     * @param[in] event_name Event name to publish.
     * @param[in] event_items Event payload.
     * @return true if notifications were dispatched successfully.
     */
    bool publish_event(const std::string &event_name, const EventItemMap &event_items);
    /**
     * @brief Send an RPC response to one client connection.
     *
     * @param[in] connection_id Transport connection id.
     * @param[in] response Response to send.
     * @return true if the response was handed to the transport.
     */
    bool respond_request(size_t connection_id, Response &&response);

    bool is_active() const
    {
        return is_active_.load();
    }

    std::string get_name() const
    {
        return name_;
    }

private:
    std::string name_;
    FunctionRegistry &function_registry_;
    EventRegistry &event_registry_;

    std::atomic<bool> is_active_{false};

    Responder responder_;
    Notifier notifier_;
    RequestHandler request_handler_;
    std::map<size_t, EventRegistry::Subscriptions> connection_subscriptions_;
};

} // namespace esp_brookesia::service::rpc
