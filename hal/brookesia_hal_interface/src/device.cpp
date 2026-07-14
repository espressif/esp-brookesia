/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/hal_interface/macro_configs.h"
#if !BROOKESIA_HAL_INTERFACE_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "brookesia/hal_interface/device.hpp"

#include <cstdlib>

namespace esp_brookesia::hal {

namespace {

using ProviderPluginRegistry = lib_utils::PluginRegistry<Device>;
using PublishedInterfacePluginRegistry = lib_utils::PluginRegistry<Interface>;

Device::PreInitCallback g_pre_init_callback;
Device::PostDeinitCallback g_post_deinit_callback;
size_t g_initialized_device_count = 0;

std::map<std::string, Device::PreInitCallback> &get_pending_pre_init_callbacks()
{
    static std::map<std::string, Device::PreInitCallback> pending;
    return pending;
}

std::map<std::string, Device::PostDeinitCallback> &get_pending_post_deinit_callbacks()
{
    static std::map<std::string, Device::PostDeinitCallback> pending;
    return pending;
}

} // namespace

namespace detail {

#if !defined(ESP_PLATFORM)
bool register_atexit_cleanup_once();
#endif

bool is_device_available(Device &device)
{
    BROOKESIA_LOGD("- [%1%] Probing...", device.name_);
    if (!device.probe()) {
        BROOKESIA_LOGD("- [%1%] Probe failed, skipped", device.name_);
        return false;
    }

    BROOKESIA_LOGD("- [%1%] Probed", device.name_);
    return true;
}

std::shared_ptr<Device> acquire_device(const std::string &plugin_name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    auto device = ProviderPluginRegistry::get_instance(plugin_name);
    BROOKESIA_CHECK_NULL_RETURN(device, nullptr, "- [%1%] Device not found", plugin_name);

    detail::apply_pending_callbacks_for(plugin_name, *device);
    BROOKESIA_CHECK_FALSE_RETURN(is_device_available(*device), nullptr, "- [%1%] Device is not available", plugin_name);

    const auto was_initialized = device->is_initialized();
    if (!was_initialized && (g_initialized_device_count == 0) && g_pre_init_callback) {
        BROOKESIA_LOGD("Invoking global pre-init callback");
        BROOKESIA_CHECK_FALSE_RETURN(g_pre_init_callback(), nullptr, "Global pre-init callback failed");
    }

    BROOKESIA_CHECK_FALSE_RETURN(device->acquire(), nullptr, "- [%1%] Failed to acquire device", plugin_name);

    if (!was_initialized) {
        g_initialized_device_count++;
#if !defined(ESP_PLATFORM)
        register_atexit_cleanup_once();
#endif
    }

    return device;
}

void cleanup_all_devices()
{
    BROOKESIA_LOG_TRACE_GUARD();

    bool had_initialized_device = false;
    for (const auto &[_, device] : ProviderPluginRegistry::get_all_instances()) {
        if (!device || !device->is_initialized()) {
            continue;
        }

        had_initialized_device = true;
        device->force_deinit();
    }

    g_initialized_device_count = 0;

    if (had_initialized_device && g_post_deinit_callback) {
        BROOKESIA_LOGD("Invoking global post-deinit callback");
        if (!g_post_deinit_callback()) {
            BROOKESIA_LOGW("Global post-deinit callback reported failure");
        }
    }
}

void cleanup_all_devices_at_exit()
{
    detail::cleanup_all_devices();
}

#if !defined(ESP_PLATFORM)
bool register_atexit_cleanup_once()
{
    static bool atexit_registered = false;
    if (atexit_registered) {
        return true;
    }

    if (std::atexit(&cleanup_all_devices_at_exit) != 0) {
        BROOKESIA_LOGW("Failed to register HAL device cleanup atexit handler");
        return false;
    }

    atexit_registered = true;
    BROOKESIA_LOGD("Registered HAL device cleanup atexit handler");
    return true;
}
#endif

AcquiredInterface try_acquire_interface(
    const std::string &plugin_name, Device &device, const InterfaceSpec &spec
)
{
    (void)device;

    auto acquired_device = acquire_device(plugin_name);
    if (!acquired_device) {
        return {};
    }

    auto interface = acquired_device->get_published_interface(spec.instance_name);
    if (!interface) {
        BROOKESIA_LOGW(
            "- [%1%] Declared interface '%2%' was not published during init",
            plugin_name, spec.instance_name
        );
        release_interface(plugin_name);
        return {};
    }

    BROOKESIA_LOGI(
        "- [%1%] Acquired interface '%2%' with reference count %3%",
        acquired_device->name_, spec.instance_name, acquired_device->reference_count_
    );

    return {
        .device_plugin_name = plugin_name,
        .instance_name = spec.instance_name,
        .interface = std::move(interface),
    };
}

void register_pre_init_callback(Device::PreInitCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: callback(%1%)", callback);

    if (g_pre_init_callback) {
        BROOKESIA_LOGW("Global pre-init callback already registered, overwriting");
    }
    g_pre_init_callback = std::move(callback);

    BROOKESIA_LOGD("Registered global pre-init callback");
}

void register_post_deinit_callback(Device::PostDeinitCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: callback(%1%)", callback);

    if (g_post_deinit_callback) {
        BROOKESIA_LOGW("Global post-deinit callback already registered, overwriting");
    }
    g_post_deinit_callback = std::move(callback);

    BROOKESIA_LOGD("Registered global post-deinit callback");
}

void apply_pending_callbacks_for(const std::string &plugin_name, Device &device)
{
    auto &pre = get_pending_pre_init_callbacks();
    if (auto it = pre.find(plugin_name); it != pre.end()) {
        device.register_pre_init_callback(std::move(it->second));
        pre.erase(it);
    }

    auto &post = get_pending_post_deinit_callbacks();
    if (auto it = post.find(plugin_name); it != post.end()) {
        device.register_post_deinit_callback(std::move(it->second));
        post.erase(it);
    }
}

void enqueue_pending_pre_init_callback(std::string plugin_name, Device::PreInitCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: plugin_name(%1%), callback(%2%)", plugin_name, callback);

    BROOKESIA_CHECK_FALSE_EXIT(static_cast<bool>(callback),
                               "- [%1%] Pending pre-init callback is empty", plugin_name);

    auto &pending = get_pending_pre_init_callbacks();
    if (pending.find(plugin_name) != pending.end()) {
        BROOKESIA_LOGW("- [%1%] Pending pre-init callback already queued, overwriting", plugin_name);
    }
    pending[std::move(plugin_name)] = std::move(callback);
}

void enqueue_pending_post_deinit_callback(std::string plugin_name, Device::PostDeinitCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: plugin_name(%1%), callback(%2%)", plugin_name, callback);

    BROOKESIA_CHECK_FALSE_EXIT(static_cast<bool>(callback),
                               "- [%1%] Pending post-deinit callback is empty", plugin_name);

    auto &pending = get_pending_post_deinit_callbacks();
    if (pending.find(plugin_name) != pending.end()) {
        BROOKESIA_LOGW("- [%1%] Pending post-deinit callback already queued, overwriting", plugin_name);
    }
    pending[std::move(plugin_name)] = std::move(callback);
}

AcquiredInterface acquire_interface(std::string_view type_name, std::string_view instance_name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    for (const auto &[plugin_name, device] : ProviderPluginRegistry::get_all_instances()) {
        if (!device) {
            continue;
        }

        for (const auto &spec : device->get_interface_specs()) {
            if ((spec.type_name != type_name) || (spec.instance_name != instance_name)) {
                continue;
            }

            auto result = try_acquire_interface(plugin_name, *device, spec);
            if (result.interface) {
                return result;
            }
        }
    }

    return {};
}

AcquiredInterface acquire_first_interface(std::string_view type_name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    for (const auto &[plugin_name, device] : ProviderPluginRegistry::get_all_instances()) {
        if (!device) {
            continue;
        }

        for (const auto &spec : device->get_interface_specs()) {
            if (spec.type_name != type_name) {
                continue;
            }

            auto result = try_acquire_interface(plugin_name, *device, spec);
            if (result.interface) {
                return result;
            }
        }
    }

    return {};
}

std::vector<AcquiredInterface> acquire_interfaces(std::string_view type_name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    std::vector<AcquiredInterface> result;
    for (const auto &[plugin_name, device] : ProviderPluginRegistry::get_all_instances()) {
        if (!device) {
            continue;
        }

        for (const auto &spec : device->get_interface_specs()) {
            if (spec.type_name != type_name) {
                continue;
            }

            auto acquired = try_acquire_interface(plugin_name, *device, spec);
            if (acquired.interface) {
                result.emplace_back(std::move(acquired));
            }
        }
    }

    return result;
}

void release_interface(std::string device_plugin_name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    if (device_plugin_name.empty()) {
        return;
    }

    auto device = ProviderPluginRegistry::get_instance(device_plugin_name);
    BROOKESIA_CHECK_NULL_EXIT(device, "- [%1%] Device not found", device_plugin_name);

    const auto was_initialized = device->is_initialized();
    device->release();

    if (was_initialized && !device->is_initialized() && (g_initialized_device_count > 0)) {
        g_initialized_device_count--;
        if ((g_initialized_device_count == 0) && g_post_deinit_callback) {
            BROOKESIA_LOGD("Invoking global post-deinit callback");
            if (!g_post_deinit_callback()) {
                BROOKESIA_LOGW("Global post-deinit callback reported failure");
            }
        }
    }
}

} // namespace detail

Device::~Device()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    // Do not call virtual on_deinit() from a base destructor; derived state is already gone.
    if (is_initialized_) {
        BROOKESIA_LOGW(
            "- [%1%] Destroyed while still initialized; performing base-only cleanup",
            name_
        );
        for (const auto &[iface_name, _] : interfaces_) {
            PublishedInterfacePluginRegistry::remove_plugin(iface_name);
        }
        interfaces_.clear();
        reference_count_ = 0;
        is_initialized_ = false;
    }
}

bool Device::publish_interface(const std::string &name, std::shared_ptr<Interface> interface)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_CHECK_FALSE_RETURN(!name.empty(), false, "- [%1%] Interface name is empty", name_);
    BROOKESIA_CHECK_NULL_RETURN(interface, false, "- [%1%] Interface '%2%' is null", name_, name);

    if (interfaces_.find(name) != interfaces_.end()) {
        BROOKESIA_LOGW("- [%1%] Interface '%2%' already published, overwriting", name_, name);
    }
    interfaces_[name] = std::move(interface);

    return true;
}

bool Device::register_pre_init_callback(PreInitCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: callback(%1%)", callback);

    BROOKESIA_CHECK_FALSE_RETURN(static_cast<bool>(callback), false, "- [%1%] Pre-init callback is empty", name_);

    if (pre_init_callback_) {
        BROOKESIA_LOGW("- [%1%] Pre-init callback already registered, overwriting", name_);
    }
    pre_init_callback_ = std::move(callback);

    BROOKESIA_LOGD("- [%1%] Registered pre-init callback", name_);

    return true;
}

bool Device::register_post_deinit_callback(PostDeinitCallback callback)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    BROOKESIA_LOGD("Params: callback(%1%)", callback);

    BROOKESIA_CHECK_FALSE_RETURN(static_cast<bool>(callback), false, "- [%1%] Post-deinit callback is empty", name_);

    if (post_deinit_callback_) {
        BROOKESIA_LOGW("- [%1%] Post-deinit callback already registered, overwriting", name_);
    }
    post_deinit_callback_ = std::move(callback);

    BROOKESIA_LOGD("- [%1%] Registered post-deinit callback", name_);

    return true;
}

bool Device::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_HAL_INTERFACE_VER_MAJOR, BROOKESIA_HAL_INTERFACE_VER_MINOR,
        BROOKESIA_HAL_INTERFACE_VER_PATCH
    );

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    BROOKESIA_LOGD("- [%1%] Initializing...", name_);

    if (pre_init_callback_) {
        BROOKESIA_LOGD("- [%1%] Invoking pre-init callback", name_);
        BROOKESIA_CHECK_FALSE_RETURN(pre_init_callback_(), false, "- [%1%] Pre-init callback failed", name_);
    }

    interfaces_.clear();
    BROOKESIA_CHECK_FALSE_RETURN(on_init(), false, "[%1%] Initialization failed", name_);

    for (const auto &[name, interface] : interfaces_) {
        if (PublishedInterfacePluginRegistry::has_plugin(name)) {
            BROOKESIA_LOGW("- [%1%] Interface '%2%' already registered, skipping", name_, name);
            continue;
        }
        PublishedInterfacePluginRegistry::register_plugin<Interface>(name, [interface]() {
            return interface;
        });
        BROOKESIA_LOGI("- [%1%] Registered interface '%2%'", name_, name);
    }

    is_initialized_ = true;

    BROOKESIA_LOGI("- [%1%] Initialized", name_);

    return true;
}

void Device::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (!is_initialized()) {
        BROOKESIA_LOGD("Not initialized, skip");
        return;
    }

    BROOKESIA_LOGD("- [%1%] Deinitializing...", name_);

    for (const auto &[name, _] : interfaces_) {
        BROOKESIA_LOGI("- [%1%] Unregistering interface '%2%'", name_, name);
        PublishedInterfacePluginRegistry::remove_plugin(name);
    }

    on_deinit();

    interfaces_.clear();
    is_initialized_ = false;

    if (post_deinit_callback_) {
        BROOKESIA_LOGD("- [%1%] Invoking post-deinit callback", name_);
        if (!post_deinit_callback_()) {
            BROOKESIA_LOGW("- [%1%] Post-deinit callback reported failure", name_);
        }
    }

    BROOKESIA_LOGI("- [%1%] Deinitialized", name_);
}

bool Device::acquire()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (reference_count_ > 0) {
        reference_count_++;
        BROOKESIA_LOGI("- [%1%] Reference count increased to %2%", name_, reference_count_);
        return true;
    }

    BROOKESIA_CHECK_FALSE_RETURN(init(), false, "- [%1%] Initialization failed", name_);
    reference_count_ = 1;
    BROOKESIA_LOGI("- [%1%] Reference count increased to %2%", name_, reference_count_);

    return true;
}

void Device::release()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (reference_count_ == 0) {
        BROOKESIA_LOGW("- [%1%] Release requested with zero references", name_);
        return;
    }

    reference_count_--;
    BROOKESIA_LOGI("- [%1%] Reference count decreased to %2%", name_, reference_count_);
    if (reference_count_ == 0) {
        if (deinit_on_zero_references()) {
            deinit();
        } else {
            BROOKESIA_LOGI("- [%1%] Keeping device initialized after release", name_);
        }
    }
}

void Device::force_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    if (reference_count_ > 0) {
        BROOKESIA_LOGW("- [%1%] Force cleanup with %2% active references", name_, reference_count_);
    }
    reference_count_ = 0;
    BROOKESIA_LOGI("- [%1%] Reference count reset to %2%", name_, reference_count_);
    deinit();
}

std::shared_ptr<Interface> Device::get_published_interface(std::string_view name) const
{
    auto it = interfaces_.find(std::string(name));
    if (it == interfaces_.end()) {
        return nullptr;
    }

    return it->second;
}

DeviceInfoList get_device_infos()
{
    BROOKESIA_LOG_TRACE_GUARD();

    DeviceInfoList infos;
    for (const auto &[_, device] : ProviderPluginRegistry::get_all_instances()) {
        if (!device || !detail::is_device_available(*device)) {
            continue;
        }

        auto specs = device->get_interface_specs();
        if (specs.empty()) {
            continue;
        }

        infos.push_back({
            .name = device->name_,
            .interfaces = std::move(specs),
        });
    }

    return infos;
}

bool has_interface(std::string_view type_name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    for (const auto &device_info : get_device_infos()) {
        for (const auto &interface_info : device_info.interfaces) {
            if (interface_info.type_name == type_name) {
                return true;
            }
        }
    }

    return false;
}

} // namespace esp_brookesia::hal
