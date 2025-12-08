/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_RPC_DATA_LINK_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/rpc/data_link_base.hpp"

namespace esp_brookesia::service::rpc {

bool DataLinkBase::handle_receive(std::shared_ptr<ConnectionInfo> connection)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%)", BROOKESIA_DESCRIBE_TO_STR(connection));

    BROOKESIA_CHECK_NULL_RETURN(connection, false, "Invalid connection");

    if (!connection->is_active.load()) {
        BROOKESIA_LOGD("Connection not active");
        return true;
    }

    boost::asio::async_read_until(*connection->socket, connection->receive_buffer, '\n',
    [this, connection](const boost::system::error_code & ec, std::size_t bytes_transferred) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Param: ec(%1%), bytes_transferred(%2%)", ec.message(), bytes_transferred);

        if (!connection->is_active.load()) {
            return;
        }

        if (!ec) {
            // Extract the received data
            std::istream is(&connection->receive_buffer);
            std::string received_data;
            std::getline(is, received_data);

            BROOKESIA_LOGD("Received data: %1%", received_data);

            if (on_data_received_) {
                on_data_received_(received_data, connection->id);
            }

            // Continue receiving data
            BROOKESIA_CHECK_FALSE_EXIT(handle_receive(connection), "Receive data failed");
        } else {
            on_handle_receive_error(connection, ec);

            // For normal close operation, return directly, do not record error
            if ((ec == boost::asio::error::operation_aborted) || (ec == boost::asio::error::eof)) {
                BROOKESIA_LOGD("Connection closed");
                return;
            }

            BROOKESIA_LOGE("Read error on connection %1%: %2%", connection->id, ec.message());
        }
    });

    return true;
}

bool DataLinkBase::handle_send(std::shared_ptr<ConnectionInfo> connection, std::string &&data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%), data(%2%)", BROOKESIA_DESCRIBE_TO_STR(connection), data);

    BROOKESIA_CHECK_NULL_RETURN(connection, false, "Invalid connection");
    BROOKESIA_CHECK_FALSE_RETURN(
        connection->is_active.load(), false, "Connection %1% not active", connection->id
    );

    // Add newline to the data to avoid temporary object lifetime issues
    data += '\n';

    // Create a shared_ptr to manage the data lifetime, ensure the data is not destroyed before the asynchronous operation is completed
    auto data_ptr = std::make_shared<std::string>(std::move(data));

    boost::asio::async_write(*connection->socket, boost::asio::buffer(*data_ptr),
    [this, connection, data_ptr](const boost::system::error_code & ec, std::size_t bytes_transferred) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Param: ec(%1%), bytes_transferred(%2%)", ec.message(), bytes_transferred);

        if (ec) {
            on_handle_send_error(connection, ec);

            // For normal operation cancellation, return directly, do not record error
            if (ec == boost::asio::error::operation_aborted) {
                BROOKESIA_LOGD("Connection closed");
                return;
            }

            BROOKESIA_LOGE("Send error on connection %1%: %2%", connection->id, ec.message());
        }
        // data_ptr will be automatically released after the lambda completes
    });

    return true;
}

bool DataLinkBase::cleanup_connection(std::shared_ptr<ConnectionInfo> connection)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%)", BROOKESIA_DESCRIBE_TO_STR(connection));

    BROOKESIA_CHECK_NULL_RETURN(connection, false, "Invalid connection");

    if (!connection->is_active.load()) {
        BROOKESIA_LOGD("Connection %2% already cleaned up", connection->id);
        return true;
    }

    connection->is_active.store(false);

    if (connection->socket && connection->socket->is_open()) {
        boost::system::error_code ec;
        connection->socket->close(ec);
        release_global_sockets();
    }

    if (on_connection_closed_) {
        on_connection_closed_(connection->id);
    }

    return true;
}

void DataLinkBase::release_global_sockets()
{
    BROOKESIA_LOG_TRACE_GUARD();


    boost::lock_guard lock(global_sockets_mutex_);

    if (active_global_sockets_.load() > 0) {
        active_global_sockets_.fetch_sub(1);
    }

    BROOKESIA_LOGD(
        "Release global socket slot, used/total: %1%/%2%", active_global_sockets_.load(),
        get_max_global_sockets_count()
    );

    // Notify the threads waiting for the global connection
    global_sockets_cv_.notify_all();
}

// Wait for the global connection slot (static method)
bool DataLinkBase::wait_for_free_global_sockets(size_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: timeout_ms(%1%)", timeout_ms);

    boost::unique_lock<boost::mutex> lock(global_sockets_mutex_);

    // Wait for the global socket slot
    if (is_global_sockets_limit_reached()) {
        BROOKESIA_CHECK_FALSE_RETURN(
            global_sockets_cv_.wait_for(lock, boost::chrono::milliseconds(timeout_ms)) != boost::cv_status::timeout,
            false, "Wait for global connection slot timeout"
        );
    }

    // Get the global connection slot
    size_t new_count = active_global_sockets_.fetch_add(1);
    // Update the global history maximum connection count
    if (new_count > max_global_sockets_.load()) {
        max_global_sockets_.store(new_count);
    }

    BROOKESIA_LOGD(
        "Get global connection slot, used/total: %1%/%2%", active_global_sockets_.load(), get_max_global_sockets_count()
    );

    return true;
}

bool DataLinkBase::try_get_global_socket()
{
    BROOKESIA_LOG_TRACE_GUARD();

    boost::lock_guard lock(global_sockets_mutex_);

    if (is_global_sockets_limit_reached()) {
        return false;
    }

    size_t new_count = active_global_sockets_.fetch_add(1);
    // Update the global history maximum socket count
    if (new_count > max_global_sockets_.load()) {
        max_global_sockets_.store(new_count);
    }

    return true;
}

} // namespace esp_brookesia::service::rpc
