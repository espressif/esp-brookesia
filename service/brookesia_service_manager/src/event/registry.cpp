/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_EVENT_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/event/registry.hpp"

namespace esp_brookesia::service {

bool EventRegistry::add(EventSchema &&event_schema)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_schema(%1%)", BROOKESIA_DESCRIBE_TO_STR(event_schema));

    BROOKESIA_CHECK_FALSE_RETURN(!event_schema.name.empty(), false, "Event name is empty");

    boost::lock_guard lock(event_infos_mutex_);

    if (event_infos_.find(event_schema.name) != event_infos_.end()) {
        BROOKESIA_LOGD("Event already exists, skip register");
        return true;
    }

    std::unique_ptr<Signal> signal;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        signal = std::make_unique<Signal>(), false, "Failed to create signal"
    );
    event_infos_[event_schema.name] = std::make_tuple(Subscriptions(), event_schema, std::move(signal));

    return true;
}

void EventRegistry::remove(const std::string &event_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%)", event_name);

    boost::lock_guard lock(event_infos_mutex_);

    auto event_it = event_infos_.find(event_name);
    if (event_it == event_infos_.end()) {
        BROOKESIA_LOGD("Event not found, skip unregister");
        return;
    }
    event_infos_.erase(event_it);
}

void EventRegistry::remove_all()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    boost::lock_guard lock(event_infos_mutex_);
    event_infos_.clear();
}

bool EventRegistry::validate_items(const std::string &event_name, const EventItemMap &event_items)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%), event_items(%2%)", event_name, BROOKESIA_DESCRIBE_TO_STR(event_items));

    BROOKESIA_CHECK_FALSE_RETURN(!event_items.empty(), false, "Event items map is empty");

    EventSchema event_schema;
    {
        boost::lock_guard lock(event_infos_mutex_);
        auto event_it = event_infos_.find(event_name);
        BROOKESIA_CHECK_FALSE_RETURN(event_it != event_infos_.end(), false, "Event not found");

        event_schema = std::get<1>(event_it->second);
    }

    // Validate the event items against the event schema
    for (const auto &item_schema : event_schema.items) {
        auto item_it = event_items.find(item_schema.name);
        BROOKESIA_CHECK_FALSE_RETURN(
            item_it != event_items.end(), false, "Missing event item: `%1%`", item_schema.name
        );
        BROOKESIA_CHECK_FALSE_RETURN(
            item_schema.is_compatible_item(item_it->second), false,
            "Invalid value for event item: `%1%`", item_schema.name
        );
    }

    for (const auto& [item_name, item_value] : event_items) {
        bool found = false;
        for (const auto &item_schema : event_schema.items) {
            if (item_schema.name == item_name) {
                found = true;
                break;
            }
        }
        if (!found) {
            BROOKESIA_LOGW("Unknown event item: '%1%', ignored", item_name);
        }
    }

    return true;
}

bool EventRegistry::on_rpc_subscribe(const std::string &event_name, std::string &subscription_id, std::string &error_message)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%)", event_name);

    boost::lock_guard lock(event_infos_mutex_);

    auto event_it = event_infos_.find(event_name);
    if (event_it == event_infos_.end()) {
        error_message = "Event not found";
#if BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG
        BROOKESIA_LOGE("%s", error_message.c_str());
#endif
        return false;
    }

    auto &subscriptions = std::get<0>(event_it->second);
    subscription_id = utils_generate_uuid();
    subscriptions.insert(subscription_id);

    return true;
}

void EventRegistry::on_rpc_unsubscribe_by_name(const std::string &event_name)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: event_name(%1%)", event_name);

    boost::lock_guard lock(event_infos_mutex_);

    auto it = event_infos_.find(event_name);
    if (it != event_infos_.end()) {
        event_infos_.erase(it);
    }
}

void EventRegistry::on_rpc_unsubscribe_by_subscriptions(const Subscriptions &subscriptions)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: subscriptions(%1%)", BROOKESIA_DESCRIBE_TO_STR(subscriptions));

    boost::lock_guard lock(event_infos_mutex_);

    for (const auto &subscription_id : subscriptions) {
        for (auto& [name, event_info] : event_infos_) {
            auto &[subscriptions, schema, signal] = event_info;
            auto it = subscriptions.find(subscription_id);
            if (it != subscriptions.end()) {
                subscriptions.erase(it);
            }
        }
    }
}

std::vector<EventSchema> EventRegistry::get_schemas()
{
    boost::lock_guard lock(event_infos_mutex_);

    std::vector<EventSchema> schemas;
    for (const auto& [name, event_info] : event_infos_) {
        auto &[subscriptions, schema, signal] = event_info;
        schemas.push_back(schema);
    }
    return schemas;
}

boost::json::array EventRegistry::get_schemas_json()
{
    boost::lock_guard lock(event_infos_mutex_);

    boost::json::array schemas;
    for (const auto& [name, event_info] : event_infos_) {
        auto &[subscriptions, schema, signal] = event_info;
        schemas.push_back(std::move(BROOKESIA_DESCRIBE_TO_JSON(schema)));
    }

    return schemas;
}

EventRegistry::Subscriptions EventRegistry::get_subscriptions(const std::string &event_name)
{
    boost::lock_guard lock(event_infos_mutex_);

    auto it = event_infos_.find(event_name);
    BROOKESIA_CHECK_FALSE_RETURN(it != event_infos_.end(), Subscriptions(), "Event not found");

    return std::get<0>(it->second);
}

EventRegistry::Signal *EventRegistry::get_signal(const std::string &event_name)
{
    boost::lock_guard lock(event_infos_mutex_);

    auto it = event_infos_.find(event_name);
    BROOKESIA_CHECK_FALSE_RETURN(it != event_infos_.end(), nullptr, "Event not found");

    return std::get<2>(it->second).get();
}

} // namespace esp_brookesia::service
