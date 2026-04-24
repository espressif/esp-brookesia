/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#include <atomic>
#include <functional>
#include <string>
#include "unity.h"
#include "common.hpp"

using namespace esp_brookesia;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////// Test Fixtures //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace esp_brookesia {

// Dedicated test device used by the `Device` member-level callback tests. Its on_init /
// on_deinit bodies stamp a monotonically increasing sequence number so tests can assert
// that registered callbacks observed the expected init/deinit ordering.
class CallbacksMemberDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestCallbacksMember";

    CallbacksMemberDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override
    {
        return true;
    }

    bool on_init() override;
    void on_deinit() override;
};

// Dedicated test device targeted by the `ESP_BROOKESIA_HAL_DEVICE_REGISTER_*` per-device
// auto-registration macros below. Keeping it separate avoids cross-contamination with the
// member-level tests that explicitly (re)register callbacks on `CallbacksMemberDevice`.
class CallbacksMacroDevice: public hal::Device {
public:
    static constexpr const char *NAME = "TestCallbacksMacro";

    CallbacksMacroDevice() : hal::Device(std::string(NAME)) {}

    bool probe() override
    {
        return true;
    }

    bool on_init() override
    {
        return true;
    }

    void on_deinit() override {}
};

} // namespace esp_brookesia

namespace {

// Monotonic sequence clock. Incremented by every observation point so tests can reason
// about relative ordering of `pre_init`, `on_init`, `on_deinit`, and `post_deinit`.
std::atomic<int> g_seq{0};

// Per-observation-point sequence markers for the member-level tests. A value of `0` means
// "not observed since last reset"; tests reset them to `0` before triggering init/deinit.
std::atomic<int> g_member_pre_init_seq{0};
std::atomic<int> g_member_on_init_seq{0};
std::atomic<int> g_member_on_deinit_seq{0};
std::atomic<int> g_member_post_deinit_seq{0};

// Invocation counters for the member-level overwrite tests; each test registers two
// distinct callbacks and verifies only the latest one fires. The pre-init and post-deinit
// overwrite tests keep separate counter pairs so that a lingering pre-init callback left
// attached by an earlier test cannot contaminate the post-deinit overwrite test (and vice
// versa).
std::atomic<int> g_member_pre_cb_a_count{0};
std::atomic<int> g_member_pre_cb_b_count{0};
std::atomic<int> g_member_post_cb_a_count{0};
std::atomic<int> g_member_post_cb_b_count{0};

// Invocation counters for global `hal::register_pre_init_callback` / `register_post_deinit_callback`.
std::atomic<int> g_global_pre_init_count{0};
std::atomic<int> g_global_post_deinit_count{0};

// Invocation counters for the `ESP_BROOKESIA_HAL_DEVICE_REGISTER_*` auto-registration macros.
// These are manipulated both by the static-ctor-installed macro callbacks and by explicit
// re-registration in the per-test teardown so test order does not matter.
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

} // namespace

namespace esp_brookesia {

bool CallbacksMemberDevice::on_init()
{
    g_member_on_init_seq.store(++g_seq);
    return true;
}

void CallbacksMemberDevice::on_deinit()
{
    g_member_on_deinit_seq.store(++g_seq);
}

} // namespace esp_brookesia

BROOKESIA_PLUGIN_REGISTER(hal::Device, CallbacksMemberDevice, std::string(CallbacksMemberDevice::NAME));
BROOKESIA_PLUGIN_REGISTER(hal::Device, CallbacksMacroDevice, std::string(CallbacksMacroDevice::NAME));

// Auto-register the macros at static-ctor time so the test also exercises the static
// init path that production code relies on. Tests reset the observed counters right before
// the behaviour they care about to make the assertions independent of test order.
ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_PRE_INIT_CALLBACK(
    test_callbacks_all_pre_init_symbol, &macro_all_pre_init_impl
)
ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK(
    test_callbacks_all_post_deinit_symbol, &macro_all_post_deinit_impl
)
ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS(
    test_callbacks_dev_pre_init_symbol,
{ std::string(CallbacksMacroDevice::NAME), &macro_dev_pre_init_impl }
)
ESP_BROOKESIA_HAL_DEVICE_REGISTER_POST_DEINIT_CALLBACKS(
    test_callbacks_dev_post_deinit_symbol,
{ std::string(CallbacksMacroDevice::NAME), &macro_dev_post_deinit_impl }
)

namespace {

// Helper: bring the whole registry back to a known clean state and clear the observation
// sequence counter so individual tests can assert exact sequencing without worrying about
// state leaked from unrelated tests. `CallbacksMemberDevice` is also re-seeded with benign
// passing callbacks so that a capturing lambda left attached by an earlier test cannot
// fire during the current test's `init_device()` / `deinit_device()` calls.
void reset_all_callback_state()
{
    hal::deinit_all_devices();

    g_seq.store(0);
    g_member_pre_init_seq.store(0);
    g_member_on_init_seq.store(0);
    g_member_on_deinit_seq.store(0);
    g_member_post_deinit_seq.store(0);
    g_member_pre_cb_a_count.store(0);
    g_member_pre_cb_b_count.store(0);
    g_member_post_cb_a_count.store(0);
    g_member_post_cb_b_count.store(0);
    g_global_pre_init_count.store(0);
    g_global_post_deinit_count.store(0);
    g_macro_all_pre_init_count.store(0);
    g_macro_all_post_deinit_count.store(0);
    g_macro_dev_pre_init_count.store(0);
    g_macro_dev_post_deinit_count.store(0);

    hal::register_pre_init_callback({});
    hal::register_post_deinit_callback({});

    if (auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME)) {
        device->register_pre_init_callback([]() {
            return true;
        });
        device->register_post_deinit_callback([]() {
            return true;
        });
    }
}

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////// Device member-level callbacks //////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("Device::register_pre_init_callback: rejects empty callback", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_FALSE(device->register_pre_init_callback({}));
}

TEST_CASE("Device::register_post_deinit_callback: rejects empty callback", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_FALSE(device->register_post_deinit_callback({}));
}

TEST_CASE("Device::register_pre_init_callback: fires before on_init during init", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        g_member_pre_init_seq.store(++g_seq);
        return true;
    }));

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));

    TEST_ASSERT_GREATER_THAN_INT(0, g_member_pre_init_seq.load());
    TEST_ASSERT_GREATER_THAN_INT(0, g_member_on_init_seq.load());
    TEST_ASSERT_LESS_THAN_INT(g_member_on_init_seq.load(), g_member_pre_init_seq.load());

    hal::deinit_device(CallbacksMemberDevice::NAME);
}

TEST_CASE("Device::register_pre_init_callback: returning false aborts init", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        g_member_pre_init_seq.store(++g_seq);
        return false;
    }));

    TEST_ASSERT_FALSE(hal::init_device(CallbacksMemberDevice::NAME));

    TEST_ASSERT_GREATER_THAN_INT(0, g_member_pre_init_seq.load());
    TEST_ASSERT_EQUAL_INT(0, g_member_on_init_seq.load());

    // Restore a passing callback so subsequent tests that rely on this device succeed.
    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        return true;
    }));
}

TEST_CASE("Device::register_post_deinit_callback: fires after on_deinit during deinit", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_TRUE(device->register_post_deinit_callback([]() {
        g_member_post_deinit_seq.store(++g_seq);
        return true;
    }));

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));
    hal::deinit_device(CallbacksMemberDevice::NAME);

    TEST_ASSERT_GREATER_THAN_INT(0, g_member_on_deinit_seq.load());
    TEST_ASSERT_GREATER_THAN_INT(0, g_member_post_deinit_seq.load());
    TEST_ASSERT_LESS_THAN_INT(g_member_post_deinit_seq.load(), g_member_on_deinit_seq.load());
}

TEST_CASE("Device::register_pre_init_callback: overwrites previously registered callback", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        ++g_member_pre_cb_a_count;
        return true;
    }));
    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        ++g_member_pre_cb_b_count;
        return true;
    }));

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));

    TEST_ASSERT_EQUAL_INT(0, g_member_pre_cb_a_count.load());
    TEST_ASSERT_EQUAL_INT(1, g_member_pre_cb_b_count.load());

    hal::deinit_device(CallbacksMemberDevice::NAME);
    // Remove the capturing lambda so it cannot fire during a later test's init_device().
    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        return true;
    }));
}

TEST_CASE("Device::register_post_deinit_callback: overwrites previously registered callback", "[hal][device][callback]")
{
    reset_all_callback_state();

    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());

    TEST_ASSERT_TRUE(device->register_post_deinit_callback([]() {
        ++g_member_post_cb_a_count;
        return true;
    }));
    TEST_ASSERT_TRUE(device->register_post_deinit_callback([]() {
        ++g_member_post_cb_b_count;
        return true;
    }));

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));
    hal::deinit_device(CallbacksMemberDevice::NAME);

    TEST_ASSERT_EQUAL_INT(0, g_member_post_cb_a_count.load());
    TEST_ASSERT_EQUAL_INT(1, g_member_post_cb_b_count.load());

    // Remove the capturing lambda so it cannot fire during a later test's deinit_device().
    TEST_ASSERT_TRUE(device->register_post_deinit_callback([]() {
        return true;
    }));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////// Global callbacks //////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("register_pre_init_callback: global callback fires before init_all_devices iterates", "[hal][callback]")
{
    reset_all_callback_state();

    hal::register_pre_init_callback([]() {
        ++g_global_pre_init_count;
        return true;
    });

    hal::init_all_devices();

    TEST_ASSERT_EQUAL_INT(1, g_global_pre_init_count.load());

    hal::deinit_all_devices();
    hal::register_pre_init_callback({});
}

TEST_CASE("register_pre_init_callback: passing empty callback clears the registration", "[hal][callback]")
{
    reset_all_callback_state();

    hal::register_pre_init_callback([]() {
        ++g_global_pre_init_count;
        return true;
    });
    hal::register_pre_init_callback({});

    hal::init_all_devices();

    TEST_ASSERT_EQUAL_INT(0, g_global_pre_init_count.load());

    hal::deinit_all_devices();
}

TEST_CASE("register_post_deinit_callback: global callback fires after deinit_all_devices completes", "[hal][callback]")
{
    reset_all_callback_state();

    hal::register_post_deinit_callback([]() {
        ++g_global_post_deinit_count;
        return true;
    });

    hal::init_all_devices();
    hal::deinit_all_devices();

    TEST_ASSERT_EQUAL_INT(1, g_global_post_deinit_count.load());

    hal::register_post_deinit_callback({});
}

TEST_CASE("register_post_deinit_callback: passing empty callback clears the registration", "[hal][callback]")
{
    reset_all_callback_state();

    hal::register_post_deinit_callback([]() {
        ++g_global_post_deinit_count;
        return true;
    });
    hal::register_post_deinit_callback({});

    hal::init_all_devices();
    hal::deinit_all_devices();

    TEST_ASSERT_EQUAL_INT(0, g_global_post_deinit_count.load());
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// detail::enqueue_pending_* deferred registration /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("detail::enqueue_pending_pre_init_callback: attached on matching init_device", "[hal][device][callback]")
{
    reset_all_callback_state();

    std::atomic<int> count{0};
    hal::detail::enqueue_pending_pre_init_callback(CallbacksMemberDevice::NAME, [&count]() {
        ++count;
        return true;
    });

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));
    TEST_ASSERT_EQUAL_INT(1, count.load());

    // The pending entry was drained into the device; subsequent init cycles should keep
    // firing because the callback now lives on the device itself.
    hal::deinit_device(CallbacksMemberDevice::NAME);
    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));
    TEST_ASSERT_EQUAL_INT(2, count.load());

    hal::deinit_device(CallbacksMemberDevice::NAME);
    // Unregister the capturing callback before `count` goes out of scope.
    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        return true;
    }));
}

TEST_CASE("detail::enqueue_pending_post_deinit_callback: attached on matching init_device", "[hal][device][callback]")
{
    reset_all_callback_state();

    std::atomic<int> count{0};
    hal::detail::enqueue_pending_post_deinit_callback(CallbacksMemberDevice::NAME, [&count]() {
        ++count;
        return true;
    });

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));
    hal::deinit_device(CallbacksMemberDevice::NAME);
    TEST_ASSERT_EQUAL_INT(1, count.load());

    // Unregister the capturing callback before `count` goes out of scope.
    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_TRUE(device->register_post_deinit_callback([]() {
        return true;
    }));
}

TEST_CASE("detail::enqueue_pending_pre_init_callback: second enqueue overwrites the pending entry",
          "[hal][device][callback]")
{
    reset_all_callback_state();

    std::atomic<int> count_a{0};
    std::atomic<int> count_b{0};
    hal::detail::enqueue_pending_pre_init_callback(CallbacksMemberDevice::NAME, [&count_a]() {
        ++count_a;
        return true;
    });
    hal::detail::enqueue_pending_pre_init_callback(CallbacksMemberDevice::NAME, [&count_b]() {
        ++count_b;
        return true;
    });

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMemberDevice::NAME));

    TEST_ASSERT_EQUAL_INT(0, count_a.load());
    TEST_ASSERT_EQUAL_INT(1, count_b.load());

    hal::deinit_device(CallbacksMemberDevice::NAME);
    auto device = hal::get_device_by_plugin_name(CallbacksMemberDevice::NAME);
    TEST_ASSERT_NOT_NULL(device.get());
    TEST_ASSERT_TRUE(device->register_pre_init_callback([]() {
        return true;
    }));
}

TEST_CASE("detail::enqueue_pending_pre_init_callback: entries for unknown devices are dropped after init_all_devices",
          "[hal][device][callback]")
{
    reset_all_callback_state();

    std::atomic<int> count{0};
    hal::detail::enqueue_pending_pre_init_callback("NonExistentDevice_DoNotRegister", [&count]() {
        ++count;
        return true;
    });

    // `init_all_devices()` iterates registered plugins only and drops any pending entry
    // whose plugin name has no matching device; the callback must never fire.
    hal::init_all_devices();
    TEST_ASSERT_EQUAL_INT(0, count.load());

    hal::deinit_all_devices();

    // A follow-up `init_all_devices()` must confirm the previously dropped entry is not
    // resurrected from any residual storage.
    hal::init_all_devices();
    TEST_ASSERT_EQUAL_INT(0, count.load());

    hal::deinit_all_devices();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////// Auto-registration macros ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_CASE("ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_PRE_INIT_CALLBACK: fires once per init_all_devices",
          "[hal][callback][macro]")
{
    reset_all_callback_state();
    // The previous reset cleared the global registration; restore the macro-installed one
    // so this test asserts the installed callback wiring, independent of test order.
    hal::register_pre_init_callback(&macro_all_pre_init_impl);

    hal::init_all_devices();
    TEST_ASSERT_EQUAL_INT(1, g_macro_all_pre_init_count.load());

    hal::init_all_devices();
    TEST_ASSERT_EQUAL_INT(2, g_macro_all_pre_init_count.load());

    hal::deinit_all_devices();
}

TEST_CASE("ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK: fires once per deinit_all_devices",
          "[hal][callback][macro]")
{
    reset_all_callback_state();
    hal::register_post_deinit_callback(&macro_all_post_deinit_impl);

    hal::init_all_devices();
    hal::deinit_all_devices();
    TEST_ASSERT_EQUAL_INT(1, g_macro_all_post_deinit_count.load());

    hal::init_all_devices();
    hal::deinit_all_devices();
    TEST_ASSERT_EQUAL_INT(2, g_macro_all_post_deinit_count.load());

    hal::register_post_deinit_callback({});
}

TEST_CASE("ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS: fires when target device is initialized",
          "[hal][device][callback][macro]")
{
    reset_all_callback_state();
    // Re-queue the per-device callback in case an earlier init_device() already drained
    // the pending entry from the macro's static-ctor enqueue.
    hal::detail::enqueue_pending_pre_init_callback(CallbacksMacroDevice::NAME, &macro_dev_pre_init_impl);

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMacroDevice::NAME));
    TEST_ASSERT_EQUAL_INT(1, g_macro_dev_pre_init_count.load());

    // After drain the callback lives on the device instance; subsequent cycles keep firing.
    hal::deinit_device(CallbacksMacroDevice::NAME);
    TEST_ASSERT_TRUE(hal::init_device(CallbacksMacroDevice::NAME));
    TEST_ASSERT_EQUAL_INT(2, g_macro_dev_pre_init_count.load());

    hal::deinit_device(CallbacksMacroDevice::NAME);
}

TEST_CASE("ESP_BROOKESIA_HAL_DEVICE_REGISTER_POST_DEINIT_CALLBACKS: fires when target device is deinitialized",
          "[hal][device][callback][macro]")
{
    reset_all_callback_state();
    hal::detail::enqueue_pending_post_deinit_callback(CallbacksMacroDevice::NAME, &macro_dev_post_deinit_impl);

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMacroDevice::NAME));
    hal::deinit_device(CallbacksMacroDevice::NAME);
    TEST_ASSERT_EQUAL_INT(1, g_macro_dev_post_deinit_count.load());

    TEST_ASSERT_TRUE(hal::init_device(CallbacksMacroDevice::NAME));
    hal::deinit_device(CallbacksMacroDevice::NAME);
    TEST_ASSERT_EQUAL_INT(2, g_macro_dev_post_deinit_count.load());
}
