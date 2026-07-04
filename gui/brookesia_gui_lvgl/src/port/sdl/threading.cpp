/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/gui_lvgl/backend.hpp"
#include "port/private/threading.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>

#include "private/utils.hpp"

namespace esp_brookesia::gui::lvgl {

namespace {

std::recursive_mutex &get_lvgl_mutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

std::atomic_bool &get_timer_managed()
{
    static std::atomic_bool managed = false;
    return managed;
}

thread_local uint32_t thread_lock_depth = 0;

} // namespace

void lock_thread()
{
    if (thread_lock_depth++ > 0) {
        return;
    }

    get_lvgl_mutex().lock();
}

void unlock_thread()
{
    if (thread_lock_depth == 0) {
        BROOKESIA_LOGW("LVGL thread unlock requested without a matching lock");
        return;
    }
    if (--thread_lock_depth > 0) {
        return;
    }

    get_lvgl_mutex().unlock();
}

bool is_timer_managed_by_port()
{
    return get_timer_managed().load();
}

void set_timer_managed_by_port(bool managed)
{
    get_timer_managed().store(managed);
}

bool request_refresh_now()
{
    return false;
}

} // namespace esp_brookesia::gui::lvgl
