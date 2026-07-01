/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "common.hpp"
#include "brookesia/hal_interface/device.hpp"

class TestSntpClientIface: public hal::network::SntpClientIface {
public:
    static constexpr const char *NAME = "TestGeneralSntp:Sntp";

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
    bool initialized_ = false;
    bool running_ = false;
    bool synced_ = false;
};

class TestGeneralSntpDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestGeneralSntp";

    TestGeneralSntpDevice()
        : hal::Device(NAME)
    {
    }

    bool probe() override;
    std::vector<hal::InterfaceSpec> get_interface_specs() const override;

protected:
    bool on_init() override;
    void on_deinit() override;
};
