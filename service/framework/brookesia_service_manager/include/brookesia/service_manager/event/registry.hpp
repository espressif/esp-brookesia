/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <map>
#include <functional>
#include <memory>
#include <string>
#include "boost/json.hpp"
#include "brookesia/lib_utils/signal.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/thread/lock_guard.hpp"
#include "brookesia/service_manager/event/definition.hpp"

namespace esp_brookesia::service {

/**
 * @brief Registry of event schemas and local subscriptions for one service.
 */
class EventRegistry {
public:
    /**
     * @brief Signal type used for local in-process event listeners.
     */
    using Signal = esp_brookesia::lib_utils::signal < void(const std::string &event_name, const EventItemMap &event_items) >;
    /**
     * @brief RAII connection handle returned by `Signal::connect()`.
     */
    using SignalConnection = esp_brookesia::lib_utils::scoped_connection;
    /**
     * @brief Slot type accepted by `subscribe_event()`.
     */
    using SignalSlot = Signal::slot_type;

    EventRegistry() = default;
    ~EventRegistry() = default;

    /**
     * @brief Register an event schema.
     *
     * @param[in] event_schema Schema to add.
     * @return true on success, false if an event with the same name already exists.
     */
    bool add(EventSchema event_schema);
    /**
     * @brief Remove a registered event schema.
     *
     * @param[in] event_name Name of the event to remove.
     */
    void remove(const std::string &event_name);
    /**
     * @brief Remove all registered event schemas.
     */
    void remove_all();

    /**
     * @brief Validate an event payload against a registered schema.
     *
     * @param[in] event_name Event name to validate against.
     * @param[in] event_items Event payload to validate.
     * @return true if the payload conforms to the schema, false otherwise.
     */
    bool validate_items(const std::string &event_name, const EventItemMap &event_items);

    /**
     * @brief Look up the schema for a registered event.
     *
     * @param[in] event_name Event name to query.
     * @return const EventSchema* Pointer to the schema, or `nullptr` if not found.
     */
    const EventSchema *get_schema(const std::string &event_name)
    {
        boost::lock_guard lock(event_infos_mutex_);
        auto it = event_infos_.find(event_name);
        if (it == event_infos_.end()) {
            return nullptr;
        }
        return &std::get<0>(it->second);
    }
    /**
     * @brief Get a snapshot of all registered event schemas.
     *
     * @return std::vector<EventSchema> Copy of the registered schemas.
     */
    std::vector<EventSchema> get_schemas() const;
    /**
     * @brief Export all registered event schemas as JSON.
     *
     * @return boost::json::array JSON representation of every schema.
     */
    boost::json::array get_schemas_json();
    /**
     * @brief Check whether an event schema exists.
     *
     * @param[in] event_name Event name to check.
     * @return true if the event is registered.
     */
    bool has(const std::string &event_name)
    {
        boost::lock_guard lock(event_infos_mutex_);
        return event_infos_.find(event_name) != event_infos_.end();
    }
    /**
     * @brief Get the number of registered event schemas.
     *
     * @return size_t Count of registered events.
     */
    size_t get_count()
    {
        boost::lock_guard lock(event_infos_mutex_);
        return event_infos_.size();
    }

    /**
     * @brief Get the in-process signal associated with an event.
     *
     * @param[in] event_name Event name to query.
     * @return Signal* Pointer to the signal, or `nullptr` if the event does not exist.
     */
    Signal *get_signal(const std::string &event_name);

private:
    using EventInfo = std::tuple<EventSchema, std::unique_ptr<Signal>>;

    mutable boost::mutex event_infos_mutex_;
    std::map<std::string /*name*/, EventInfo> event_infos_;
};

} // namespace esp_brookesia::service
