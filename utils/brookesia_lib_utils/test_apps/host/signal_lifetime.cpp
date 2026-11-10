/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <atomic>
#include <barrier>
#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "brookesia/lib_utils/signal.hpp"

using esp_brookesia::lib_utils::connection;
using esp_brookesia::lib_utils::signal;

void test_external_drain()
{
    signal<void()> events;
    std::promise<void> entered;
    std::promise<void> finish;
    auto ready_to_finish = finish.get_future().share();
    auto subscription = events.connect([&]() {
        entered.set_value();
        ready_to_finish.wait();
    });
    std::thread emitter([&]() {
        events();
    });
    entered.get_future().wait();
    auto disconnect = std::async(std::launch::async, [&]() {
        return subscription.disconnect_and_wait();
    });
    assert(disconnect.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout);
    finish.set_value();
    emitter.join();
    assert(disconnect.get() && !subscription.connected());
}

void test_self_and_cross_signal_drain()
{
    signal<void()> self;
    connection self_connection;
    self_connection = self.connect([&]() {
        assert(!self_connection.disconnect_and_wait());
    });
    self();
    assert(self_connection.disconnect_and_wait());

    signal<void()> a;
    signal<void()> b;
    std::promise<void> entered;
    std::promise<void> finish;
    auto finish_callback = finish.get_future().share();
    auto owner = std::make_shared<int>(42);
    std::weak_ptr<int> weak_owner = owner;
    std::shared_ptr<int> deferred;
    auto b_connection = b.connect([raw_owner = owner.get(), &entered, &finish_callback]() {
        entered.set_value();
        finish_callback.wait();
        assert(*raw_owner == 42);
    });
    auto a_connection = a.connect([&]() {
        assert(!b_connection.disconnect_and_wait());
        deferred = std::move(owner);
    });
    std::thread emitter([&]() {
        b();
    });
    entered.get_future().wait();
    a();
    assert(!weak_owner.expired());
    finish.set_value();
    emitter.join();
    assert(b_connection.disconnect_and_wait());
    deferred.reset();
    assert(weak_owner.expired());
    assert(a_connection.disconnect_and_wait());
}

void test_mutual_disconnect_and_exception()
{
    signal<void()> a;
    signal<void()> b;
    connection a_connection;
    connection b_connection;
    std::barrier arrived(2);
    std::atomic_bool a_drained = true;
    std::atomic_bool b_drained = true;
    a_connection = a.connect([&]() {
        arrived.arrive_and_wait();
        b_drained = b_connection.disconnect_and_wait();
    });
    b_connection = b.connect([&]() {
        arrived.arrive_and_wait();
        a_drained = a_connection.disconnect_and_wait();
    });
    std::thread first([&]() {
        a();
    });
    std::thread second([&]() {
        b();
    });
    first.join();
    second.join();
    assert(!a_drained || !b_drained);
    assert(a_connection.disconnect_and_wait() && b_connection.disconnect_and_wait());

    signal<void()> throwing;
    auto subscription = throwing.connect([]() {
        throw std::runtime_error("injected");
    });
    try {
        throwing();
        assert(false);
    } catch (const std::runtime_error &) {
    }
    assert(subscription.disconnect_and_wait());
}

int main()
{
    test_external_drain();
    test_self_and_cross_signal_drain();
    test_mutual_disconnect_and_exception();
}
