/*
 * SPDX-FileCopyrightText: 2024-2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include "esp_brookesia_systems_internal.h"
#if !ESP_BROOKESIA_BASE_EVENT_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_base_utils.hpp"
#include "esp_brookesia_base_event.hpp"

using namespace std;

namespace esp_brookesia::systems::base {

Event::Event():
    _free_event_id(ID::CUSTOM)
{
}

Event::~Event()
{
}

Event::ID &operator++(Event::ID &id)
{
    id = static_cast<Event::ID>(static_cast<int>(id) + 1);

    return id;
}

Event::ID operator++(Event::ID &id, int)
{
    Event::ID old = id;
    ++id;

    return old;
}

void Event::reset(void)
{
    _free_event_id = ID::CUSTOM;
    _event_handlers.clear();
    _available_event_ids.clear();
}

bool Event::registerEvent(void *object, Handler handler, ID id, void *user_data)
{
    ESP_UTILS_LOGD("Register event for object(0x%p) ID(%d) handler(0x%p), user_data(0x%p)", object, static_cast<int>(id),
                   handler, user_data);
    ESP_UTILS_CHECK_NULL_RETURN(handler, false, "Invalid handler");

    auto &handlers_for_object = _event_handlers[object];
    handlers_for_object[id].emplace_back(handler, user_data);

    return true;
}

bool Event::sendEvent(void *object, ID id, void *param) const
{
    ESP_UTILS_LOGD("Send event for object(0x%p) ID(%d) param(0x%p)", object, static_cast<int>(id), param);

    auto object_it = _event_handlers.find(object);
    if (object_it == _event_handlers.end()) {
        return true;
    }

    auto &handlers_for_object = object_it->second;
    auto handler_it = handlers_for_object.find(id);
    if (handler_it == handlers_for_object.end()) {
        return true;
    }

    HandlerData data = {};
    bool ret = true;
    for (auto &handler_data_pair : handler_it->second) {
        auto &handler = handler_data_pair.first;
        auto &user_data = handler_data_pair.second;
        if (handler == nullptr) {
            ESP_UTILS_LOGE("Handler is nullptr");
            continue;
        }
        data = {id, object, param, user_data};
        if (!handler(data)) {
            ret = false;
            ESP_UTILS_LOGE("Do handler failed");
        }
    }

    return ret;
}

void Event::unregisterEvent(void *object)
{
    ESP_UTILS_LOGD("Unregister event for object(0x%p)", object);

    auto object_it = _event_handlers.find(object);
    if (object_it == _event_handlers.end()) {
        return;
    }

    // Save event IDs to be removed
    std::unordered_set<ID> event_ids;
    for (auto &id_handlers_pair : object_it->second) {
        event_ids.insert(id_handlers_pair.first);
    }

    // Remove handlers for the given object
    size_t handlers_count = getEventHandlersCount();
    _event_handlers.erase(object_it);
    ESP_UTILS_LOGD("Remove %d event handlers", (int)(handlers_count - getEventHandlersCount() ));

    // Add removed event IDs to available event IDs
    for (const auto &id : event_ids) {
        if (!checkUsedEventID(id)) {
            ESP_UTILS_LOGD("Recycle event ID(%d)", static_cast<int>(id));
            _available_event_ids.insert(id);
        }
    }
}

void Event::unregisterEvent(void *object, ID id)
{
    ESP_UTILS_LOGD("Unregister event for object(0x%p) ID(%d)", object, static_cast<int>(id));

    auto object_it = _event_handlers.find(object);
    if (object_it == _event_handlers.end()) {
        return;
    }

    auto &handlers_for_object = object_it->second;
    size_t handlers_count = getEventHandlersCount();
    if (handlers_for_object.erase(id) == 0) {
        return;
    }
    if (handlers_for_object.empty()) {
        _event_handlers.erase(object_it);
    }
    ESP_UTILS_LOGD("Remove %d event handlers", (int)(handlers_count - getEventHandlersCount() ));

    // Add removed event IDs to available event IDs
    if (!checkUsedEventID(id)) {
        ESP_UTILS_LOGD("Recycle event ID(%d)", static_cast<int>(id));
        _available_event_ids.insert(id);
    }
}

void Event::unregisterEvent(void *object, Handler handler, ID id)
{
    ESP_UTILS_LOGD("Unregister event for object(0x%p) ID(%d) handler(0x%p)", object, static_cast<int>(id), handler);

    auto object_it = _event_handlers.find(object);
    if (object_it == _event_handlers.end()) {
        return;
    }

    auto &handlers_for_object = object_it->second;
    auto handler_it = handlers_for_object.find(id);
    if (handler_it == handlers_for_object.end()) {
        return;
    }

    auto &handlers = handler_it->second;
    auto it = std::remove_if(handlers.begin(), handlers.end(),
    [&](const std::pair<Handler, void *> &pair) {
        return handler == pair.first;
    });
    if (it == handlers.end()) {
        return;
    }

    size_t handlers_count = getEventHandlersCount();
    handlers.erase(it, handlers.end());
    if (handlers.empty()) {
        handlers_for_object.erase(handler_it);
    }
    if (handlers_for_object.empty()) {
        _event_handlers.erase(object_it);
    }
    ESP_UTILS_LOGD("Remove %d event handlers", (int)(handlers_count - getEventHandlersCount() ));

    // Add removed event IDs to available event IDs
    if (!checkUsedEventID(id)) {
        _available_event_ids.insert(id);
    }
}

void Event::unregisterEvent(ID id)
{
    ESP_UTILS_LOGD("Unregister event for ID(%d)", static_cast<int>(id));

    size_t handlers_count = getEventHandlersCount();
    for (auto &object_handler_pair : _event_handlers) {
        auto &handlers_for_object = object_handler_pair.second;
        if (handlers_for_object.find(id) != handlers_for_object.end()) {
            handlers_for_object.erase(id);
        }
    }
    cleanEmptyHandlers();
    ESP_UTILS_LOGD("Remove %d event handlers", (int)(handlers_count - getEventHandlersCount() ));

    // Add removed event IDs to available event IDs
    ESP_UTILS_LOGD("Recycle event ID(%d)", static_cast<int>(id));
    _available_event_ids.insert(id);
}

void Event::unregisterEvent(Handler handler)
{
    ESP_UTILS_LOGD("Unregister event for handler(0x%p)", handler);

    // Save event IDs to be removed
    std::unordered_set<ID> event_ids;
    size_t handlers_count = getEventHandlersCount();
    for (auto &object_handler_pair : _event_handlers) {
        auto &handlers_for_object = object_handler_pair.second;
        for (auto &id_handlers_pair : handlers_for_object) {
            auto &handlers = id_handlers_pair.second;
            auto it = std::remove_if(handlers.begin(), handlers.end(),
            [&](const std::pair<Handler, void *> &pair) {
                return handler == pair.first;
            });
            if (it != handlers.end()) {
                event_ids.insert(id_handlers_pair.first);
                handlers.erase(it, handlers.end());
            }
        }
    }
    cleanEmptyHandlers();
    ESP_UTILS_LOGD("Remove %d event handlers", (int)(handlers_count - getEventHandlersCount() ));

    // Add removed event IDs to available event IDs
    for (const auto &id : event_ids) {
        if (!checkUsedEventID(id)) {
            ESP_UTILS_LOGD("Recycle event ID(%d)", static_cast<int>(id));
            _available_event_ids.insert(id);
        }
    }
}

bool Event::checkUsedEventID(ID id) const
{
    for (auto &object_handler_pair : _event_handlers) {
        auto &handlers_for_object = object_handler_pair.second;
        if (handlers_for_object.find(id) != handlers_for_object.end()) {
            return true;
        }
    }
    return false;
}

Event::ID Event::getFreeEventID()
{
    if (!_available_event_ids.empty()) {
        ID id = *_available_event_ids.begin();
        _available_event_ids.erase(id);

        return id;
    }

    return ++_free_event_id;
}

size_t Event::getEventHandlersCount(void) const
{
    size_t count = 0;
    for (auto &object_handler_pair : _event_handlers) {
        for (auto &id_handlers_pair : object_handler_pair.second) {
            count += id_handlers_pair.second.size();
        }
    }

    return count;
}

void Event::cleanEmptyHandlers()
{
    for (auto it = _event_handlers.begin(); it != _event_handlers.end(); ) {
        auto &id_map = it->second;
        for (auto id_it = id_map.begin(); id_it != id_map.end(); ) {
            if (id_it->second.empty()) {
                id_it = id_map.erase(id_it);
            } else {
                ++id_it;
            }
        }
        if (id_map.empty()) {
            it = _event_handlers.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace esp_brookesia::systems::base
