/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <atomic>
#include <deque>
#include <future>
#include <thread>
#include "gsp_sim_bridge.h"
#include "brookesia/gui_gsp/bundle.hpp"
#include "brookesia/gui_gsp/session.hpp"
#include "private/desktop.hpp"

namespace esp_brookesia::gui::gsp {
namespace {
struct Executor {
    std::mutex mutex;
    std::deque<std::function<void()>> jobs;
    std::thread thread;
    std::atomic<bool> stopping{false};
    std::atomic<bool> failed{false};
    std::thread::id owner;
    esp_gsp_handle_t handle = nullptr;
    std::shared_ptr<const DesktopBundle> bundle;
    std::shared_ptr<DesktopDisplay> display;
    std::shared_ptr<void> callback_owner;
    std::shared_ptr<const void> bundle_owner;
};
std::mutex executors_mutex;
std::map<esp_gsp_handle_t, std::shared_ptr<Executor>> executors;

std::shared_ptr<Executor> find_executor(esp_gsp_handle_t handle)
{
    std::lock_guard lock(executors_mutex);
    const auto found = executors.find(handle);
    return found == executors.end() ? nullptr : found->second;
}
}

esp_gsp_err_t desktop_invoke(esp_gsp_handle_t handle, std::function<esp_gsp_err_t()> operation)
{
    auto executor = find_executor(handle);
    if (!executor) {
        return ESP_GSP_ERR_INVALID_STATE;
    }
    if (std::this_thread::get_id() == executor->owner) {
        return operation();
    }
    auto promise = std::make_shared<std::promise<esp_gsp_err_t>>();
    auto future = promise->get_future();
    {
        std::lock_guard lock(executor->mutex);
        if (executor->stopping || executor->failed) {
            return ESP_GSP_ERR_INVALID_STATE;
        }
        executor->jobs.push_back([promise, operation = std::move(operation)] { promise->set_value(operation()); });
    }
    return future.get();
}

esp_gsp_err_t desktop_property_info(esp_gsp_handle_t handle, gsp_component_key_t component,
                                    gsp_property_key_t property, gsp_property_info_t *info)
{
    if (info == nullptr) {
        return ESP_GSP_ERR_INVALID_ARG;
    }
    auto executor = find_executor(handle);
    if (!executor) {
        return ESP_GSP_ERR_INVALID_STATE;
    }
    // desktop_api.cpp dispatches this lookup through desktop_invoke, alongside poll on the owner thread.
    const auto scene_id = gsp_sim_bridge_current_scene(handle);
    for (const auto &scene_value : executor->bundle->metadata.at("scenes").as_array()) {
        const auto &scene = scene_value.as_object();
        if (scene.at("scene_id").to_number<uint16_t>() != scene_id) {
            continue;
        }
        for (const auto &object_value : scene.at("objects").as_array()) {
            const auto &object = object_value.as_object();
            if (object.at("component_key").to_number<uint32_t>() != component) {
                continue;
            }
            for (const auto &property_value : object.at("properties").as_array()) {
                const auto &entry = property_value.as_object();
                if (entry.at("property_key").to_number<uint32_t>() != property) {
                    continue;
                }
                const std::string kind(entry.at("value_kind").as_string());
                *info = {};
                info->key = property;
                // Match the compiler-authored tagged type and range; generic callers only use these fields.
                if (kind == "i32") {
                    info->value_type = GSP_VALUE_I32;
                    info->storage = GSP_PROPERTY_STORAGE_I32;
                } else if (kind == "u32") {
                    info->value_type = GSP_VALUE_U32;
                    info->storage = GSP_PROPERTY_STORAGE_U32;
                } else if (kind == "color") {
                    info->value_type = GSP_VALUE_COLOR;
                    info->storage = GSP_PROPERTY_STORAGE_COLOR;
                } else if (kind == "bool") {
                    info->value_type = GSP_VALUE_BOOL;
                    info->storage = GSP_PROPERTY_STORAGE_VISIBLE;
                } else {
                    return ESP_GSP_ERR_NOT_SUPPORTED;
                }
                info->min_value = entry.at("min").to_number<int64_t>();
                info->max_value = entry.at("max").to_number<int64_t>();
                return ESP_GSP_OK;
            }
        }
    }
    return ESP_GSP_ERR_NOT_FOUND;
}

class Session::Impl {
public:
    std::shared_ptr<Executor> executor;
    std::shared_ptr<const Bundle> bundle;
};

Session::Session() : impl_(std::make_shared<Impl>()) {}
Session::~Session()
{
    if (impl_ && impl_->executor && impl_->executor->owner == std::this_thread::get_id()) {
        auto executor = impl_->executor;
        executor->stopping = true;
        executor->thread.detach();
        impl_->executor.reset();
        return;
    }
    (void)stop();
}

bool Session::start(const esp_gsp_config_t &config, const DisplayLease &display,
                    EventCallback on_event, void *user_ctx, std::shared_ptr<void> callback_owner,
                    std::shared_ptr<const void> bundle_owner)
{
    return start_impl(config, {}, display, on_event, user_ctx, std::move(callback_owner), std::move(bundle_owner));
}

bool Session::start(std::shared_ptr<const Bundle> bundle, const DisplayLease &display,
                    EventCallback on_event, void *user_ctx, std::shared_ptr<void> callback_owner)
{
    if (!bundle) {
        return false;
    }
    const auto config = bundle->config();
    return start_impl(config, std::move(bundle), display, on_event, user_ctx, std::move(callback_owner), {});
}

bool Session::start_impl(const esp_gsp_config_t &config, std::shared_ptr<const Bundle> owned_bundle,
                         const DisplayLease &display, EventCallback on_event, void *user_ctx,
                         std::shared_ptr<void> callback_owner, std::shared_ptr<const void> bundle_owner)
{
    if (running() || !display.desktop || !config.bundle || config.bundle_size == 0 ||
            (user_ctx != nullptr && !callback_owner) || (!owned_bundle && !bundle_owner)) {
        return false;
    }
    auto bundle = find_desktop_bundle({reinterpret_cast<const std::byte *>(config.bundle), config.bundle_size});
    if (!bundle) {
        return false;
    }
    auto executor = std::make_shared<Executor>();
    executor->bundle = std::move(bundle);
    executor->display = display.desktop;
    executor->callback_owner = std::move(callback_owner);
    executor->bundle_owner = std::move(bundle_owner);
    auto ready = std::make_shared<std::promise<bool>>();
    auto result = ready->get_future();
    executor->thread = std::thread([executor, ready, on_event, user_ctx] {
        executor->owner = std::this_thread::get_id();
        auto &display = executor->display->implementation();
        const auto endpoint = display.start(*executor->bundle);
        if (!endpoint || gsp_sim_bridge_open(endpoint->c_str(), &executor->handle) != ESP_GSP_OK)
        {
            if (endpoint) {
                std::lock_guard lock(display.mutex);
                display.failure = "Cannot connect the official GSP simulator bridge";
            }
            display.stop();
            ready->set_value(false);
            return;
        }
        {
            std::lock_guard lock(executors_mutex);
            executors[executor->handle] = executor;
        }
        if (esp_gsp_on_event(executor->handle, on_event, user_ctx) != ESP_GSP_OK)
        {
            executor->failed = true;
        }
        ready->set_value(!executor->failed);
        while (!executor->stopping && !executor->failed)
        {
            std::deque<std::function<void()>> jobs;
            {
                std::lock_guard lock(executor->mutex);
                jobs.swap(executor->jobs);
            }
            for (auto &job : jobs) {
                job();
            }
            if (gsp_sim_bridge_poll(executor->handle, 5) != ESP_GSP_OK) {
                std::lock_guard lock(executor->mutex);
                executor->failed = true;
                std::lock_guard display_lock(display.mutex);
                display.failure = "GSP bridge polling failed; see " + display.run_dir + "/renderer.log";
            }
        }
        // Drain accepted synchronous calls before closing their handle.
        std::deque<std::function<void()>> jobs;
        {
            std::lock_guard lock(executor->mutex);
            jobs.swap(executor->jobs);
        }
        for (auto &job : jobs)
        {
            job();
        }
        esp_gsp_on_event(executor->handle, nullptr, nullptr);
        {
            std::lock_guard lock(executors_mutex);
            executors.erase(executor->handle);
        }
        gsp_sim_bridge_close(executor->handle);
        display.stop();
    });
    if (!result.get()) {
        executor->stopping = true;
        executor->thread.join();
        return false;
    }
    impl_->executor = std::move(executor);
    impl_->bundle = std::move(owned_bundle);
    return true;
}

bool Session::stop()
{
    if (!impl_->executor) {
        return true;
    }
    auto executor = impl_->executor;
    if (executor->owner == std::this_thread::get_id()) {
        return false;
    }
    {
        std::lock_guard lock(executor->mutex);
        executor->stopping = true;
    }
    executor->thread.join();
    impl_->executor.reset();
    impl_->bundle.reset();
    return true;
}

bool Session::running() const
{
    return impl_ && impl_->executor != nullptr;
}
esp_gsp_handle_t Session::handle() const
{
    return running() && !impl_->executor->failed && !impl_->executor->stopping
           ? impl_->executor->handle : nullptr;
}
} // namespace esp_brookesia::gui::gsp
