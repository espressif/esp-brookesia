/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boost/format.hpp"
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_RPC_SERVER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/rpc/connection.hpp"

namespace esp_brookesia::service::rpc {

bool ServerConnection::publish_event(const std::string &event_name, const EventItemMap &event_items)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: event_name(%1%), event_items(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(event_items)
    );

    BROOKESIA_CHECK_NULL_RETURN(notifier_, false, "Notifier not valid");

    if (!event_items.empty()) {
        BROOKESIA_CHECK_FALSE_RETURN(
            event_registry_.validate_items(event_name, event_items), false, "Failed to validate event data"
        );
    }

    auto subscriptions = event_registry_.get_subscriptions(event_name);
    if (subscriptions.empty()) {
        BROOKESIA_LOGD("No subscriptions found for event: %1%, skip notify", event_name);
        return true;
    }

    Notify notify{event_name, {}, boost::json::object()};
    notify.data = BROOKESIA_DESCRIBE_TO_JSON(event_items).as_object();

    for (const auto& [connection_id, connect_subscriptions] : connection_subscriptions_) {
        for (const auto &connect_subscription : connect_subscriptions) {
            if (subscriptions.find(connect_subscription) != subscriptions.end()) {
                notify.subscription_ids.push_back(connect_subscription);
            }
        }
        notifier_(connection_id, std::move(notify));
    }

    return true;
}

bool ServerConnection::respond_request(size_t connection_id, Response &&response)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_NULL_RETURN(responder_, false, "Responder not valid");
    BROOKESIA_CHECK_FALSE_RETURN(
        responder_(connection_id, std::move(response)), false,
        "Failed to respond to connection `%1%` with request `%2%`", connection_id, response.id
    );

    return true;
}

std::expected<std::shared_ptr<FunctionResult>, std::string> ServerConnection::on_request(
    std::string &&request_id, size_t connection_id, std::string &&method, FunctionParameterMap &&parameters
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    std::shared_ptr<FunctionResult> result;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        result = std::make_shared<FunctionResult>(), std::unexpected("Failed to create function result"),
        "Failed to create function result"
    );
    result->success = true;

    // Handle event subscription and unsubscription
    if ((method == SUBSCRIBE_EVENT_FUNC_NAME)) {
        BROOKESIA_LOGD("Received event subscription request");
        // Get the event name from the request
        auto &event_name = std::get<std::string>(parameters.at(SUBSCRIBE_EVENT_FUNC_PARAM_NAME));
        // Trigger handler of the event registry to subscribe to the event
        std::string subscription_id;
        if (!event_registry_.on_rpc_subscribe(event_name, subscription_id, result->error_message)) {
            return std::unexpected(result->error_message);
        }
        // Add the subscription id to the connection subscriptions
        connection_subscriptions_[connection_id].insert(subscription_id);
        // Return the subscription id to the client
        result->data = subscription_id;
        return result;
    } else if ((method == UNSUBSCRIBE_EVENT_FUNC_NAME)) {
        BROOKESIA_LOGD("Received event unsubscription request");
        // Get the subscription ids from the request
        auto &subscription_ids_json = std::get<boost::json::array>(parameters.at(UNSUBSCRIBE_EVENT_FUNC_PARAM_NAME));
        // Convert the subscription ids to a set of strings
        EventRegistry::Subscriptions subscription_ids;
        for (const auto &subscription_id : subscription_ids_json) {
            subscription_ids.insert(boost::json::value_to<std::string>(subscription_id));
        }
        // Trigger handler of the event registry to unsubscribe from the events
        event_registry_.on_rpc_unsubscribe_by_subscriptions(subscription_ids);
        // Remove the subscription ids from the connection subscriptions
        for (const auto &subscription_id : subscription_ids) {
            connection_subscriptions_[connection_id].erase(subscription_id);
        }
        // Return the subscription ids to the client
        result->data = subscription_ids_json;
        return result;
    }

    BROOKESIA_LOGD("Received function call request");

    if (!function_registry_.has(method)) {
        return std::unexpected("Function not found: " + method);
    }

    if (request_handler_) {
        BROOKESIA_LOGD("Using request handler to process request");
        // If request_handler_ exists, it is assumed that the request is processed by request_handler_
        if (!request_handler_(connection_id, std::move(request_id), std::move(method), std::move(parameters))) {
            return std::unexpected("Request handler failed");
        }
        return nullptr;
    }

    *result = function_registry_.call(method, std::move(parameters));

    return result;
}

void ServerConnection::on_connection_closed(size_t connection_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%)", connection_id);

    auto it = connection_subscriptions_.find(connection_id);
    if (it == connection_subscriptions_.end()) {
        return;
    }

    auto &subscriptions = it->second;
    if (!subscriptions.empty()) {
        event_registry_.on_rpc_unsubscribe_by_subscriptions(subscriptions);
    }

    connection_subscriptions_.erase(it);
}

} // namespace esp_brookesia::service::rpc
