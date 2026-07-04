/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace esp_brookesia::lib_utils {

/*
 * Minimal, thread-safe signal/slot facility covering the subset of the
 * `boost::signals2` API that this code base actually uses: void-returning
 * signals, `connect()` returning a `connection`, `scoped_connection` RAII,
 * `connected()`/`disconnect()` and reentrancy-safe emission. It deliberately
 * omits the heavyweight features (return-value combiners, automatic `track()`
 * lifetime management, shared connection blocks) that are unused here, which is
 * what makes it dramatically smaller than `boost::signals2`.
 */

namespace signal_detail {

struct SlotControl {
    std::atomic<bool> connected{true};
};

} // namespace signal_detail

/**
 * @brief Lightweight handle to a single signal/slot connection.
 *
 * Copyable; all copies refer to the same underlying connection state.
 */
class connection {
public:
    connection() noexcept = default;
    explicit connection(std::shared_ptr<signal_detail::SlotControl> control) noexcept
        : control_(std::move(control)) {}

    bool connected() const noexcept
    {
        auto control = control_.lock();
        return control && control->connected.load(std::memory_order_acquire);
    }

    void disconnect() const noexcept
    {
        if (auto control = control_.lock()) {
            control->connected.store(false, std::memory_order_release);
        }
    }

private:
    std::weak_ptr<signal_detail::SlotControl> control_;
};

/**
 * @brief RAII connection that disconnects on destruction.
 *
 * Movable but not copyable, mirroring `boost::signals2::scoped_connection`.
 */
class scoped_connection {
public:
    scoped_connection() noexcept = default;
    scoped_connection(const connection &conn) noexcept : conn_(conn) {}      // NOLINT(google-explicit-constructor)
    scoped_connection(connection &&conn) noexcept : conn_(std::move(conn)) {} // NOLINT(google-explicit-constructor)

    scoped_connection(const scoped_connection &) = delete;
    scoped_connection &operator=(const scoped_connection &) = delete;

    scoped_connection(scoped_connection &&other) noexcept : conn_(other.conn_)
    {
        other.conn_ = connection{};
    }

    scoped_connection &operator=(scoped_connection &&other) noexcept
    {
        if (this != &other) {
            conn_.disconnect();
            conn_ = other.conn_;
            other.conn_ = connection{};
        }
        return *this;
    }

    scoped_connection &operator=(const connection &conn) noexcept
    {
        conn_.disconnect();
        conn_ = conn;
        return *this;
    }

    ~scoped_connection()
    {
        conn_.disconnect();
    }

    bool connected() const noexcept
    {
        return conn_.connected();
    }

    void disconnect() const noexcept
    {
        conn_.disconnect();
    }

    connection release() noexcept
    {
        connection released = conn_;
        conn_ = connection{};
        return released;
    }

private:
    connection conn_;
};

template <typename Signature>
class signal;

/**
 * @brief Thread-safe multicast signal for void-returning slots.
 *
 * @tparam Args Slot parameter types.
 */
template <typename... Args>
class signal<void(Args...)> {
public:
    using slot_type = std::function<void(Args...)>;

    signal() = default;
    signal(const signal &) = delete;
    signal &operator=(const signal &) = delete;

    // Non-copyable but movable, matching boost::signals2::signal. Existing connections
    // stay valid across a move because they reference the per-slot control block, which
    // travels with the slot entries.
    signal(signal &&other) noexcept
    {
        std::lock_guard<std::mutex> lock(other.mutex_);
        slots_ = std::move(other.slots_);
    }

    signal &operator=(signal &&other) noexcept
    {
        if (this != &other) {
            std::scoped_lock lock(mutex_, other.mutex_);
            slots_ = std::move(other.slots_);
        }
        return *this;
    }

    /**
     * @brief Register a slot and return a handle controlling its lifetime.
     */
    connection connect(slot_type slot)
    {
        auto control = std::make_shared<signal_detail::SlotControl>();
        auto entry = std::make_shared<Slot>(Slot{control, std::move(slot)});
        std::lock_guard<std::mutex> lock(mutex_);
        slots_.push_back(std::move(entry));
        return connection{control};
    }

    /**
     * @brief Invoke every connected slot.
     *
     * A snapshot of the slot list is taken under the lock so slots may safely
     * connect/disconnect (including themselves) during emission, and so
     * emission is safe against concurrent mutation from other threads.
     */
    void operator()(Args... args) const
    {
        std::vector<std::shared_ptr<Slot>> snapshot;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            snapshot = slots_;
        }

        bool stale = false;
        for (const auto &entry : snapshot) {
            if (entry->control->connected.load(std::memory_order_acquire)) {
                entry->fn(args...);
            } else {
                stale = true;
            }
        }

        if (stale) {
            prune();
        }
    }

    /**
     * @brief Disconnect every slot currently attached to this signal.
     */
    void disconnect_all_slots()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &entry : slots_) {
            entry->control->connected.store(false, std::memory_order_release);
        }
        slots_.clear();
    }

    /**
     * @brief Number of currently connected slots.
     */
    std::size_t num_slots() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::size_t count = 0;
        for (const auto &entry : slots_) {
            if (entry->control->connected.load(std::memory_order_acquire)) {
                ++count;
            }
        }
        return count;
    }

    bool empty() const
    {
        return num_slots() == 0;
    }

private:
    struct Slot {
        std::shared_ptr<signal_detail::SlotControl> control;
        slot_type fn;
    };

    void prune() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto it = slots_.begin(); it != slots_.end();) {
            if ((*it)->control->connected.load(std::memory_order_acquire)) {
                ++it;
            } else {
                it = slots_.erase(it);
            }
        }
    }

    mutable std::mutex mutex_;
    mutable std::vector<std::shared_ptr<Slot>> slots_;
};

} // namespace esp_brookesia::lib_utils
