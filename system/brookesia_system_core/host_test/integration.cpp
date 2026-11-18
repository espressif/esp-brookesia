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
#include <future>
#include <thread>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(BROOKESIA_GUI_GSP_DESKTOP)
#include "brookesia/gui_gsp/desktop_display.hpp"
#endif
#include "private/runtime/call_context.hpp"
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
constexpr std::string_view UPDATE_APP_ID = "update-target";
constexpr std::string_view INTERRUPTED_APP_ID = "interrupted-update";
constexpr std::string_view BROKEN_APP_ID = "broken-installed";
constexpr std::string_view PARTIAL_APP_ID = "partial-update";
constexpr std::string_view COMMITTED_APP_ID = "committed-update";
constexpr std::string_view UNCHECKED_BACKUP_APP_ID = "unchecked-backup";
constexpr std::string_view UNCHECKED_TARGET_APP_ID = "unchecked-target";
constexpr std::string_view MARKER_APPLIED_APP_ID = "marker-applied";
constexpr std::string_view MARKER_FAULT_APP_ID = "marker-fault";
constexpr std::string_view FAILING_INSTALL_VERSION = "9.9.9";

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

    void fail_stat(const std::filesystem::path &path)
    {
        std::lock_guard lock(failure_mutex_);
        failing_stat_paths_.insert(path.lexically_normal().generic_string());
    }

    // Fails calls whose path contains `pattern`; an empty pattern clears the fault. `after_effect` performs
    // the operation before reporting the error, like a real Storage that creates the file and then fails
    // to allocate its buffer.
    void set_write_text_fault(std::string pattern, bool after_effect)
    {
        std::lock_guard lock(failure_mutex_);
        write_text_fault_ = {std::move(pattern), after_effect};
    }

    void set_rename_fault(std::string from_pattern, bool after_effect)
    {
        std::lock_guard lock(failure_mutex_);
        rename_fault_ = {std::move(from_pattern), after_effect};
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
        auto fs_stat = [this](FunctionParameterMap && parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            if (should_fail_stat(*path)) {
                return make_error("Injected stat failure: " + *path);
            }
            return make_success(make_file_info(*path));
        };
        auto fs_list = [](FunctionParameterMap && parameters) {
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
        auto fs_mkdir = [this](FunctionParameterMap && parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            std::filesystem::create_directories(*path, error);
            return error ? make_error(error.message()) : make_success();
        };
        auto fs_read_text = [](FunctionParameterMap && parameters) {
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
        auto fs_read = [this](FunctionParameterMap && parameters) {
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
        auto fs_write_text = [this](FunctionParameterMap && parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            const auto *data = get_parameter<std::string>(parameters, "Data");
            if (path == nullptr || data == nullptr) {
                return make_error("Invalid FSWriteText parameters");
            }
            const auto fault = match_fault(write_text_fault_, *path);
            if (fault && !*fault) {
                return make_error("Injected write failure: " + *path);
            }
            mutation_count_.fetch_add(1);
            std::ofstream output(*path, std::ios::binary | std::ios::trunc);
            output << *data;
            if (fault) {
                return make_error("Injected write failure after creating: " + *path);
            }
            return output ? make_success() : make_error("Failed to write file: " + *path);
        };
        auto fs_write = [this](FunctionParameterMap && parameters) {
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
        auto fs_remove = [this](FunctionParameterMap && parameters) {
            const auto *path = get_parameter<std::string>(parameters, "Path");
            if (path == nullptr) {
                return make_error("Missing Path");
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            std::filesystem::remove_all(*path, error);
            return error ? make_error(error.message()) : make_success();
        };
        auto fs_rename = [this](FunctionParameterMap && parameters) {
            const auto *from = get_parameter<std::string>(parameters, "From");
            const auto *to = get_parameter<std::string>(parameters, "To");
            if (from == nullptr || to == nullptr) {
                return make_error("Invalid FSRename parameters");
            }
            const auto fault = match_fault(rename_fault_, *from);
            if (fault && !*fault) {
                return make_error("Injected rename failure: " + *from);
            }
            mutation_count_.fetch_add(1);
            std::error_code error;
            std::filesystem::rename(*from, *to, error);
            if (fault) {
                return make_error("Injected rename failure after renaming: " + *from);
            }
            return error ? make_error(error.message()) : make_success();
        };
        auto fs_copy_tree = [this](FunctionParameterMap && parameters) {
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
        auto make_kv_name = [](FunctionParameterMap && parameters) {
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
    bool should_fail_stat(const std::string &path)
    {
        std::lock_guard lock(failure_mutex_);
        return failing_stat_paths_.contains(std::filesystem::path(path).lexically_normal().generic_string());
    }

    struct Fault {
        std::string pattern;
        bool after_effect = false;
    };

    // Returns whether the fault applies after the effect, or nothing when the path does not contain the pattern.
    std::optional<bool> match_fault(const Fault &fault, const std::string &path)
    {
        std::lock_guard lock(failure_mutex_);
        if (fault.pattern.empty() || (path.find(fault.pattern) == std::string::npos)) {
            return std::nullopt;
        }
        return fault.after_effect;
    }

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
    std::mutex failure_mutex_;
    std::set<std::string> failing_stat_paths_;
    Fault write_text_fault_;
    Fault rename_fault_;
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
            }
        };
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

class ConcurrentProbeApp final: public IApp {
public:
    AppManifest get_manifest() const override
    {
        AppManifest manifest;
        manifest.id = "concurrent-record-probe";
        manifest.name = "Concurrent record probe";
        manifest.version = "1.0.0";
        return manifest;
    }
};

class TestSystem final: public System {
public:
    using KeyboardShow = std::function<std::expected<void, std::string>(AppId,
                         esp_brookesia::system::core::KeyboardRequestId)>;
    KeyboardShow keyboard_show;
    std::function<std::expected<void, std::string>(AppId, MessageDialogRequestId,
            const MessageDialogOptions &)> dialog_show;

    std::expected<void, std::string> invoke_app_task(std::function<void()> task)
    {
        return post_system_app_task(std::move(task));
    }

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
    std::expected<void, std::string> on_init() override
    {
        return {};
    }
    std::expected<void, std::string> on_start() override
    {
        return {};
    }

    std::expected<void, std::string> on_show_app_keyboard(
        AppId app_id, esp_brookesia::system::core::KeyboardRequestId request_id,
        const esp_brookesia::system::core::KeyboardRequestOptions &
    ) override
    {
        return keyboard_show ? keyboard_show(app_id, request_id) : std::expected<void, std::string> {};
    }
    void on_hide_app_keyboard(AppId, esp_brookesia::system::core::KeyboardRequestId) override {}

    void on_app_start_failed(const AppInfo &, std::string_view) override
    {
        start_failure_count_.fetch_add(1);
    }

    std::expected<void, std::string> on_app_installed(const AppInfo &app) override
    {
        if (app.manifest.version == FAILING_INSTALL_VERSION) {
            return std::unexpected("Injected install hook failure");
        }
        return {};
    }

    std::expected<void, std::string> on_show_message_dialog(
        AppId app_id,
        MessageDialogRequestId request_id,
        const MessageDialogOptions &options
    ) override
    {
        {
            std::lock_guard lock(dialog_mutex_);
            last_dialog_ = std::pair{request_id, options};
        }
        return dialog_show ? dialog_show(app_id, request_id, options) : std::expected<void, std::string> {};
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

std::string make_runtime_manifest(std::string_view app_id, std::string_view version)
{
    return R"({"package":{"id":")" + std::string(app_id) + R"(","name":{"en":"Runtime test"},"version":")" +
           std::string(version) + R"("},"runtime":{"type":"JavaScript","entry":"app.js","resource_dir":"res"}})";
}

// Power was lost after the installed copy moved to previous/ and before the new copy took its place.
bool create_interrupted_update(const std::filesystem::path &storage_root)
{
    const auto installing = storage_root / "apps" / ".installing";
    const auto previous = installing / "interrupted-update-1" / "previous";
    const auto staged = installing / "interrupted-update-1" / INTERRUPTED_APP_ID;
    return write_text_file(installing / "interrupted-update-1" / "target", std::string(INTERRUPTED_APP_ID)) &&
           write_text_file(previous / "manifest.json", make_runtime_manifest(INTERRUPTED_APP_ID, "1.0.0")) &&
           write_text_file(previous / "res" / "profile.json", "{}") &&
           write_text_file(previous / "data" / "keep.txt", "survived-interrupted-update") &&
           write_text_file(staged / "manifest.json", make_runtime_manifest(INTERRUPTED_APP_ID, "2.0.0")) &&
           write_text_file(installing / "stale-install-2" / "partial" / "app.js", "stale");
}

enum class TargetState {
    ManifestOnly,
    Complete,
    Missing,
};

// One update that stopped after the installed copy moved to previous/.
bool create_unfinished_update(
    const std::filesystem::path &storage_root, std::string_view app_id, TargetState target, bool committed
)
{
    const auto staging = storage_root / "apps" / ".installing" / (std::string(app_id) + "-1");
    const auto previous = staging / "previous";
    const auto install_dir = storage_root / "apps" / app_id;
    bool ok = write_text_file(staging / "target", std::string(app_id)) &&
              write_text_file(previous / "manifest.json", make_runtime_manifest(app_id, "1.0.0")) &&
              write_text_file(previous / "res" / "profile.json", "{}") &&
              write_text_file(previous / "data" / "keep.txt", "old-data");
    if (target != TargetState::Missing) {
        ok = ok && write_text_file(install_dir / "manifest.json", make_runtime_manifest(app_id, "2.0.0")) &&
             write_text_file(install_dir / ".install-transaction", staging.filename().generic_string());
    }
    if (target == TargetState::Complete) {
        ok = ok && write_text_file(install_dir / "res" / "profile.json", "{}") &&
             write_text_file(install_dir / "data" / "keep.txt", "new-data");
    }
    if (committed) {
        ok = ok && write_text_file(staging / "committed", "");
    }
    return ok;
}

bool create_unverified_updates(const std::filesystem::path &storage_root)
{
    const auto apps = storage_root / "apps";
    const auto staging = apps / ".installing";
    return write_text_file(staging / "traversal-1" / "target", "../outside") &&
           write_text_file(staging / "traversal-1" / "previous" / "manifest.json",
                           make_runtime_manifest("../outside", "1.0.0")) &&
           write_text_file(staging / "mismatched-1" / "target", "mismatched") &&
           write_text_file(staging / "mismatched-1" / "previous" / "manifest.json",
                           make_runtime_manifest(STAGED_APP_ID, "1.0.0")) &&
           create_unfinished_update(storage_root, "unowned", TargetState::Complete, false) &&
           write_text_file(apps / "unowned" / ".install-transaction", "another-update") &&
           write_text_file(storage_root / "outside" / "keep.txt", "outside-data");
}

bool create_broken_installed_app(const std::filesystem::path &storage_root)
{
    const auto app_root = storage_root / "apps" / BROKEN_APP_ID;
    return write_text_file(app_root / "manifest.json", make_runtime_manifest(BROKEN_APP_ID, "1.0.0")) &&
           write_text_file(app_root / "res" / "profile.json", "[not json");
}

std::optional<std::string> read_text_file(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }
    return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
}

std::optional<std::filesystem::path> find_staging(const std::filesystem::path &installing_root, std::string_view app_id)
{
    std::error_code error;
    for (std::filesystem::directory_iterator it(installing_root, error), end; !error && it != end; it.increment(error)) {
        if (it->path().filename().generic_string().starts_with(std::string(app_id) + "-")) {
            return it->path();
        }
    }
    return std::nullopt;
}

System::Config make_test_config(const std::filesystem::path &storage_root)
{
    System::Config config;
    config.install_registered_apps = false;
    config.install_package_apps = true;
    config.storage.internal_override = StorageVolume{
        .id = "internal",
        .partition = StoragePartition::Internal,
        .mount_point = storage_root.generic_string(),
        .root_path = storage_root.generic_string(),
        .available = true,
    };
    return config;
}

// Staging directories that boot recovery must keep because their state could not be checked.
bool only_unchecked_staging_left(const std::filesystem::path &installing_root)
{
    const std::set<std::string> expected = {
        std::string(UNCHECKED_BACKUP_APP_ID) + "-1",
        std::string(UNCHECKED_TARGET_APP_ID) + "-1",
        "traversal-1", "mismatched-1", "unowned-1",
    };
    std::set<std::string> names;
    std::error_code error;
    for (std::filesystem::directory_iterator it(installing_root, error), end; !error && it != end; it.increment(error)) {
        names.insert(it->path().filename().generic_string());
    }
    return !error && (names == expected);
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

using BpkFiles = std::vector<std::pair<std::string, std::string>>;

// Writes a stored (uncompressed) ZIP. CRC fields stay zero because the installer does not check them.
bool create_bpk(const std::filesystem::path &path, const BpkFiles &files)
{
    std::vector<uint8_t> data;
    std::vector<uint32_t> local_offsets;
    for (const auto &[name, contents] : files) {
        local_offsets.push_back(static_cast<uint32_t>(data.size()));
        append_u32(data, 0x04034b50U);
        append_u16(data, 20);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u32(data, 0);
        append_u32(data, static_cast<uint32_t>(contents.size()));
        append_u32(data, static_cast<uint32_t>(contents.size()));
        append_u16(data, static_cast<uint16_t>(name.size()));
        append_u16(data, 0);
        data.insert(data.end(), name.begin(), name.end());
        data.insert(data.end(), contents.begin(), contents.end());
    }

    const auto central_offset = static_cast<uint32_t>(data.size());
    for (size_t index = 0; index < files.size(); ++index) {
        const auto &[name, contents] = files[index];
        append_u32(data, 0x02014b50U);
        append_u16(data, 20);
        append_u16(data, 20);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u32(data, 0);
        append_u32(data, static_cast<uint32_t>(contents.size()));
        append_u32(data, static_cast<uint32_t>(contents.size()));
        append_u16(data, static_cast<uint16_t>(name.size()));
        append_u16(data, 0);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u16(data, 0);
        append_u32(data, 0);
        append_u32(data, local_offsets[index]);
        data.insert(data.end(), name.begin(), name.end());
    }

    const auto central_size = static_cast<uint32_t>(data.size()) - central_offset;
    append_u32(data, 0x06054b50U);
    append_u16(data, 0);
    append_u16(data, 0);
    append_u16(data, static_cast<uint16_t>(files.size()));
    append_u16(data, static_cast<uint16_t>(files.size()));
    append_u32(data, central_size);
    append_u32(data, central_offset);
    append_u16(data, 0);

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(output);
}

bool create_manifest_only_bpk(const std::filesystem::path &path, std::string_view manifest)
{
    return create_bpk(path, {{"manifest.json", std::string(manifest)}});
}

bool create_runtime_bpk(
    const std::filesystem::path &path,
    std::string_view version,
    std::string_view profile = "{}",
    std::string_view app_id = UPDATE_APP_ID
)
{
    return create_bpk(path, {
        {"manifest.json", make_runtime_manifest(app_id, version)},
        {"app.js", "// version " + std::string(version)},
        {"res/profile.json", std::string(profile)},
    });
}

std::optional<AppInfo> find_app(const System &system, std::string_view manifest_id)
{
    const auto apps = system.list_apps();
    const auto it = std::find_if(apps.begin(), apps.end(), [manifest_id](const auto & app) {
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

bool run_package_lifecycle_tests(
    TestSystem &system, HostStorageService &storage, const std::filesystem::path &storage_root
)
{
    const auto apps_root = storage_root / "apps";
    const auto installing_root = apps_root / ".installing";
    const auto app_dir = apps_root / UPDATE_APP_ID;
    const auto user_data = app_dir / "data" / "keep.txt";

    const auto interrupted = find_app(system, INTERRUPTED_APP_ID);
    bool passed = require(interrupted.has_value(), "Interrupted update did not restore the previous version");
    if (interrupted) {
        passed &= require(interrupted->manifest.version == "1.0.0", "Interrupted update restored the wrong version");
    }
    passed &= require(
                  read_text_file(apps_root / INTERRUPTED_APP_ID / "data" / "keep.txt") == "survived-interrupted-update",
                  "Interrupted update lost the previous app data"
              );
    passed &= require(!find_app(system, BROKEN_APP_ID).has_value(), "Broken installed app was registered");

    const auto partial = find_app(system, PARTIAL_APP_ID);
    passed &= require(partial && partial->manifest.version == "1.0.0",
                      "Uncommitted update with a manifest-only target did not restore 1.0.0");
    passed &= require(read_text_file(apps_root / PARTIAL_APP_ID / "data" / "keep.txt") == "old-data",
                      "Uncommitted update lost the previous app data");
    const auto committed = find_app(system, COMMITTED_APP_ID);
    passed &= require(committed && committed->manifest.version == "2.0.0", "Committed update was rolled back");
    passed &= require(read_text_file(apps_root / COMMITTED_APP_ID / "data" / "keep.txt") == "new-data",
                      "Committed update lost the new app data");
    const auto unchecked_backup = installing_root / (std::string(UNCHECKED_BACKUP_APP_ID) + "-1") / "previous";
    passed &= require(read_text_file(unchecked_backup / "data" / "keep.txt") == "old-data",
                      "Boot recovery removed a backup it could not check");
    passed &= require(!find_app(system, UNCHECKED_BACKUP_APP_ID).has_value(),
                      "Boot recovery restored a backup it could not check");
    const auto unchecked_target = installing_root / (std::string(UNCHECKED_TARGET_APP_ID) + "-1") / "previous";
    passed &= require(read_text_file(unchecked_target / "data" / "keep.txt") == "old-data",
                      "Boot recovery removed a backup when the target could not be checked");
    passed &= require(read_text_file(apps_root / UNCHECKED_TARGET_APP_ID / "data" / "keep.txt") == "new-data",
                      "Boot recovery changed a target it could not check");
    passed &= require(only_unchecked_staging_left(installing_root), "Boot recovery left or removed the wrong staging");

    const auto v1_bpk = storage_root / "update-1.0.0.bpk";
    const auto v2_bpk = storage_root / "update-2.0.0.bpk";
    const auto failing_bpk = storage_root / "update-failing.bpk";
    const auto damaged_bpk = storage_root / "update-damaged.bpk";
    passed &= require(
                  create_runtime_bpk(v1_bpk, "1.0.0") && create_runtime_bpk(v2_bpk, "2.0.0") &&
                  create_runtime_bpk(failing_bpk, FAILING_INSTALL_VERSION) &&
                  create_runtime_bpk(damaged_bpk, "3.0.0", "[damaged"),
                  "Failed to create update BPKs"
              );

    auto first_install = system.install_runtime_app_package(v1_bpk.generic_string(), true);
    passed &= require(first_install.has_value(), first_install ? "" : "First install failed: " + first_install.error());
    passed &= require(write_text_file(user_data, "user-data"), "Failed to write app data");

    auto update = system.install_runtime_app_package(v2_bpk.generic_string(), true);
    passed &= require(update.has_value(), update ? "" : "Update failed: " + update.error());
    auto updated = find_app(system, UPDATE_APP_ID);
    passed &= require(updated && updated->manifest.version == "2.0.0", "Update did not register version 2.0.0");
    passed &= require(read_text_file(app_dir / "app.js") == "// version 2.0.0", "Update did not replace app files");
    passed &= require(read_text_file(user_data) == "user-data", "Update lost app data");

    auto failed = system.install_runtime_app_package(failing_bpk.generic_string(), true);
    passed &= require(!failed.has_value(), "Update with a failing install hook was accepted");
    if (!failed) {
        passed &= require(
                      failed.error().find("previous version 2.0.0 restored") != std::string::npos,
                      "Failed update did not report the restore: " + failed.error()
                  );
    }
    auto restored = find_app(system, UPDATE_APP_ID);
    passed &= require(restored && restored->manifest.version == "2.0.0", "Failed update did not re-register 2.0.0");
    passed &= require(read_text_file(app_dir / "app.js") == "// version 2.0.0", "Failed update left new app files");
    passed &= require(read_text_file(user_data) == "user-data", "Failed update lost app data");

    auto damaged = system.install_runtime_app_package(damaged_bpk.generic_string(), true);
    passed &= require(!damaged.has_value(), "Damaged update was accepted");
    auto kept = find_app(system, UPDATE_APP_ID);
    passed &= require(
                  kept && restored && (kept->app_id == restored->app_id) && (kept->manifest.version == "2.0.0"),
                  "Damaged update changed the installed app"
              );
    passed &= require(read_text_file(user_data) == "user-data", "Damaged update lost app data");

    const auto commit_failure_bpk = storage_root / "update-commit-failure.bpk";
    passed &= require(create_runtime_bpk(commit_failure_bpk, "3.0.1"), "Failed to create commit failure BPK");
    storage.set_write_text_fault("/committing", false);
    auto uncommitted = system.install_runtime_app_package(commit_failure_bpk.generic_string(), true);
    storage.set_write_text_fault("", false);
    passed &= require(!uncommitted.has_value(), "Update without a commit marker was accepted");
    if (!uncommitted) {
        passed &= require(
                      (uncommitted.error().find("Failed to commit the update") != std::string::npos) &&
                      (uncommitted.error().find("previous version 2.0.0 restored") != std::string::npos),
                      "Commit failure did not restore the previous version: " + uncommitted.error()
                  );
    }
    kept = find_app(system, UPDATE_APP_ID);
    passed &= require(kept && kept->manifest.version == "2.0.0", "Commit failure did not keep 2.0.0 registered");
    passed &= require(read_text_file(app_dir / "app.js") == "// version 2.0.0", "Commit failure left new app files");
    passed &= require(read_text_file(user_data) == "user-data", "Commit failure lost app data");

    const auto unpublished_bpk = storage_root / "update-unpublished.bpk";
    passed &= require(create_runtime_bpk(unpublished_bpk, "3.0.2"), "Failed to create unpublished commit BPK");
    storage.set_rename_fault("/committing", false);
    auto unpublished = system.install_runtime_app_package(unpublished_bpk.generic_string(), true);
    storage.set_rename_fault("", false);
    passed &= require(!unpublished.has_value(), "Update whose commit marker was not published was accepted");
    kept = find_app(system, UPDATE_APP_ID);
    passed &= require(kept && kept->manifest.version == "2.0.0", "Unpublished commit did not keep 2.0.0");
    passed &= require(read_text_file(user_data) == "user-data", "Unpublished commit lost app data");

    const auto applied_v1_bpk = storage_root / "marker-applied-1.0.0.bpk";
    const auto applied_v2_bpk = storage_root / "marker-applied-2.0.0.bpk";
    passed &= require(
                  create_runtime_bpk(applied_v1_bpk, "1.0.0", "{}", MARKER_APPLIED_APP_ID) &&
                  create_runtime_bpk(applied_v2_bpk, "2.0.0", "{}", MARKER_APPLIED_APP_ID),
                  "Failed to create applied commit BPKs"
              );
    passed &= require(system.install_runtime_app_package(applied_v1_bpk.generic_string(), true).has_value(),
                      "Failed to install the applied commit base version");
    storage.set_rename_fault("/committing", true);
    auto applied = system.install_runtime_app_package(applied_v2_bpk.generic_string(), true);
    storage.set_rename_fault("", false);
    passed &= require(applied.has_value(), applied ? "" :
                      "Update whose commit marker was published despite an error was rejected: " + applied.error());
    const auto applied_app = find_app(system, MARKER_APPLIED_APP_ID);
    passed &= require(applied_app && applied_app->manifest.version == "2.0.0",
                      "Published commit did not keep 2.0.0");
    passed &= require(only_unchecked_staging_left(installing_root), "Package installs left staging directories");

    if (kept) {
        auto core_service = ServiceManager::get_instance().get_service("SystemCore");
        passed &= require(core_service != nullptr, "SystemCore service is unavailable");
        if (core_service) {
            auto uninstalled = core_service->call_function_sync(
            "UninstallApp", boost::json::object{{"AppId", kept->app_id}}
                               );
            passed &= require(uninstalled.success, "Runtime uninstall failed: " + uninstalled.error_message);
        }
    }
    passed &= require(!find_app(system, UPDATE_APP_ID).has_value(), "Uninstall kept the app record");
    passed &= require(!std::filesystem::exists(app_dir), "Uninstall kept the app directory");

    auto reinstall = system.install_runtime_app_package(v2_bpk.generic_string(), true);
    passed &= require(reinstall.has_value(), reinstall ? "" : "Reinstall failed: " + reinstall.error());
    passed &= require(std::filesystem::is_directory(app_dir / "data"), "Reinstall did not recreate app data storage");
    passed &= require(!std::filesystem::exists(user_data), "Reinstall resurrected uninstalled app data");

    // The commit marker write creates the file and then fails, and rollback cannot move the old version back.
    // The staging directory must not look committed, so the restart restores the old version.
    const auto fault_v1_bpk = storage_root / "marker-fault-1.0.0.bpk";
    const auto fault_v2_bpk = storage_root / "marker-fault-2.0.0.bpk";
    passed &= require(
                  create_runtime_bpk(fault_v1_bpk, "1.0.0", "{}", MARKER_FAULT_APP_ID) &&
                  create_runtime_bpk(fault_v2_bpk, "2.0.0", "{}", MARKER_FAULT_APP_ID),
                  "Failed to create commit fault BPKs"
              );
    passed &= require(system.install_runtime_app_package(fault_v1_bpk.generic_string(), true).has_value(),
                      "Failed to install the commit fault base version");
    passed &= require(write_text_file(apps_root / MARKER_FAULT_APP_ID / "data" / "keep.txt", "fault-data"),
                      "Failed to write commit fault app data");
    // "/commit" matches both "committing" and "committed", so the same fault applies whichever name is written.
    storage.set_write_text_fault("/commit", true);
    storage.set_rename_fault("/previous", false);
    auto faulted = system.install_runtime_app_package(fault_v2_bpk.generic_string(), true);
    storage.set_write_text_fault("", false);
    storage.set_rename_fault("", false);
    passed &= require(!faulted.has_value(), "Update with a half-written commit marker was accepted");
    const auto fault_staging = find_staging(installing_root, MARKER_FAULT_APP_ID);
    passed &= require(fault_staging.has_value(), "Failed rollback did not keep its staging directory");
    if (fault_staging) {
        passed &= require(!std::filesystem::exists(*fault_staging / "committed"),
                          "Half-written commit marker was published as committed");
        passed &= require(read_text_file(*fault_staging / "previous" / "data" / "keep.txt") == "fault-data",
                          "Failed rollback lost the previous app data");
    }
    return passed;
}

// Boots a second System on the same storage, as after a power cycle.
bool run_restart_tests(const std::filesystem::path &storage_root)
{
    const auto apps_root = storage_root / "apps";
    TestSystem restarted;
    auto init_result = restarted.init(make_test_config(storage_root));
    bool passed = require(init_result.has_value(), init_result ? "" : "Restart init failed: " + init_result.error());
    if (passed) {
        passed &= require(read_text_file(storage_root / "outside" / "keep.txt") == "outside-data",
                          "Recovery modified a path outside the app root");
        passed &= require(read_text_file(apps_root / "unowned" / "data" / "keep.txt") == "new-data",
                          "Recovery replaced an unrelated installed directory");
        const auto fault_app = find_app(restarted, MARKER_FAULT_APP_ID);
        passed &= require(fault_app && fault_app->manifest.version == "1.0.0",
                          "Restart did not restore the version kept by the failed rollback");
        passed &= require(read_text_file(apps_root / MARKER_FAULT_APP_ID / "data" / "keep.txt") == "fault-data",
                          "Restart lost the app data kept by the failed rollback");
        const auto applied_app = find_app(restarted, MARKER_APPLIED_APP_ID);
        passed &= require(applied_app && applied_app->manifest.version == "2.0.0",
                          "Restart rolled back a published commit");
        passed &= require(only_unchecked_staging_left(apps_root / ".installing"),
                          "Restart left or removed the wrong staging directories");
    }
    restarted.deinit();
    return passed;
}

#if defined(BROOKESIA_GUI_GSP_DESKTOP)
// Acceptance must precede UI work, and cancelling a pending head must still drain the shared FIFO.
bool verify_deferred_message_dialog(TestSystem &system, AppId app_id)
{
    using namespace esp_brookesia::system::core;
    MessageDialogOptions options;
    options.buttons.push_back({"OK", MessageDialogButtonRole::Accept});
    std::promise<std::expected<MessageDialogRequestId, std::string>> accepted;
    std::promise<MessageDialogResult> completed;
    std::atomic<int> closes{0};
    system.dialog_show = [&system](AppId owner, MessageDialogRequestId request, const MessageDialogOptions &) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        return system.complete_app_message_dialog(owner, request, 0, MessageDialogCloseReason::Button);
    };
    auto accepted_future = accepted.get_future();
    auto completed_future = completed.get_future();
    auto submit = [&]() {
        esp_brookesia::service::ScopedCallContext context({{RUNTIME_BLOCKING_SERVICE_CALL_CONTEXT_KEY, "true"}});
        accepted.set_value(system.show_app_message_dialog(app_id, options, [&](const MessageDialogResult & result) {
            if (closes.fetch_add(1) == 0) {
                completed.set_value(result);
            }
        }));
    };
    auto posted = system.invoke_app_task(submit);
    if (!require(posted.has_value(), "Failed to post dialog acceptance test")) {
        return false;
    }
    const bool timely = accepted_future.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready;
    accepted_future.wait();
    auto request = accepted_future.get();
    bool passed = require(timely && request.has_value(), "Dialog acceptance waited for slow renderer");
    passed &= require(completed_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
                      "Accepted dialog completion was lost");
    if (completed_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const auto result = completed_future.get();
        passed &= require(request && result.request_id == *request && result.app_id == app_id &&
                          result.button_role == MessageDialogButtonRole::Accept && closes.load() == 1,
                          "Dialog completion lost request ID, owner, role, or completed twice");
    }

    std::promise<bool> cancelled;
    auto cancelled_future = cancelled.get_future();
    std::atomic<int> shown{0};
    std::atomic<int> cancelled_count{0};
    system.dialog_show = [&](AppId owner, MessageDialogRequestId id, const MessageDialogOptions &) {
        shown.fetch_add(1);
        return system.complete_app_message_dialog(owner, id, -1, MessageDialogCloseReason::Closed);
    };
    auto cancel_and_queue = [&]() {
        esp_brookesia::service::ScopedCallContext context({{RUNTIME_BLOCKING_SERVICE_CALL_CONTEXT_KEY, "true"}});
        auto pending = system.show_app_message_dialog(app_id, {}, [&](const MessageDialogResult &) {
            cancelled_count.fetch_add(1);
        });
        auto following = system.show_system_message_dialog({}, [&](const MessageDialogResult & result) {
            cancelled.set_value(result.app_id == INVALID_APP_ID && shown.load() == 1 && cancelled_count.load() == 1);
        });
        auto hidden = pending ? system.hide_app_message_dialog(app_id, *pending) :
                      std::expected<void, std::string>(std::unexpected("No request"));
        if (!following || !hidden) {
            cancelled.set_value(false);
        }
    };
    posted = system.invoke_app_task(cancel_and_queue);
    passed &= require(posted.has_value() &&
                      cancelled_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
                      "Cancelled dialog head left subsequent system dialog stranded");
    if (cancelled_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        passed &= require(cancelled_future.get(), "Cancelled dialog displayed, completed twice, or violated FIFO");
    }

    std::promise<MessageDialogResult> failed;
    auto failed_future = failed.get_future();
    std::atomic<MessageDialogRequestId> failed_id{INVALID_MESSAGE_DIALOG_REQUEST_ID};
    system.dialog_show = [](AppId, MessageDialogRequestId, const MessageDialogOptions &)
    -> std::expected<void, std::string> {
        return std::unexpected("Injected dialog renderer failure");
    };
    auto submit_failure = [&]() {
        esp_brookesia::service::ScopedCallContext context({{RUNTIME_BLOCKING_SERVICE_CALL_CONTEXT_KEY, "true"}});
        auto pending = system.show_app_message_dialog(app_id, {}, [&](const MessageDialogResult & result) {
            failed.set_value(result);
        });
        if (pending) {
            failed_id = *pending;
        } else {
            failed.set_value({});
        }
    };
    posted = system.invoke_app_task(submit_failure);
    passed &= require(posted.has_value() &&
                      failed_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
                      "Accepted failed dialog did not complete");
    if (failed_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const auto result = failed_future.get();
        const auto owner = system.get_app(app_id);
        passed &= require(failed_id != INVALID_MESSAGE_DIALOG_REQUEST_ID && result.request_id == failed_id &&
                          result.app_id == app_id && result.reason == MessageDialogCloseReason::Closed &&
                          owner && owner->last_error == "Injected dialog renderer failure",
                          "Failed dialog lost accepted ID, owner, close reason, or actual error");
    }
    system.dialog_show = {};
    return passed;
}

// Production request manager and scheduler, with a deliberately slow UI adapter; no renderer is launched.
bool verify_deferred_keyboard(TestSystem &system, AppId app_id)
{
    using namespace esp_brookesia::system::core;
    std::promise<std::expected<KeyboardRequestId, std::string>> accepted;
    std::promise<KeyboardResult> completed;
    std::atomic<int> closes{0};
    system.keyboard_show = [&system](AppId owner, KeyboardRequestId request) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        return system.complete_app_keyboard(owner, request, true, "accepted input");
    };
    auto accepted_future = accepted.get_future();
    auto completed_future = completed.get_future();
    auto posted = system.invoke_app_task([&]() {
        esp_brookesia::service::ScopedCallContext context({{RUNTIME_BLOCKING_SERVICE_CALL_CONTEXT_KEY, "true"}});
        accepted.set_value(system.show_app_keyboard(app_id, {}, [&](const KeyboardResult & result) {
            if (closes.fetch_add(1) == 0) {
                completed.set_value(result);
            }
        }));
    });
    if (!require(posted.has_value(), "Failed to post keyboard acceptance test")) {
        return false;
    }
    if (!require(accepted_future.wait_for(std::chrono::milliseconds(500)) == std::future_status::ready,
                 "Keyboard acceptance waited for the slow renderer adapter")) {
        // Keep callback state alive while a synchronous-regression adapter finishes.
        accepted_future.wait();
        completed_future.wait();
        return false;
    }
    auto request = accepted_future.get();
    if (!require(request.has_value(), "Deferred keyboard request was rejected") ||
            !require(completed_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
                     "Deferred keyboard completion was lost")) {
        return false;
    }
    const auto result = completed_future.get();
    bool passed = require(result.request_id == *request && result.app_id == app_id && result.confirmed &&
                          result.text == "accepted input" && closes.load() == 1,
                          "Deferred keyboard result lost ownership, text, or completed twice");

    std::promise<KeyboardResult> failed;
    auto failed_future = failed.get_future();
    system.keyboard_show = [](AppId, KeyboardRequestId) -> std::expected<void, std::string> {
        return std::unexpected("Injected desktop renderer failure");
    };
    auto failure_posted = system.invoke_app_task([&]() {
        esp_brookesia::service::ScopedCallContext context({{RUNTIME_BLOCKING_SERVICE_CALL_CONTEXT_KEY, "true"}});
        auto pending = system.show_app_keyboard(app_id, {}, [&](const KeyboardResult & closed) {
            failed.set_value(closed);
        });
        if (!pending) {
            failed.set_value({});
        }
    });
    passed &= require(failure_posted.has_value() &&
                      failed_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
                      "Failed deferred keyboard did not close its accepted request");
    if (failed_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        const auto failure = failed_future.get();
        passed &= require(failure.app_id == app_id && !failure.confirmed,
                          "Failed deferred keyboard reported a confirmed result or wrong owner");
    }
    const auto owner = system.get_app(app_id);
    passed &= require(owner && owner->last_error == "Injected desktop renderer failure",
                      "Deferred renderer failure did not retain its real error");

    std::promise<bool> cancelled;
    auto cancelled_future = cancelled.get_future();
    std::atomic<int> shown{0};
    std::atomic<int> cancelled_count{0};
    system.keyboard_show = [&](AppId, KeyboardRequestId) -> std::expected<void, std::string> {
        shown.fetch_add(1); return {};
    };
    auto cancel_posted = system.invoke_app_task([&]() {
        esp_brookesia::service::ScopedCallContext context({{RUNTIME_BLOCKING_SERVICE_CALL_CONTEXT_KEY, "true"}});
        auto pending = system.show_app_keyboard(app_id, {}, [&](const KeyboardResult &) {
            cancelled_count.fetch_add(1);
        });
        auto hidden = pending ? system.hide_app_keyboard(app_id, *pending) :
                      std::expected<void, std::string>(std::unexpected("No request"));
        auto fence = system.invoke_app_task([&]() {
            cancelled.set_value(shown.load() == 0 && cancelled_count.load() == 1);
        });
        if (!hidden || !fence) {
            cancelled.set_value(false);
        }
    });
    passed &= require(cancel_posted.has_value() &&
                      cancelled_future.wait_for(std::chrono::seconds(3)) == std::future_status::ready,
                      "Pending keyboard cancellation did not complete");
    if (cancelled_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        passed &= require(cancelled_future.get(), "Cancelled pending keyboard was displayed or closed twice");
    }
    system.keyboard_show = {};
    return passed;
}
#endif

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
            ) ||
            !require(
                create_interrupted_update(temporary_directory.get_path()),
                "Failed to create interrupted update"
            ) ||
            !require(
                create_broken_installed_app(temporary_directory.get_path()),
                "Failed to create broken installed app"
            ) ||
            !require(
                create_unfinished_update(temporary_directory.get_path(), PARTIAL_APP_ID, TargetState::ManifestOnly,
                                         false) &&
                create_unfinished_update(temporary_directory.get_path(), COMMITTED_APP_ID, TargetState::Complete,
                                         true) &&
                create_unfinished_update(temporary_directory.get_path(), UNCHECKED_BACKUP_APP_ID,
                                         TargetState::Missing, false) &&
                create_unfinished_update(temporary_directory.get_path(), UNCHECKED_TARGET_APP_ID,
                                         TargetState::Complete, false) &&
                create_unverified_updates(temporary_directory.get_path()),
                "Failed to create unfinished updates"
            )) {
        return false;
    }
    const auto installing = temporary_directory.get_path() / "apps" / ".installing";

    auto &manager = ServiceManager::get_instance();
    if (!require(manager.init(), "Failed to initialize ServiceManager")) {
        return false;
    }
    auto storage = std::make_shared<HostStorageService>();
    storage->fail_stat(installing / (std::string(UNCHECKED_BACKUP_APP_ID) + "-1") / "previous");
    storage->fail_stat(temporary_directory.get_path() / "apps" / UNCHECKED_TARGET_APP_ID);
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
    bool native_supported = false;
    auto config = make_test_config(temporary_directory.get_path());
#if defined(BROOKESIA_GUI_GSP_DESKTOP)
    config.gsp_desktop_display = std::make_shared<esp_brookesia::gui::gsp::DesktopDisplay>(
                                     esp_brookesia::gui::gsp::DesktopDisplay::Config{});
#endif
    config.app_support_check = [&native_supported](const AppManifest & manifest) -> std::optional<std::string> {
        if (manifest.id == NATIVE_APP_ID && !native_supported)
        {
            return "Native GUI is unavailable on this platform";
        }
        return std::nullopt;
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
    std::atomic_bool stop_record_reader = false;
    std::atomic_bool invalid_record = false;
    std::thread record_reader([&]() {
        while (!stop_record_reader.load()) {
            for (const auto &app : system.list_apps()) {
                auto current = system.get_app(app.app_id);
                if (current && current->app_id != app.app_id) {
                    invalid_record = true;
                }
            }
        }
    });
    for (int iteration = 0; iteration < 50; ++iteration) {
        auto installed = system.install_app(std::make_shared<ConcurrentProbeApp>());
        if (!require(installed.has_value(), installed ? "" : "Concurrent install failed: " + installed.error())) {
            passed = false;
            break;
        }
        auto removed = system.uninstall_app(*installed);
        if (!require(removed.has_value(), removed ? "" : "Concurrent uninstall failed: " + removed.error())) {
            passed = false;
            break;
        }
    }
    stop_record_reader = true;
    record_reader.join();
    passed &= require(!invalid_record.load(), "Concurrent app record lookup returned the wrong app");

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
        auto core_service = manager.get_service("SystemCore");
        passed &= require(core_service != nullptr, "SystemCore service is unavailable");
        if (core_service) {
            auto rejected_uninstall = core_service->call_function_sync(
            "UninstallApp", boost::json::object{{"AppId", *native_install}}
                                      );
            passed &= require(!rejected_uninstall.success &&
                              rejected_uninstall.error_message.find("Only managed runtime apps") != std::string::npos,
                              "SystemCore allowed native app uninstall");
            passed &= require(native_app->get_uninstall_count() == 0 && find_app(system, NATIVE_APP_ID).has_value(),
                              "Rejected native uninstall changed the app");
        }
        auto unsupported = system.app_support_error(*native_install);
        passed &= require(unsupported.has_value(), "Unsupported native app omitted its availability reason");
        auto rejected = system.start_app(*native_install);
        passed &= require(!rejected, "Unsupported native app unexpectedly started");
        passed &= require(native_app->get_start_count() == 0, "Unsupported native app executed on_start");
        auto retained = find_app(system, NATIVE_APP_ID);
        passed &= require(retained && retained->state == AppState::Installed,
                          "Platform support rejection did not retain the installed native app");
        passed &= require(retained && unsupported && retained->last_error == *unsupported,
                          "Platform support rejection lost its error reason");
        const auto warning = system.get_last_dialog();
        passed &= require(warning && unsupported && warning->second.informative_text == *unsupported,
                          "Unsupported app did not show its actual platform support error");
        if (warning) {
            auto hidden = system.hide_system_message_dialog(warning->first);
            passed &= require(hidden.has_value(), "Failed to close platform support warning");
        }
        native_supported = true;
        passed &= require(!system.app_support_error(*native_install), "Native app support did not refresh");
        auto native_start = system.start_app(*native_install);
        passed &= require(
                      native_start.has_value(),
                      native_start ? "" : "Trusted native app start failed: " + native_start.error()
                  );
        passed &= require(native_app->get_start_count() == 1, "Trusted native app did not execute on_start");
#if defined(BROOKESIA_GUI_GSP_DESKTOP)
        passed &= verify_deferred_keyboard(system, *native_install);
        passed &= verify_deferred_message_dialog(system, *native_install);
#endif
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
                  only_unchecked_staging_left(installing_root),
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

    passed &= run_package_lifecycle_tests(system, *storage, temporary_directory.get_path());

    system.deinit();
    passed &= run_restart_tests(temporary_directory.get_path());
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
