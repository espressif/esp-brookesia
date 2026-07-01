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

#include <memory>
#include <utility>

#include "brookesia/lib_utils/signal.hpp"
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
    using Signal = esp_brookesia::lib_utils::signal<void(std::string_view, std::string_view)>;

    mutable boost::mutex mutex;
    boost::unordered_flat_map<std::string, std::string> values;
    boost::unordered_flat_map<std::string, std::shared_ptr<Signal>> signals;
    boost::unordered_flat_map<SubscriptionId, esp_brookesia::lib_utils::connection> connections;
    SubscriptionId next_subscription_id = 1;
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

    BROOKESIA_LOGD("Params: key(%1%), value(%2%)", key, value);

    const std::string key_string(key);
    std::string stored_value;
    std::shared_ptr<Impl::Signal> signal;
    {
        boost::lock_guard lock(impl_->mutex);
        impl_->values[key_string] = std::move(value);
        stored_value = impl_->values.at(key_string);

        auto signal_it = impl_->signals.find(key_string);
        if (signal_it != impl_->signals.end()) {
            signal = signal_it->second;
        }
    }

    if (signal != nullptr) {
        (*signal)(key_string, stored_value);
    }
}

void MemoryDataStore::set_string(
    DocumentId document_id,
    std::string_view absolute_path,
    std::string_view key,
    std::string value)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD(
        "Params: document_id(%1%), absolute_path(%2%), key(%3%), value(%4%)",
        document_id,
        absolute_path,
        key,
        value);

    set_string(make_scoped_store_key(document_id, absolute_path, key), std::move(value));
}

IDataStore::SubscriptionId MemoryDataStore::subscribe(std::string_view key, Listener listener)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: key(%1%)", key);

    boost::lock_guard lock(impl_->mutex);
    const SubscriptionId id = impl_->next_subscription_id++;
    auto &signal = impl_->signals[std::string(key)];
    if (signal == nullptr) {
        signal = std::make_shared<Impl::Signal>();
    }
    impl_->connections.emplace(id, signal->connect(listener));
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
    auto it = impl_->connections.find(id);
    if (it == impl_->connections.end()) {
        return;
    }

    it->second.disconnect();
    impl_->connections.erase(it);
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
    for (auto it = impl_->signals.begin(); it != impl_->signals.end(); ) {
        if (it->first.compare(0, prefix.size(), prefix) == 0) {
            it = impl_->signals.erase(it);
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
    return impl_->connections.size();
}

std::size_t MemoryDataStore::debug_signal_count() const
{
    boost::lock_guard lock(impl_->mutex);
    return impl_->signals.size();
}

} // namespace esp_brookesia::gui
