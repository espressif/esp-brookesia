/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_RPC_DATA_LINK_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/rpc/data_link_server.hpp"

namespace esp_brookesia::service::rpc {

constexpr int ACCEPT_FAIL_RETRY_DELAY = 10;

DataLinkServer::~DataLinkServer()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_running()) {
        stop();
    }
}

bool DataLinkServer::start(uint16_t port, size_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: port(%1%), timeout_ms(%2%)", port, timeout_ms);

    if (is_running()) {
        return true;  // Already running
    }

    BROOKESIA_CHECK_FALSE_RETURN(
        DataLinkBase::wait_for_free_global_sockets(timeout_ms), false,
        "Wait for free global socket timeout after %1%ms", timeout_ms
    );

    is_running_.store(true);

    lib_utils::FunctionGuard stop_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        stop();
    });

    BROOKESIA_CHECK_EXCEPTION_RETURN(
        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(executor_), false, "Failed to allocate acceptor"
    );
    acceptor_->open(boost::asio::ip::tcp::v4());
    acceptor_->set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_->bind(boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
    acceptor_->listen(max_connections_);

    BROOKESIA_CHECK_FALSE_RETURN(handle_accept(), false, "Handle accept failed");

    stop_guard.release();

    BROOKESIA_LOGD("Server started on port %1%", port);

    return true;
}

void DataLinkServer::stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_running()) {
        return;
    }

    DataLinkBase::release_global_sockets();

    if (acceptor_) {
        boost::system::error_code ec;
        acceptor_->close(ec);
        if (ec) {
            BROOKESIA_LOGE("Acceptor close error: %1%", ec.message());
        }
        acceptor_.reset();
    }

    remove_all_connections();
    connections_.clear();

    is_running_.store(false);
}

void DataLinkServer::on_handle_receive_error(std::shared_ptr<ConnectionInfo> connection, const boost::system::error_code &error)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%), error(%2%)", BROOKESIA_DESCRIBE_TO_STR(connection), error.message());

    remove_connection(connection->id);
}

void DataLinkServer::on_handle_send_error(std::shared_ptr<ConnectionInfo> connection, const boost::system::error_code &error)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%), error(%2%)", BROOKESIA_DESCRIBE_TO_STR(connection), error.message());

    remove_connection(connection->id);
}

bool DataLinkServer::cleanup_connection(std::shared_ptr<ConnectionInfo> connection)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%)", BROOKESIA_DESCRIBE_TO_STR(connection));

    BROOKESIA_CHECK_NULL_RETURN(connection, false, "Invalid connection");
    {
        boost::lock_guard lock(connections_mutex_);
        auto it = connections_.find(connection->id);
        BROOKESIA_CHECK_FALSE_RETURN(
            it != connections_.end(), false, "Connection %1% not found", connection->id
        );
    }

    BROOKESIA_CHECK_FALSE_RETURN(DataLinkBase::cleanup_connection(connection), false, "Cleanup connection failed");

    {
        boost::lock_guard lock(connections_mutex_);
        connections_.erase(connection->id);
    }

    if (is_running() && get_active_connections_count() == (max_connections_ - 1)) {
        BROOKESIA_LOGD("Get free connection, trigger handle accept");
        BROOKESIA_CHECK_FALSE_RETURN(handle_accept(), false, "Handle accept failed");
    }

    return true;
}

bool DataLinkServer::handle_accept()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(is_running(), false, "Server is not running");
    BROOKESIA_CHECK_FALSE_RETURN(!is_connection_limit_reached(), false, "Connection limit reached");

    // Retry when an error occurs or the global connection count limit is reached, delay for a period of time and then accept the connection again
    lib_utils::FunctionGuard retry_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        std::shared_ptr<boost::asio::steady_timer> timer;
        BROOKESIA_CHECK_EXCEPTION_EXIT(
            timer = std::make_shared<boost::asio::steady_timer>(executor_), "Failed to allocate timer"
        );
        timer->expires_after(std::chrono::milliseconds(ACCEPT_FAIL_RETRY_DELAY));
        timer->async_wait([this, timer](const boost::system::error_code & ec) {
            BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

            BROOKESIA_LOGD("Params: ec(%1%)", ec.message());

            if (!ec) {
                BROOKESIA_CHECK_FALSE_EXIT(handle_accept(), "Handle accept failed");
            }
        });
    });

    if (!try_get_global_socket()) {
        BROOKESIA_LOGD("Global socket limit reached, reject connection. Try again later.");
        return true;
    }

    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        socket = std::make_shared<boost::asio::ip::tcp::socket>(executor_), false, "Failed to allocate socket"
    );

    acceptor_->async_accept(*socket,
    [this, socket, retry_guard = std::move(retry_guard)](boost::system::error_code ec) mutable {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: ec(%1%)", ec.message());

        if (!ec)
        {
            // 建立连接
            std::shared_ptr<ConnectionInfo> connection;
            BROOKESIA_CHECK_EXCEPTION_EXIT(
                connection = std::make_shared<ConnectionInfo>(), "Failed to allocate connection"
            );
            connection->socket = socket;
            connection->id = allocate_connection_id();
            connection->is_active.store(true);

            BROOKESIA_CHECK_FALSE_EXIT(handle_receive(connection), "Handle receive failed");
            add_connection(connection);

            if (on_connection_established_) {
                on_connection_established_(connection->id);
            }

            BROOKESIA_LOGD(
                "New connection accepted (id: %1%) [Local: %2%/%3%, Global: %4%/%5%]",
                connection->id, get_active_connections_count(), max_connections_, get_active_global_sockets_count(),
                get_max_global_sockets_count()
            );
        } else
        {
            if (ec == boost::asio::error::operation_aborted) {
                BROOKESIA_LOGD("Acceptor closed, stop accepting.");
                retry_guard.release();
                return;
            }

            BROOKESIA_LOGE("Accept error: %1%", ec.message());

            // Exit and delay for a period of time and then accept the connection again
            return;
        }

        retry_guard.release();
        // If the server is still running and the local connection count is not reached the limit, continue the next round of accept
        // Otherwise, the handle_accept will be triggered in cleanup_connection
        if (is_running() && !is_connection_limit_reached())
        {
            BROOKESIA_CHECK_FALSE_EXIT(handle_accept(), "Handle accept failed");
        }
    });

    return true;
}

std::shared_ptr<DataLinkBase::ConnectionInfo> DataLinkServer::find_connection(size_t connection_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%)", connection_id);

    auto it = connections_.find(connection_id);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

bool DataLinkServer::send_data(size_t connection_id, std::string &&data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%), data(%2%)", connection_id, data);

    boost::lock_guard lock(connections_mutex_);
    auto connection = find_connection(connection_id);

    if (!connection || !connection->is_active.load()) {
        BROOKESIA_LOGD("Connection %1% not found or not active", connection_id);
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(handle_send(connection, std::move(data)), false, "Send data failed");

    return true;
}

size_t DataLinkServer::get_active_connections_count()
{
    boost::lock_guard lock(connections_mutex_);
    size_t active_count = 0;
    for (const auto& [id, connection] : connections_) {
        if (connection && connection->is_active.load()) {
            active_count++;
        }
    }

    return active_count;
}

std::vector<size_t> DataLinkServer::get_active_connection_ids()
{
    std::vector<size_t> active_ids;
    boost::lock_guard lock(connections_mutex_);

    for (const auto& [id, connection] : connections_) {
        if (connection && connection->is_active.load()) {
            active_ids.push_back(id);
        }
    }
    return active_ids;
}

void DataLinkServer::add_connection(std::shared_ptr<ConnectionInfo> connection)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection(%1%)", BROOKESIA_DESCRIBE_TO_STR(connection));

    {
        boost::lock_guard lock(connections_mutex_);
        connections_[connection->id] = connection;
    }

    // Update the local history maximum connection count
    if (get_active_connections_count() > max_active_ever_.load()) {
        max_active_ever_.store(get_active_connections_count());
    }
}

void DataLinkServer::remove_connection(size_t connection_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: connection_id(%1%)", connection_id);

    boost::lock_guard lock(connections_mutex_);
    auto connection = find_connection(connection_id);
    if (!connection) {
        return;
    }

    BROOKESIA_CHECK_FALSE_EXIT(cleanup_connection(connection), "Cleanup connection failed");
}

void DataLinkServer::remove_all_connections()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(connections_mutex_);
    while (!connections_.empty()) {
        auto connection = connections_.begin()->second;
        if (!cleanup_connection(connection)) {
            BROOKESIA_LOGE("Cleanup connection %1% failed", connection->id);
        }
    }
}

} // namespace esp_brookesia::service::rpc
