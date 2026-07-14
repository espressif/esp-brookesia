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

void System::Impl::cancel_app_timers(AppRecord &record)
{
    if (!task_scheduler_) {
        std::lock_guard<std::recursive_mutex> map_lock(timer_map_mutex_);
        record.timers.clear();
        return;
    }
    {
        std::lock_guard<std::recursive_mutex> map_lock(timer_map_mutex_);
        for (const auto &[timer_id, _] : record.timers) {
            task_scheduler_->cancel(timer_id);
        }
        record.timers.clear();
    }
    const auto app_id = record.info.app_id;
    std::lock_guard lock(timer_mutex_);
    std::erase_if(pending_timers_, [app_id](const PendingTimer & timer) {
        return timer.app_id == app_id;
    });
}


std::expected<void, std::string> System::Impl::dispatch_timer(AppId app_id, TimerId timer_id, std::string name)
{
    auto record_result = get_record(app_id);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    auto &record = *record_result.value();
    {
        std::lock_guard<std::recursive_mutex> map_lock(timer_map_mutex_);
        if (!record.timers.contains(timer_id)) {
            return {};
        }
    }
    if (record.info.state != AppState::Running) {
        return {};
    }
    if (record.info.manifest.kind == AppKind::Native) {
        if (!record.native_app || !record.context) {
            return std::unexpected("Native app is not available");
        }
        auto result = record.native_app->on_timer(*record.context, timer_id, name);
        auto latest_record = get_record(app_id);
        if (latest_record) {
            std::lock_guard<std::recursive_mutex> map_lock(timer_map_mutex_);
            auto it = latest_record.value()->timers.find(timer_id);
            if (it != latest_record.value()->timers.end() && !it->second.periodic) {
                latest_record.value()->timers.erase(it);
            }
        }
        return result;
    }
    auto result = call_runtime_lifecycle(record, LIFECYCLE_ON_TIMER, {
        static_cast<double>(timer_id),
        std::string(name),
    });
    auto latest_record = get_record(app_id);
    if (latest_record) {
        std::lock_guard<std::recursive_mutex> map_lock(timer_map_mutex_);
        auto it = latest_record.value()->timers.find(timer_id);
        if (it != latest_record.value()->timers.end() && !it->second.periodic) {
            latest_record.value()->timers.erase(it);
        }
    }
    return result;
}


std::expected<TimerId, std::string> System::timer_start_periodic(
    AppId app_id,
    std::string_view name,
    int interval_ms
)
{
    auto record_result = impl_->get_record(app_id);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    if (!impl_->task_scheduler_) {
        return std::unexpected("System timer scheduler is not available");
    }
    if (interval_ms <= 0) {
        return std::unexpected("Timer interval must be positive");
    }
    auto &record = *record_result.value();
    auto timer_id_holder = std::make_shared<TimerId>(INVALID_TIMER_ID);
    auto timer_task = [this, app_id, timer_id_holder, name = std::string(name)]() -> bool {
        {
            std::lock_guard lock(impl_->timer_mutex_);
            const bool already_pending = std::any_of(
                impl_->pending_timers_.begin(),
                impl_->pending_timers_.end(),
                [app_id, timer_id_holder](const Impl::PendingTimer & timer)
            {
                return timer.app_id == app_id && timer.timer_id == *timer_id_holder;
            }
            );
            if (already_pending)
            {
                return true;
            }
            impl_->pending_timers_.push_back({
                .app_id = app_id,
                .timer_id = *timer_id_holder,
                .name = name,
            });
        }
        auto post_result = impl_->post_task(
            SYSTEM_APP_TASK_GROUP,
            [this, app_id, timer_id = *timer_id_holder, name]()
        {
            lib_utils::FunctionGuard clear_pending_guard([this, app_id, timer_id]() {
                std::lock_guard lock(impl_->timer_mutex_);
                std::erase_if(impl_->pending_timers_, [app_id, timer_id](const Impl::PendingTimer & timer) {
                    return (timer.app_id == app_id) && (timer.timer_id == timer_id);
                });
            });
            auto result = impl_->dispatch_timer(app_id, timer_id, name);
            if (!result) {
                BROOKESIA_LOGW(
                    "Dispatch app timer failed: app_id(%1%), timer_id(%2%), name(%3%), error(%4%)",
                    app_id, timer_id, name, result.error()
                );
            }
        }
        );
        if (!post_result)
        {
            std::lock_guard lock(impl_->timer_mutex_);
            std::erase_if(impl_->pending_timers_, [app_id, timer_id = *timer_id_holder](const Impl::PendingTimer & timer) {
                return (timer.app_id == app_id) && (timer.timer_id == timer_id);
            });
            return false;
        }
        return true;
    };
    TimerId timer_id = INVALID_TIMER_ID;
    std::lock_guard<std::recursive_mutex> map_lock(impl_->timer_map_mutex_);
    if (!impl_->task_scheduler_->post_periodic(std::move(timer_task), interval_ms, &timer_id, SYSTEM_TIMER_TASK_GROUP)) {
        return std::unexpected("Failed to start periodic timer");
    }
    *timer_id_holder = timer_id;
    record.timers.emplace(timer_id, Impl::AppRecord::TimerRecord{
        .name = std::string(name),
        .periodic = true,
    });
    return timer_id;
}

std::expected<TimerId, std::string> System::timer_start_delayed(
    AppId app_id,
    std::string_view name,
    int delay_ms
)
{
    auto record_result = impl_->get_record(app_id);
    if (!record_result) {
        return std::unexpected(record_result.error());
    }
    if (!impl_->task_scheduler_) {
        return std::unexpected("System timer scheduler is not available");
    }
    if (delay_ms <= 0) {
        return std::unexpected("Timer delay must be positive");
    }
    auto &record = *record_result.value();
    auto timer_id_holder = std::make_shared<TimerId>(INVALID_TIMER_ID);
    auto timer_task = [this, app_id, timer_id_holder, name = std::string(name)]() {
        auto post_result = impl_->post_task(
                               SYSTEM_APP_TASK_GROUP,
        [this, app_id, timer_id = *timer_id_holder, name]() {
            auto result = impl_->dispatch_timer(app_id, timer_id, name);
            if (!result) {
                BROOKESIA_LOGW(
                    "Dispatch delayed app timer failed: app_id(%1%), timer_id(%2%), name(%3%), error(%4%)",
                    app_id, timer_id, name, result.error()
                );
            }
        }
                           );
        if (!post_result) {
            BROOKESIA_LOGW(
                "Failed to post delayed app timer: app_id(%1%), timer_id(%2%), name(%3%), error(%4%)",
                app_id, *timer_id_holder, name, post_result.error()
            );
        }
    };
    TimerId timer_id = INVALID_TIMER_ID;
    std::lock_guard<std::recursive_mutex> map_lock(impl_->timer_map_mutex_);
    if (!impl_->task_scheduler_->post_delayed(std::move(timer_task), delay_ms, &timer_id, SYSTEM_TIMER_TASK_GROUP)) {
        return std::unexpected("Failed to start delayed timer");
    }
    *timer_id_holder = timer_id;
    record.timers.emplace(timer_id, Impl::AppRecord::TimerRecord{
        .name = std::string(name),
        .periodic = false,
    });
    return timer_id;
}

bool System::timer_stop(AppId app_id, TimerId timer_id)
{
    auto record_result = impl_->get_record(app_id);
    if (!record_result || !impl_->task_scheduler_) {
        return false;
    }
    auto &record = *record_result.value();
    {
        std::lock_guard<std::recursive_mutex> map_lock(impl_->timer_map_mutex_);
        auto it = record.timers.find(timer_id);
        if (it == record.timers.end()) {
            return false;
        }
        impl_->task_scheduler_->cancel(timer_id);
        record.timers.erase(it);
    }
    std::lock_guard lock(impl_->timer_mutex_);
    std::erase_if(impl_->pending_timers_, [app_id, timer_id](const Impl::PendingTimer & timer) {
        return (timer.app_id == app_id) && (timer.timer_id == timer_id);
    });
    return true;
}

} // namespace esp_brookesia::system::core
