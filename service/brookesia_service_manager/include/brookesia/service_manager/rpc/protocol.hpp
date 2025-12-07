/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
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

struct Request {
    std::string id;
    std::string service;
    std::string method;
    boost::json::object params = boost::json::object();
};
BROOKESIA_DESCRIBE_STRUCT(Request, (), (id, service, method, params))

struct ResponseError {
    int code = -1;
    std::string message = "";
};
BROOKESIA_DESCRIBE_STRUCT(ResponseError, (), (code, message))

struct Response {
    std::string id;
    std::optional<boost::json::value> result = std::nullopt;
    std::optional<ResponseError> error = std::nullopt;

    bool is_valid() const
    {
        return !id.empty();
    }
    bool is_success() const
    {
        return !error.has_value();
    }
    bool has_result() const
    {
        return result.has_value();
    }
};
BROOKESIA_DESCRIBE_STRUCT(Response, (), (id, result, error))

struct Notify {
    std::string event;
    std::vector<std::string> subscription_ids;
    boost::json::object data = boost::json::object();

    bool is_valid() const
    {
        return !event.empty() && !subscription_ids.empty();
    }
};
BROOKESIA_DESCRIBE_STRUCT(Notify, (), (event, subscription_ids, data))

constexpr const char *SUBSCRIBE_EVENT_FUNC_NAME = "subscribe_event";
constexpr const char *SUBSCRIBE_EVENT_FUNC_PARAM_NAME = "event_name";
constexpr const char *UNSUBSCRIBE_EVENT_FUNC_NAME = "unsubscribe_event";
constexpr const char *UNSUBSCRIBE_EVENT_FUNC_PARAM_NAME = "subscription_ids";

} // namespace esp_brookesia::service::rpc
