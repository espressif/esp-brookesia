/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/service_manager/rpc/data_link_base.hpp"

namespace esp_brookesia::service::rpc {

/**
 * @brief TCP transport used by RPC clients.
 */
class DataLinkClient : public DataLinkBase {
public:
    /**
     * @brief Construct a client transport on the given executor.
     *
     * @param[in] executor Executor used for socket operations.
     */
    DataLinkClient(boost::asio::io_context::executor_type executor)
        : DataLinkBase(executor)
    {}
    /**
     * @brief Destructor.
     */
    ~DataLinkClient() override;

    /**
     * @brief Connect to a remote server.
     *
     * @param[in] host Remote host name or address.
     * @param[in] port Remote TCP port.
     * @param[in] timeout_ms Connection timeout in milliseconds.
     * @return true if the connection succeeds.
     */
    bool connect(const std::string &host, uint16_t port, size_t timeout_ms);
    /**
     * @brief Disconnect the active client connection.
     *
     * @return true if a connection existed and cleanup succeeded.
     */
    bool disconnect();

    /**
     * @brief Send one payload over the active connection.
     *
     * @param[in] data Framed payload to send.
     * @return true if the send operation was queued successfully.
     */
    bool send_data(std::string &&data);

    /**
     * @brief Check whether the client currently has an active connection.
     *
     * @return true if connected.
     */
    bool is_connected();

private:
    bool cleanup_connection(std::shared_ptr<ConnectionInfo> connection) override;

    boost::mutex connection_mutex_;
    std::shared_ptr<ConnectionInfo> connection_;
};

} // namespace esp_brookesia::service::rpc
