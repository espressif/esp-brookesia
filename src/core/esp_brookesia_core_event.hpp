/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <memory>

class ESP_Brookesia_CoreEvent {
public:
    enum class ID {
        APP,
        STYLESHEET,
        NAVIGATION,
        CUSTOM,
    };
    struct HandlerData {
        ID id;
        void *object;
        void *param;
        void *user_data;
    };
    using Handler = bool (*)(const HandlerData &data);

    ESP_Brookesia_CoreEvent();
    ~ESP_Brookesia_CoreEvent();

    void reset(void);
    bool registerEvent(void *object, Handler handler, ID id, void *user_data = nullptr);
    bool sendEvent(void *object, ID id, void *param = nullptr) const;
    void unregisterEvent(void *object);
    void unregisterEvent(void *object, ID id);
    void unregisterEvent(void *object, Handler handler, ID id);
    void unregisterEvent(ID id);
    void unregisterEvent(Handler handler);

    ID getFreeEventID();

private:
    using HandlerList = std::vector<std::pair<Handler, void *>>;

    bool checkUsedEventID(ID id) const;
    size_t getEventHandlersCount(void) const;
    void cleanEmptyHandlers();

    ID _free_event_id;
    std::unordered_map<void *, std::unordered_map<ID, HandlerList>> _event_handlers;
    std::unordered_set<ID> _available_event_ids;
};
