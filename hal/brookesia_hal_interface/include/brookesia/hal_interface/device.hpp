/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file device.hpp
 * @brief Provider-side HAL device base type.
 */
#pragma once

#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>
#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/hal_interface/interface.hpp"

namespace esp_brookesia::hal {

using InterfaceSpec = InterfaceInfo;

class Device;

namespace detail {

void apply_pending_callbacks_for(const std::string &plugin_name, Device &device);
bool is_device_available(Device &device);
std::shared_ptr<Device> acquire_device(const std::string &plugin_name);
void cleanup_all_devices();
AcquiredInterface try_acquire_interface(const std::string &plugin_name, Device &device, const InterfaceSpec &spec);

} // namespace detail

/**
 * @brief Base class for HAL provider devices.
 *
 * A device groups related interface instances and owns their provider lifecycle.
 * Applications should use the acquire APIs from `interface.hpp` instead of handling
 * devices directly.
 */
class Device {
public:
    using PreInitCallback = std::function<bool()>;
    using PostDeinitCallback = std::function<bool()>;

    Device(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(const Device &) = delete;
    Device &operator=(Device &&) = delete;

protected:
    Device(std::string name)
        : name_(std::move(name))
    {
    }

    /**
     * @brief Virtual destructor for HAL devices.
     */
    ~Device();

    virtual bool probe() = 0;

    virtual std::vector<InterfaceSpec> get_interface_specs() const
    {
        return {};
    }

    virtual bool deinit_on_zero_references() const
    {
        return true;
    }

    virtual bool on_init() = 0;
    virtual void on_deinit() = 0;

    bool publish_interface(const std::string &name, std::shared_ptr<Interface> interface);

    bool is_interface_published(const std::string &name) const
    {
        return interfaces_.find(name) != interfaces_.end();
    }

    bool is_iface_initialized(const std::string &name) const
    {
        return is_interface_published(name);
    }

    std::map<std::string, std::shared_ptr<Interface>> interfaces_;

private:
    friend DeviceInfoList get_device_infos();
    friend detail::AcquiredInterface detail::acquire_interface(std::string_view type_name, std::string_view instance_name);
    friend detail::AcquiredInterface detail::acquire_first_interface(std::string_view type_name);
    friend std::vector<detail::AcquiredInterface> detail::acquire_interfaces(std::string_view type_name);
    friend void detail::release_interface(std::string device_plugin_name);
    friend void detail::apply_pending_callbacks_for(const std::string &plugin_name, Device &device);
    friend bool detail::is_device_available(Device &device);
    friend std::shared_ptr<Device> detail::acquire_device(const std::string &plugin_name);
    friend void detail::cleanup_all_devices();
    friend detail::AcquiredInterface detail::try_acquire_interface(
        const std::string &plugin_name, Device &device, const InterfaceSpec &spec
    );

    bool is_initialized() const
    {
        return is_initialized_;
    }

    bool register_pre_init_callback(PreInitCallback callback);
    bool register_post_deinit_callback(PostDeinitCallback callback);
    bool init();
    void deinit();
    bool acquire();
    void release();
    void force_deinit();
    std::shared_ptr<Interface> get_published_interface(std::string_view name) const;

    std::string name_;
    size_t reference_count_ = 0;
    bool is_initialized_ = false;
    PreInitCallback pre_init_callback_;
    PostDeinitCallback post_deinit_callback_;
};

namespace detail {

void register_pre_init_callback(Device::PreInitCallback callback);
void register_post_deinit_callback(Device::PostDeinitCallback callback);
void apply_pending_callbacks_for(const std::string &plugin_name, Device &device);
void enqueue_pending_pre_init_callback(std::string plugin_name, Device::PreInitCallback callback);
void enqueue_pending_post_deinit_callback(std::string plugin_name, Device::PostDeinitCallback callback);

} // namespace detail

template<typename T>
concept IsDevice = std::is_base_of_v<Device, std::decay_t<T>>;

} // namespace esp_brookesia::hal

/**
 * @brief Auto-register a global callback that runs before the first HAL device is initialized.
 */
#define ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_PRE_INIT_CALLBACK(symbol_name, callback) \
    namespace { \
        struct BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_hal_all_pre_init_cb_helper_, __LINE__)() { \
                ::esp_brookesia::hal::detail::register_pre_init_callback((callback)); \
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
 * @brief Auto-register a global callback that runs after the last HAL device is deinitialized.
 */
#define ESP_BROOKESIA_HAL_DEVICE_REGISTER_ALL_POST_DEINIT_CALLBACK(symbol_name, callback) \
    namespace { \
        struct BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_helper_, __LINE__) { \
            BROOKESIA_PLUGIN_CONCAT(_hal_all_post_deinit_cb_helper_, __LINE__)() { \
                ::esp_brookesia::hal::detail::register_post_deinit_callback((callback)); \
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
 * @brief Auto-register pre-init callbacks for specific provider devices.
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
 * @brief Auto-register post-deinit callbacks for specific provider devices.
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
