/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#if !defined(ESP_PLATFORM)
#include <atomic>
#include <chrono>
#include <future>
#include <map>
#include <memory>
#include <vector>

#include "boost/thread.hpp"
#include "brookesia/hal_interface/device.hpp"
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_device/service_device.hpp"
#include "brookesia/service_manager.hpp"

namespace {

using namespace esp_brookesia;
using namespace std::chrono_literals;

class MockExpansion : public hal::expansion::ModuleManagerIface {
public:
    std::atomic<size_t> additions = 0;
    std::atomic<size_t> removals = 0;
    bool fail_registration = false;
    std::function<void()> before_add;
    std::function<void()> before_remove;

    std::vector<hal::expansion::ModuleInfo> get_module_infos() const override
    {
        return {};
    }

    EventListenerId add_event_listener(EventListener listener) override
    {
        const auto id = ++additions;
        if (before_add) {
            before_add();
        }
        if (fail_registration) {
            return 0;
        }
        boost::lock_guard lock(mutex_);
        listeners_.emplace(id, std::move(listener));
        return id;
    }

    bool remove_event_listener(EventListenerId id) override
    {
        if (before_remove) {
            before_remove();
        }
        ++removals;
        boost::lock_guard lock(mutex_);
        return listeners_.erase(id) == 1;
    }

    size_t listener_count()
    {
        boost::lock_guard lock(mutex_);
        return listeners_.size();
    }

private:
    boost::mutex mutex_;
    std::map<EventListenerId, EventListener> listeners_;
};

std::shared_ptr<MockExpansion> mock;

class MockExpansionDevice : public hal::Device {
public:
    MockExpansionDevice() : Device("ExpansionSubscriptionTest") {}

private:
    bool probe() override
    {
        return mock != nullptr;
    }
    std::vector<hal::InterfaceSpec> get_interface_specs() const override
    {
        return {{
                hal::expansion::ModuleManagerIface::NAME,
                hal::expansion::ModuleManagerIface::get_default_instance_name()
            }};
    }
    bool on_init() override
    {
        interfaces_.emplace(hal::expansion::ModuleManagerIface::get_default_instance_name(), mock);
        return true;
    }
    void on_deinit() override
    {
        interfaces_.clear();
    }
};

BROOKESIA_PLUGIN_REGISTER(hal::Device, MockExpansionDevice, "ExpansionSubscriptionTest");

class ExpansionFixture {
public:
    ExpansionFixture()
    {
        mock = std::make_shared<MockExpansion>();
        auto &manager = service::ServiceManager::get_instance();
        TEST_ASSERT_TRUE(manager.start());
        binding_ = manager.bind(service::helper::Device::get_name().data());
        TEST_ASSERT_TRUE(binding_.is_valid());
    }
    ~ExpansionFixture()
    {
        binding_.release();
        service::ServiceManager::get_instance().deinit();
        hal::detail::cleanup_all_devices();
        mock.reset();
    }

    void stop()
    {
        binding_.release();
    }

    bool start()
    {
        binding_ = service::ServiceManager::get_instance().bind(service::helper::Device::get_name().data());
        return binding_.is_valid();
    }

private:
    service::ServiceBinding binding_;
};

auto subscribe()
{
    auto callback = [](const std::string &, const service::EventItemMap &) {};
    return service::Device::get_instance().subscribe_event("ExpansionModuleChanged", callback);
}

} // namespace

BROOKESIA_TEST_CASE(test_device_expansion_concurrent_subscriptions,
                    "Device expansion: concurrent subscriptions share one HAL listener", "[service][device][expansion]")
{
    ExpansionFixture fixture;
    std::promise<void> entered;
    std::promise<void> release;
    auto released = release.get_future().share();
    std::atomic<bool> first = true;
    mock->before_add = [&]() {
        if (first.exchange(false)) {
            entered.set_value();
        }
        released.wait();
    };

    constexpr size_t subscriber_count = 8;
    std::vector<service::EventRegistry::SignalConnection> connections(subscriber_count);
    std::vector<boost::thread> threads;
    threads.emplace_back([&]() {
        connections[0] = subscribe();
    });
    const bool entered_in_time = entered.get_future().wait_for(2s) == std::future_status::ready;
    for (size_t i = 1; i < subscriber_count; ++i) {
        threads.emplace_back([ &, i]() {
            connections[i] = subscribe();
        });
    }
    // Keep the first registration in flight while the other subscribers enter.
    boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    release.set_value();
    for (auto &thread : threads) {
        thread.join();
    }
    TEST_ASSERT_TRUE(entered_in_time);
    for (auto &connection : connections) {
        TEST_ASSERT_TRUE(connection.connected());
    }
    TEST_ASSERT_EQUAL(1, mock->additions.load());
    TEST_ASSERT_EQUAL(1, mock->listener_count());

    fixture.stop();
    TEST_ASSERT_EQUAL(1, mock->removals.load());
    TEST_ASSERT_EQUAL(0, mock->listener_count());
}

BROOKESIA_TEST_CASE(test_device_expansion_subscription_during_stop,
                    "Device expansion: stopping defers subscriptions until restart", "[service][device][expansion]")
{
    ExpansionFixture fixture;
    auto first_connection = subscribe();
    TEST_ASSERT_TRUE(first_connection.connected());
    std::promise<void> removing;
    std::promise<void> release;
    auto released = release.get_future().share();
    mock->before_remove = [&]() {
        removing.set_value();
        released.wait();
    };
    boost::thread stopper([&]() {
        fixture.stop();
    });
    const bool removing_in_time = removing.get_future().wait_for(2s) == std::future_status::ready;
    service::EventRegistry::SignalConnection concurrent_connection;
    boost::thread subscriber([&]() {
        concurrent_connection = subscribe();
    });
    release.set_value();
    stopper.join();
    subscriber.join();
    mock->before_remove = {};
    TEST_ASSERT_TRUE(removing_in_time);
    TEST_ASSERT_TRUE(concurrent_connection.connected());
    TEST_ASSERT_EQUAL(1, mock->additions.load());
    TEST_ASSERT_EQUAL(0, mock->listener_count());

    auto stopped_connection = subscribe();
    TEST_ASSERT_TRUE(stopped_connection.connected());
    TEST_ASSERT_EQUAL(0, mock->listener_count());
    TEST_ASSERT_TRUE(fixture.start());
    TEST_ASSERT_EQUAL(2, mock->additions.load());
    TEST_ASSERT_EQUAL(1, mock->listener_count());
    fixture.stop();
    TEST_ASSERT_EQUAL(2, mock->removals.load());
    TEST_ASSERT_EQUAL(0, mock->listener_count());
}

BROOKESIA_TEST_CASE(test_device_expansion_registration_retry,
                    "Device expansion: failed HAL registration can be retried", "[service][device][expansion]")
{
    ExpansionFixture fixture;
    mock->fail_registration = true;
    auto first_connection = subscribe();
    TEST_ASSERT_TRUE(first_connection.connected());
    TEST_ASSERT_EQUAL(0, mock->listener_count());
    mock->fail_registration = false;
    auto second_connection = subscribe();
    TEST_ASSERT_TRUE(second_connection.connected());
    TEST_ASSERT_EQUAL(2, mock->additions.load());
    TEST_ASSERT_EQUAL(1, mock->listener_count());
    fixture.stop();
    TEST_ASSERT_EQUAL(1, mock->removals.load());
    TEST_ASSERT_EQUAL(0, mock->listener_count());
}
#endif
