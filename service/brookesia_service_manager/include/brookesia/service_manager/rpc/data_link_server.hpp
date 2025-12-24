/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <vector>
#include <queue>
#include "boost/thread.hpp"
#include "brookesia/service_manager/rpc/data_link_base.hpp"

namespace esp_brookesia::service::rpc {

// Server data link class
class DataLinkServer : public DataLinkBase {
public:
    explicit DataLinkServer(boost::asio::io_context::executor_type executor, size_t max_connections)
        : DataLinkBase(executor)
        , max_connections_(max_connections)
    {
    }
    ~DataLinkServer() override;

    bool start(uint16_t port, size_t timeout_ms);
    void stop();

    bool send_data(size_t connection_id, std::string &&data);

    size_t get_active_connections_count();
    std::vector<size_t> get_active_connection_ids();

    bool is_running()
    {
        return is_running_.load();
    }
    size_t get_max_connections_count()
    {
        return max_connections_;
    }
    bool is_connection_limit_reached()
    {
        return get_active_connections_count() >= max_connections_;
    }
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
