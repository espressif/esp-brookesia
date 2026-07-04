/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"

/**
 * @file interface.hpp
 * @brief Defines the base HAL interface type and shared interface registry aliases.
 */

namespace esp_brookesia::hal {

/**
 * @brief Public description of one HAL interface instance.
 */
struct InterfaceInfo {
    std::string type_name;      ///< Stable interface type name, such as `DisplayPanel`.
    std::string instance_name;  ///< Concrete interface instance name, such as `Display:Panel`.
};

/**
 * @brief Public description of one available HAL device and its interfaces.
 */
struct DeviceInfo {
    std::string name;                       ///< Logical device name.
    std::vector<InterfaceInfo> interfaces;  ///< Interfaces declared by the device.
};

/**
 * @brief List of currently available HAL devices.
 */
using DeviceInfoList = std::vector<DeviceInfo>;

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

namespace detail {

struct AcquiredInterface {
    std::string device_plugin_name;
    std::string instance_name;
    std::shared_ptr<Interface> interface;
};

AcquiredInterface acquire_interface(std::string_view type_name, std::string_view instance_name);
AcquiredInterface acquire_first_interface(std::string_view type_name);
std::vector<AcquiredInterface> acquire_interfaces(std::string_view type_name);
void release_interface(std::string device_plugin_name);

} // namespace detail

template<typename T>
requires IsInterface<T>
class InterfaceHandle;

template<typename T>
requires IsInterface<T>
InterfaceHandle<T> acquire_interface(std::string_view instance_name);

template<typename T>
requires IsInterface<T>
InterfaceHandle<T> acquire_first_interface();

template<typename T>
requires IsInterface<T>
std::vector<InterfaceHandle<T>> acquire_interfaces();

/**
 * @brief RAII handle that keeps the owning HAL device initialized while an interface is in use.
 *
 * The handle only manages provider/device lifetime. The concrete interface's own
 * `init/deinit/open/close/start/stop` semantics remain the caller's responsibility.
 *
 * @tparam T Interface type that derives from @ref Interface.
 */
template<typename T>
requires IsInterface<T>
class InterfaceHandle {
public:
    InterfaceHandle() = default;

    InterfaceHandle(const InterfaceHandle &) = delete;
    InterfaceHandle &operator=(const InterfaceHandle &) = delete;

    InterfaceHandle(InterfaceHandle &&other) noexcept
    {
        move_from(std::move(other));
    }

    InterfaceHandle &operator=(InterfaceHandle &&other) noexcept
    {
        if (this != &other) {
            reset();
            move_from(std::move(other));
        }
        return *this;
    }

    ~InterfaceHandle()
    {
        reset();
    }

    explicit operator bool() const
    {
        return static_cast<bool>(interface_);
    }

    bool operator==(std::nullptr_t) const
    {
        return !interface_;
    }

    bool operator!=(std::nullptr_t) const
    {
        return static_cast<bool>(interface_);
    }

    T *operator->() const
    {
        return interface_.get();
    }

    T &operator*() const
    {
        return *interface_;
    }

    std::shared_ptr<T> get() const
    {
        return interface_;
    }

    const std::string &instance_name() const
    {
        return instance_name_;
    }

    void reset()
    {
        interface_.reset();
        instance_name_.clear();
        if (!device_plugin_name_.empty()) {
            detail::release_interface(std::move(device_plugin_name_));
            device_plugin_name_.clear();
        }
    }

private:
    template<typename U>
    requires IsInterface<U>
    friend InterfaceHandle<U> acquire_interface(std::string_view instance_name);
    template<typename U>
    requires IsInterface<U>
    friend InterfaceHandle<U> acquire_first_interface();
    template<typename U>
    requires IsInterface<U>
    friend std::vector<InterfaceHandle<U>> acquire_interfaces();

    InterfaceHandle(std::string device_plugin_name, std::string instance_name, std::shared_ptr<T> interface)
        : device_plugin_name_(std::move(device_plugin_name))
        , instance_name_(std::move(instance_name))
        , interface_(std::move(interface))
    {
    }

    void move_from(InterfaceHandle &&other)
    {
        device_plugin_name_ = std::move(other.device_plugin_name_);
        instance_name_ = std::move(other.instance_name_);
        interface_ = std::move(other.interface_);
        other.device_plugin_name_.clear();
        other.instance_name_.clear();
    }

    std::string device_plugin_name_;
    std::string instance_name_;
    std::shared_ptr<T> interface_;
};

/**
 * @brief Acquire one interface by concrete instance name.
 *
 * @tparam T Interface type that derives from @ref Interface.
 * @param[in] instance_name Concrete instance name reported by @ref get_device_infos.
 * @return Owning interface handle, or an empty handle if no matching interface is available.
 */
template<typename T>
requires IsInterface<T>
InterfaceHandle<T> acquire_interface(std::string_view instance_name)
{
    auto result = detail::acquire_interface(T::NAME, instance_name);
    if (!result.interface) {
        return {};
    }

    auto casted = std::dynamic_pointer_cast<T>(result.interface);
    if (!casted) {
        detail::release_interface(std::move(result.device_plugin_name));
        return {};
    }

    return InterfaceHandle<T>(
               std::move(result.device_plugin_name),
               std::move(result.instance_name),
               std::move(casted)
           );
}

/**
 * @brief Acquire the first available interface of a type.
 *
 * @tparam T Interface type that derives from @ref Interface.
 * @return Owning interface handle, or an empty handle if no matching interface is available.
 */
template<typename T>
requires IsInterface<T>
InterfaceHandle<T> acquire_first_interface()
{
    auto result = detail::acquire_first_interface(T::NAME);
    if (!result.interface) {
        return {};
    }

    auto casted = std::dynamic_pointer_cast<T>(result.interface);
    if (!casted) {
        detail::release_interface(std::move(result.device_plugin_name));
        return {};
    }

    return InterfaceHandle<T>(
               std::move(result.device_plugin_name),
               std::move(result.instance_name),
               std::move(casted)
           );
}

/**
 * @brief Acquire all available interfaces of a type.
 *
 * @tparam T Interface type that derives from @ref Interface.
 * @return Owning interface handles for every matching interface that can be initialized.
 */
template<typename T>
requires IsInterface<T>
std::vector<InterfaceHandle<T>> acquire_interfaces()
{
    std::vector<InterfaceHandle<T>> handles;

    for (auto &result : detail::acquire_interfaces(T::NAME)) {
        auto casted = std::dynamic_pointer_cast<T>(result.interface);
        if (!casted) {
            detail::release_interface(std::move(result.device_plugin_name));
            continue;
        }
        handles.push_back(InterfaceHandle<T>(
                              std::move(result.device_plugin_name),
                              std::move(result.instance_name),
                              std::move(casted)
                          ));
    }

    return handles;
}

/**
 * @brief Check whether any available device declares an interface type.
 *
 * @param[in] type_name Interface type name, such as `DisplayBacklight`.
 * @return `true` if at least one probed device declares the interface.
 */
bool has_interface(std::string_view type_name);

template<typename T>
requires IsInterface<T>
bool has_interface()
{
    return has_interface(T::NAME);
}

/**
 * @brief Get read-only information for all currently available HAL devices.
 *
 * Only devices that pass `probe()` are returned. Each entry contains the logical device
 * name and its declared interface type/instance names.
 *
 * @return Available device information list.
 */
DeviceInfoList get_device_infos();

BROOKESIA_DESCRIBE_STRUCT(InterfaceInfo, (), (type_name, instance_name));
BROOKESIA_DESCRIBE_STRUCT(DeviceInfo, (), (name, interfaces));

} // namespace esp_brookesia::hal
