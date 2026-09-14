/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <algorithm>
#include <map>
#include <optional>
#include <utility>
#include <vector>

#include "boost/thread/lock_guard.hpp"
#include "boost/thread/mutex.hpp"
#include "brookesia/hal_interface/interfaces/expansion/module_manager.hpp"

namespace esp_brookesia::system::super {

// Shared inbox: service callbacks never retain the Shell or access its GUI.
class ExpansionNotifications {
public:
    struct Notification {
        hal::expansion::ModuleInfo module;
        bool removed = false;
    };

    void update(const hal::expansion::ModuleInfo &module)
    {
        using ModuleState = hal::expansion::ModuleState;
        if (module.provider.empty() || module.slot.empty()) {
            return;
        }

        boost::lock_guard lock(mutex_);
        auto &slot = slots_[ {module.provider, module.slot}];
        // A queried snapshot can overtake an already queued event.
        if (slot.generation.has_value() && module.generation <= *slot.generation) {
            return;
        }
        slot.generation = module.generation;

        std::optional<Notification> notification;
        if (module.state == ModuleState::Empty) {
            if (slot.identified.has_value()) {
                notification = Notification{*slot.identified, true};
                slot.identified.reset();
            }
        } else if (module.state == ModuleState::Ready || module.state == ModuleState::Active ||
                   module.state == ModuleState::Unsupported) {
            if (!slot.identified.has_value() || slot.identified->board_id != module.board_id ||
                    slot.identified->board_name != module.board_name || slot.identified->type != module.type) {
                notification = Notification{module, false};
            }
            slot.identified = module;
        }
        // Camera release temporarily reports Unknown; errors do not prove removal either.
        if (!notification.has_value()) {
            return;
        }

        // Keep only the latest notice per slot while the app task is busy.
        std::erase_if(pending_, [&module](const Notification & item) {
            return item.module.provider == module.provider && item.module.slot == module.slot;
        });
        pending_.push_back(std::move(*notification));
    }

    std::vector<Notification> take_pending()
    {
        boost::lock_guard lock(mutex_);
        std::vector<Notification> result;
        result.swap(pending_);
        return result;
    }

private:
    struct Slot {
        std::optional<uint64_t> generation;
        std::optional<hal::expansion::ModuleInfo> identified;
    };

    boost::mutex mutex_;
    std::map<std::pair<std::string, std::string>, Slot> slots_;
    std::vector<Notification> pending_;
};

} // namespace esp_brookesia::system::super
