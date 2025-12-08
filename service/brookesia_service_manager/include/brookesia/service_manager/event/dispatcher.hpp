/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <functional>
#include <vector>
#include <string>
#include "boost/thread.hpp"
#include "brookesia/service_manager/event/definition.hpp"

namespace esp_brookesia::service {

class EventDispatcher {
public:
    using NotifyCallback = std::function<void(const EventItemMap &)>;

    EventDispatcher() = default;
    ~EventDispatcher() = default;

    bool subscribe(const std::string &subscription_id, NotifyCallback callback);
    void unsubscribe(const std::string &subscription_id);
    void on_notify(const std::vector<std::string> &subscription_ids, const EventItemMap &event_items);

private:
    void register_callback(const std::string &subscription_id, NotifyCallback callback);
    void unregister_callback(const std::string &subscription_id);

private:
    boost::mutex callbacks_mutex_;
    std::map<std::string, NotifyCallback> callbacks_;
};

} // namespace esp_brookesia::service
