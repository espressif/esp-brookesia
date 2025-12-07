/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

class ServerConnection {
public:
    using Responder = std::function < bool(size_t /*connection_id*/, Response && /*response*/) >;
    using Notifier = std::function < bool(std::size_t /*connection_id*/, Notify && /*notify*/) >;
    using RequestHandler = std::function < bool(
                               size_t /*connection_id*/, std::string && /*request_id*/, std::string && /*method*/,
                               FunctionParameterMap && /*parameters*/
                           ) >;

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

    void set_responder(Responder responder)
    {
        responder_ = responder;
    }
    void set_notifier(Notifier notifier)
    {
        notifier_ = notifier;
    }
    void set_request_handler(RequestHandler request_handler)
    {
        request_handler_ = request_handler;
    }
    void activate(bool active)
    {
        is_active_.store(active);
    }

    std::expected<std::shared_ptr<FunctionResult>, std::string> on_request(
        std::string &&request_id, size_t connection_id, std::string &&method,
        FunctionParameterMap &&parameters
    );
    void on_connection_closed(size_t connection_id);

    bool publish_event(const std::string &event_name, const EventItemMap &event_items);
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
