/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <optional>
#include <vector>
#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"

namespace esp_brookesia::service::rpc {

/**
 * @brief RPC request packet sent from a client to a service endpoint.
 */
struct Request {
    std::string id;
    std::string service;
    std::string method;
    boost::json::object params = boost::json::object();
};
BROOKESIA_DESCRIBE_STRUCT(Request, (), (id, service, method, params))

/**
 * @brief Structured error payload returned in an RPC response.
 */
struct ResponseError {
    int code = -1;
    std::string message = "";
};
BROOKESIA_DESCRIBE_STRUCT(ResponseError, (), (code, message))

/**
 * @brief RPC response packet returned for a request.
 */
struct Response {
    std::string id;
    std::optional<boost::json::value> result = std::nullopt;
    std::optional<ResponseError> error = std::nullopt;

    /**
     * @brief Check whether the response has a non-empty request ID.
     *
     * @return `true` when the response can be matched to a request.
     */
    bool is_valid() const
    {
        return !id.empty();
    }
    /**
     * @brief Check whether the response reports success.
     *
     * @return `true` when no error payload is present.
     */
    bool is_success() const
    {
        return !error.has_value();
    }
    /**
     * @brief Check whether the response carries a result payload.
     *
     * @return `true` when `result` has a value.
     */
    bool has_result() const
    {
        return result.has_value();
    }
};
BROOKESIA_DESCRIBE_STRUCT(Response, (), (id, result, error))

/**
 * @brief RPC notification packet used for event delivery.
 */
struct Notify {
    std::string event;
    std::vector<std::string> subscription_ids;
    boost::json::object data = boost::json::object();

    /**
     * @brief Check whether the notification contains the required routing fields.
     *
     * @return `true` when the event name and subscription list are both non-empty.
     */
    bool is_valid() const
    {
        return !event.empty() && !subscription_ids.empty();
    }
};
BROOKESIA_DESCRIBE_STRUCT(Notify, (), (event, subscription_ids, data))

/**
 * @brief RPC function name used to subscribe to service events.
 */
constexpr const char *SUBSCRIBE_EVENT_FUNC_NAME = "subscribe_event";
/**
 * @brief Parameter name used by the event-subscription RPC function.
 */
constexpr const char *SUBSCRIBE_EVENT_FUNC_PARAM_NAME = "event_name";
/**
 * @brief RPC function name used to unsubscribe from service events.
 */
constexpr const char *UNSUBSCRIBE_EVENT_FUNC_NAME = "unsubscribe_event";
/**
 * @brief Parameter name used by the event-unsubscription RPC function.
 */
constexpr const char *UNSUBSCRIBE_EVENT_FUNC_PARAM_NAME = "subscription_ids";

} // namespace esp_brookesia::service::rpc
