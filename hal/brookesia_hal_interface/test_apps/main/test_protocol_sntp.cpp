/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include "unity.h"
#include "test_protocol_sntp.hpp"

bool TestSntpClientIface::init(const Config &config)
{
    if (!set_servers(config.servers)) {
        return false;
    }
    if (!set_timezone(config.timezone)) {
        return false;
    }
    initialized_ = true;
    running_ = false;
    synced_ = false;
    return true;
}

void TestSntpClientIface::deinit()
{
    initialized_ = false;
    running_ = false;
    synced_ = false;
}

bool TestSntpClientIface::start()
{
    if (!initialized_) {
        return false;
    }
    running_ = true;
    synced_ = false;
    return true;
}

void TestSntpClientIface::stop()
{
    running_ = false;
    synced_ = false;
}

bool TestSntpClientIface::is_initialized() const
{
    return initialized_;
}

bool TestSntpClientIface::is_running() const
{
    return running_;
}

bool TestSntpClientIface::wait_time_sync(uint32_t timeout_ms)
{
    (void)timeout_ms;

    if (!running_) {
        return false;
    }
    synced_ = true;
    return true;
}

bool TestSntpClientIface::is_time_synced() const
{
    return synced_;
}

bool TestSntpClientIface::set_servers(const std::vector<std::string> &servers)
{
    if (servers.size() > get_max_servers()) {
        return false;
    }
    servers_ = servers;
    if (servers_.empty()) {
        servers_.push_back("pool.ntp.org");
    }
    return true;
}

std::vector<std::string> TestSntpClientIface::get_servers() const
{
    return servers_;
}

std::size_t TestSntpClientIface::get_max_servers() const
{
    return 4;
}

bool TestSntpClientIface::set_timezone(const std::string &timezone)
{
    if (timezone.empty()) {
        return false;
    }
    timezone_ = timezone;
    return true;
}

std::string TestSntpClientIface::get_timezone() const
{
    return timezone_;
}

bool TestGeneralSntpDevice::probe()
{
    return true;
}

std::vector<hal::InterfaceSpec> TestGeneralSntpDevice::get_interface_specs() const
{
    return {{hal::network::SntpClientIface::NAME, TestSntpClientIface::NAME}};
}

bool TestGeneralSntpDevice::on_init()
{
    interfaces_.emplace(TestSntpClientIface::NAME, std::make_shared<TestSntpClientIface>());

    return true;
}

void TestGeneralSntpDevice::on_deinit()
{
    interfaces_.erase(TestSntpClientIface::NAME);
}

BROOKESIA_PLUGIN_REGISTER(hal::Device, TestGeneralSntpDevice, std::string(TestGeneralSntpDevice::NAME));

TEST_CASE("network::SntpClientIface: acquire registers interface", "[hal][device]")
{

    auto iface_handle = hal::acquire_first_interface<hal::network::SntpClientIface>();
    auto name = std::string(iface_handle.instance_name());
    auto iface = iface_handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());
    TEST_ASSERT_EQUAL_STRING(TestSntpClientIface::NAME, name.c_str());
}

TEST_CASE("network::SntpClientIface: configuration and sync lifecycle", "[hal][interface]")
{

    auto iface_handle = hal::acquire_first_interface<hal::network::SntpClientIface>();
    auto name = std::string(iface_handle.instance_name());
    auto iface = iface_handle.get();
    TEST_ASSERT_NOT_NULL(iface.get());

    std::vector<std::string> servers = {"pool.ntp.org", "time.nist.gov"};
    TEST_ASSERT_TRUE(iface->init({
        .servers = servers,
        .timezone = "UTC",
        .use_dhcp = true,
    }));
    TEST_ASSERT_TRUE(iface->is_initialized());
    TEST_ASSERT_EQUAL_STRING("UTC", iface->get_timezone().c_str());
    TEST_ASSERT_EQUAL(servers.size(), iface->get_servers().size());
    TEST_ASSERT_TRUE(iface->start());
    TEST_ASSERT_TRUE(iface->is_running());
    TEST_ASSERT_TRUE(iface->wait_time_sync(0));
    TEST_ASSERT_TRUE(iface->is_time_synced());
    iface->stop();
    TEST_ASSERT_FALSE(iface->is_running());
    iface->deinit();
    TEST_ASSERT_FALSE(iface->is_initialized());
}
