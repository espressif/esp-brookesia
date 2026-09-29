/*
 * SPDX-FileCopyrightText: 2025-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#if defined(ESP_PLATFORM)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <condition_variable>
#include <thread>
#endif

namespace esp_brookesia::lib_utils {

/*
 * A small signal and slot helper.
 *
 * disconnect() destroys the callback after it has let go of the list lock.
 * A callback can disconnect another slot while it is being destroyed.
 *
 * A caller that is not already inside a callback waits until a callback
 * running on another thread returns. A caller that is already inside a
 * callback does not wait, so two callbacks can cancel each other safely.
 */

namespace signal_detail {

// On the device, a normal FreeRTOS task has no pthread id. Asking for one
// aborts, so the device uses the task handle. The PC uses the C++ thread id.
#if defined(ESP_PLATFORM)
using ExecutionId = TaskHandle_t;
inline ExecutionId current_execution_id()
{
    return xTaskGetCurrentTaskHandle();
}
#else
using ExecutionId = std::thread::id;
inline ExecutionId current_execution_id()
{
    return std::this_thread::get_id();
}
#endif

inline int &emission_depth()
{
    thread_local int depth = 0;
    return depth;
}

struct EmissionGuard {
    EmissionGuard()
    {
        ++emission_depth();
    }

    ~EmissionGuard()
    {
        --emission_depth();
    }

    EmissionGuard(const EmissionGuard &) = delete;
    EmissionGuard &operator=(const EmissionGuard &) = delete;
};

inline bool inside_signal_emission()
{
    return emission_depth() > 0;
}

struct SlotControl {
    mutable std::mutex mutex;
#if !defined(ESP_PLATFORM)
    std::condition_variable idle;
#endif
    bool connected = true;
    int inflight = 0;
    std::vector<ExecutionId> runners;
    std::function<void()> detach_fn;

    bool running_on_this_thread_locked() const
    {
        const auto current = current_execution_id();
        return std::find(runners.begin(), runners.end(), current) != runners.end();
    }

    void wait_until_idle(std::unique_lock<std::mutex> &lock)
    {
#if defined(ESP_PLATFORM)
        while (inflight > 0) {
            lock.unlock();
            vTaskDelay(1);
            lock.lock();
        }
#else
        idle.wait(lock, [this]() {
            return inflight == 0;
        });
#endif
    }

    void notify_idle()
    {
#if !defined(ESP_PLATFORM)
        idle.notify_all();
#endif
    }

    bool enter()
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!connected) {
            return false;
        }
        ++inflight;
        runners.push_back(current_execution_id());
        return true;
    }

    void leave()
    {
        std::function<void()> detach;
        {
            std::lock_guard<std::mutex> lock(mutex);
            const auto current = current_execution_id();
            const auto runner = std::find(runners.begin(), runners.end(), current);
            if (runner != runners.end()) {
                runners.erase(runner);
            }
            if (inflight > 0) {
                --inflight;
            }
            if ((inflight == 0) && !connected) {
                detach = std::move(detach_fn);
            }
            if (inflight == 0) {
                notify_idle();
            }
        }
        if (detach) {
            detach();
        }
    }

    void disconnect() noexcept
    {
        std::function<void()> detach;
        try {
            std::unique_lock<std::mutex> lock(mutex);
            connected = false;
            if (inflight == 0) {
                detach = std::move(detach_fn);
            } else if (running_on_this_thread_locked() || inside_signal_emission()) {
                // Already inside a callback: do not wait. Two callbacks that
                // cancel each other would otherwise wait forever.
                return;
            } else {
                wait_until_idle(lock);
                detach = std::move(detach_fn);
            }
        } catch (...) {
            return;
        }
        if (!detach) {
            return;
        }
        try {
            detach();
        } catch (...) {
        }
    }

    bool is_connected() const
    {
        std::lock_guard<std::mutex> lock(mutex);
        return connected;
    }
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
        return control && control->is_connected();
    }

    /**
     * @brief Drop this subscription.
     *
     * The callback is destroyed after the list lock is released.
     * Outside a callback, this waits for a call that is still running
     * on another thread. Inside a callback, it does not wait.
     */
    void disconnect() const noexcept
    {
        if (auto control = control_.lock()) {
            control->disconnect();
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

    signal() : list_(std::make_shared<SlotList>()) {}
    signal(const signal &) = delete;
    signal &operator=(const signal &) = delete;

    // Non-copyable but movable, matching boost::signals2::signal. Existing connections
    // stay valid across a move because they reference the per-slot control block, which
    // travels with the slot entries.
    signal(signal &&other) noexcept
        : list_(std::make_shared<SlotList>())
    {
        std::lock_guard<std::mutex> lock(other.list_->mutex);
        list_ = std::move(other.list_);
        other.list_ = std::make_shared<SlotList>();
    }

    signal &operator=(signal &&other) noexcept
    {
        if (this != &other) {
            disconnect_all_slots();
            std::lock_guard<std::mutex> lock(other.list_->mutex);
            list_ = std::move(other.list_);
            other.list_ = std::make_shared<SlotList>();
        }
        return *this;
    }

    ~signal()
    {
        disconnect_all_slots();
    }

    /**
     * @brief Register a slot and return a handle controlling its lifetime.
     */
    connection connect(slot_type slot)
    {
        auto control = std::make_shared<signal_detail::SlotControl>();
        std::weak_ptr<SlotList> list = list_;
        signal_detail::SlotControl *control_ptr = control.get();
        control->detach_fn = [list, control_ptr]() {
            std::shared_ptr<Slot> removed;
            if (const auto locked = list.lock()) {
                std::lock_guard<std::mutex> guard(locked->mutex);
                for (auto it = locked->slots.begin(); it != locked->slots.end(); ++it) {
                    if ((*it)->control.get() == control_ptr) {
                        removed = std::move(*it);
                        locked->slots.erase(it);
                        break;
                    }
                }
            }
            removed.reset();
        };
        auto entry = std::make_shared<Slot>(std::move(control), std::move(slot));
        {
            std::lock_guard<std::mutex> lock(list_->mutex);
            list_->slots.push_back(entry);
        }
        return connection{entry->control};
    }

    /**
     * @brief Call every connected slot.
     *
     * The slot list is copied first. A slot may connect or disconnect
     * during the call, including itself.
     */
    void operator()(Args... args) const
    {
        [[maybe_unused]] const signal_detail::EmissionGuard emission_guard;
        struct LeaveGuard {
            signal_detail::SlotControl *control = nullptr;
            ~LeaveGuard()
            {
                control->leave();
            }
        };

        const auto list = list_;
        std::vector<std::shared_ptr<Slot>> snapshot;
        {
            std::lock_guard<std::mutex> lock(list->mutex);
            snapshot = list->slots;
        }

        for (const auto &entry : snapshot) {
            if (!entry->control->enter()) {
                continue;
            }
            LeaveGuard guard{entry->control.get()};
            entry->fn(args...);
        }
    }

    /**
     * @brief Disconnect every slot currently attached to this signal.
     */
    void disconnect_all_slots()
    {
        std::vector<std::shared_ptr<Slot>> taken;
        {
            std::lock_guard<std::mutex> lock(list_->mutex);
            taken.swap(list_->slots);
        }
        for (const auto &entry : taken) {
            entry->control->disconnect();
        }
    }

    /**
     * @brief Number of currently connected slots.
     */
    std::size_t num_slots() const
    {
        std::lock_guard<std::mutex> lock(list_->mutex);
        std::size_t count = 0;
        for (const auto &entry : list_->slots) {
            if (entry->control->is_connected()) {
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
        Slot(std::shared_ptr<signal_detail::SlotControl> control_in, slot_type fn_in)
            : control(std::move(control_in)), fn(std::move(fn_in))
        {
        }

        // GCC 16 may misdiagnose std::function destruction as array-bounds
        // when this destructor is inlined through shared_ptr's control block.
        [[gnu::noinline]] ~Slot() = default;

        std::shared_ptr<signal_detail::SlotControl> control;
        slot_type fn;
    };

    struct SlotList {
        mutable std::mutex mutex;
        std::vector<std::shared_ptr<Slot>> slots;
    };

    std::shared_ptr<SlotList> list_;
};

} // namespace esp_brookesia::lib_utils
