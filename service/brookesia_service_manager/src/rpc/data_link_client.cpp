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
#include "brookesia/service_manager/rpc/data_link_client.hpp"

namespace esp_brookesia::service::rpc {

DataLinkClient::~DataLinkClient()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (is_connected()) {
        disconnect();
    }
}

bool DataLinkClient::connect(const std::string &host, uint16_t port, size_t timeout_ms)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: host(%1%), port(%2%), timeout_ms(%3%)", host, port, timeout_ms);

    if (is_connected()) {
        BROOKESIA_LOGW("Already connected to server, please disconnect first");
        return true;
    }

    auto start_time = std::chrono::steady_clock::now();

    // Wait for the global connection slot
    BROOKESIA_CHECK_FALSE_RETURN(
        wait_for_free_global_sockets(timeout_ms), false, "Connection rejected: global connection timeout after %zums",
        timeout_ms
    );
    lib_utils::FunctionGuard release_connection_guard([this]() {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        release_global_sockets();
    });

    // Allocate socket
    std::shared_ptr<boost::asio::ip::tcp::socket> socket;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        socket = std::make_shared<boost::asio::ip::tcp::socket>(executor_), false, "Failed to allocate socket"
    );

    // Resolve server address
    boost::asio::ip::tcp::resolver resolver(executor_);
    auto endpoints = resolver.resolve(host, std::to_string(port));
    BROOKESIA_CHECK_FALSE_RETURN(
        endpoints.size() > 0, false, "Failed to resolve server address"
    );

    // Asynchronously connect to the server
    using ConnectInfo = std::pair<bool, std::string>;
    std::shared_ptr<std::promise<ConnectInfo>> connected_promise;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        connected_promise = std::make_shared<std::promise<ConnectInfo>>(), false, "Failed to allocate connected promise"
    );
    boost::asio::async_connect(*socket, endpoints,
    [this, socket, connected_promise](const boost::system::error_code & ec, const boost::asio::ip::tcp::endpoint & /* endpoint */) {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

        BROOKESIA_LOGD("Params: ec(%1%)", ec.message());

        ConnectInfo info;
        if (!ec) {
            info = std::make_pair(true, "");
        } else {
            info = std::make_pair(false, ec.message());
        }
        connected_promise->set_value(info);
    });

    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time);
    auto remaining = (elapsed < std::chrono::milliseconds(timeout_ms)) ? (std::chrono::milliseconds(timeout_ms) - elapsed) : std::chrono::milliseconds(0);
    auto connected_future = connected_promise->get_future();
    BROOKESIA_CHECK_FALSE_RETURN(
        connected_future.wait_for(remaining) == std::future_status::ready, false, "Connect to server[%1%:%2%] timeout",
        host.c_str(), port
    );
    auto connect_info = connected_future.get();
    BROOKESIA_CHECK_FALSE_RETURN(
        connect_info.first, false, "Connect to server[%1%:%2%] failed: %3%", host, port, connect_info.second
    );

    {
        boost::lock_guard lock(connection_mutex_);
        BROOKESIA_CHECK_EXCEPTION_RETURN(
            connection_ = std::make_shared<ConnectionInfo>(), false, "Failed to allocate connection"
        );
        connection_->socket = socket;
        connection_->is_active.store(true);
    }

    // Start receiving data
    BROOKESIA_CHECK_FALSE_RETURN(handle_receive(connection_), false, "Handle receive failed");

    if (on_connection_established_) {
        on_connection_established_(connection_->id);
    }

    release_connection_guard.release();
    BROOKESIA_LOGD("Connected to server %1%:%2%", host, port);

    return true;
}

bool DataLinkClient::disconnect()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(cleanup_connection(connection_), false, "Cleanup connection failed");

    return true;
}

bool DataLinkClient::is_connected()
{
    boost::lock_guard lock(connection_mutex_);
    return connection_ && connection_->is_active.load();
}

bool DataLinkClient::cleanup_connection(std::shared_ptr<ConnectionInfo> connection)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(DataLinkBase::cleanup_connection(connection), false, "Cleanup connection failed");
    release_global_sockets();

    {
        boost::lock_guard lock(connection_mutex_);
        connection_.reset();
    }

    return true;
}

bool DataLinkClient::send_data(std::string &&data)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: data(%1%)", data);

    boost::lock_guard lock(connection_mutex_);
    BROOKESIA_CHECK_NULL_RETURN(connection_, false, "Connection not found");
    BROOKESIA_CHECK_FALSE_RETURN(
        connection_->is_active.load(), false, "Connection not active"
    );

    BROOKESIA_CHECK_FALSE_RETURN(handle_send(connection_, std::move(data)), false, "Send data failed");

    return true;
}

} // namespace esp_brookesia::service::rpc
