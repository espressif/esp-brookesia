/*
 * SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/lib_utils/state_base.hpp"
#include "brookesia/lib_utils/state_machine.hpp"
#include "brookesia/service_helper/wifi.hpp"
#include "brookesia/service_wifi/hal.hpp"

namespace esp_brookesia::service::wifi {

enum class GeneralState {
    Deinited,
    Inited,
    Started,
    Connected,
    Max,
};

class StateMachine  {
public:
    StateMachine(
        std::shared_ptr<lib_utils::TaskScheduler> task_scheduler, std::shared_ptr<Hal> hal
    )
        : hal_(hal)
        , task_scheduler_(task_scheduler)
    {}
    ~StateMachine();

    bool init();
    void deinit();
    bool start();
    void stop();

    bool is_inited()
    {
        return state_machine_ != nullptr;
    }
    bool is_running()
    {
        return (state_machine_ != nullptr) && state_machine_->is_running();
    }

    bool trigger_general_action(GeneralAction action, bool use_dispatch = false);
    bool force_transition_to(const std::string &state);

private:
    std::shared_ptr<Hal> hal_;
    std::shared_ptr<lib_utils::TaskScheduler> task_scheduler_;
    std::unique_ptr<lib_utils::StateMachine> state_machine_;
};

BROOKESIA_DESCRIBE_ENUM(GeneralState, Deinited, Inited, Started, Connected, Max);

} // namespace esp_brookesia::service::wifi
