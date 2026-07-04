/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

namespace esp_brookesia::system::core {

thread_local const void *current_system_task_owner = nullptr;
thread_local lib_utils::TaskScheduler::Group current_system_task_group;

namespace {

bool is_gui_task_group(std::string_view group)
{
    return (group == SYSTEM_GUI_TASK_GROUP) || (group == SYSTEM_GUI_INPUT_TASK_GROUP);
}

bool is_app_task_group(std::string_view group)
{
    return (group == SYSTEM_APP_TASK_GROUP) || (group == SYSTEM_APP_INPUT_TASK_GROUP);
}

} // namespace

System::Impl::Impl(System &owner)
    : owner_(owner)
{}


bool System::Impl::is_current_task_group(std::string_view group) const
{
    return (current_system_task_owner == this) && (current_system_task_group == group);
}


bool System::Impl::is_current_task_domain(std::string_view group) const
{
    if (current_system_task_owner != this) {
        return false;
    }
    if (is_gui_task_group(group) && is_gui_task_group(current_system_task_group)) {
        return true;
    }
    if (is_app_task_group(group) && is_app_task_group(current_system_task_group)) {
        return true;
    }
    return current_system_task_group == group;
}


lib_utils::TaskScheduler::GroupConfig System::Impl::make_task_group_config(
    const lib_utils::TaskScheduler::Group &group,
    bool use_gui_thread_guard,
    bool use_gui_runtime_gate,
    bool use_app_callback_gate
)
{
    auto pre_execute_callback =
        [this, group, use_gui_thread_guard, use_gui_runtime_gate, use_app_callback_gate](
            const lib_utils::TaskScheduler::Group &,
            lib_utils::TaskScheduler::TaskId,
            lib_utils::TaskScheduler::TaskType
    ) {
        if (use_app_callback_gate) {
            app_callback_mutex_.lock();
        }
        if (use_gui_runtime_gate) {
            gui_runtime_mutex_.lock();
        }
        if (use_gui_thread_guard && gui_thread_guard_.has_value() && gui_thread_guard_->lock) {
            gui_thread_guard_->lock();
        }
        current_system_task_owner = this;
        current_system_task_group = group;
    };
    auto post_execute_callback =
        [this, use_gui_thread_guard, use_gui_runtime_gate, use_app_callback_gate](
            const lib_utils::TaskScheduler::Group &,
            lib_utils::TaskScheduler::TaskId,
            lib_utils::TaskScheduler::TaskType,
            bool
    ) {
        if (use_gui_thread_guard && gui_thread_guard_.has_value() && gui_thread_guard_->unlock) {
            gui_thread_guard_->unlock();
        }
        current_system_task_group.clear();
        current_system_task_owner = nullptr;
        if (use_gui_runtime_gate) {
            gui_runtime_mutex_.unlock();
        }
        if (use_app_callback_gate) {
            app_callback_mutex_.unlock();
        }
    };

    return {
        .enable_serial_execution = true,
        .pre_execute_callback = std::move(pre_execute_callback),
        .post_execute_callback = std::move(post_execute_callback),
    };
}


bool System::Impl::configure_task_groups()
{
    if (!task_scheduler_ || !task_scheduler_->is_running()) {
        return false;
    }
    return task_scheduler_->configure_group(
               SYSTEM_GUI_TASK_GROUP,
               make_task_group_config(SYSTEM_GUI_TASK_GROUP, true, true)
           ) &&
           task_scheduler_->configure_group(
               SYSTEM_GUI_INPUT_TASK_GROUP,
               make_task_group_config(SYSTEM_GUI_INPUT_TASK_GROUP, true, true)
           ) &&
           task_scheduler_->configure_group(
               SYSTEM_APP_TASK_GROUP,
               make_task_group_config(SYSTEM_APP_TASK_GROUP, false, false, true)
           ) &&
           task_scheduler_->configure_group(
               SYSTEM_APP_INPUT_TASK_GROUP,
               make_task_group_config(SYSTEM_APP_INPUT_TASK_GROUP, false, false, true)
           ) &&
           task_scheduler_->configure_group(
               SYSTEM_TIMER_TASK_GROUP,
               make_task_group_config(SYSTEM_TIMER_TASK_GROUP)
           );
}


bool System::Impl::should_schedule_app_task() const
{
    return task_scheduler_ && task_scheduler_->is_running() && !is_current_task_domain(SYSTEM_APP_TASK_GROUP);
}


bool System::Impl::should_defer_gui_task(std::string_view group) const
{
#if defined(__EMSCRIPTEN__)
    return is_gui_task_group(group) && esp_brookesia::gui::wasm::is_gui_task_blocked();
#else
    (void)group;
    return false;
#endif
}


void System::Impl::execute_task_with_group_context(
    const lib_utils::TaskScheduler::Group &group,
    lib_utils::TaskScheduler::OnceTask task
)
{
    if (task == nullptr) {
        return;
    }

    const bool use_gui_guard = is_gui_task_group(group);
    const bool use_app_guard = is_app_task_group(group);
    if (use_app_guard) {
        app_callback_mutex_.lock();
    }
    if (use_gui_guard) {
        gui_runtime_mutex_.lock();
        if (gui_thread_guard_.has_value() && gui_thread_guard_->lock) {
            gui_thread_guard_->lock();
        }
    }

    const auto *previous_task_owner = current_system_task_owner;
    auto previous_task_group = current_system_task_group;
    auto cleanup_guard = lib_utils::FunctionGuard([this, use_gui_guard, use_app_guard]() {
        if (use_gui_guard) {
            if (gui_thread_guard_.has_value() && gui_thread_guard_->unlock) {
                gui_thread_guard_->unlock();
            }
            gui_runtime_mutex_.unlock();
        }
        if (use_app_guard) {
            app_callback_mutex_.unlock();
        }
    });
    auto context_guard = lib_utils::FunctionGuard([previous_task_owner, previous_task_group = std::move(previous_task_group)]() mutable {
        current_system_task_owner = previous_task_owner;
        current_system_task_group = std::move(previous_task_group);
    });

    current_system_task_owner = this;
    current_system_task_group = group;
    task();
}


std::expected<void, std::string> System::Impl::post_task(
    const lib_utils::TaskScheduler::Group &group,
    lib_utils::TaskScheduler::OnceTask task
)
{
    if (is_current_task_domain(group) || !task_scheduler_ || !task_scheduler_->is_running()) {
        task();
        return {};
    }
#if defined(__EMSCRIPTEN__)
    if (esp_brookesia::gui::wasm::is_gui_task_blocked() && (is_gui_task_group(group) || is_app_task_group(group))) {
        auto queued = esp_brookesia::gui::wasm::post_gui_task([this, group, task = std::move(task)]() mutable {
            execute_task_with_group_context(group, std::move(task));
        });
        if (!queued) {
            return std::unexpected("Failed to queue wasm GUI task to group: " + group);
        }
        return {};
    }
    if (group == SYSTEM_APP_INPUT_TASK_GROUP) {
        if (!task_scheduler_->post_delayed(std::move(task), 0, nullptr, group)) {
            return std::unexpected("Failed to post delayed wasm app input task to group: " + group);
        }
        return {};
    }
#endif
    if (!task_scheduler_->post(std::move(task), nullptr, group)) {
        return std::unexpected("Failed to post system task to group: " + group);
    }
    return {};
}

std::expected<void, std::string> System::Impl::post_gui_task(lib_utils::TaskScheduler::OnceTask task)
{
    return post_task(SYSTEM_GUI_TASK_GROUP, std::move(task));
}


std::expected<void, std::string> System::Impl::post_gui_input_task(lib_utils::TaskScheduler::OnceTask task)
{
    return post_task(SYSTEM_GUI_INPUT_TASK_GROUP, std::move(task));
}



} // namespace esp_brookesia::system::core
