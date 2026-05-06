/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include "boost/json.hpp"
#include "brookesia/service_helper/device.hpp"

namespace esp_brookesia::test_apps::service_device::board {

struct EventState {
    std::mutex mutex;
    std::condition_variable cv;
    bool received = false;
    double number = 0;
    bool boolean = false;
    boost::json::object object;
};

bool startup();
void shutdown();

service::helper::Device::Capabilities get_capabilities();

bool wait_for_number_event(EventState &event, double expected, uint32_t timeout_ms = 1000);
bool wait_for_boolean_event(EventState &event, bool expected, uint32_t timeout_ms = 1000);
bool wait_for_object_event(EventState &event, uint32_t timeout_ms = 1000);

} // namespace esp_brookesia::test_apps::service_device::board
