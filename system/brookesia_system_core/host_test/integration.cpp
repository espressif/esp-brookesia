/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/service_helper/system/device.hpp"
#include "brookesia/service_helper/system/storage.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/system_core/app/iapp.hpp"
#include "brookesia/system_core/system/system.hpp"

namespace {

using esp_brookesia::service::FunctionParameterMap;
using esp_brookesia::service::FunctionResult;
using esp_brookesia::service::FunctionSchema;
using esp_brookesia::service::FunctionValue;
using esp_brookesia::service::RawBuffer;
using esp_brookesia::service::ServiceBase;
using esp_brookesia::service::ServiceManager;
using DeviceHelper = esp_brookesia::service::helper::Device;
using StorageHelper = esp_brookesia::service::helper::Storage;
using esp_brookesia::system::core::AppGuiDescriptor;
using esp_brookesia::system::core::AppId;
using esp_brookesia::system::core::AppInfo;
using esp_brookesia::system::core::AppKind;
using esp_brookesia::system::core::AppManifest;
using esp_brookesia::system::core::AppManifestService;
using esp_brookesia::system::core::AppState;
using esp_brookesia::system::core::IApp;
using esp_brookesia::system::core::MessageDialogOptions;
using esp_brookesia::system::core::MessageDialogRequestId;
using esp_brookesia::system::core::StoragePartition;
using esp_brookesia::system::core::StorageVolume;
using esp_brookesia::system::core::System;
using FunctionHandlerMap = ServiceBase::FunctionHandlerMap;

constexpr std::string_view MISSING_RPC_NAME = "MissingRpc";
constexpr std::string_view SECOND_MISSING_RPC_NAME = "SecondMissingRpc";
constexpr std::string_view STAGED_APP_ID = "staged-incompatible";
constexpr std::string_view NATIVE_APP_ID = "native-incompatible";

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        std::error_code error;
        const auto temporary_root = std::filesystem::temp_directory_path(error);
        if (error) {
            error_ = error.message();
            return;
        }
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = temporary_root / ("brookesia-system-gate-test-" + std::to_string(suffix));
        if (!std::filesystem::create_directories(path_, error) || error) {
            error_ = error ? error.message() : "temporary directory already exists";
            path_.clear();
        }
    }

    ~TemporaryDirectory()
    {
        if (path_.empty()) {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory &) = delete;
    TemporaryDirectory &operator=(const TemporaryDirectory &) = delete;

    bool is_valid() const
    {
        return !path_.empty();
    }

    const std::filesystem::path &get_path() const
    {
        return path_;
    }

    const std::string &get_error() const
    {
        return error_;
    }

private:
    std::filesystem::path path_;
    std::string error_;
};

bool require(bool condition, std::string_view message)
{
    if (!condition) {
        std::cerr << message << '\n';
        return false;
    }
    return true;
}

template <typename T>
const T *get_parameter(const FunctionParameterMap &parameters, std::string_view name)
{
    const auto it = parameters.find(std::string(name));
    return it == parameters.end() ? nullptr : std::get_if<T>(&it->second);
}

FunctionResult make_error(std::string message)
{
    return {
        .success = false,
        .error_message = std::move(message),
    };
}

FunctionResult make_success(std::optional<FunctionValue> data = std::nullopt)
{
    return {
        .success = true,
        .data = std::move(data),
    };
}

boost::json::object make_file_info(const std::filesystem::path &path)
{
    std::error_code error;
    const auto status = std::filesystem::status(path, error);
    if (error || !std::filesystem::exists(status)) {
        return {
            {"type", "Missing"},
            {"size", 0},
            {"mtime_ms", 0},
            {"exists", false},
        };
    }

    std::string_view type = "Other";
    uint64_t size = 0;
    if (std::filesystem::is_regular_file(status)) {
        type = "File";
        size = std::filesystem::file_size(path, error);
        if (error) {
            size = 0;
        }
    } else if (std::filesystem::is_directory(status)) {
        type = "Directory";
    }
    return {
        {"type", type},
        {"size", size},
        {"mtime_ms", 0},
        {"exists", true},
    };
}

class HostStorageService final: public ServiceBase {
public:
    HostStorageService()
        : ServiceBase(Attributes{
            .name = std::string(StorageHelper::get_name()),
            .description = "Host filesystem service for System gate integration tests.",
            .version = "0.8.2",
        })
    {}

    int get_mutation_count() const
    {
        return mutation_count_.load();
    }

    void replace_file_after_next_read(
        std::filesystem::path target,
        std::filesystem::path replacement
    )
    {
        std::lock_guard lock(file_replacement_mutex_);
        file_replacement_target_ = std::move(target);
        file_replacement_source_ = std::move(replacement);
    }

    std::vector<FunctionSchema> get_function_schemas() override
    {
        constexpr std::string_view REQUIRED_SCHEMAS[] = {
            "KVGet",
            "GetFileSystems",
            "FSStat",
            "FSList",
            "FSMkdir",
            "FSReadText",
            "FSRead",
            "FSWriteText",
            "FSWrite",
            "FSRemove",
            "FSRename",
            "FSCopyTree",
            "MakeKVKey",
            "MakeKVNamespace",
        };

        std::vector<FunctionSchema> result;
        for (const auto &schema : StorageHelper::get_function_schemas()) {
            if (std::find(std::begin(REQUIRED_SCHEMAS), std::end(REQUIRED_SCHEMAS), schema.name) !=
                    std::end(REQUIRED_SCHEMAS)) {
                result.push_back(schema);
            }
        }
        return result;
    }

protected:
    FunctionHandlerMap get_function_handlers() override
    {
        auto kv_get = [](FunctionParameterMap &&) {
            return make_success(boost::json::object{});
        };
        auto get_file_systems = [](FunctionParameterMap &&) {
            return make_success(boost::json::array{});
        };
        auto fs_stat = [](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            return path == nullptr ? make_error("Missing Path") : make_success(make_file_info(*path));
        };
        auto fs_list = [](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            std::error_code error;
            boost::json::array entries;
            for (std::filesystem::directory_iterator it(*path, error), end; !error && it != end; it.increment(error)) {
                entries.emplace_back(boost::json::object{
                    {"name", it->path().filename().generic_string()},
                    {"info", make_file_info(it->path())},
                });
            }
            return error ? make_error(error.message()) : make_success(std::move(entries));
        };
        auto fs_mkdir = [this](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            std::filesystem::create_directories(*path, error);
            return error ? make_error(error.message()) : make_success();
        };
        auto fs_read_text = [](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            std::ifstream input(*path, std::ios::binary);
            if (!input.is_open()) {
                return make_error("Failed to open file: " + *path);
            }
            std::string contents{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>()
            };
            return make_success(std::move(contents));
        };
        auto fs_read = [this](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            const auto *buffer = get_parameter<RawBuffer>(parameters, "Buffer");
            if (path == nullptr || buffer == nullptr || buffer->to_ptr<uint8_t>() == nullptr) {
                return make_error("Invalid FSRead parameters");
            }
            std::ifstream input(*path, std::ios::binary);
            if (!input.is_open()) {
                return make_error("Failed to open file: " + *path);
            }
            input.read(
                reinterpret_cast<char *>(buffer->to_ptr<uint8_t>()),
                static_cast<std::streamsize>(buffer->data_size)
            );
            if (!input && !input.eof()) {
                return make_error("Failed to read file: " + *path);
            }
            const auto read_count = input.gcount();
            input.close();
            auto replacement_result = replace_file_if_armed(*path);
            if (!replacement_result) {
                return make_error(replacement_result.error());
            }
            return make_success(static_cast<double>(read_count));
        };
        auto fs_write_text = [this](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            const auto *data = get_parameter<std::string>(parameters, "Data");
            if (path == nullptr || data == nullptr) {
                return make_error("Invalid FSWriteText parameters");
            }
            mutation_count_.fetch_add(1);
            std::ofstream output(*path, std::ios::binary | std::ios::trunc);
            output << *data;
            return output ? make_success() : make_error("Failed to write file: " + *path);
        };
        auto fs_write = [this](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            const auto *data = get_parameter<RawBuffer>(parameters, "Data");
            if (path == nullptr || data == nullptr) {
                return make_error("Invalid FSWrite parameters");
            }
            mutation_count_.fetch_add(1);
            std::ofstream output(*path, std::ios::binary | std::ios::trunc);
            output.write(
                reinterpret_cast<const char *>(data->to_const_ptr<uint8_t>()),
                static_cast<std::streamsize>(data->data_size)
            );
            return output ? make_success() : make_error("Failed to write file: " + *path);
        };
        auto fs_remove = [this](FunctionParameterMap &&parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            std::filesystem::remove_all(*path, error);
            return error ? make_error(error.message()) : make_success();
        };
        auto fs_rename = [this](FunctionParameterMap &&parameters) {
            const auto *from = get_parameter<std::string>(parameters, "From");
            const auto *to = get_parameter<std::string>(parameters, "To");
            if (from == nullptr || to == nullptr) {
                return make_error("Invalid FSRename parameters");
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            std::filesystem::rename(*from, *to, error);
            return error ? make_error(error.message()) : make_success();
        };
        auto fs_copy_tree = [this](FunctionParameterMap &&parameters) {
            const auto *from = get_parameter<std::string>(parameters, "From");
            const auto *to = get_parameter<std::string>(parameters, "To");
            const auto *overwrite = get_parameter<bool>(parameters, "Overwrite");
            if (from == nullptr || to == nullptr || overwrite == nullptr) {
                return make_error("Invalid FSCopyTree parameters");
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            auto options = std::filesystem::copy_options::recursive;
            if (*overwrite) {
                options |= std::filesystem::copy_options::overwrite_existing;
            }
            std::filesystem::copy(*from, *to, options, error);
            return error ? make_error(error.message()) : make_success();
        };
        auto make_kv_name = [](FunctionParameterMap &&parameters) {
            const auto *parts = get_parameter<boost::json::array>(parameters, "Parts");
            const auto *separator = get_parameter<std::string>(parameters, "Separator");
            if (parts == nullptr || separator == nullptr) {
                return make_error("Invalid KV name parameters");
            }
            std::string name;
            for (const auto &part : *parts) {
                if (!part.is_string()) {
                    return make_error("KV name part is not a string");
                }
                if (!name.empty()) {
                    name += *separator;
                }
                name += part.as_string().c_str();
            }
            return make_success(boost::json::object{
                {"name", name},
                {"original_name", name},
                {"hashed", false},
                {"warning", ""},
            });
        };

        return {
            {"KVGet", std::move(kv_get)},
            {"GetFileSystems", std::move(get_file_systems)},
            {"FSStat", std::move(fs_stat)},
            {"FSList", std::move(fs_list)},
            {"FSMkdir", std::move(fs_mkdir)},
            {"FSReadText", std::move(fs_read_text)},
            {"FSRead", std::move(fs_read)},
            {"FSWriteText", std::move(fs_write_text)},
            {"FSWrite", std::move(fs_write)},
            {"FSRemove", std::move(fs_remove)},
            {"FSRename", std::move(fs_rename)},
            {"FSCopyTree", std::move(fs_copy_tree)},
            {"MakeKVKey", make_kv_name},
            {"MakeKVNamespace", std::move(make_kv_name)},
        };
    }

private:
    std::expected<void, std::string> replace_file_if_armed(const std::filesystem::path &path)
    {
        std::filesystem::path replacement;
        {
            std::lock_guard lock(file_replacement_mutex_);
            if (file_replacement_target_.empty() ||
                    (file_replacement_target_.lexically_normal() != path.lexically_normal())) {
                return {};
            }
            replacement = std::move(file_replacement_source_);
            file_replacement_target_.clear();
        }

        std::error_code error;
        std::filesystem::copy_file(
            replacement,
            path,
            std::filesystem::copy_options::overwrite_existing,
            error
        );
        if (error) {
            return std::unexpected("Failed to replace package after manifest read: " + error.message());
        }
        return {};
    }

    std::atomic<int> mutation_count_{0};
    std::mutex file_replacement_mutex_;
    std::filesystem::path file_replacement_target_;
    std::filesystem::path file_replacement_source_;
};

class HostDeviceService final: public ServiceBase {
public:
    HostDeviceService()
        : ServiceBase(Attributes{
            .name = std::string(DeviceHelper::get_name()),
            .description = "Host device dependency for System gate integration tests.",
            .version = "0.8.2",
        })
    {}
};

class LifecycleProbeApp final: public IApp {
public:
    AppManifest get_manifest() const override
    {
        AppManifest manifest;
        manifest.id = std::string(NATIVE_APP_ID);
        manifest.name = "Native incompatible";
        manifest.version = "1.0.0";
        manifest.services = {{
                .name = std::string(MISSING_RPC_NAME),
                .version = "1.0.0",
            }};
        return manifest;
    }

    AppGuiDescriptor get_gui_descriptor() const override
    {
        return {};
    }

    std::expected<void, std::string> on_install(esp_brookesia::system::core::AppContext &) override
    {
        install_count_.fetch_add(1);
        return {};
    }

    void on_uninstall(esp_brookesia::system::core::AppContext &) override
    {
        uninstall_count_.fetch_add(1);
    }

    std::expected<void, std::string> on_start(esp_brookesia::system::core::AppContext &) override
    {
        start_count_.fetch_add(1);
        return {};
    }

    int get_install_count() const
    {
        return install_count_.load();
    }

    int get_uninstall_count() const
    {
        return uninstall_count_.load();
    }

    int get_start_count() const
    {
        return start_count_.load();
    }

private:
    std::atomic<int> install_count_{0};
    std::atomic<int> uninstall_count_{0};
    std::atomic<int> start_count_{0};
};

class TestSystem final: public System {
public:
    int get_start_failure_count() const
    {
        return start_failure_count_.load();
    }

    std::optional<std::pair<MessageDialogRequestId, MessageDialogOptions>> get_last_dialog() const
    {
        std::lock_guard lock(dialog_mutex_);
        return last_dialog_;
    }

protected:
    void on_app_start_failed(const AppInfo &, std::string_view) override
    {
        start_failure_count_.fetch_add(1);
    }

    std::expected<void, std::string> on_show_message_dialog(
        AppId,
        MessageDialogRequestId request_id,
        const MessageDialogOptions &options
    ) override
    {
        std::lock_guard lock(dialog_mutex_);
        last_dialog_ = std::pair{request_id, options};
        return {};
    }

private:
    std::atomic<int> start_failure_count_{0};
    mutable std::mutex dialog_mutex_;
    std::optional<std::pair<MessageDialogRequestId, MessageDialogOptions>> last_dialog_;
};

bool write_text_file(const std::filesystem::path &path, std::string_view contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << contents;
    return static_cast<bool>(output);
}

bool create_staged_runtime_app(const std::filesystem::path &storage_root)
{
    const auto app_root = storage_root / "apps" / STAGED_APP_ID;
    const std::string manifest =
        R"({"package":{"id":")" + std::string(STAGED_APP_ID) +
        R"(","name":{"en":"Staged incompatible"},"version":"1.0.0"},)"
        R"("runtime":{"type":"JavaScript","entry":"app.js","resource_dir":"res"},)"
        R"("services":[{"name":")" + std::string(MISSING_RPC_NAME) +
        R"(","version":"1.0.0"},{"name":")" + std::string(SECOND_MISSING_RPC_NAME) +
        R"(","version":"1.0.0"}]})";
    return write_text_file(app_root / "manifest.json", manifest) &&
           write_text_file(app_root / "res" / "profile.json", "{}") &&
           write_text_file(app_root / "data" / "keep.txt", "preserve-existing-app");
}

void append_u16(std::vector<uint8_t> &output, uint16_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8U));
}

void append_u32(std::vector<uint8_t> &output, uint32_t value)
{
    output.push_back(static_cast<uint8_t>(value));
    output.push_back(static_cast<uint8_t>(value >> 8U));
    output.push_back(static_cast<uint8_t>(value >> 16U));
    output.push_back(static_cast<uint8_t>(value >> 24U));
}

bool create_manifest_only_bpk(const std::filesystem::path &path, std::string_view manifest)
{
    constexpr std::string_view FILE_NAME = "manifest.json";
    std::vector<uint8_t> data;
    append_u32(data, 0x04034b50U);
    append_u16(data, 20);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u32(data, 0);
    append_u32(data, static_cast<uint32_t>(manifest.size()));
    append_u32(data, static_cast<uint32_t>(manifest.size()));
    append_u16(data, static_cast<uint16_t>(FILE_NAME.size()));
    append_u16(data, 0);
    data.insert(data.end(), FILE_NAME.begin(), FILE_NAME.end());
    data.insert(data.end(), manifest.begin(), manifest.end());

    const auto central_offset = static_cast<uint32_t>(data.size());
    append_u32(data, 0x02014b50U);
    append_u16(data, 20);
    append_u16(data, 20);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u32(data, 0);
    append_u32(data, static_cast<uint32_t>(manifest.size()));
    append_u32(data, static_cast<uint32_t>(manifest.size()));
    append_u16(data, static_cast<uint16_t>(FILE_NAME.size()));
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u32(data, 0);
    append_u32(data, 0);
    data.insert(data.end(), FILE_NAME.begin(), FILE_NAME.end());

    const auto central_size = static_cast<uint32_t>(data.size()) - central_offset;
    append_u32(data, 0x06054b50U);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, 1);
    append_u16(data, 1);
    append_u32(data, central_size);
    append_u32(data, central_offset);
    append_u16(data, 0);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(output);
}

std::optional<AppInfo> find_app(const System &system, std::string_view manifest_id)
{
    const auto apps = system.list_apps();
    const auto it = std::find_if(apps.begin(), apps.end(), [manifest_id](const auto &app) {
        return app.manifest.id == manifest_id;
    });
    return it == apps.end() ? std::nullopt : std::optional<AppInfo>(*it);
}

bool verify_blocked_start(
    TestSystem &system,
    AppId app_id,
    AppState expected_state,
    std::string_view context
)
{
    const int failures_before = system.get_start_failure_count();
    auto start_result = system.start_app(app_id);
    if (!require(!start_result.has_value(), std::string(context) + " unexpectedly started") ||
            !require(
                start_result.error().find(MISSING_RPC_NAME) != std::string::npos,
                std::string(context) + " did not return the dependency failure"
            )) {
        return false;
    }
    const auto app = system.get_app(app_id);
    return require(app.has_value(), std::string(context) + " disappeared after blocked start") &&
           require(app->state == expected_state, std::string(context) + " state changed after blocked start") &&
           require(
               app->last_error.find(MISSING_RPC_NAME) != std::string::npos,
               std::string(context) + " did not retain the dependency error"
           ) &&
           require(
               system.get_start_failure_count() == (failures_before + 1),
               std::string(context) + " did not invoke the start-failed hook"
           );
}

bool run_tests()
{
    TemporaryDirectory temporary_directory;
    if (!require(
            temporary_directory.is_valid(),
            "Failed to create integration test directory: " + temporary_directory.get_error()
        ) ||
            !require(
                create_staged_runtime_app(temporary_directory.get_path()),
                "Failed to create staged runtime app"
            )) {
        return false;
    }

    auto &manager = ServiceManager::get_instance();
    if (!require(manager.init(), "Failed to initialize ServiceManager")) {
        return false;
    }
    auto storage = std::make_shared<HostStorageService>();
    if (!require(manager.add_service(storage), "Failed to add host Storage service")) {
        manager.deinit();
        return false;
    }
    auto device = std::make_shared<HostDeviceService>();
    if (!require(manager.add_service(device), "Failed to add host Device service")) {
        manager.deinit();
        return false;
    }

    TestSystem system;
    System::Config config;
    config.install_registered_apps = false;
    config.install_package_apps = true;
    config.storage.internal_override = StorageVolume{
        .id = "internal",
        .partition = StoragePartition::Internal,
        .mount_point = temporary_directory.get_path().generic_string(),
        .root_path = temporary_directory.get_path().generic_string(),
        .available = true,
    };
    auto init_result = system.init(std::move(config));
    if (!require(init_result.has_value(), init_result ? "" : "System init failed: " + init_result.error())) {
        system.deinit();
        manager.stop();
        manager.deinit();
        return false;
    }
    auto system_start_result = system.start();
    bool passed = require(
                      system_start_result.has_value(),
                      system_start_result ? "" : "System start failed: " + system_start_result.error()
                  );

    const auto staged_app = find_app(system, STAGED_APP_ID);
    passed &= require(staged_app.has_value(), "Incompatible staged app prevented System init or was not installed");
    if (staged_app) {
        passed &= require(
                      staged_app->state == AppState::Installed,
                      "Staged app was not initially Installed"
                  );
        passed &= verify_blocked_start(system, staged_app->app_id, AppState::Installed, "Staged runtime app");
        const auto dialog = system.get_last_dialog();
        passed &= require(dialog.has_value(), "Blocked staged app start did not show a warning dialog");
        if (dialog) {
            passed &= require(dialog->second.auto_close_ms == 0, "Service requirement dialog was not persistent");
            passed &= require(
                          dialog->second.informative_text.find(MISSING_RPC_NAME) != std::string::npos,
                          "Service requirement dialog omitted the missing RPC"
                      );
            passed &= require(
                          dialog->second.informative_text.find(SECOND_MISSING_RPC_NAME) != std::string::npos,
                          "Service requirement dialog did not aggregate every missing RPC"
                      );
            auto hide_result = system.hide_system_message_dialog(dialog->first);
            passed &= require(hide_result.has_value(), "Failed to close staged app requirement dialog");

            passed &= verify_blocked_start(
                          system,
                          staged_app->app_id,
                          AppState::Installed,
                          "Retried staged runtime app"
                      );
            const auto retried_dialog = system.get_last_dialog();
            passed &= require(retried_dialog.has_value(), "Retried blocked start did not show a warning dialog");
            if (retried_dialog) {
                passed &= require(
                              retried_dialog->first != dialog->first,
                              "Retried blocked start reused the closed warning dialog"
                          );
                auto retry_hide_result = system.hide_system_message_dialog(retried_dialog->first);
                passed &= require(retry_hide_result.has_value(), "Failed to close retried requirement dialog");
            }
        }
    }

    auto native_app = std::make_shared<LifecycleProbeApp>();
    auto native_install = system.install_app(native_app);
    passed &= require(
        native_install.has_value(),
        native_install ? "" : "Native app install failed: " + native_install.error()
    );
    passed &= require(native_app->get_install_count() == 1, "Native app install lifecycle was not called");
    if (native_install) {
        auto native_start = system.start_app(*native_install);
        passed &= require(
            native_start.has_value(),
            native_start ? "" : "Trusted native app start failed: " + native_start.error()
        );
        passed &= require(native_app->get_start_count() == 1, "Trusted native app did not execute on_start");
    }

    const std::string replacement_manifest =
        R"({"package":{"id":")" + std::string(STAGED_APP_ID) +
        R"(","name":{"en":"Staged replacement"},"version":"2.0.0"},)"
        R"("runtime":{"type":"JavaScript","entry":"app.js"},)"
        R"("services":[{"name":")" + std::string(MISSING_RPC_NAME) +
        R"(","version":"1.0.0"},{"name":")" + std::string(SECOND_MISSING_RPC_NAME) +
        R"(","version":"1.0.0"}]})";
    const auto bpk_path = temporary_directory.get_path() / "incompatible.bpk";
    passed &= require(create_manifest_only_bpk(bpk_path, replacement_manifest), "Failed to create test BPK");

    const int mutations_before = storage->get_mutation_count();
    const auto installing_root = temporary_directory.get_path() / "apps" / ".installing";
    const auto retained_file = temporary_directory.get_path() / "apps" / STAGED_APP_ID / "data" / "keep.txt";
    auto package_install = system.install_runtime_app_package(bpk_path.generic_string(), true);
    passed &= require(!package_install.has_value(), "Incompatible replacement BPK unexpectedly installed");
    if (!package_install) {
        passed &= require(
                      package_install.error().find(MISSING_RPC_NAME) != std::string::npos,
                      "BPK install gate error omitted the missing RPC"
                  );
        passed &= require(
                      package_install.error().find(SECOND_MISSING_RPC_NAME) != std::string::npos,
                      "BPK install gate did not aggregate every missing RPC"
                  );
    }
    passed &= require(
                  storage->get_mutation_count() == mutations_before,
                  "BPK install gate ran after a Storage mutation"
              );
    passed &= require(
                  !std::filesystem::exists(installing_root),
                  "BPK install gate created a staging directory"
              );
    passed &= require(
                  std::filesystem::exists(retained_file),
                  "BPK install gate removed an existing app file"
              );
    if (staged_app) {
        const auto retained_app = system.get_app(staged_app->app_id);
        passed &= require(retained_app.has_value(), "BPK install gate removed the existing app record");
        if (retained_app) {
            passed &= require(
                          retained_app->app_id == staged_app->app_id,
                          "BPK install gate changed the existing App ID"
                      );
            passed &= require(
                          retained_app->manifest.id == STAGED_APP_ID,
                          "BPK install gate replaced the existing app record"
                      );
            passed &= require(
                          retained_app->manifest.version == "1.0.0",
                          "BPK install gate changed the existing app version"
            );
        }
    }

    const std::string compatible_replacement_manifest =
        R"({"package":{"id":")" + std::string(STAGED_APP_ID) +
        R"(","name":{"en":"Compatible replacement"},"version":"2.0.0"},)"
        R"("runtime":{"type":"JavaScript","entry":"app.js"},)"
        R"("services":[{"name":")" + std::string(DeviceHelper::get_name()) +
        R"(","version":"0.8.1"}]})";
    const auto changed_bpk_path = temporary_directory.get_path() / "changed-after-check.bpk";
    passed &= require(
        create_manifest_only_bpk(changed_bpk_path, compatible_replacement_manifest),
        "Failed to create initially compatible BPK"
    );
    storage->replace_file_after_next_read(changed_bpk_path, bpk_path);

    const int staged_mutations_before = storage->get_mutation_count();
    auto changed_package_install = system.install_runtime_app_package(changed_bpk_path.generic_string(), true);
    passed &= require(
        !changed_package_install.has_value(),
        "BPK changed to incompatible services after the first check unexpectedly installed"
    );
    if (!changed_package_install) {
        passed &= require(
            changed_package_install.error().find(MISSING_RPC_NAME) != std::string::npos,
            "Staged manifest recheck error omitted the changed missing RPC"
        );
    }
    passed &= require(
        storage->get_mutation_count() > staged_mutations_before,
        "Changed BPK was rejected before reaching the staged manifest recheck"
    );
    passed &= require(
        std::filesystem::exists(retained_file),
        "Staged manifest recheck removed an existing app file"
    );
    if (staged_app) {
        const auto retained_app = system.get_app(staged_app->app_id);
        passed &= require(retained_app.has_value(), "Staged manifest recheck removed the existing app record");
        if (retained_app) {
            passed &= require(
                retained_app->manifest.version == "1.0.0",
                "Staged manifest recheck replaced the existing app version"
            );
        }
    }

    system.deinit();
    manager.stop();
    manager.deinit();
    return passed;
}

} // namespace

int main()
{
    try {
        if (!run_tests()) {
            return EXIT_FAILURE;
        }
        std::cout << "brookesia_system_core install/start gate integration test passed" << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << "Unhandled integration test exception: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
