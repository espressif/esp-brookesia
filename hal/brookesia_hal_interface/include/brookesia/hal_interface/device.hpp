/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Defines the HAL device base type and typed lookup helpers for devices and interfaces.
 */
#pragma once

#include <map>
#include <memory>
#include <string>
#include <type_traits>
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_interface/interface.hpp"

namespace esp_brookesia::hal {

/**
 * @brief Base class for HAL devices.
 *
 * A concrete device can publish one or more interface instances through `interfaces_`
 * during initialization, then those interfaces can be queried from the global registry.
 */
class Device {
public:
    Device(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

    /**
     * @brief Check whether the device is supported on current runtime conditions.
     *
     * @return `true` if the device can be initialized; otherwise `false`.
     */
    virtual bool probe() = 0;

    /**
     * @brief Get the registry name of this device.
     *
     * @return Device name.
     */
    const std::string &get_name() const
    {
        return name_;
    }

    /**
     * @brief Retrieve a typed interface owned by this device by interface registry name.
     *
     * @tparam T Interface type that derives from `Interface`.
     * @param[in] name Fully-qualified interface name in the registry.
     * @return Matching typed interface pointer, or `nullptr` if the name is missing
     *         or the type does not match.
     */
    template<typename T>
    requires IsInterface<T>
    std::shared_ptr<T> get_interface(const std::string &name) const
    {
        auto it = interfaces_.find(name);
        if (it == interfaces_.end()) {
            return nullptr;
        }

        if (auto casted = std::dynamic_pointer_cast<T>(it->second)) {
            return casted;
        }
        return nullptr;
    }

protected:
    friend void init_all_devices();
    friend bool init_device(const std::string &name);
    friend void deinit_all_devices();
    friend void deinit_device(const std::string &name);

    Device(std::string name)
        : name_(name)
    {
    }

    ~Device();

    virtual bool on_init() = 0;

    virtual void on_deinit() = 0;

    bool is_iface_initialized(const std::string &name) const
    {
        return interfaces_.find(name) != interfaces_.end();
    }

    std::map<std::string, std::shared_ptr<Interface>> interfaces_;

private:
    bool is_initialized() const
    {
        return is_initialized_;
    }

    bool init();
    void deinit();

    std::string name_;
    bool is_initialized_ = false;
};

/**
 * @brief Registry alias for HAL devices.
 */
using DeviceRegistry = lib_utils::PluginRegistry<Device>;

/**
 * @brief Initialize all registered devices that pass `probe()`.
 */
void init_all_devices();

/**
 * @brief Initialize one registered device by name.
 *
 * @param[in] name Device registry name.
 * @return `true` on success; otherwise `false`.
 */
bool init_device(const std::string &name);

/**
 * @brief Deinitialize all registered devices.
 */
void deinit_all_devices();

/**
 * @brief Deinitialize one registered device by name.
 *
 * @param[in] name Device registry name.
 */
void deinit_device(const std::string &name);

/**
 * @brief Type constraint for HAL device classes.
 *
 * `T` must inherit from `Device`.
 */
template<typename T>
concept IsDevice = std::is_base_of_v<Device, std::decay_t<T>>;

/**
 * @brief Get the registered device by plugin name.
 *
 * @param[in] plugin_name Device plugin name.
 * @return Matching device pointer, or `nullptr` if no match exists.
 */
static inline std::shared_ptr<Device> get_device_by_plugin_name(const std::string &plugin_name)
{
    return DeviceRegistry::get_instance(plugin_name);
}

/**
 * @brief Get the registered device by device name.
 *
 * @param[in] device_name Device name.
 * @return Matching device pointer, or `nullptr` if no match exists.
 */
static inline std::shared_ptr<Device> get_device_by_device_name(const std::string &device_name)
{
    for (const auto &[_, dev] : DeviceRegistry::get_all_instances()) {
        if (dev->get_name() == device_name) {
            return dev;
        }
    }
    return nullptr;
}

/**
 * @brief Get all registered interfaces that can be cast to `T`.
 *
 * @tparam T Interface type that derives from `Interface`.
 * @return Map of interface registry name to typed interface pointer.
 */
template<typename T>
requires IsInterface<T>
std::map<std::string, std::shared_ptr<T>> get_interfaces()
{
    std::map<std::string, std::shared_ptr<T>> result;
    for (const auto &[iface_name, iface] : InterfaceRegistry::get_all_instances()) {
        if (auto casted = std::dynamic_pointer_cast<T>(iface)) {
            result.emplace(iface_name, casted);
        }
    }
    return result;
}

/**
 * @brief Get the first registered interface that can be cast to `T`.
 *
 * @tparam T Interface type that derives from `Interface`.
 * @return Pair of interface registry name and typed pointer. If not found, returns `{"", nullptr}`.
 */
template<typename T>
requires IsInterface<T>
std::pair<std::string, std::shared_ptr<T>> get_first_interface()
{
    for (const auto &[iface_name, iface] : InterfaceRegistry::get_all_instances()) {
        if (auto casted = std::dynamic_pointer_cast<T>(iface)) {
            return {iface_name, casted};
        }
    }
    return {"", nullptr};
}

} // namespace esp_brookesia::hal
