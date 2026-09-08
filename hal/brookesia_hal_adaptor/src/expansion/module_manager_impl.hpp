/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <mutex>
#include <set>

#include "brookesia/hal_interface/interfaces/expansion/module_manager.hpp"

namespace esp_brookesia::hal::expansion {

class ModuleManagerImpl final: public ModuleManagerIface {
public:
    ModuleManagerImpl() = default;
    ~ModuleManagerImpl() override;

    std::vector<ModuleInfo> get_module_infos() const override;
    EventListenerId add_event_listener(EventListener listener) override;
    bool remove_event_listener(EventListenerId id) override;

private:
    mutable std::mutex listener_mutex_;
    std::set<EventListenerId> listener_ids_;
};

} // namespace esp_brookesia::hal::expansion
