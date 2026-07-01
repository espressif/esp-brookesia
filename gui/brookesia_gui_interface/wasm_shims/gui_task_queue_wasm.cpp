/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "brookesia/gui_interface/wasm/gui_task_queue.hpp"

#include <deque>
#include <exception>
#include <mutex>
#include <utility>

#include "brookesia/lib_utils/log.hpp"

namespace esp_brookesia::gui::wasm {
namespace {

std::mutex &get_queue_mutex()
{
    static std::mutex mutex;
    return mutex;
}

std::deque<GuiTask> &get_queue()
{
    static std::deque<GuiTask> queue;
    return queue;
}

bool &get_blocked_flag()
{
    static bool blocked = false;
    return blocked;
}

bool &get_draining_flag()
{
    static bool draining = false;
    return draining;
}

} // namespace

bool post_gui_task(GuiTask task)
{
    if (!task) {
        return false;
    }

    std::lock_guard lock(get_queue_mutex());
    get_queue().push_back(std::move(task));
    return true;
}

void drain_gui_tasks(size_t max_task_count)
{
    if (max_task_count == 0 || is_gui_task_blocked() || is_gui_task_draining()) {
        return;
    }

    get_draining_flag() = true;
    size_t drained_count = 0;
    while (drained_count < max_task_count) {
        GuiTask task;
        {
            std::lock_guard lock(get_queue_mutex());
            if (get_queue().empty()) {
                break;
            }
            task = std::move(get_queue().front());
            get_queue().pop_front();
        }

        try {
            task();
        } catch (const std::exception &e) {
            BROOKESIA_LOGE("Wasm GUI task failed: %1%", e.what());
        } catch (...) {
            BROOKESIA_LOGE("Wasm GUI task failed with unknown exception");
        }
        ++drained_count;
    }
    get_draining_flag() = false;

    if (drained_count >= max_task_count) {
        BROOKESIA_LOGW("Wasm GUI task queue reached drain limit: %1%", max_task_count);
    }
}

void set_gui_task_blocked(bool blocked)
{
    get_blocked_flag() = blocked;
}

bool is_gui_task_blocked()
{
    return get_blocked_flag();
}

bool is_gui_task_draining()
{
    return get_draining_flag();
}

} // namespace esp_brookesia::gui::wasm
