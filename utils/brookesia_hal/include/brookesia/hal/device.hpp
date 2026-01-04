/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <list>
#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <array>
#include <memory>
#include "brookesia/lib_utils/plugin.hpp"

namespace esp_brookesia::hal {

//---------------- Interface ID ----------------
static inline constexpr uint64_t fnv1a_64(std::string_view s)
{
    uint64_t hash = 0xcbf29ce484222325;
    for (char c : s) {
        hash ^= static_cast<uint8_t>(c);
        hash *= 0x100000001b3;
    }
    return hash;
}
template<typename I> constexpr uint64_t IID()
{
    return fnv1a_64(I::interface_name);
}

/**
 * @brief The core app class. This serves as the base class for all internal app classes. User-defined app classes
 *        should not inherit from this class directly.
 *
 */
class Device {
public:

    using Registry = esp_brookesia::lib_utils::PluginRegistry<Device>;

    /**
     * @brief Delete copy constructor and assignment operator
     */
    Device(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

    /**
     * @brief Construct a core app with detailed configuration.
     *
     * @param data The configuration data for the core app.
     *
     */
    Device(const char *name)
        : _name(name)
    {
    }

    /**
     * @brief Destructor for the core app, should be defined by the user's app class.
     */
    virtual ~Device() = default;

    virtual void *query_interface(uint64_t)
    {
        return nullptr;
    }
    template<typename I> I *query()
    {
        return static_cast<I *>(query_interface(IID<I>()));
    }

    virtual bool probe()
    {
        return false;
    }

    /**
     * @brief  Check if the app is initialized
     *
     * @return true if the app is initialized, otherwise false
     *
     */
    virtual bool check_initialized(void) const
    {
        return false;
    }

    /**
     * @brief Get the app name
     *
     * @return name: the string pointer of the app name
     *
     */
    const std::string &get_name(void) const
    {
        return _name;
    }

    /**
     * @brief Get the handle object of the device
     *
     * @return const void*
     */
    const void *get_handle() const
    {
        return _dev_handle;
    }

    /**
     * @brief Called when the app starts to install. The app can perform initialization here.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool init(void)
    {
        return true;
    }

    /**
     * @brief Called when the app starts to uninstall. The app can perform deinitialization here.
     *
     * @return true if successful, otherwise false
     *
     */
    virtual bool deinit(void)
    {
        return true;
    }

    /**
     * @brief Initialize all devices from the registry
     *
     * @return always true
     *
     */
    static bool init_device_from_registry(void);

protected:
    void *_dev_handle = nullptr;   /**< The handle of the device */

private:
    std::string _name = "BaseDevice"; /**< The name of the app */
};

//----------------  CRTP Implementation ----------------
template<typename Derived>
class DeviceImpl : public Device {

protected:
    DeviceImpl(const char *name): Device(name) {}
    template<typename... Ifaces>
    void *build_table(uint64_t id)
    {
        static const Entry table[] = { Make<Ifaces>()... };
        for (const auto &e : table) if (e.id == id) {
                return e.cast(static_cast<Derived *>(this));
            }
        return nullptr;
    }

private:
    struct Entry {
        uint64_t id;
        void *(*cast)(Derived *);
    };
    template<typename I> static Entry Make()
    {
        return {IID<I>(), [](Derived * self)->void *{ return static_cast<I *>(self); }};
    }
};

//---------------- Convenience Functions ----------------
/**
 * @brief Get device by name
 * @param name Device name
 * @return shared_ptr to device, or nullptr if not found
 *
 * @code
 * auto dev = get_device("display");
 * if (!dev) {
 *     ESP_LOGE(TAG, "Device not found");
 *     return ESP_ERR_NOT_FOUND;
 * }
 * const void *handle = dev->get_handle();
 * @endcode
 */
static inline std::shared_ptr<Device> get_device(const char *name)
{
    return Device::Registry::get_instance(name);
}

/**
 * @brief Get interface by device name
 * @param name Device name
 * @return Interface pointer, or nullptr if device or interface not found
 *
 * @code
 * ILCD_Display *display = get_interface<ILCD_Display>("display");
 * ESP_RETURN_ON_FALSE(display != nullptr, ESP_ERR_NOT_FOUND, TAG, "Display not found");
 * auto config = display->get_lcd_config();
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

} // namespace esp_brookesia::hal
