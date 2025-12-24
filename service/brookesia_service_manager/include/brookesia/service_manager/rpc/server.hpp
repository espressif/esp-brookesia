/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <atomic>
#include <map>
#include <chrono>
#include "boost/thread.hpp"
#include "boost/asio.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/rpc/data_link_server.hpp"
#include "brookesia/service_manager/rpc/protocol.hpp"
#include "brookesia/service_manager/rpc/connection.hpp"

namespace esp_brookesia::service::rpc {

class Server {
public:
    struct Config {
        uint16_t listen_port;
        size_t max_connections;

        Config()
            : listen_port(BROOKESIA_SERVICE_MANAGER_RPC_SERVER_LISTEN_PORT)
            , max_connections(BROOKESIA_SERVICE_MANAGER_RPC_SERVER_MAX_CONNECTIONS)
        {}
    };

    Server(boost::asio::io_context::executor_type executor, const Config &config = Config())
        : executor_(executor)
        , config_(config)
    {
    }
    ~Server();

    // Delete copy constructor and assignment operators
    Server(const Server &) = delete;
    Server(Server &&) = delete;
    Server &operator=(const Server &) = delete;
    Server &operator=(Server &&) = delete;

    bool init();
    void deinit();
    bool start(uint32_t timeout_ms);
    void stop();

    bool add_connection(std::shared_ptr<ServerConnection> connection);
    bool remove_connection(const std::string &name);
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
