/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <chrono>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>
#include <thread>
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include "zlib.h"
#include "private/desktop.hpp"

extern char **environ;

namespace esp_brookesia::gui::gsp {
namespace {
std::mutex registry_mutex;
std::vector<std::weak_ptr<const DesktopBundle>> registry;

std::string read_file(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Cannot read " + path);
    }
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

DesktopDisplay::DesktopDisplay(Config config) : impl_(std::make_unique<Impl>(std::move(config))) {}
DesktopDisplay::~DesktopDisplay()
{
    impl_->stop();
}
DesktopDisplay::Impl &DesktopDisplay::implementation()
{
    return *impl_;
}

std::expected<void, std::string> DesktopDisplay::register_bundle(
    const std::string &bundle_path, const std::string &api_sidecar_path)
{
    try {
        auto entry = std::make_shared<DesktopBundle>();
        entry->path = std::filesystem::absolute(bundle_path).string();
        entry->sidecar_path = std::filesystem::absolute(api_sidecar_path).string();
        const auto bytes = read_file(entry->path);
        entry->bytes.resize(bytes.size());
        std::memcpy(entry->bytes.data(), bytes.data(), bytes.size());
        auto json = boost::json::parse(read_file(entry->sidecar_path));
        if (!json.is_object()) {
            return std::unexpected("GSP sidecar must be an object");
        }
        entry->metadata = std::move(json.as_object());
        auto &metadata = entry->metadata;
        if (metadata.at("schema_version").as_int64() != 2 || metadata.at("scenes").as_array().empty()) {
            return std::unexpected("GSP sidecar requires schema 2 and at least one scene");
        }
        // Validate every field consumed by Session and Bundle before publishing immutable metadata.
        // A malformed sidecar must fail here, never throw on the bridge callback thread.
        const auto integer = [](const boost::json::value & value) -> int64_t {
            if (!value.is_int64() && !value.is_uint64())
            {
                throw std::invalid_argument("GSP sidecar numeric metadata must contain integers");
            }
            return value.to_number<int64_t>();
        };
        std::set<uint16_t> scene_ids;
        if (metadata.at("scenes").as_array().size() > UINT16_MAX) {
            return std::unexpected("GSP sidecar has too many scenes");
        }
        for (const auto &scene_value : metadata.at("scenes").as_array()) {
            const auto &scene = scene_value.as_object();
            const auto scene_number = integer(scene.at("scene_id"));
            if (scene_number < 0 || scene_number > UINT16_MAX || !scene_ids.insert(scene_number).second) {
                return std::unexpected("GSP sidecar has an invalid or duplicate scene ID");
            }
            std::set<uint16_t> action_ids;
            for (const auto &callback_value : scene.at("callbacks").as_array()) {
                const auto &callback = callback_value.as_object();
                const auto action_number = integer(callback.at("action_id"));
                if (action_number < 0 || action_number > UINT16_MAX ||
                        !action_ids.insert(action_number).second || callback.at("name").as_string().empty()) {
                    return std::unexpected("GSP sidecar has an invalid or duplicate action");
                }
            }
            std::set<uint32_t> component_keys;
            for (const auto &object_value : scene.at("objects").as_array()) {
                const auto &object = object_value.as_object();
                const auto component_key = integer(object.at("component_key"));
                if (component_key < 0 || component_key > UINT32_MAX ||
                        !component_keys.insert(component_key).second) {
                    return std::unexpected("GSP sidecar has an invalid or duplicate component key");
                }
                std::set<uint32_t> property_keys;
                for (const auto &property_value : object.at("properties").as_array()) {
                    const auto &property = property_value.as_object();
                    const auto property_key = integer(property.at("property_key"));
                    if (property_key < 0 || property_key > UINT32_MAX ||
                            !property_keys.insert(property_key).second) {
                        return std::unexpected("GSP sidecar has an invalid or duplicate property key");
                    }
                    if (property.at("value_kind").as_string().empty() ||
                            integer(property.at("min")) > integer(property.at("max"))) {
                        return std::unexpected("GSP sidecar has an invalid property type or range");
                    }
                }
            }
        }
        const auto &artifact = metadata.at("artifact").as_object();
        const auto crc_text = std::string(artifact.at("crc32").as_string());
        size_t consumed = 0;
        const auto expected_crc = std::stoul(crc_text, &consumed, 0);
        const auto actual_crc = crc32(0, reinterpret_cast<const Bytef *>(bytes.data()), bytes.size());
        if (consumed != crc_text.size() || artifact.at("bytes").to_number<uint64_t>() != bytes.size() ||
                expected_crc != actual_crc || artifact.at("kind").as_string() != "bundle") {
            return std::unexpected("GSP bundle does not match its compiler sidecar (size or CRC32)");
        }
        std::lock_guard lock(registry_mutex);
        for (auto iterator = registry.begin(); iterator != registry.end();) {
            const auto registered = iterator->lock();
            if (!registered) {
                iterator = registry.erase(iterator);
                continue;
            }
            if (registered->bytes == entry->bytes) {
                if (registered->metadata != entry->metadata) {
                    return std::unexpected("The same GSP bytes have conflicting compiler metadata");
                }
                std::lock_guard display_lock(impl_->mutex);
                if (std::find(impl_->bundles.begin(), impl_->bundles.end(), registered) == impl_->bundles.end()) {
                    impl_->bundles.push_back(registered);
                }
                return {};
            }
            ++iterator;
        }
        registry.push_back(entry);
        std::lock_guard display_lock(impl_->mutex);
        impl_->bundles.push_back(std::move(entry));
        return {};
    } catch (const std::exception &error) {
        return std::unexpected("Unable to register GSP bundle: " + std::string(error.what()));
    }
}

std::string DesktopDisplay::browser_url() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->url;
}

bool DesktopDisplay::alive() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->pid > 0 && kill(impl_->pid, 0) == 0;
}

std::string DesktopDisplay::error() const
{
    std::lock_guard lock(impl_->mutex);
    return impl_->failure;
}

std::shared_ptr<const DesktopBundle> find_desktop_bundle(std::span<const std::byte> bytes)
{
    std::lock_guard lock(registry_mutex);
    for (const auto &registered : registry) {
        const auto entry = registered.lock();
        if (entry && entry->bytes.size() == bytes.size() &&
                std::equal(bytes.begin(), bytes.end(), entry->bytes.begin())) {
            return entry;
        }
    }
    return {};
}

uint32_t desktop_name_key(std::string_view name)
{
    uint32_t key = 2166136261u;
    for (unsigned char byte : name) {
        key = (key ^ byte) * 16777619u;
    }
    return key;
}

std::expected<std::string, std::string> DesktopDisplay::Impl::start(const DesktopBundle &bundle)
{
    stop();
    std::unique_lock lock(mutex);
    try {
        // Keep only the most recent failed run for diagnostics; successful runs are removed by stop().
        if (!run_dir.empty()) {
            std::filesystem::remove_all(run_dir);
            run_dir.clear();
        }
        failure.clear();
        const auto root = config.work_dir.empty() ? std::filesystem::temp_directory_path() :
                          std::filesystem::path(config.work_dir);
        std::filesystem::create_directories(root);
        std::string pattern = (root / "brookesia-gsp-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr) {
            failure = "Cannot create GSP renderer working directory";
            return std::unexpected(failure);
        }
        run_dir = pattern;
        const auto ready_path = run_dir + "/ready.json";
        // Snapshot the registered bytes so later source rebuilds cannot mix bundle and metadata.
        const auto bundle_path = run_dir + "/app.gspb";
        const auto sidecar_path = run_dir + "/app.api.json";
        std::ofstream output(bundle_path, std::ios::binary);
        output.write(reinterpret_cast<const char *>(bundle.bytes.data()), bundle.bytes.size());
        output.close();
        std::ofstream sidecar(sidecar_path);
        sidecar << boost::json::serialize(bundle.metadata);
        sidecar.close();
        if (!output || !sidecar) {
            failure = "Cannot snapshot GSP compiler artifacts";
            return std::unexpected(failure);
        }
        std::vector<std::string> args = {config.simulator, "--bundle", bundle_path, "--api-json", sidecar_path,
                                         "--frames", "0", "--idle-poll-ms", "1", "--backend-listen", "tcp://127.0.0.1:0",
                                         "--backend-required",
                                         "--ready-file", ready_path
                                        };
        if (config.headless) {
            args.push_back("--headless");
        } else {
            args.insert(args.end(), {"--visual-listen", "127.0.0.1:0"});
        }
        if (!config.dynamic_font.empty()) {
            args.insert(args.end(), {"--dynamic-font", config.dynamic_font});
        }
        std::vector<char *> argv;
        for (auto &arg : args) {
            argv.push_back(arg.data());
        }
        argv.push_back(nullptr);
        posix_spawn_file_actions_t actions;
        int action_result = posix_spawn_file_actions_init(&actions);
        if (action_result != 0) {
            failure = "Cannot initialize GSP renderer process actions: " + std::string(std::strerror(action_result));
            return std::unexpected(failure);
        }
        action_result = posix_spawn_file_actions_addopen(
                            &actions, STDOUT_FILENO, (run_dir + "/renderer.log").c_str(),
                            O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (action_result == 0) {
            action_result = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
        }
        if (action_result != 0) {
            posix_spawn_file_actions_destroy(&actions);
            failure = "Cannot configure GSP renderer process output: " + std::string(std::strerror(action_result));
            return std::unexpected(failure);
        }
        pid_t child = -1;
        const int result = posix_spawn(&child, config.simulator.c_str(), &actions, nullptr, argv.data(), environ);
        posix_spawn_file_actions_destroy(&actions);
        if (result != 0) {
            failure = "Cannot spawn GSP renderer: " + std::string(std::strerror(result));
            return std::unexpected(failure);
        }
        pid = child;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < deadline) {
            int status = 0;
            if (waitpid(pid, &status, WNOHANG) == pid) {
                pid = -1;
                failure = "GSP renderer exited before readiness; see " + run_dir + "/renderer.log";
                return std::unexpected(failure);
            }
            if (std::filesystem::exists(ready_path)) {
                boost::system::error_code error;
                const auto info = boost::json::parse(read_file(ready_path), error);
                if (!error && info.is_object() && info.as_object().contains("backend")) {
                    const auto &object = info.as_object();
                    url = object.contains("browser") && object.at("browser").is_string()
                          ? std::string(object.at("browser").as_string()) : "";
                    if (!url.empty()) {
                        // The native server matches the whole request target and rejects query strings.
                        // Consumers recreate their iframe when this session fragment changes.
                        url += "#brookesia-session=" + std::filesystem::path(run_dir).filename().string();
                    }
                    return std::string(object.at("backend").as_string());
                }
            }
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            lock.lock();
        }
        failure = "GSP renderer readiness timed out; see " + run_dir + "/renderer.log";
        return std::unexpected(failure);
    } catch (const std::exception &error) {
        failure = "Cannot start GSP renderer: " + std::string(error.what());
        return std::unexpected(failure);
    }
}

void DesktopDisplay::Impl::stop()
{
    std::lock_guard lock(mutex);
    if (pid > 0) {
        kill(pid, SIGTERM);
        bool reaped = false;
        for (int attempt = 0; attempt < 100; ++attempt) {
            const auto waited = waitpid(pid, nullptr, WNOHANG);
            if (waited == pid || (waited < 0 && errno == ECHILD)) {
                reaped = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!reaped) {
            kill(pid, SIGKILL);
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
        }
        pid = -1;
    }
    url.clear();
    if (failure.empty() && !run_dir.empty()) {
        std::error_code error;
        std::filesystem::remove_all(run_dir, error);
        if (error) {
            failure = "Cannot remove GSP renderer working directory: " + error.message();
        } else {
            run_dir.clear();
        }
    }
}
} // namespace esp_brookesia::gui::gsp
