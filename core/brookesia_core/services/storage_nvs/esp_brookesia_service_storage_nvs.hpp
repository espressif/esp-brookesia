/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <bitset>
#include <queue>
#include <future>
#include "boost/thread.hpp"

namespace esp_brookesia::services {

constexpr size_t NVS_VALUE_STR_MAX_LEN = 128;

class StorageNVS {
public:
    using Key = std::string;
    using Value = std::variant<int, std::string>;

    enum class Operation {
        UpdateNVS,
        UpdateParam,
        EraseNVS,
        Max,
    };

    struct Event {
        void dump() const;

        Operation operation;
        Key key;
    };

    StorageNVS(const StorageNVS &) = delete;
    StorageNVS(StorageNVS &&) = delete;
    ~StorageNVS() = default;

    StorageNVS &operator=(const StorageNVS &) = delete;
    StorageNVS &operator=(StorageNVS &&) = delete;

    bool begin();

    bool sendEvent(const Event &event, int wait_finish_timeout_ms = 0);

    bool setLocalParam(const Key &key, const Value &value, std::optional<int> wait_finish_timeout_ms = 0);
    bool getLocalParam(const Key &key, Value &value);
    bool eraseNVS(std::optional<int> wait_finish_timeout_ms = 0);

    static StorageNVS &requestInstance()
    {
        static StorageNVS instance;
        return instance;
    }

private:
    using EventPromise = std::promise<bool>;
    struct EventWrapper {
        Event event;
        std::shared_ptr<EventPromise> promise;
    };

    StorageNVS() = default;

    bool processEvent(const Event &event);
    bool doEventOperationUpdateNVS(const Key &key);
    bool doEventOperationUpdateParam();
    bool doEventOperationEraseNVS();

    std::map<Key, Value> _local_params;
    std::mutex _params_mutex;

    std::queue<EventWrapper> _event_queue;
    std::mutex _event_mutex;
    std::condition_variable _event_cv;
    boost::thread _event_thread;
};

} // namespace esp_brookesia::service
