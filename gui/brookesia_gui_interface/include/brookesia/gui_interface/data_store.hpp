/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "brookesia/gui_interface/handles.hpp"
#include "brookesia/gui_interface/macro_configs.h"

namespace esp_brookesia::gui {

class IDataStore {
public:
    using SubscriptionId = uint64_t;
    using Listener = std::function<void(std::string_view key, std::string_view value)>;

    IDataStore() = default;
    IDataStore(const IDataStore &) = delete;
    IDataStore &operator=(const IDataStore &) = delete;
    virtual ~IDataStore() = default;

    virtual std::optional<std::string> get_string(std::string_view key) const = 0;
    virtual std::optional<std::string> get_string(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key) const = 0;
    virtual void set_string(std::string_view key, std::string value) = 0;
    virtual void set_string(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key,
        std::string value) = 0;
    virtual SubscriptionId subscribe(std::string_view key, Listener listener) = 0;
    virtual SubscriptionId subscribe(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key,
        Listener listener) = 0;
    virtual void unsubscribe(SubscriptionId id) = 0;

    // Drop every value and signal scoped to `document_id`. Called by Runtime when a
    // document is unloaded so the store does not accumulate stale per-document state.
    // Default implementation is a no-op for backends that do not cache per-document data.
    virtual void forget_document(DocumentId /*document_id*/) {}

    // Debug-introspection accessors used by the runtime's optional [HeapTrace] logs to report how many
    // live connections/signals a store holds. Always present (cheap size reads) so the virtual layout
    // stays ABI-stable across translation units; they are only invoked when memory tracing is enabled.
    virtual std::size_t debug_connection_count() const
    {
        return 0;
    }
    virtual std::size_t debug_signal_count() const
    {
        return 0;
    }
};

class MemoryDataStore final: public IDataStore {
public:
    MemoryDataStore();
    MemoryDataStore(const MemoryDataStore &) = delete;
    MemoryDataStore &operator=(const MemoryDataStore &) = delete;
    ~MemoryDataStore() override;

    std::optional<std::string> get_string(std::string_view key) const override;
    std::optional<std::string> get_string(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key) const override;
    void set_string(std::string_view key, std::string value) override;
    void set_string(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key,
        std::string value) override;
    SubscriptionId subscribe(std::string_view key, Listener listener) override;
    SubscriptionId subscribe(
        DocumentId document_id,
        std::string_view absolute_path,
        std::string_view key,
        Listener listener) override;
    void unsubscribe(SubscriptionId id) override;
    void forget_document(DocumentId document_id) override;

    std::size_t debug_connection_count() const override;
    std::size_t debug_signal_count() const override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace esp_brookesia::gui
