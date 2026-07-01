/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <string>
#include "unity.h"
#include "common.hpp"
#include "brookesia/hal_interface/device.hpp"

using namespace esp_brookesia;

namespace esp_brookesia {

class TestCallbacksIface: public hal::Interface {
public:
    static constexpr const char *TYPE_NAME = "TestCallbacksIface";
    static constexpr const char *NAME = TYPE_NAME;
    static constexpr const char *MEMBER_NAME = "TestCallbacks:Member";
    static constexpr const char *MACRO_NAME = "TestCallbacks:Macro";

    explicit TestCallbacksIface(std::string_view name)
        : hal::Interface(name)
    {
    }
};

class CallbacksMemberDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestCallbacksMember";

    CallbacksMemberDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override
    {
        return true;
    }

    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {{TestCallbacksIface::TYPE_NAME, TestCallbacksIface::MEMBER_NAME}};
    }

    bool on_init() override;
    void on_deinit() override;
};

class CallbacksMacroDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestCallbacksMacro";

    CallbacksMacroDevice()
        : hal::Device(std::string(NAME))
    {
    }

    bool probe() override
    {
        return true;
    }

    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {{TestCallbacksIface::TYPE_NAME, TestCallbacksIface::MACRO_NAME}};
    }

    bool on_init() override;
    void on_deinit() override;
};

} // namespace esp_brookesia

namespace {

std::atomic<int> g_seq{0};
std::atomic<int> g_member_on_init_seq{0};
std::atomic<int> g_member_on_deinit_seq{0};
std::atomic<int> g_global_pre_init_seq{0};
std::atomic<int> g_global_post_deinit_seq{0};
std::atomic<int> g_macro_all_pre_init_count{0};
std::atomic<int> g_macro_all_post_deinit_count{0};
std::atomic<int> g_macro_dev_pre_init_count{0};
std::atomic<int> g_macro_dev_post_deinit_count{0};

bool macro_all_pre_init_impl()
{
    ++g_macro_all_pre_init_count;
    return true;
}

bool macro_all_post_deinit_impl()
{
    ++g_macro_all_post_deinit_count;
    return true;
}

bool macro_dev_pre_init_impl()
{
    ++g_macro_dev_pre_init_count;
    return true;
}

bool macro_dev_post_deinit_impl()
{
    ++g_macro_dev_post_deinit_count;
    return true;
}

void reset_callback_state()
{
    g_seq.store(0);
    g_member_on_init_seq.store(0);
    g_member_on_deinit_seq.store(0);
    g_global_pre_init_seq.store(0);
    g_global_post_deinit_seq.store(0);
    g_macro_all_pre_init_count.store(0);
    g_macro_all_post_deinit_count.store(0);
    g_macro_dev_pre_init_count.store(0);
    g_macro_dev_post_deinit_count.store(0);
    hal::detail::register_pre_init_callback({});
    hal::detail::register_post_deinit_callback({});
}

} // namespace

namespace esp_brookesia {

bool CallbacksMemberDevice::on_init()
{
    g_member_on_init_seq.store(++g_seq);
    interfaces_.emplace(TestCallbacksIface::MEMBER_NAME, std::make_shared<TestCallbacksIface>(TestCallbacksIface::MEMBER_NAME));
    return true;
}

void CallbacksMemberDevice::on_deinit()
{
    g_member_on_deinit_seq.store(++g_seq);
    interfaces_.erase(TestCallbacksIface::MEMBER_NAME);
}

bool CallbacksMacroDevice::on_init()
{
    interfaces_.emplace(TestCallbacksIface::MACRO_NAME, std::make_shared<TestCallbacksIface>(TestCallbacksIface::MACRO_NAME));
    return true;
}

void CallbacksMacroDevice::on_deinit()
{
    interfaces_.erase(TestCallbacksIface::MACRO_NAME);
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, CallbacksMemberDevice, std::string(CallbacksMemberDevice::NAME));
BROOKESIA_PLUGIN_REGISTER(hal::Device, CallbacksMacroDevice, std::string(CallbacksMacroDevice::NAME));

ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_PRE_INIT_CALLBACK(
    test_callbacks_all_pre_init_symbol, &macro_all_pre_init_impl
)
ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK(
    test_callbacks_all_post_deinit_symbol, &macro_all_post_deinit_impl
)
ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS(
    test_callbacks_dev_pre_init_symbol,
{std::string(CallbacksMacroDevice::NAME), &macro_dev_pre_init_impl}
)
ESP_BROOKESIA_HAL_DEVICE_REGISTER_POST_DEINIT_CALLBACKS(
    test_callbacks_dev_post_deinit_symbol,
{std::string(CallbacksMacroDevice::NAME), &macro_dev_post_deinit_impl}
)

TEST_CASE("Callbacks: global callbacks wrap first and last acquired provider", "[hal][callback]")
{
    reset_callback_state();
    hal::detail::register_pre_init_callback([]() {
        g_global_pre_init_seq.store(++g_seq);
        return true;
    });
    hal::detail::register_post_deinit_callback([]() {
        g_global_post_deinit_seq.store(++g_seq);
        return true;
    });

    auto handle = hal::acquire_interface<TestCallbacksIface>(TestCallbacksIface::MEMBER_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_GREATER_THAN_INT(0, g_global_pre_init_seq.load());
    TEST_ASSERT_GREATER_THAN_INT(g_global_pre_init_seq.load(), g_member_on_init_seq.load());

    handle.reset();
    TEST_ASSERT_GREATER_THAN_INT(g_member_on_deinit_seq.load(), g_global_post_deinit_seq.load());

    hal::detail::register_pre_init_callback({});
    hal::detail::register_post_deinit_callback({});
}

TEST_CASE("Callbacks: pending per-device callbacks attach on acquire", "[hal][callback]")
{
    reset_callback_state();
    hal::detail::enqueue_pending_pre_init_callback(std::string(CallbacksMacroDevice::NAME), &macro_dev_pre_init_impl);
    hal::detail::enqueue_pending_post_deinit_callback(std::string(CallbacksMacroDevice::NAME), &macro_dev_post_deinit_impl);

    auto handle = hal::acquire_interface<TestCallbacksIface>(TestCallbacksIface::MACRO_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL(1, g_macro_dev_pre_init_count.load());

    handle.reset();
    TEST_ASSERT_EQUAL(1, g_macro_dev_post_deinit_count.load());
}

TEST_CASE("Callbacks: registered global callbacks run on acquire cycle", "[hal][callback]")
{
    reset_callback_state();
    hal::detail::register_pre_init_callback(&macro_all_pre_init_impl);
    hal::detail::register_post_deinit_callback(&macro_all_post_deinit_impl);

    auto handle = hal::acquire_interface<TestCallbacksIface>(TestCallbacksIface::MACRO_NAME);
    TEST_ASSERT_TRUE(static_cast<bool>(handle));
    TEST_ASSERT_EQUAL(1, g_macro_all_pre_init_count.load());

    handle.reset();
    TEST_ASSERT_EQUAL(1, g_macro_all_post_deinit_count.load());
    hal::detail::register_pre_init_callback({});
    hal::detail::register_post_deinit_callback({});
}
