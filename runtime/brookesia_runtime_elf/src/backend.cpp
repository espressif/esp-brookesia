/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_elf/backend.hpp"

#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <boost/thread.hpp>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/runtime_elf/macro_configs.h"
#include "brookesia/runtime_manager/detail/native_utils.hpp"
#include "brookesia/service_helper.hpp"
#if !BROOKESIA_RUNTIME_ELF_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#if defined(BROOKESIA_RUNTIME_ELF_HAS_ESP_ELF)
extern "C" {
#include "esp_elf.h"
#include "esp_err.h"
}
#endif

namespace esp_brookesia::runtime::elf {

namespace {

constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

std::expected<std::vector<uint8_t>, std::string> read_binary_file(const std::string &path)
{
    auto info = service::helper::Storage::fs_stat(path, STORAGE_FS_TIMEOUT_MS);
    if (!info) {
        return std::unexpected("Failed to stat ELF app: " + path + ", error: " + info.error());
    }
    if (!info->exists || info->type != service::helper::Storage::FileType::File || info->size == 0) {
        return std::unexpected("ELF app is empty: " + path);
    }

    std::vector<uint8_t> data(static_cast<size_t>(info->size));
    auto read_result = service::helper::Storage::fs_read(
                           path, service::RawBuffer(data.data(), data.size()), STORAGE_FS_TIMEOUT_MS
                       );
    if (!read_result) {
        return std::unexpected("Failed to read ELF app: " + path + ", error: " + read_result.error());
    }
    if (read_result.value() != data.size()) {
        return std::unexpected("ELF app read size mismatch: " + path);
    }
    return data;
}

std::string esp_error_to_string(int error)
{
#if defined(BROOKESIA_RUNTIME_ELF_HAS_ESP_ELF)
    return std::string(esp_err_to_name(error)) + "(" + std::to_string(error) + ")";
#else
    return std::to_string(error);
#endif
}

} // namespace

#if defined(BROOKESIA_RUNTIME_ELF_HAS_ESP_ELF)

class Backend::Impl {
public:
    struct AppRecord {
        AppConfig config;
        std::vector<uint8_t> payload;
        esp_elf_t elf = {};
        AppState app_state = AppState::Unloaded;
        bool elf_initialized = false;
    };

    std::vector<NativeModule> native_modules_;
    std::map<AppId, AppRecord> apps_;
    bool initialized_ = false;
};

Backend::Backend()
    : IRuntimeBackend(BackendType::Elf, {
        .name = BROOKESIA_RUNTIME_ELF_BACKEND_NAME,
    })
    , impl_(std::make_unique<Impl>())
{}

Backend::~Backend()
{
    deinit();
}

void Backend::set_native_modules(std::vector<NativeModule> modules)
{
    impl_->native_modules_ = std::move(modules);
}

std::expected<void, std::string> Backend::init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%", BROOKESIA_RUNTIME_ELF_VER_MAJOR, BROOKESIA_RUNTIME_ELF_VER_MINOR,
        BROOKESIA_RUNTIME_ELF_VER_PATCH
    );
    BROOKESIA_LOGD("Params: initialized(%1%)", impl_->initialized_);
    impl_->initialized_ = true;
    BROOKESIA_LOGD("ELF backend initialized");
    return {};
}

void Backend::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_count(%1%), initialized(%2%)", impl_->apps_.size(), impl_->initialized_);
    for (auto &[id, app] : impl_->apps_) {
        if (app.elf_initialized) {
            esp_elf_deinit(&app.elf);
            app.elf_initialized = false;
        }
    }
    impl_->apps_.clear();
    impl_->initialized_ = false;
    BROOKESIA_LOGD("ELF backend deinitialized");
}

std::expected<void, std::string> Backend::load_app(AppId id, const AppConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: app_id(%1%), config(%2%), module_count(%3%)",
        id, config, impl_->native_modules_.size()
    );
    if (!impl_->initialized_) {
        BROOKESIA_LOGW("ELF app load failed: backend is not initialized, id(%1%)", id);
        return std::unexpected("ELF backend is not initialized");
    }
    if (impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("ELF app load failed: id(%1%) already exists", id);
        return std::unexpected("ELF app id already exists");
    }

    const std::string entry_path = detail::make_entry_path(config);
    auto payload = read_binary_file(entry_path);
    if (!payload) {
        BROOKESIA_LOGE("ELF app load failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, payload.error());
        return std::unexpected(payload.error());
    }

    Impl::AppRecord record;
    record.config = config;
    record.payload = std::move(payload.value());
    int result = esp_elf_init(&record.elf);
    if (result != ESP_OK) {
        const std::string error = "Failed to initialize ELF object: " + esp_error_to_string(result);
        BROOKESIA_LOGE("ELF app load failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, error);
        return std::unexpected(error);
    }
    record.elf_initialized = true;

    {
        // Since the esp_elf_relocate will operation the flash memory, we need to ensure the stack is in internal memory.
        BROOKESIA_THREAD_CONFIG_GUARD({
            .stack_in_ext = false,
        });
        boost::thread([&result, &record]() {
            result = esp_elf_relocate(&record.elf, record.payload.data());
        }).join();
    }
    if (result != ESP_OK) {
        const std::string error = "Failed to relocate ELF app: " + esp_error_to_string(result);
        esp_elf_deinit(&record.elf);
        record.elf_initialized = false;
        BROOKESIA_LOGE("ELF app load failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, error);
        return std::unexpected(error);
    }

    record.app_state = AppState::Loaded;
    impl_->apps_.emplace(id, std::move(record));
    BROOKESIA_LOGD("ELF app loaded: id(%1%), entry(%2%)", id, entry_path);
    return {};
}

std::expected<void, std::string> Backend::unload_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("ELF app unload failed: id(%1%) not found", id);
        return std::unexpected("ELF app not found");
    }

    if (it->second.elf_initialized) {
        esp_elf_deinit(&it->second.elf);
        it->second.elf_initialized = false;
    }
    impl_->apps_.erase(it);
    BROOKESIA_LOGD("ELF app unloaded: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::start_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("ELF app start failed: id(%1%) not found", id);
        return std::unexpected("ELF app not found");
    }
    if (!it->second.elf_initialized) {
        BROOKESIA_LOGW("ELF app start failed: id(%1%) is not relocated", id);
        return std::unexpected("ELF app is not relocated");
    }

    std::vector<std::string> arg_storage = it->second.config.arguments;
    std::vector<char *> argv;
    argv.reserve(arg_storage.size());
    for (auto &arg : arg_storage) {
        argv.push_back(arg.data());
    }

    BROOKESIA_LOGD("ELF app starting: id(%1%), arg_count(%2%)", id, argv.size());
    it->second.app_state = AppState::Running;
    const int result = esp_elf_request(
                           &it->second.elf, 0, static_cast<int>(argv.size()), argv.empty() ? nullptr : argv.data()
                       );
    if (result != ESP_OK) {
        const std::string error = "Failed to request ELF app: " + esp_error_to_string(result);
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("ELF app start failed: id(%1%), error(%2%)", id, error);
        return std::unexpected(error);
    }

    it->second.app_state = AppState::Stopped;
    BROOKESIA_LOGD("ELF app finished: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::stop_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("ELF app stop failed: id(%1%) not found", id);
        return std::unexpected("ELF app not found");
    }

    if (it->second.app_state == AppState::Running) {
        BROOKESIA_LOGW("ELF app stop requested: id(%1%), synchronous ELF execution cannot be interrupted", id);
    }
    it->second.app_state = AppState::Stopped;
    BROOKESIA_LOGD("ELF app stopped: id(%1%)", id);
    return {};
}

std::expected<NativeValue, std::string> Backend::call_function(
    AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
)
{
    (void)module_name;
    (void)function_name;
    (void)args;
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    if (!impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("ELF function call failed: id(%1%) not found", id);
        return std::unexpected("ELF app not found");
    }
    BROOKESIA_LOGW("ELF function call failed: id(%1%), function calls are not supported", id);
    return std::unexpected("ELF runtime does not support function calls");
}

AppState Backend::get_app_state(AppId id) const
{
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        return AppState::Unloaded;
    }
    return it->second.app_state;
}

#else
#error "brookesia_runtime_elf requires ESP-IDF elf_loader support"
#endif

#if BROOKESIA_RUNTIME_ELF_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    IRuntimeBackend, Backend, Backend::get_instance().get_attributes().name, Backend::get_instance(),
    BROOKESIA_RUNTIME_ELF_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::runtime::elf
