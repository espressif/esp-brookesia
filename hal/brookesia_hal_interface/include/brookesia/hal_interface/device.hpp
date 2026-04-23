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
    using PreInitCallback = std::function<bool()>;
    using PostDeinitCallback = std::function<bool()>;

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
     * @brief Register a callback that runs before this device is initialized.
     *
     * The callback is invoked once just before `on_init()` inside `init()`. If the callback
     * returns `false`, initialization is aborted. Registering a new callback overwrites
     * any previously registered one.
     *
     * @param[in] callback Callback to invoke; must be non-empty.
     * @return `true` on success; `false` if the callback is empty.
     */
    bool register_pre_init_callback(PreInitCallback callback);

    /**
     * @brief Register a callback that runs after this device is deinitialized.
     *
     * The callback is invoked once right after `on_deinit()` returns. Its return value is
     * only used for logging; deinitialization always proceeds to completion. Registering a
     * new callback overwrites any previously registered one.
     *
     * @param[in] callback Callback to invoke; must be non-empty.
     * @return `true` on success; `false` if the callback is empty.
     */
    bool register_post_deinit_callback(PostDeinitCallback callback);

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
    PreInitCallback pre_init_callback_;
    PostDeinitCallback post_deinit_callback_;
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
 * @brief Register a global pre-init callback used by `init_all_devices()`.
 *
 * The callback is invoked once before iterating registered devices. If it returns
 * `false`, `init_all_devices()` aborts before any device is initialized. Registering a
 * new callback overwrites the previously registered one.
 *
 * @param[in] callback Callback to invoke; passing an empty callback clears the current one.
 */
void register_pre_init_callback(Device::PreInitCallback callback);

/**
 * @brief Register a global post-deinit callback used by `deinit_all_devices()`.
 *
 * The callback is invoked once after all registered devices have been deinitialized. Its
 * return value is only used for logging. Registering a new callback overwrites the
 * previously registered one.
 *
 * @param[in] callback Callback to invoke; passing an empty callback clears the current one.
 */
void register_post_deinit_callback(Device::PostDeinitCallback callback);

namespace detail {

/**
 * @brief Queue a pre-init callback to be attached to a device by plugin name.
 *
 * The callback is stored in a process-wide pending map keyed by plugin name. When
 * `init_device()` later initializes the matching device, the pending entry is drained
 * and attached through `Device::register_pre_init_callback()`.
 *
 * This indirection exists to tolerate C++ static initialization order between the
 * device plugin registrar (`BROOKESIA_PLUGIN_REGISTER`) and the callback auto-registrar
 * (`ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS`), which live in different
 * translation units and therefore have unspecified relative ordering.
 *
 * @param[in] plugin_name Target device plugin name.
 * @param[in] callback    Callback to attach once the device is initialized.
 */
void enqueue_pending_pre_init_callback(std::string plugin_name, Device::PreInitCallback callback);

/**
 * @brief Queue a post-deinit callback to be attached to a device by plugin name.
 *
 * See `enqueue_pending_pre_init_callback()` for the lifecycle rationale. The pending
 * entry is drained inside `init_device()` (so the callback is registered before the
 * matching `deinit()` runs).
 *
 * @param[in] plugin_name Target device plugin name.
 * @param[in] callback    Callback to attach once the device is initialized.
 */
void enqueue_pending_post_deinit_callback(std::string plugin_name, Device::PostDeinitCallback callback);

} // namespace detail

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
 * @brief Get all registered devices by name.
 *
 * @return Map of device name to device pointer.
 */
static inline std::map<std::string, std::shared_ptr<Device>> get_all_devices()
{
    std::map<std::string, std::shared_ptr<Device>> result;
    for (const auto &[name, dev] : DeviceRegistry::get_all_instances()) {
        result.emplace(name, dev);
    }
    return result;
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

/**
 * @brief Get all registered interfaces.
 *
 * @return Map of interface registry name to interface pointer.
 */
static inline std::map<std::string, std::shared_ptr<Interface>> get_all_interfaces()
{
    std::map<std::string, std::shared_ptr<Interface>> result;
    for (const auto &[iface_name, iface] : InterfaceRegistry::get_all_instances()) {
        result.emplace(iface_name, iface);
    }
    return result;
}

} // namespace esp_brookesia::hal

/**
 * @brief Auto-register a global pre-init callback used by `init_all_devices()`.
 *
 * Generates a file-local registrar whose constructor forwards to
 * `esp_brookesia::hal::register_pre_init_callback()`.
 *
 * @param symbol_name Linker-visible identifier used with `-u` to keep the registrar alive.
 * @param callback    Callback expression convertible to `Device::PreInitCallback`.
 */
#define ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_PRE_INIT_CALLBACK(symbol_name, callback) \
    namespace { \
        struct BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_helper_, __LINE__)() { \
                ::esp_brookesia::hal::register_pre_init_callback((callback)); \
            } \
        }; \
        static BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_helper_, __LINE__) \
            BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_instance_, __LINE__); \
    } \
    BROOKESIA_PLUGIN_CREATE_SYMBOL( \
        symbol_name, \
        BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_instance_, __LINE__) \
    )

/**
 * @brief Auto-register a global post-deinit callback used by `deinit_all_devices()`.
 *
 * Generates a file-local registrar whose constructor forwards to
 * `esp_brookesia::hal::register_post_deinit_callback()`.
 *
 * @param symbol_name Linker-visible identifier used with `-u` to keep the registrar alive.
 * @param callback    Callback expression convertible to `Device::PostDeinitCallback`.
 */
#define ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK(symbol_name, callback) \
    namespace { \
        struct BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_helper_, __LINE__)() { \
                ::esp_brookesia::hal::register_post_deinit_callback((callback)); \
            } \
        }; \
        static BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_helper_, __LINE__) \
            BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_instance_, __LINE__); \
    } \
    BROOKESIA_PLUGIN_CREATE_SYMBOL( \
        symbol_name, \
        BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_instance_, __LINE__) \
    )

/**
 * @brief Auto-register pre-init callbacks for multiple HAL devices at static init time.
 *
 * Each variadic entry must be a brace-enclosed `{plugin_name, callback}` pair whose types
 * match `std::pair<std::string, Device::PreInitCallback>`. Because the target devices and
 * these callbacks are both registered through C++ static constructors in different
 * translation units, their relative ordering is unspecified; therefore the macro does not
 * attempt to resolve the device immediately. Instead each entry is queued through
 * `detail::enqueue_pending_pre_init_callback()` and attached via
 * `Device::register_pre_init_callback()` inside `init_device()` when the matching device
 * is known to exist. Entries whose plugin name has no matching device are reported by
 * `init_all_devices()` after the init pass finishes.
 *
 * @param symbol_name Linker-visible identifier used with `-u` to keep the registrar alive.
 * @param ...         One or more `{plugin_name, PreInitCallback}` entries separated by commas.
 *
 * @code
 * ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS(
 *     my_pre_init_callbacks_symbol,
 *     {"dev_a", []() { return true; }},
 *     {std::string(hal::AudioDevice::DEVICE_NAME), []() { return true; }}
 * );
 * @endcode
 */
#define ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS(symbol_name, ...) \
    namespace { \
        struct BROOKESIA_PLUGIN_CONCAT(_hal_dev_pre_init_cbs_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_hal_dev_pre_init_cbs_helper_, __LINE__)() { \
                const std::initializer_list< \
                    std::pair<std::string, ::esp_brookesia::hal::Device::PreInitCallback>> _entries = { \
                    __VA_ARGS__ \
                }; \
                for (const auto &_entry : _entries) { \
                    ::esp_brookesia::hal::detail::enqueue_pending_pre_init_callback( \
                        _entry.first, _entry.second); \
                } \
            } \
        }; \
        static BROOKESIA_PLUGIN_CONCAT(_hal_dev_pre_init_cbs_helper_, __LINE__) \
            BROOKESIA_PLUGIN_CONCAT(_hal_dev_pre_init_cbs_instance_, __LINE__); \
    } \
    BROOKESIA_PLUGIN_CREATE_SYMBOL( \
        symbol_name, \
        BROOKESIA_PLUGIN_CONCAT(_hal_dev_pre_init_cbs_instance_, __LINE__) \
    )

/**
 * @brief Auto-register post-deinit callbacks for multiple HAL devices at static init time.
 *
 * Behaves like `ESP_BROOKESIA_HAL_DEVICE_REGISTER_PRE_INIT_CALLBACKS`: each entry is queued
 * through `detail::enqueue_pending_post_deinit_callback()` and attached via
 * `Device::register_post_deinit_callback()` when the device is initialized, so the
 * callback is in place before `Device::deinit()` runs.
 *
 * @param symbol_name Linker-visible identifier used with `-u` to keep the registrar alive.
 * @param ...         One or more `{plugin_name, PostDeinitCallback}` entries separated by commas.
 */
#define ESP_BROOKESIA_HAL_DEVICE_REGISTER_POST_DEINIT_CALLBACKS(symbol_name, ...) \
    namespace { \
        struct BROOKESIA_PLUGIN_CONCAT(_hal_dev_post_deinit_cbs_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_hal_dev_post_deinit_cbs_helper_, __LINE__)() { \
                const std::initializer_list< \
                    std::pair<std::string, ::esp_brookesia::hal::Device::PostDeinitCallback>> _entries = { \
                    __VA_ARGS__ \
                }; \
                for (const auto &_entry : _entries) { \
                    ::esp_brookesia::hal::detail::enqueue_pending_post_deinit_callback( \
                        _entry.first, _entry.second); \
                } \
            } \
        }; \
        static BROOKESIA_PLUGIN_CONCAT(_hal_dev_post_deinit_cbs_helper_, __LINE__) \
            BROOKESIA_PLUGIN_CONCAT(_hal_dev_post_deinit_cbs_instance_, __LINE__); \
    } \
    BROOKESIA_PLUGIN_CREATE_SYMBOL( \
        symbol_name, \
        BROOKESIA_PLUGIN_CONCAT(_hal_dev_post_deinit_cbs_instance_, __LINE__) \
    )
