/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <string>
#include <type_traits>
#include <utility>
#include <string_view>
#include "brookesia/lib_utils/plugin.hpp"

/**
 * @file interface.hpp
 * @brief Defines the base HAL interface type and shared interface registry aliases.
 */

namespace esp_brookesia::hal {

/**
 * @brief Base class for all HAL interfaces exposed by a device.
 *
 * Each concrete interface derives from this type and provides a stable interface name
 * for runtime lookup via the plugin registry.
 */
class Interface {
public:
    /**
     * @brief Construct an interface object with a runtime name.
     *
     * @param[in] name Interface name used by the registry.
     */
    explicit Interface(std::string_view name)
        : name_(name)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic interface usage.
     */
    virtual ~Interface() = default;

    /**
     * @brief Get the registered interface name.
     *
     * @return Interface name.
     */
    std::string_view get_name() const noexcept
    {
        return name_;
    }

private:
    std::string_view name_;
};

/**
 * @brief Type constraint for HAL interface classes.
 *
 * `T` must inherit from `Interface`.
 */
template<typename T>
concept IsInterface = std::is_base_of_v<Interface, std::decay_t<T>>;

/**
 * @brief Registry alias for globally discoverable HAL interfaces.
 */
using InterfaceRegistry = lib_utils::PluginRegistry<Interface>;

} // namespace esp_brookesia::hal
