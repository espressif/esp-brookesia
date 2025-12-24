/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/service_manager/macro_configs.h"
#if !BROOKESIA_SERVICE_MANAGER_EVENT_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/service_manager/event/dispatcher.hpp"

namespace esp_brookesia::service {

bool EventDispatcher::subscribe(const std::string &subscription_id, NotifyCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: subscription_id(%1%), callback(%2%)", subscription_id, BROOKESIA_DESCRIBE_TO_STR(callback));

    BROOKESIA_CHECK_FALSE_RETURN(!subscription_id.empty(), false, "invalid subscription id");

    register_callback(subscription_id, callback);

    return true;
}

void EventDispatcher::unsubscribe(const std::string &subscription_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: subscription_id(%1%)", subscription_id);

    unregister_callback(subscription_id);
}

void EventDispatcher::on_notify(
    const std::vector<std::string> &subscription_ids, const EventItemMap &event_items
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: subscription_ids(%1%), event_items(%2%)", BROOKESIA_DESCRIBE_TO_STR(subscription_ids),
        BROOKESIA_DESCRIBE_TO_STR(event_items)
    );

    NotifyCallback callback;

    {
        boost::lock_guard lock(callbacks_mutex_);
        for (const auto &subscription_id : subscription_ids) {
            auto it = callbacks_.find(subscription_id);
            if (it != callbacks_.end()) {
                it->second(event_items);
                break;
            }
        }
    }
    if (callback) {
        callback(event_items);
    }
}

void EventDispatcher::register_callback(const std::string &subscription_id, NotifyCallback cb)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: subscription_id(%1%), callback(%2%)", subscription_id, BROOKESIA_DESCRIBE_TO_STR(cb));

    boost::lock_guard lock(callbacks_mutex_);
    callbacks_[subscription_id] = std::move(cb);
}

void EventDispatcher::unregister_callback(const std::string &subscription_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: subscription_id(%1%)", subscription_id);

    boost::lock_guard lock(callbacks_mutex_);
    callbacks_.erase(subscription_id);
}

} // namespace esp_brookesia::service
