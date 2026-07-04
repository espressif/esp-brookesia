/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/system_core/macro_configs.h"
#if !BROOKESIA_SYSTEM_CORE_SYSTEM_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "private/system/impl.hpp"

#include <charconv>

#include "brookesia/service_manager/common.hpp"
#include "brookesia/system_core/app/package.hpp"

namespace esp_brookesia::system::core {

namespace {

constexpr const char *RUNTIME_APP_ID_CALL_CONTEXT_KEY = "brookesia.system.runtime_app_id";

bool is_missing_runtime_function_error(std::string_view error)
{
    return (error.find("not found") != std::string_view::npos) ||
           (error.find("nil value") != std::string_view::npos);
}

std::expected<runtime::AppId, std::string> get_runtime_app_id_from_call_context()
{
    auto value = service::get_current_call_context_value(RUNTIME_APP_ID_CALL_CONTEXT_KEY);
    if (!value || value->empty()) {
        return std::unexpected("No active runtime app context");
    }

    runtime::AppId id = runtime::INVALID_APP_ID;
    const auto *begin = value->data();
    const auto *end = begin + value->size();
    auto result = std::from_chars(begin, end, id);
    if ((result.ec != std::errc()) || (result.ptr != end) || (id == runtime::INVALID_APP_ID)) {
        return std::unexpected("Invalid runtime app id call context");
    }
    return id;
}

} // namespace

std::expected<void, std::string> System::Impl::ensure_runtime_initialized()
{
    if (runtime_initialized_) {
        return {};
    }
    if (!runtime_) {
        return std::unexpected("Runtime manager is not available");
    }
    auto result = runtime_->init();
    if (!result) {
        return result;
    }
    runtime_initialized_ = true;
    return {};
}


std::expected<void, std::string> System::Impl::call_runtime_lifecycle(
    AppRecord &record,
    std::string_view function_name,
    const runtime::NativeArgs &args
)
{
    if (!runtime_ || !record.runtime_app_id.has_value()) {
        return {};
    }
    auto result = runtime_->call_function(*record.runtime_app_id, APP_MODULE_NAME, function_name, args);
    if (!result) {
        if (is_missing_runtime_function_error(result.error())) {
            return {};
        }
        return std::unexpected(result.error());
    }
    if (const auto *value = std::get_if<bool>(&result.value()); (value != nullptr) && !*value) {
        return std::unexpected("Runtime lifecycle function returned false");
    }
    return {};
}


std::expected<void, std::string> System::Impl::ensure_runtime_loaded(AppRecord &record)
{
    if (record.info.manifest.kind != AppKind::Runtime) {
        return {};
    }
    auto init_result = ensure_runtime_initialized();
    if (!init_result) {
        return init_result;
    }
    if (!record.runtime_loaded) {
        auto config = make_runtime_app_config(record.info.manifest);
        if (!config) {
            return std::unexpected(config.error());
        }
        auto load_result = runtime_->load_app(*config);
        if (!load_result) {
            return std::unexpected(load_result.error());
        }
        record.runtime_app_id = *load_result;
        runtime_to_app_.emplace(*record.runtime_app_id, record.info.app_id);
        record.runtime_loaded = true;
    }
    return {};
}


void System::Impl::unload_runtime(AppRecord &record)
{
    if (!runtime_ || !record.runtime_app_id.has_value()) {
        return;
    }
    auto unload_result = runtime_->unload_app(*record.runtime_app_id);
    if (!unload_result) {
        BROOKESIA_LOGW("Failed to unload runtime app: %1%", unload_result.error());
    }
    runtime_to_app_.erase(*record.runtime_app_id);
    record.runtime_app_id.reset();
    record.runtime_loaded = false;
    record.runtime_started = false;
}


std::expected<AppId, std::string> System::get_current_runtime_app_owner() const
{
    if (!impl_->runtime_function_bridge_) {
        return std::unexpected("Runtime function bridge is not available");
    }
    auto runtime_app_id = impl_->runtime_function_bridge_->get_current_app_id();
    if (!runtime_app_id) {
        runtime_app_id = get_runtime_app_id_from_call_context();
        if (!runtime_app_id) {
            return std::unexpected(runtime_app_id.error());
        }
    }
    auto it = impl_->runtime_to_app_.find(*runtime_app_id);
    if (it == impl_->runtime_to_app_.end()) {
        return std::unexpected("Runtime app owner not found");
    }
    return it->second;
}

} // namespace esp_brookesia::system::core
