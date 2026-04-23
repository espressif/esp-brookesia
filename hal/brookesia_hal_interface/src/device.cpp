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

namespace esp_brookesia::hal {

namespace {

Device::PreInitCallback g_pre_init_callback;
Device::PostDeinitCallback g_post_deinit_callback;

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

// Drain any pending per-device callbacks queued by the auto-registration macros and attach
// them to `device` through the public `register_*` API. Called from `init_device()` once the
// device instance is known to exist, so it decouples macro registration from the plugin
// registration order.
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

} // namespace

namespace detail {

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

} // namespace detail

Device::~Device()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();

    deinit();
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

    if (is_initialized()) {
        BROOKESIA_LOGD("Already initialized");
        return true;
    }

    BROOKESIA_LOGD("- [%1%] Initializing...", name_);

    if (pre_init_callback_) {
        BROOKESIA_LOGD("- [%1%] Invoking pre-init callback", name_);
        BROOKESIA_CHECK_FALSE_RETURN(pre_init_callback_(), false,
                                     "- [%1%] Pre-init callback failed", name_);
    }

    BROOKESIA_CHECK_FALSE_RETURN(on_init(), false, "[%1%] Initialization failed", name_);

    for (const auto &[name, interface] : interfaces_) {
        if (InterfaceRegistry::has_plugin(name)) {
            BROOKESIA_LOGW("- [%1%] Interface '%2%' already registered, skipping", name_, name);
            continue;
        }
        InterfaceRegistry::register_plugin<Interface>(name, [interface]() {
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

    for (const auto &[name, interface] : interfaces_) {
        BROOKESIA_LOGI("- [%1%] Unregistering interface '%2%'", name_, name);
        InterfaceRegistry::remove_plugin(name);
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

void init_all_devices()
{
    BROOKESIA_LOG_TRACE_GUARD();

    if (g_pre_init_callback) {
        BROOKESIA_LOGD("Invoking global pre-init callback");
        BROOKESIA_CHECK_FALSE_EXIT(g_pre_init_callback(), "Global pre-init callback failed");
    }

    BROOKESIA_LOGI("Found %1% devices", DeviceRegistry::get_plugin_count());
    size_t count = 0;
    for (const auto &[name, _] : DeviceRegistry::get_all_instances()) {
        if (init_device(name)) {
            count++;
        }
    }
    BROOKESIA_LOGI("Initialized %1%/%2% devices", count, DeviceRegistry::get_plugin_count());

    for (const auto &[name, _] : get_pending_pre_init_callbacks()) {
        BROOKESIA_LOGW("- [%1%] Pending pre-init callback has no matching device, dropped", name);
    }
    get_pending_pre_init_callbacks().clear();
    for (const auto &[name, _] : get_pending_post_deinit_callbacks()) {
        BROOKESIA_LOGW("- [%1%] Pending post-deinit callback has no matching device, dropped", name);
    }
    get_pending_post_deinit_callbacks().clear();
}

bool init_device(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto device = DeviceRegistry::get_instance(name);
    BROOKESIA_CHECK_NULL_RETURN(device, false, "- [%1%] Device not found", name);

    apply_pending_callbacks_for(name, *device);

    BROOKESIA_LOGD("- [%1%] Probing...", name);
    BROOKESIA_CHECK_FALSE_RETURN(device->probe(), false, "- [%1%] Probe failed, skipped", name);
    BROOKESIA_LOGD("- [%1%] Probed", name);

    BROOKESIA_CHECK_FALSE_RETURN(device->init(), false, "- [%1%] Initialization failed, skipped", name);

    return true;
}

void deinit_all_devices()
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Found %1% devices", DeviceRegistry::get_plugin_count());
    for (const auto &[name, _] : DeviceRegistry::get_all_instances()) {
        deinit_device(name);
    }

    if (g_post_deinit_callback) {
        BROOKESIA_LOGD("Invoking global post-deinit callback");
        if (!g_post_deinit_callback()) {
            BROOKESIA_LOGW("Global post-deinit callback reported failure");
        }
    }
}

void deinit_device(const std::string &name)
{
    BROOKESIA_LOG_TRACE_GUARD();

    BROOKESIA_LOGD("Params: name(%1%)", name);

    auto device = DeviceRegistry::get_instance(name);
    BROOKESIA_CHECK_NULL_EXIT(device, "- [%1%] Device not found", name);

    device->deinit();
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

} // namespace esp_brookesia::hal
