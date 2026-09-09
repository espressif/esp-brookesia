/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Serialize Brookesia-managed hardware lifecycle transactions. Recursive on the calling thread. */
void brookesia_hal_lifecycle_lock(void);
/** Release one acquisition of the lifecycle lock on the calling thread. */
void brookesia_hal_lifecycle_unlock(void);

#ifdef __cplusplus
}

namespace esp_brookesia::hal::detail {

/**
 * @brief Guard a Brookesia-managed hardware lifecycle transaction.
 *
 * Acquire runtime references before this guard and release them after it. Never
 * wait for user callbacks or a scanning task that requires this lock while holding this guard. Direct
 * calls into third-party managers outside Brookesia are not serialized by it.
 */
class LifecycleGuard {
public:
    LifecycleGuard()
    {
        brookesia_hal_lifecycle_lock();
    }
    ~LifecycleGuard()
    {
        brookesia_hal_lifecycle_unlock();
    }
    LifecycleGuard(const LifecycleGuard &) = delete;
    LifecycleGuard &operator=(const LifecycleGuard &) = delete;
};

} // namespace esp_brookesia::hal::detail
#endif
