/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <memory>

#include "brookesia/hal_interface/interfaces/network/http_client.hpp"

namespace esp_brookesia::hal {

class HttpClientLinuxIface: public network::HttpClientIface {
public:
    std::shared_ptr<Transaction> create_transaction() override;
};

} // namespace esp_brookesia::hal
