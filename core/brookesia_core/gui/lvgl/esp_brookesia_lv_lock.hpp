/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <atomic>
#include <functional>

namespace esp_brookesia::gui {

class LvLock {
public:
    using LockCallback = std::function<bool(int timeout_ms)>;
    using UnlockCallback = std::function<bool()>;

    bool lock(int timeout_ms = -1);
    bool unlock();

    static LvLock &getInstance();
    static void registerCallbacks(LockCallback lock_cb, UnlockCallback unlock_cb);

private:
    LvLock() = default;
    ~LvLock() = default;
    LvLock(const LvLock &) = delete;
    LvLock &operator=(const LvLock &) = delete;

    LockCallback lock_cb_;
    UnlockCallback unlock_cb_;
    std::atomic<size_t> lock_count_ = 0;
};

class LvLockGuard {
public:
    LvLockGuard();
    ~LvLockGuard();

    LvLockGuard(const LvLockGuard &) = delete;
    LvLockGuard &operator=(const LvLockGuard &) = delete;

private:
    bool locked_ = false;
};

} // namespace esp_brookesia::gui
