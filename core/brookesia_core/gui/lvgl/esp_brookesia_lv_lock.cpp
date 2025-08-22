/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "esp_brookesia_gui_internal.h"
#if !ESP_BROOKESIA_LVGL_LOCK_ENABLE_DEBUG_LOG
#   define ESP_BROOKESIA_UTILS_DISABLE_DEBUG_LOG
#endif
#include "private/esp_brookesia_lv_utils.hpp"
#include "esp_brookesia_lv_lock.hpp"

namespace esp_brookesia::gui {

LvLock &LvLock::getInstance()
{
    static LvLock s_instance;
    return s_instance;
}

void LvLock::registerCallbacks(LockCallback lock_cb, UnlockCallback unlock_cb)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    auto &inst = getInstance();
    inst.lock_cb_ = std::move(lock_cb);
    inst.unlock_cb_ = std::move(unlock_cb);
}

bool LvLock::lock(int timeout_ms)
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_LOGD("Param: timeout_ms(%d)", timeout_ms);

    ESP_UTILS_CHECK_FALSE_RETURN(lock_cb_.operator bool(), false, "Lock callback not registered");
    ESP_UTILS_CHECK_FALSE_RETURN(lock_cb_(timeout_ms), false, "Lock callback failed");
    lock_count_++;
    ESP_UTILS_LOGD("Locked count: %d", static_cast<int>(lock_count_));

    return true;
}

bool LvLock::unlock()
{
    ESP_UTILS_LOG_TRACE_GUARD();

    ESP_UTILS_CHECK_FALSE_RETURN(unlock_cb_.operator bool(), false, "Unlock callback not registered");
    ESP_UTILS_CHECK_FALSE_RETURN(unlock_cb_(), false, "Unlock callback failed");
    if (lock_count_ > 0) {
        lock_count_--;
    }
    ESP_UTILS_LOGD("Locked count: %d", static_cast<int>(lock_count_));

    return true;
}

LvLockGuard::LvLockGuard()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    locked_ = LvLock::getInstance().lock();
}

LvLockGuard::~LvLockGuard()
{
    ESP_UTILS_LOG_TRACE_GUARD_WITH_THIS();

    if (locked_) {
        LvLock::getInstance().unlock();
    }
}

} // namespace esp_brookesia::gui
