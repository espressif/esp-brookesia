/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Base device abstractions, interface lookup helpers, and registry utilities for the HAL layer.
 */
#pragma once

#include <list>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <array>
#include <memory>
#include <vector>
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/check.hpp"

namespace esp_brookesia::hal {

//---------------- Interface ID ----------------
/**
 * @brief Calculates the FNV-1a 64-bit hash of s.
 *
 * @param s Interface name string to hash.
 * @return FNV-1a 64-bit hash value for s.
 */
static inline constexpr uint64_t cal_fnv1a_64(std::string_view s)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (char c : s) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 0x100000001b3;
    }
    return hash;
}

/**
 * @brief Returns the hashed interface identifier for I.
 *
 * @tparam I Interface type exposing `interface_name`.
 * @return FNV-1a hash of `I::interface_name`.
 */
template<typename I> constexpr uint64_t get_device_interface_fnv1a()
{
    return cal_fnv1a_64(I::interface_name);
}

/**
 * @brief Abstract base class for all HAL devices registered in the runtime registry.
 *
 * Concrete devices are responsible for exposing one or more public interfaces,
 * implementing the lifecycle hooks, and answering runtime interface queries.
 */
class Device {
public:
    using Registry = esp_brookesia::lib_utils::PluginRegistry<Device>; ///< Registry type used to create and enumerate devices.

    /**
     * @brief Disables copy and move operations for device instances.
     */
    Device(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

    /**
     * @brief Constructs a device with the given registry name.
     *
     * @param name Device name used by the registry and lookup helpers.
     */
    Device(const char *name)
        : _name(name)
    {
    }

    /**
     * @brief Destroys the device.
     */
    virtual ~Device() = default;

    /**
     * @brief Queries the device for interface I.
     *
     * @tparam I Interface type to query.
     * @return Pointer to the requested interface, or `nullptr` when unsupported.
     */
    template<typename I> I *query()
    {
        return static_cast<I *>(query_interface(get_device_interface_fnv1a<I>()));
    }

    /**
     * @brief Checks whether the device is supported on the current platform.
     *
     * @return true when the device should be used, otherwise false.
     */
    virtual bool probe() = 0;

    /**
     * @brief Returns whether the device has already been initialized.
     *
     * @return true when the device is initialized, otherwise false.
     */
    virtual bool check_initialized() const = 0;

    /**
     * @brief Returns the registry name of the device.
     *
     * @return Immutable reference to the device name.
     */
    const std::string &get_name(void) const
    {
        return _name;
    }

    /**
     * @brief Initializes the device and its exposed interfaces.
     *
     * @return true on success, otherwise false.
     */
    virtual bool init() = 0;

    /**
     * @brief Deinitializes the device and releases owned resources.
     *
     * @return true on success, otherwise false.
     */
    virtual bool deinit() = 0;

    /**
     * @brief Initializes every registered device that passes `probe()`.
     *
     * @return true when initialization finishes successfully, otherwise false.
     */
    static bool init_device_from_registry();

private:
    std::string _name = "BaseDevice"; /**< Registry name of the device. */

    /**
     * @brief Queries the device by hashed interface identifier.
     *
     * @param id Interface identifier produced by `get_device_interface_fnv1a()`.
     * @return Pointer to the matching interface, or `nullptr` when unsupported.
     */
    virtual void *query_interface(uint64_t) = 0;
};

//----------------  CRTP Implementation ----------------
/**
 * @brief CRTP helper that builds a static interface-dispatch table for a device implementation.
 *
 * @tparam Derived Concrete device type inheriting from `DeviceImpl`.
 */
template<typename Derived>
class DeviceImpl : public Device {
protected:
    /**
     * @brief Constructs the CRTP device helper with the given registry name.
     *
     * @param name Device name used by the base `Device` class.
     */
    DeviceImpl(const char *name): Device(name) {}

    /**
     * @brief Builds and searches a static interface-dispatch table.
     *
     * @tparam Ifaces Interface types supported by the device.
     * @param id Hashed interface identifier to resolve.
     * @return Pointer to the requested interface, or `nullptr` when no match exists.
     */
    template<typename... Ifaces>
    void *build_table(uint64_t id)
    {
        static const Entry table[] = { Make<Ifaces>()... };
        for (const auto &e : table) {
            if (e.id == id) {
                return e.cast(static_cast<Derived *>(this));
            }
        }
        return nullptr;
    }

private:
    /**
     * @brief One entry in the static interface-dispatch table.
     */
    struct Entry {
        uint64_t id;             ///< Hashed interface identifier.
        void *(*cast)(Derived *); ///< Cast function returning the requested interface view.
    };

    /**
     * @brief Creates one dispatch-table entry for interface I.
     *
     * @tparam I Interface type supported by the device.
     * @return Dispatch-table entry for I.
     */
    template<typename I> static Entry Make()
    {
        return {get_device_interface_fnv1a<I>(), [](Derived * self)->void *{ return static_cast<I *>(self); }};
    }
};

//---------------- Convenience Functions ----------------
/**
 * @brief Returns the registered device named name.
 *
 * @param name Device registry name.
 * @return Shared pointer to the device, or `nullptr` when not found.
 *
 * @code
 * auto dev = get_device("display");
 * if (!dev) {
 *     ESP_LOGE(TAG, "Device not found");
 *     return ESP_ERR_NOT_FOUND;
 * }
 * @endcode
 */
static inline std::shared_ptr<Device> get_device(const char *name)
{
    return Device::Registry::get_instance(name);
}

/**
 * @brief Returns interface T from the registered device named name.
 *
 * @tparam T Interface type to query.
 * @param name Device registry name.
 * @return Pointer to the requested interface, or `nullptr` when the device or interface is unavailable.
 *
 * @code
 * DisplayPanelIface *display = get_interface<DisplayPanelIface>("name");
 * if (!display) {
 *     ESP_LOGE(TAG, "Display not found");
 *     return ESP_ERR_NOT_FOUND;
 * }
 * auto &config = display->get_config();
 * @endcode
 */
template <typename T>
static inline T *get_interface(const char *name)
{
    auto dev = get_device(name);
    if (!dev) {
        return nullptr;
    }
    return dev->query<T>();
}

/**
 * @brief Returns all registered interfaces of type T.
 *
 * @tparam T Interface type to enumerate.
 * @return Vector of `(device name, interface pointer)` pairs.
 *
 * @note The iteration order follows the registry order.
 */
template <typename T>
static inline std::vector<std::pair<std::string, T *>> get_interfaces()
{
    std::vector<std::pair<std::string, T *>> interfaces;
    auto devices = Device::Registry::get_all_instances();
    for (const auto &[name, dev] : devices) {
        if (!dev) {
            continue;
        }
        auto *iface = dev->query<T>();
        if (iface) {
            interfaces.emplace_back(name, iface);
        }
    }
    return interfaces;
}

/**
 * @brief Returns the first registered interface of type T.
 *
 * @tparam T Interface type to search for.
 * @return Pointer to the first matching interface, or `nullptr` when no device exposes it.
 */
template <typename T>
static inline T *get_first_interface()
{
    auto devices = Device::Registry::get_all_instances();
    for (const auto &[name, dev] : devices) {
        (void)name;
        if (!dev) {
            continue;
        }

        auto *iface = dev->query<T>();
        if (iface) {
            return iface;
        }
    }
    return nullptr;
}

} // namespace esp_brookesia::hal
