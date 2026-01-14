/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <unordered_set>
#include <functional>
#include <memory>
#include <string>
#include "boost/json.hpp"
#include "boost/signals2.hpp"
#include "boost/thread.hpp"
#include "brookesia/service_manager/event/definition.hpp"

namespace esp_brookesia::service {

class EventRegistry {
public:
    using Subscriptions = std::unordered_set<std::string>;
    using Signal = boost::signals2::signal < void(const std::string &event_name, const EventItemMap &event_items) >;
    using SignalConnection = boost::signals2::scoped_connection;
    using SignalSlot = Signal::slot_type;

    EventRegistry() = default;
    ~EventRegistry() = default;

    bool add(EventSchema &&event_schema);
    void remove(const std::string &event_name);
    void remove_all();

    bool validate_items(const std::string &event_name, const EventItemMap &event_items);
    bool on_rpc_subscribe(const std::string &event_name, std::string &subscription_id, std::string &error_message);
    void on_rpc_unsubscribe_by_name(const std::string &event_name);
    void on_rpc_unsubscribe_by_subscriptions(const Subscriptions &subscriptions);

    const EventSchema *get_schema(const std::string &event_name)
    {
        boost::lock_guard lock(event_infos_mutex_);
        auto it = event_infos_.find(event_name);
        if (it == event_infos_.end()) {
            return nullptr;
        }
        return &std::get<1>(it->second);
    }
    std::vector<EventSchema> get_schemas();
    boost::json::array get_schemas_json();
    bool has(const std::string &event_name)
    {
        boost::lock_guard lock(event_infos_mutex_);
        return event_infos_.find(event_name) != event_infos_.end();
    }
    size_t get_count()
    {
        boost::lock_guard lock(event_infos_mutex_);
        return event_infos_.size();
    }

    Subscriptions get_subscriptions(const std::string &event_name);
    Signal *get_signal(const std::string &event_name);

private:
    using EventInfo = std::tuple<Subscriptions, EventSchema, std::unique_ptr<Signal>>;

    boost::mutex event_infos_mutex_;
    std::map<std::string /*name*/, EventInfo> event_infos_;
};

} // namespace esp_brookesia::service
