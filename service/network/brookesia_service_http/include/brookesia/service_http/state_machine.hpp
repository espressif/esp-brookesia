/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <functional>
#include <memory>
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/service_helper/network/http.hpp"

namespace esp_brookesia::service::http {

using GeneralState = helper::Http::GeneralState;

class StateMachine {
public:
    using TimeSyncChecker = std::function<bool()>;
    using StateChangedCallback = std::function<void(GeneralState)>;

    StateMachine() = default;
    ~StateMachine() noexcept;

    bool init();
    void deinit();
    bool start(std::shared_ptr<lib_utils::TaskScheduler> scheduler, const lib_utils::TaskScheduler::Group &group);
    void stop();

    bool is_inited() const
    {
        return state_machine_ != nullptr;
    }
    bool is_running() const
    {
        return (state_machine_ != nullptr) && state_machine_->is_running();
    }

    void set_time_sync_checker(TimeSyncChecker checker);
    void set_state_changed_callback(StateChangedCallback callback);
    bool check_if_time_synced() const;
    bool complete_time_sync();
    bool trigger_state(GeneralState state);
    GeneralState get_current_state() const;

private:
    std::unique_ptr<lib_utils::StateMachine> state_machine_;
    TimeSyncChecker time_sync_checker_;
    StateChangedCallback state_changed_callback_;
};

} // namespace esp_brookesia::service::http
