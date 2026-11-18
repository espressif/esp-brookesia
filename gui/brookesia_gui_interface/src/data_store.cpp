/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/gui_interface/data_store.hpp"
#include "brookesia/gui_interface/macro_configs.h"
#if !BROOKESIA_GUI_INTERFACE_DATA_STORE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "boost/unordered/unordered_flat_map.hpp"

namespace esp_brookesia::gui {

namespace {

std::string make_scoped_store_key(DocumentId document_id, std::string_view absolute_path, std::string_view key)
{
    return "doc:" + std::to_string(document_id.value()) + "|path:" + std::string(absolute_path) +
           "|key:" + std::string(key);
}

} // namespace

class MemoryDataStore::Impl {
public:
    struct ListenerControl {
        explicit ListenerControl(Listener callback)
            : listener(std::move(callback))
        {
        }

        std::atomic_bool active {true};
        Listener listener;
    };

    struct ListenerRecord {
        SubscriptionId id = 0;
        std::shared_ptr<ListenerControl> control;
    };

    mutable boost::mutex mutex;
    // Runtime bindings overwhelmingly retain only a value. Keep listeners in a sparse side table so
    // value-only entries do not pay for an empty vector/optional, while still avoiding the former
    // per-key generic signal, mutex, slot graph, and global connection map.
    boost::unordered_flat_map<std::string, std::string> values;
    boost::unordered_flat_map<std::string, std::vector<ListenerRecord>> listener_buckets;
    SubscriptionId next_subscription_id = 1;
    std::size_t listener_count = 0;
};

MemoryDataStore::MemoryDataStore()
    : impl_(std::make_unique<Impl>())
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
}

MemoryDataStore::~MemoryDataStore() = default;

std::optional<std::string> MemoryDataStore::get_string(std::string_view key) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: key(%1%)", key);

    boost::lock_guard lock(impl_->mutex);
    auto it = impl_->values.find(std::string(key));
    if (it == impl_->values.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::optional<std::string> MemoryDataStore::get_string(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key) const
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: document_id(%1%), absolute_path(%2%), key(%3%)", document_id, absolute_path, key);

    return get_string(make_scoped_store_key(document_id, absolute_path, key));
}

void MemoryDataStore::set_string(std::string_view key, std::string value)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: key(%1%)", key);

    const std::string expected_value = value;
    set_string_silent(key, std::move(value));
    notify_string_if_value(key, expected_value);
}

void MemoryDataStore::set_string_silent(std::string_view key, std::string value)
{
    boost::lock_guard lock(impl_->mutex);
    impl_->values[std::string(key)] = std::move(value);
}

void MemoryDataStore::set_string_silent(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value)
{
    set_string_silent(make_scoped_store_key(document_id, absolute_path, key), std::move(value));
}

void MemoryDataStore::notify_string_if_value(std::string_view key, std::string_view expected_value)
{
    const std::string key_string(key);
    std::string stored_value;
    std::vector<std::shared_ptr<Impl::ListenerControl>> listeners;
    {
        boost::lock_guard lock(impl_->mutex);
        auto value_it = impl_->values.find(key_string);
        if (value_it == impl_->values.end() || value_it->second != expected_value) {
            return;
        }
        stored_value = value_it->second;
        auto bucket_it = impl_->listener_buckets.find(key_string);
        if (bucket_it != impl_->listener_buckets.end()) {
            listeners.reserve(bucket_it->second.size());
            for (const auto &record : bucket_it->second) {
                listeners.push_back(record.control);
            }
        }
    }

    // Controls are shared with the snapshot so unsubscribe/forget_document may invalidate pending
    // callbacks without holding the store mutex while user code executes.
    for (const auto &control : listeners) {
        {
            boost::lock_guard lock(impl_->mutex);
            auto value_it = impl_->values.find(key_string);
            if (value_it == impl_->values.end() || value_it->second != expected_value) {
                break;
            }
        }
        if (control->active.load(std::memory_order_acquire)) {
            control->listener(key_string, stored_value);
        }
    }
}

void MemoryDataStore::notify_string_if_value(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string_view expected_value)
{
    notify_string_if_value(make_scoped_store_key(document_id, absolute_path, key), expected_value);
}

void MemoryDataStore::set_string(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: document_id(%1%), absolute_path(%2%), key(%3%)",
        document_id,
        absolute_path,
        key);

    set_string(make_scoped_store_key(document_id, absolute_path, key), std::move(value));
}

IDataStore::SubscriptionId MemoryDataStore::subscribe(std::string_view key, Listener listener)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: key(%1%)", key);

    boost::lock_guard lock(impl_->mutex);
    const SubscriptionId id = impl_->next_subscription_id++;
    auto &listeners = impl_->listener_buckets[std::string(key)];
    listeners.push_back(Impl::ListenerRecord{
        .id = id,
        .control = std::make_shared<Impl::ListenerControl>(std::move(listener)),
    });
    ++impl_->listener_count;
    return id;
}

IDataStore::SubscriptionId MemoryDataStore::subscribe(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key,
    Listener listener)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: document_id(%1%), absolute_path(%2%), key(%3%)", document_id, absolute_path, key);

    return subscribe(make_scoped_store_key(document_id, absolute_path, key), std::move(listener));
}

void MemoryDataStore::unsubscribe(SubscriptionId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: id(%1%)", id);

    boost::lock_guard lock(impl_->mutex);
    for (auto bucket_it = impl_->listener_buckets.begin(); bucket_it != impl_->listener_buckets.end(); ++bucket_it) {
        auto &listeners = bucket_it->second;
        auto matches_id = [id](const Impl::ListenerRecord & record) {
            return record.id == id;
        };
        auto listener_it = std::find_if(listeners.begin(), listeners.end(), matches_id);
        if (listener_it == listeners.end()) {
            continue;
        }

        listener_it->control->active.store(false, std::memory_order_release);
        listeners.erase(listener_it);
        --impl_->listener_count;
        if (listeners.empty()) {
            impl_->listener_buckets.erase(bucket_it);
        }
        return;
    }
}

void MemoryDataStore::forget_document(DocumentId document_id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: document_id(%1%)", document_id);

    const std::string prefix = "doc:" + std::to_string(document_id.value()) + "|";
    boost::lock_guard lock(impl_->mutex);
    for (auto it = impl_->values.begin(); it != impl_->values.end(); ) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) {
            it = impl_->values.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = impl_->listener_buckets.begin(); it != impl_->listener_buckets.end(); ) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) {
            for (const auto &record : it->second) {
                record.control->active.store(false, std::memory_order_release);
            }
            impl_->listener_count -= it->second.size();
            it = impl_->listener_buckets.erase(it);
        } else {
            ++it;
        }
    }
}

// Debug-introspection accessors (see IDataStore): report live connection/signal counts for the
// runtime's optional [HeapTrace] memory logs. Cheap size reads, safe to keep always compiled.
std::size_t MemoryDataStore::debug_connection_count() const
{
    boost::lock_guard lock(impl_->mutex);
    return impl_->listener_count;
}

std::size_t MemoryDataStore::debug_signal_count() const
{
    boost::lock_guard lock(impl_->mutex);
    return impl_->listener_buckets.size();
}

} // namespace esp_brookesia::gui
