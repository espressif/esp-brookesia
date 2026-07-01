/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <atomic>
#include <map>
#include <chrono>
#include "boost/asio/io_context.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/rpc/data_link_server.hpp"
#include "brookesia/service_manager/rpc/protocol.hpp"
#include "brookesia/service_manager/rpc/connection.hpp"

namespace esp_brookesia::service::rpc {

/**
 * @brief RPC server that exposes registered services over TCP.
 */
class Server {
public:
    /**
     * @brief Runtime configuration for the RPC server transport.
     */
    struct Config {
        uint16_t listen_port; ///< TCP port used for listening.
        size_t max_connections; ///< Maximum number of simultaneous client connections.

        Config()
            : listen_port(BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT)
            , max_connections(BROOKESIA_SERVICE_MANAGER_RPC_SERVER_MAX_CONNECTIONS)
        {}
    };

    /**
     * @brief Construct an RPC server on the given executor.
     *
     * @param[in] executor Executor used for socket operations.
     * @param[in] config Transport configuration.
     */
    Server(boost::asio::io_context::executor_type executor, const Config &config = Config())
        : executor_(executor)
        , config_(config)
    {
    }
    /**
     * @brief Destructor.
     */
    ~Server();

    // Delete copy constructor and assignment operators
    Server(const Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(const Server &) = delete;
    Server &operator=(Server &&) = delete;

    /**
     * @brief Initialize internal transport resources.
     *
     * @return true if initialization succeeds.
     */
    bool init();
    /**
     * @brief Deinitialize the server and release transport resources.
     */
    void deinit();
    /**
     * @brief Start listening for RPC clients.
     *
     * @param[in] timeout_ms Startup timeout in milliseconds.
     * @return true if the server starts successfully.
     */
    bool start(uint32_t timeout_ms);
    /**
     * @brief Stop the RPC server.
     */
    void stop();

    /**
     * @brief Attach a service connection to the server.
     *
     * @param[in] connection Service connection wrapper to expose.
     * @return true if the connection is added successfully.
     */
    bool add_connection(std::shared_ptr<ServerConnection> connection);
    /**
     * @brief Remove a service connection by service name.
     *
     * @param[in] name Service name to remove.
     * @return true if a matching connection was removed.
     */
    bool remove_connection(const std::string &name);
    /**
     * @brief Get a registered service connection.
     *
     * @param[in] name Service name to query.
     * @return std::shared_ptr<ServerConnection> Matching connection, or `nullptr`.
     */
    std::shared_ptr<ServerConnection> get_connection(const std::string &name);

    bool is_initialized() const
    {
        return (data_link_ != nullptr);
    }

    bool is_running() const
    {
        return is_running_;
    }

private:
    // Data reception processing
    void on_connection_established(size_t connection_id);
    void on_data_received(size_t connection_id, const std::string &data);
    void on_connection_closed(size_t connection_id);

    // Request routing and processing
    bool send_response(size_t connection_id, const Response &response);
    bool send_notify(size_t connection_id, const Notify &notify);

    boost::asio::io_context::executor_type executor_;
    Config config_;

    std::atomic<bool> is_running_ = false;
    std::unique_ptr<DataLinkServer> data_link_;

    boost::mutex connections_mutex_;
    std::unordered_set<std::shared_ptr<ServerConnection>> connections_;
    std::unordered_set<size_t> active_connection_ids_;
};

BROOKESIA_DESCRIBE_STRUCT(Server::Config, (), (listen_port, max_connections))

} // namespace esp_brookesia::service
