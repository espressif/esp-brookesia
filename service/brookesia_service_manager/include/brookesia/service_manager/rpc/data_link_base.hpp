/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include <functional>
#include <atomic>
#include "boost/asio/io_context.hpp"
#include "boost/asio/streambuf.hpp"
#include "boost/asio/ip/tcp.hpp"
#include "brookesia/service_manager/macro_configs.h"

namespace esp_brookesia::service::rpc {

/**
 * @brief Abstract TCP data-link layer shared by RPC clients and servers.
 */
class DataLinkBase {
public:
    /**
     * @brief Callback invoked when a complete payload string is received.
     */
    using OnDataReceived = std::function<void(const std::string &data, size_t connection_id)>;
    /**
     * @brief Callback invoked when a transport connection becomes active.
     */
    using OnConnectionEstablished = std::function<void(size_t connection_id)>;
    /**
     * @brief Callback invoked when a transport connection closes.
     */
    using OnConnectionClosed = std::function<void(size_t connection_id)>;

    /**
     * @brief Construct the base transport with an executor.
     *
     * @param[in] executor Executor that owns asynchronous I/O operations.
     */
    DataLinkBase(boost::asio::io_context::executor_type executor)
        : executor_(executor)
        , executor_guard_(executor)
    {
    }
    virtual ~DataLinkBase() = default;

    /**
     * @brief Set the callback used for incoming payloads.
     *
     * @param[in] callback Callback to install.
     */
    void set_on_data_received(OnDataReceived callback)
    {
        on_data_received_ = callback;
    }
    /**
     * @brief Set the callback used for successful connection establishment.
     *
     * @param[in] callback Callback to install.
     */
    void set_on_connection_established(OnConnectionEstablished callback)
    {
        on_connection_established_ = callback;
    }
    /**
     * @brief Set the callback used for connection teardown.
     *
     * @param[in] callback Callback to install.
     */
    void set_on_connection_closed(OnConnectionClosed callback)
    {
        on_connection_closed_ = callback;
    }

    /**
     * @brief Get the number of active sockets across all data-link instances.
     *
     * @return size_t Current global socket count.
     */
    static size_t get_active_global_sockets_count()
    {
        return active_global_sockets_.load();
    }
    /**
     * @brief Get the configured global socket cap.
     *
     * @return size_t Maximum number of sockets allowed globally.
     */
    static size_t get_max_global_sockets_count()
    {
        return BROOKESIA_SERVICE_MANAGER_RPC_GLOBAL_MAX_SOCKETS;
    }
    /**
     * @brief Check whether the global socket cap has been reached.
     *
     * @return true if no more sockets should be opened.
     */
    static bool is_global_sockets_limit_reached()
    {
        return get_active_global_sockets_count() >= get_max_global_sockets_count();
    }
    /**
     * @brief Get the maximum number of simultaneously active sockets observed.
     *
     * @return size_t Historical peak socket count.
     */
    static size_t get_max_active_global_sockets_count()
    {
        return max_global_sockets_.load();
    }

protected:
    struct ConnectionInfo {
        std::shared_ptr<boost::asio::ip::tcp::socket> socket; ///< Owned TCP socket.
        std::atomic<bool> is_active{false}; ///< Whether the connection is still considered active.
        size_t id; ///< Transport-level connection identifier.
        boost::asio::streambuf receive_buffer; ///< Accumulates incoming bytes until a frame is complete.
    };

    virtual void on_handle_receive_error(std::shared_ptr<ConnectionInfo> connection, const boost::system::error_code &error)
    {
        (void)connection;
        (void)error;
    }
    virtual void on_handle_send_error(std::shared_ptr<ConnectionInfo> connection, const boost::system::error_code &error)
    {
        (void)connection;
        (void)error;
    }
    virtual bool cleanup_connection(std::shared_ptr<ConnectionInfo> connection);

    // Subclass accessible methods
    bool handle_receive(std::shared_ptr<ConnectionInfo> connection);
    bool handle_send(std::shared_ptr<ConnectionInfo> connection, std::string &&data);

    // Global connection count management (protected method)
    static bool wait_for_free_global_sockets(size_t timeout_ms);
    static bool try_get_global_socket();
    static void release_global_sockets();

    boost::asio::io_context::executor_type executor_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> executor_guard_;

    // Callback functions
    OnDataReceived on_data_received_;
    OnConnectionEstablished on_connection_established_;
    OnConnectionClosed on_connection_closed_;

private:
    // Global connection count statistics (static members)
    inline static boost::mutex global_sockets_mutex_;
    inline static boost::condition_variable global_sockets_cv_;
    inline static std::atomic<size_t> active_global_sockets_{0};
    inline static std::atomic<size_t> max_global_sockets_{0};
};

} // namespace esp_brookesia::service::rpc
