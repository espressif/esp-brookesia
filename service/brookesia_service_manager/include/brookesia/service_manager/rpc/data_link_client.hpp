/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>
#include "brookesia/service_manager/rpc/data_link_base.hpp"

namespace esp_brookesia::service::rpc {

// Client data link class
class DataLinkClient : public DataLinkBase {
public:
    DataLinkClient(boost::asio::io_context::executor_type executor)
        : DataLinkBase(executor)
    {}
    ~DataLinkClient() override;

    // Client interface
    bool connect(const std::string &host, uint16_t port, size_t timeout_ms);
    bool disconnect();

    bool send_data(std::string &&data);

    // Get connection status
    bool is_connected();

private:
    bool cleanup_connection(std::shared_ptr<ConnectionInfo> connection) override;

    boost::mutex connection_mutex_;
    std::shared_ptr<ConnectionInfo> connection_;
};

} // namespace esp_brookesia::service::rpc
