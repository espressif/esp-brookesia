/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "boost/format.hpp"
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_RPC_SERVER_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/rpc/server.hpp"

namespace esp_brookesia::service::rpc {

Server::~Server()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        deinit();
    }
}

bool Server::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    lib_utils::FunctionGuard deinit_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        deinit();
    });

    // Initialize data link
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        data_link_ = std::make_unique<DataLinkServer>(executor_, config_.max_connections), false,
        "Failed to create DataLinkServer"
    );
    // Set data received callback
    data_link_->set_on_connection_established([this](size_t connection_id) {
        on_connection_established(connection_id);
    });
    data_link_->set_on_data_received([this](const std::string & data, size_t connection_id) {
        on_data_received(connection_id, data);
    });
    data_link_->set_on_connection_closed([this](size_t connection_id) {
        on_connection_closed(connection_id);
    });

    deinit_guard.release();

    BROOKESIA_LOGI("Initialized with config: %1%", BROOKESIA_DESCRIBE_TO_STR(config_));

    return true;
}

void Server::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized");
        return;
    }

    if (is_running()) {
        stop();
    }

    data_link_.reset();

    BROOKESIA_LOGI("Deinitialized");
}

bool Server::start(uint32_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: timeout_ms(%1%)", timeout_ms);

    if (is_running()) {
        BROOKESIA_LOGD("Already running");
        return true;
    }
    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized, initializing...");
        BROOKESIA_CHECK_FALSE_RETURN(init(), false, "Failed to initialize");
    }

    is_running_ = true;

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    // Start data link
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        data_link_->start(config_.listen_port, timeout_ms), false, "Failed to start server on port %1%",
        config_.listen_port
    );

    stop_guard.release();

    BROOKESIA_LOGI("Started server on port %1%", config_.listen_port);

    return true;
}

void Server::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        BROOKESIA_LOGD("Not running");
        return;
    }

    is_running_ = false;

    // Stop data link
    if (data_link_) {
        data_link_->stop();
    }

    BROOKESIA_LOGI("Stopped server on port %1%", config_.listen_port);
}

bool Server::add_connection(std::shared_ptr<ServerConnection> connection)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%)", connection);

    BROOKESIA_CHECK_NULL_RETURN(connection, false, "Invalid connection");

    boost::lock_guard lock(connections_mutex_);

    if (connections_.find(connection) != connections_.end()) {
        BROOKESIA_LOGD("Connection(%1%) already added", connection->get_name());
        return true;
    }

    auto notifier = [this](size_t connection_id, Notify && notify) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_RETURN(send_notify(connection_id, notify), false, "Failed to send notify");
        return true;
    };
    connection->set_notifier(notifier);

    auto responder = [this](size_t connection_id, Response && response) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_RETURN(send_response(connection_id, response), false, "Failed to send response");
        return true;
    };
    connection->set_responder(responder);

    connections_.insert(connection);

    return true;
}

bool Server::remove_connection(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto connection = get_connection(name);
    if (!connection) {
        BROOKESIA_LOGD("Connection(%1%) not found", name);
        return true;
    }

    boost::lock_guard lock(connections_mutex_);
    connections_.erase(connection);

    return true;
}

std::shared_ptr<ServerConnection> Server::get_connection(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    boost::lock_guard lock(connections_mutex_);
    for (const auto &connection : connections_) {
        if (connection->get_name() == name) {
            return connection;
        }
    }

    return nullptr;
}

void Server::on_connection_established(size_t connection_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%)", connection_id);

    boost::lock_guard lock(connections_mutex_);
    active_connection_ids_.insert(connection_id);
}

void Server::on_data_received(size_t connection_id, const std::string &data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%), data(%2%)", connection_id, data);

    Response response;
    std::string error_message;

    lib_utils::FunctionGuard send_response_guard([this, &response, connection_id, &error_message]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (!error_message.empty()) {
            response.error = ResponseError{
                .message = error_message,
            };
        }
        BROOKESIA_CHECK_FALSE_EXIT(send_response(connection_id, response), "Failed to send response");
    });
#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
    lib_utils::FunctionGuard show_error_message_guard([this, &error_message]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (!error_message.empty()) {
            BROOKESIA_LOGE("%1%", error_message);
        }
    });
#endif

    {
        boost::lock_guard lock(connections_mutex_);
        if (active_connection_ids_.find(connection_id) == active_connection_ids_.end()) {
            error_message = (boost::format("Connection(%1%) not established") % connection_id).str();
            return;
        }
    }

    // Convert to request object
    Request request;
    if (!BROOKESIA_DESCRIBE_JSON_DESERIALIZE(data, request)) {
        error_message = (boost::format("Invalid data: %1%") % data).str();
        return;
    }

    // Set response ID
    response.id = request.id;

    // Get target service
    auto connection = get_connection(request.service);
    if (!connection) {
        error_message = (boost::format("Connection(`%1%`) not found") % request.service).str();
        return;
    }
    if (!connection->is_active()) {
        error_message = (boost::format("Connection(`%1%`) not active") % request.service).str();
        return;
    }

    // Convert parameters format
    FunctionParameterMap parameters;
    if (!BROOKESIA_DESCRIBE_FROM_JSON(request.params, parameters)) {
        error_message = (boost::format("Invalid parameters: %1%") % boost::json::serialize(request.params)).str();
        return;
    }

    // Route to target service
    auto result = connection->on_request(
                      std::move(request.id), connection_id, std::move(request.method), std::move(parameters)
                  );
    if (!result) {
        error_message = result.error();
        return;
    }

    auto function_result = result.value();
    if (!function_result) {
        // If there is no return result, it is assumed that the connection handles the request,
        // so no response is sent here
        send_response_guard.release();
        return;
    }

    if (!function_result->success) {
        error_message = (
                            boost::format("Connection(`%1%`) failed to process request (%2%)") % request.service %
                            function_result->error_message
                        ).str();
        return;
    }
    // No error occurred, set the correct response
    response.result = std::move(BROOKESIA_DESCRIBE_TO_JSON(*function_result));

#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
    // Release error message
    show_error_message_guard.release();
#endif

    // Trigger send_response_guard to release, send response
}

void Server::on_connection_closed(size_t connection_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    {
        boost::lock_guard lock(connections_mutex_);
        active_connection_ids_.erase(connection_id);
    }

    BROOKESIA_LOGD("Client disconnected (id: %1%)", connection_id);

    // Notify all service connections that they have been closed
    for (const auto &connection : connections_) {
        if (connection) {
            connection->on_connection_closed(connection_id);
        }
    }
}

bool Server::send_response(size_t connection_id, const Response &response)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: connection_id(%1%), response(%2%)", connection_id, BROOKESIA_DESCRIBE_TO_STR(response)
    );

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    BROOKESIA_CHECK_FALSE_RETURN(
        data_link_->send_data(connection_id, std::move(BROOKESIA_DESCRIBE_JSON_SERIALIZE(response))), false,
        "Failed to send response"
    );

    return true;
}

bool Server::send_notify(size_t connection_id, const Notify &notify)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%), notify(%2%)", connection_id, BROOKESIA_DESCRIBE_TO_STR(notify));

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Not running");

    BROOKESIA_CHECK_FALSE_RETURN(
        data_link_->send_data(connection_id, std::move(BROOKESIA_DESCRIBE_JSON_SERIALIZE(notify))), false,
        "Failed to send notify"
    );

    return true;
}

} // namespace esp_brookesia::service::rpc
