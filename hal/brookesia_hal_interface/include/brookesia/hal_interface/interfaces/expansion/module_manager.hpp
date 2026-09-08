/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file module_manager.hpp
 * @brief Declares the hot-pluggable expansion module discovery interface.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

namespace esp_brookesia::hal::expansion {

/** @brief Stable state of one expansion slot. */
enum class ModuleState {
    Unknown,     ///< The slot has not produced a stable scan result yet.
    Empty,       ///< No module is present.
    Invalid,     ///< A module is present, but its identity data is invalid.
    Unsupported, ///< A valid module is present, but no implementation supports it.
    Ready,       ///< A supported module is available for claiming.
    Active,      ///< A consumer currently owns the module.
    Error,       ///< The provider reported an operational error.
};

/** @brief Stable snapshot of one provider slot. */
struct ModuleInfo {
    std::string provider;       ///< Provider name, unique within the process.
    std::string slot;           ///< Provider-local slot name.
    std::string type;           ///< Stable module type used for driver matching.
    uint32_t board_id = 0;      ///< Provider-defined board identifier.
    std::string board_name;     ///< Human-readable board name.
    uint64_t generation = 0;    ///< Monotonic slot generation used by atomic claim.
    ModuleState state = ModuleState::Unknown; ///< Current stable slot state.
};

/** @brief Expansion module discovery and state-event interface. */
class ModuleManagerIface: public Interface {
public:
    static constexpr const char *NAME = "ExpansionModuleManager";

    using EventListenerId = uint64_t;
    using EventListener = std::function<void(const ModuleInfo &)>;

    /** @brief Get the conventional instance name. */
    static std::string get_default_instance_name(size_t id = 0)
    {
        return std::string("Expansion:ModuleManager:") + std::to_string(id);
    }

    ModuleManagerIface()
        : Interface(NAME)
    {
    }

    virtual ~ModuleManagerIface() = default;

    /** @brief Get stable snapshots for all registered provider slots. */
    virtual std::vector<ModuleInfo> get_module_infos() const = 0;

    /**
     * @brief Subscribe to stable slot-state changes.
     *
     * @return Non-zero listener id on success, or zero when the callback is empty.
     */
    virtual EventListenerId add_event_listener(EventListener listener) = 0;

    /**
     * @brief Remove a previously registered listener.
     *
     * When called outside the listener, return waits for any in-flight callback
     * to finish. A listener may remove itself; its current invocation then
     * completes normally, and no later invocation is made.
     */
    virtual bool remove_event_listener(EventListenerId id) = 0;
};

BROOKESIA_DESCRIBE_ENUM(ModuleState, Unknown, Empty, Invalid, Unsupported, Ready, Active, Error);
BROOKESIA_DESCRIBE_STRUCT(
    ModuleInfo, (), (provider, slot, type, board_id, board_name, generation, state)
);

} // namespace esp_brookesia::hal::expansion
