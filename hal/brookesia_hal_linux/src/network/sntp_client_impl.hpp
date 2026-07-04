/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "brookesia/hal_interface/interfaces/network/sntp_client.hpp"

namespace esp_brookesia::hal {

class SntpClientLinuxIface: public network::SntpClientIface {
public:
    bool init(const Config &config) override;
    void deinit() override;
    bool start() override;
    void stop() override;
    bool is_initialized() const override;
    bool is_running() const override;
    bool wait_time_sync(uint32_t timeout_ms) override;
    bool is_time_synced() const override;
    bool set_servers(const std::vector<std::string> &servers) override;
    std::vector<std::string> get_servers() const override;
    std::size_t get_max_servers() const override;
    bool set_timezone(const std::string &timezone) override;
    std::string get_timezone() const override;

private:
    std::vector<std::string> servers_ = {"pool.ntp.org"};
    std::string timezone_ = "CST-8";
    bool is_initialized_ = false;
    bool is_running_ = false;
    bool is_time_synced_ = false;
};

} // namespace esp_brookesia::hal
