/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <vector>
#include <queue>
#include "boost/thread/recursive_mutex.hpp"
#include "brookesia/service_manager/rpc/data_link_base.hpp"

namespace esp_brookesia::service::rpc {

/**
 * @brief TCP transport used by the RPC server to accept multiple clients.
 */
class DataLinkServer : public DataLinkBase {
public:
    /**
     * @brief Construct a server transport.
     *
     * @param[in] executor Executor used for socket operations.
     * @param[in] max_connections Maximum number of concurrent client connections.
     */
    explicit DataLinkServer(boost::asio::io_context::executor_type executor, size_t max_connections)
        : DataLinkBase(executor)
        , max_connections_(max_connections)
    {
    }
    /**
     * @brief Destructor.
     */
    ~DataLinkServer() override;

    /**
     * @brief Start listening for client connections.
     *
     * @param[in] port Local TCP port.
     * @param[in] timeout_ms Startup timeout in milliseconds.
     * @return true if the server starts successfully.
     */
    bool start(uint16_t port, size_t timeout_ms);
    /**
     * @brief Stop accepting clients and close all active connections.
     */
    void stop();

    /**
     * @brief Send one payload to a connected client.
     *
     * @param[in] connection_id Client connection id.
     * @param[in] data Framed payload to send.
     * @return true if the payload was queued successfully.
     */
    bool send_data(size_t connection_id, std::string &&data);

    /**
     * @brief Get the number of currently active client connections.
     *
     * @return size_t Active connection count.
     */
    size_t get_active_connections_count();
    /**
     * @brief Get the ids of all active client connections.
     *
     * @return std::vector<size_t> Snapshot of active connection ids.
     */
    std::vector<size_t> get_active_connection_ids();

    /**
     * @brief Check whether the server transport is currently accepting traffic.
     *
     * @return true if running.
     */
    bool is_running()
    {
        return is_running_.load();
    }
    /**
     * @brief Get the configured maximum number of client connections.
     *
     * @return size_t Connection cap.
     */
    size_t get_max_connections_count()
    {
        return max_connections_;
    }
    /**
     * @brief Check whether the local connection cap has been reached.
     *
     * @return true if no more clients should be accepted.
     */
    bool is_connection_limit_reached()
    {
        return get_active_connections_count() >= max_connections_;
    }
    /**
     * @brief Get the peak number of simultaneously active client connections.
     *
     * @return size_t Historical maximum for this server instance.
     */
    size_t get_max_active_connections_count()
    {
        return max_active_ever_.load();
    }

private:
    void on_handle_receive_error(std::shared_ptr<ConnectionInfo> connection, const boost::system::error_code &error) override;
    void on_handle_send_error(std::shared_ptr<ConnectionInfo> connection, const boost::system::error_code &error) override;
    bool cleanup_connection(std::shared_ptr<ConnectionInfo> connection) override;

    size_t allocate_connection_id()
    {
        return next_connection_id_.fetch_add(1);
    }
    std::shared_ptr<ConnectionInfo> find_connection(size_t connection_id);

    // Add connection to management list
    void add_connection(std::shared_ptr<ConnectionInfo> connection);
    void remove_connection(size_t connection_id);
    void remove_all_connections();

    bool handle_accept();

    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::atomic<bool> is_running_{false};

    const size_t max_connections_;
    boost::recursive_mutex connections_mutex_;
    std::map<size_t, std::shared_ptr<ConnectionInfo>> connections_;
    std::atomic<size_t> max_active_ever_{0};  // Local history maximum connection count
    std::atomic<size_t> next_connection_id_{0};
};

} // namespace esp_brookesia::service::rpc
