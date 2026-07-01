/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <functional>
#include <vector>
#include <string>
#include "boost/thread/mutex.hpp"
#include "brookesia/service_manager/event/definition.hpp"

namespace esp_brookesia::service {

/**
 * @brief Dispatches RPC event notifications to local subscription callbacks.
 */
class EventDispatcher {
public:
    /**
     * @brief Callback invoked when a subscribed event notification arrives.
     */
    using NotifyCallback = std::function<void(const EventItemMap &)>;

    EventDispatcher() = default;
    ~EventDispatcher() = default;

    /**
     * @brief Register a callback for a subscription identifier.
     *
     * @param[in] subscription_id Subscription id returned by the RPC server.
     * @param[in] callback Callback to invoke when matching notifications arrive.
     * @return true on success, false if the subscription id is already registered.
     */
    bool subscribe(const std::string &subscription_id, NotifyCallback callback);
    /**
     * @brief Remove a previously registered subscription callback.
     *
     * @param[in] subscription_id Subscription id to remove.
     */
    void unsubscribe(const std::string &subscription_id);
    /**
     * @brief Dispatch an incoming notification to all matching local callbacks.
     *
     * @param[in] subscription_ids Subscription ids targeted by the notification.
     * @param[in] event_items Event payload associated with the notification.
     */
    void on_notify(const std::vector<std::string> &subscription_ids, const EventItemMap &event_items);

private:
    void register_callback(const std::string &subscription_id, NotifyCallback callback);
    void unregister_callback(const std::string &subscription_id);

private:
    boost::mutex callbacks_mutex_;
    std::map<std::string, NotifyCallback> callbacks_;
};

} // namespace esp_brookesia::service
