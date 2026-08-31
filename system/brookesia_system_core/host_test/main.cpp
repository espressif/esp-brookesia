/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <atomic>
#include <array>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#include "brookesia/runtime_manager/types.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/system_core/app/package.hpp"
#include "brookesia/system_core/app/types.hpp"
#include "brookesia/system_core/service/gui.hpp"
#include "brookesia/system_core/service/system.hpp"
#include "brookesia/system_core/service/timer.hpp"
#include "private/app/service_requirement.hpp"
#include "private/runtime/host_bridge.hpp"

namespace {

using esp_brookesia::runtime::AppId;
using esp_brookesia::runtime::NativeArgs;
using esp_brookesia::runtime::NativeFunctionSpec;
using esp_brookesia::runtime::NativeModule;
using esp_brookesia::runtime::NativeResult;
using esp_brookesia::runtime::NativeValue;
using esp_brookesia::service::EventItemSchema;
using esp_brookesia::service::EventItemType;
using esp_brookesia::service::EventSchema;
using esp_brookesia::service::FunctionParameterMap;
using esp_brookesia::service::FunctionParameterSchema;
using esp_brookesia::service::FunctionResult;
using esp_brookesia::service::FunctionSchema;
using esp_brookesia::service::FunctionValueType;
using esp_brookesia::service::ManagerService;
using esp_brookesia::service::ServiceBase;
using esp_brookesia::service::ServiceManager;
using esp_brookesia::system::core::AppKind;
using esp_brookesia::system::core::AppManifest;
using esp_brookesia::system::core::AppManifestService;
using esp_brookesia::system::core::read_unpacked_app_manifest;
using esp_brookesia::system::core::SystemCoreHelper;
using esp_brookesia::system::core::SystemGuiHelper;
using esp_brookesia::system::core::SystemHostBridge;
using esp_brookesia::system::core::SystemTimerHelper;
using esp_brookesia::system::core::detail::evaluate_service_requirements;
using esp_brookesia::system::core::detail::is_service_version_compatible;
using FunctionHandlerMap = ServiceBase::FunctionHandlerMap;

constexpr std::string_view TEST_RPC_NAME = "RequirementTestRpc";
constexpr std::string_view INVALID_VERSION_RPC_NAME = "InvalidVersionRpc";
constexpr std::string_view CASE_MISMATCHED_SYSTEM_RPC_NAME = "systemcore";

class RequirementTestService final: public ServiceBase {
public:
    explicit RequirementTestService(
        std::string name = std::string(TEST_RPC_NAME),
        std::string version = "0.8.3"
    )
        : ServiceBase(Attributes{
            .name = std::move(name),
            .description = "SystemHostBridge service requirement host test.",
            .version = std::move(version),
        })
    {}

    int get_call_count() const
    {
        return call_count_.load();
    }

    int get_start_count() const
    {
        return start_count_.load();
    }

    std::vector<FunctionSchema> get_function_schemas() override
    {
        return {{
                .name = "touch",
                .description = "Record that the service function handler executed.",
                .require_scheduler = false,
                .return_value = esp_brookesia::service::FunctionReturnSchema{
                    .type = FunctionValueType::Boolean,
                    .description = "Always true.",
                },
            }};
    }

    std::vector<EventSchema> get_event_schemas() override
    {
        return {{
                .name = "changed",
                .description = "Test event.",
                .items = {EventItemSchema{
                    .name = "value",
                    .description = "Test value.",
                    .type = EventItemType::String,
                }},
                .require_scheduler = false,
            }};
    }

protected:
    bool on_start() override
    {
        start_count_.fetch_add(1);
        return true;
    }

    FunctionHandlerMap get_function_handlers() override
    {
        auto touch_handler = [this](FunctionParameterMap &&) -> FunctionResult {
            call_count_.fetch_add(1);
            return FunctionResult{
                .success = true,
                .data = true,
            };
        };
        return {{"touch", std::move(touch_handler)}};
    }

private:
    std::atomic<int> call_count_{0};
    std::atomic<int> start_count_{0};
};

class HostStorageService final: public ServiceBase {
public:
    HostStorageService()
        : ServiceBase(Attributes{
            .name = "Storage",
            .description = "Host-only text file reader for package parser tests.",
            .version = "0.0.0",
        })
    {}

    std::vector<FunctionSchema> get_function_schemas() override
    {
        return {{
                .name = "FSReadText",
                .description = "Read a host text file.",
                .parameters = {FunctionParameterSchema{
                    .name = "Path",
                    .description = "Host file path.",
                    .type = FunctionValueType::String,
                }},
                .require_scheduler = false,
                .return_value = esp_brookesia::service::FunctionReturnSchema{
                    .type = FunctionValueType::String,
                    .description = "File contents.",
                },
            }};
    }

protected:
    FunctionHandlerMap get_function_handlers() override
    {
        auto read_text_handler = [](FunctionParameterMap &&parameters) -> FunctionResult {
            const auto path_it = parameters.find("Path");
            if (path_it == parameters.end()) {
                return FunctionResult{
                    .success = false,
                    .error_message = "Missing Path",
                };
            }
            const auto *path = std::get_if<std::string>(&path_it->second);
            if (path == nullptr) {
                return FunctionResult{
                    .success = false,
                    .error_message = "Path is not a string",
                };
            }

            std::ifstream input(*path, std::ios::binary);
            if (!input.is_open()) {
                return FunctionResult{
                    .success = false,
                    .error_message = "Failed to open host file: " + *path,
                };
            }
            std::string contents{
                std::istreambuf_iterator<char>(input),
                std::istreambuf_iterator<char>()
            };
            return FunctionResult{
                .success = true,
                .data = std::move(contents),
            };
        };
        return {{"FSReadText", std::move(read_text_handler)}};
    }
};

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
        const auto unique_suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = temporary_root / ("brookesia-system-core-host-test-" + std::to_string(unique_suffix));
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

const NativeFunctionSpec *find_native_function(
    const std::vector<NativeModule> &modules,
    std::string_view name
)
{
    for (const auto &module : modules) {
        if (module.name != "brookesia") {
            continue;
        }
        for (const auto &function : module.functions) {
            if (function.name == name) {
                return &function;
            }
        }
    }
    return nullptr;
}

NativeResult call_native(
    const std::vector<NativeModule> &modules,
    std::string_view name,
    NativeArgs args
)
{
    const auto *function = find_native_function(modules, name);
    if (function == nullptr) {
        return std::unexpected("Native function not found: " + std::string(name));
    }
    if (!function->function) {
        return std::unexpected("Native function is not synchronous: " + std::string(name));
    }
    return function->function(args);
}

bool require_bool_result(const NativeResult &result, bool expected, std::string_view context)
{
    if (!result) {
        std::cerr << context << " returned error: " << result.error() << '\n';
        return false;
    }
    const auto *value = std::get_if<bool>(&*result);
    if ((value == nullptr) || (*value != expected)) {
        std::cerr << context << " returned an unexpected value" << '\n';
        return false;
    }
    return true;
}

bool require_unavailable_error(
    const NativeResult &result,
    std::string_view context,
    std::string_view expected_reason = {}
)
{
    if (result) {
        std::cerr << context << " unexpectedly succeeded" << '\n';
        return false;
    }
    if (result.error().find("Service unavailable") == std::string::npos) {
        std::cerr << context << " returned an unexpected error: " << result.error() << '\n';
        return false;
    }
    if (!expected_reason.empty() && (result.error().find(expected_reason) == std::string::npos)) {
        std::cerr << context << " did not report the expected reason: " << result.error() << '\n';
        return false;
    }
    return true;
}

AppManifest make_manifest(std::string id, std::vector<AppManifestService> services)
{
    AppManifest manifest;
    manifest.id = std::move(id);
    manifest.name = manifest.id;
    manifest.kind = AppKind::Runtime;
    manifest.services = std::move(services);
    return manifest;
}

AppManifestService make_test_requirement(std::string version)
{
    return {
        .name = std::string(TEST_RPC_NAME),
        .version = std::move(version),
    };
}

std::string make_package_manifest_json(std::string_view services_json = {})
{
    std::string manifest =
        R"({"package":{"id":"weather","name":{"en":"Weather"},"version":"1.0.0"},)"
        R"("runtime":{"type":"JavaScript","entry":"index.js"})";
    if (!services_json.empty()) {
        manifest += R"(,"services":)";
        manifest += services_json;
    }
    manifest += '}';
    return manifest;
}

std::expected<AppManifest, std::string> parse_manifest_case(
    const std::filesystem::path &directory,
    std::string_view manifest_json
)
{
    const auto manifest_path = directory / "manifest.json";
    std::ofstream output(manifest_path, std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return std::unexpected("Failed to create manifest test file");
    }
    output << manifest_json;
    output.close();
    if (!output) {
        return std::unexpected("Failed to write manifest test file");
    }
    return read_unpacked_app_manifest(directory.generic_string());
}

bool verify_manifest_parser()
{
    TemporaryDirectory temporary_directory;
    if (!require(
            temporary_directory.is_valid(),
            "Failed to create package parser temporary directory: " + temporary_directory.get_error()
        )) {
        return false;
    }

    auto parsed = parse_manifest_case(temporary_directory.get_path(), make_package_manifest_json());
    if (!require(parsed.has_value(), "Manifest without services was rejected") ||
            !require(parsed->services.empty(), "Manifest without services produced requirements")) {
        return false;
    }

    parsed = parse_manifest_case(temporary_directory.get_path(), make_package_manifest_json("[]"));
    if (!require(parsed.has_value(), "Manifest with empty services was rejected") ||
            !require(parsed->services.empty(), "Empty services produced requirements")) {
        return false;
    }

    constexpr std::string_view WEATHER_SERVICES = R"([
        {"name":"Storage","version":"0.8.1"},
        {"name":"Http","version":"0.8.1","future":true}
    ])";
    parsed = parse_manifest_case(
                 temporary_directory.get_path(),
                 make_package_manifest_json(WEATHER_SERVICES)
             );
    if (!require(parsed.has_value(), "Weather services manifest was rejected") ||
            !require(parsed->services.size() == 2, "Weather services count is incorrect") ||
            !require(parsed->services[0].name == "Storage", "Storage requirement was not preserved") ||
            !require(parsed->services[0].version == "0.8.1", "Storage version was not preserved") ||
            !require(parsed->services[1].name == "Http", "HTTP requirement was not preserved") ||
            !require(parsed->services[1].version == "0.8.1", "HTTP version was not preserved")) {
        return false;
    }

    constexpr std::string_view IGNORED_COMPONENT_CASES[] = {
        R"([{"name":"Http","component":"brookesia_service_http","version":"0.8.1"}])",
        R"([{"name":"Http","component":{"opaque":true},"version":"0.8.1"}])",
    };
    for (const auto component_case : IGNORED_COMPONENT_CASES) {
        parsed = parse_manifest_case(
                     temporary_directory.get_path(),
                     make_package_manifest_json(component_case)
                 );
        if (!require(parsed.has_value(), "Opaque component field was rejected") ||
                !require(parsed->services.size() == 1, "Opaque-component service count is incorrect") ||
                !require(parsed->services[0].name == "Http", "Opaque component changed the service name") ||
                !require(parsed->services[0].version == "0.8.1", "Opaque component changed the service version")) {
            return false;
        }
    }

    parsed = parse_manifest_case(
                 temporary_directory.get_path(),
                 make_package_manifest_json(
                     R"([{"name":"Http","version":"0.8.1"},{"name":"http","version":"0.8.1"}])"
                 )
             );
    if (!require(parsed.has_value(), "Case-distinct RPC names were treated as duplicates") ||
            !require(parsed->services.size() == 2, "Case-distinct RPC service count is incorrect")) {
        return false;
    }

    struct InvalidCase {
        const char *name;
        const char *services_json;
    };
    constexpr InvalidCase INVALID_CASES[] = {
        {"services is not an array", R"({})"},
        {"service item is not an object", R"([1])"},
        {"name is missing", R"([{"version":"1.0.0"}])"},
        {"version is missing", R"([{"name":"Test"}])"},
        {"name has wrong type", R"([{"name":1,"version":"1.0.0"}])"},
        {"version has wrong type", R"([{"name":"Test","version":1}])"},
        {"name is empty", R"([{"name":"","version":"1.0.0"}])"},
        {"version is empty", R"([{"name":"Test","version":""}])"},
        {"name is blank", R"([{"name":"   ","version":"1.0.0"}])"},
        {"version is blank", R"([{"name":"Test","version":"   "}])"},
        {"version is invalid", R"([{"name":"Test","version":"1.0"}])"},
        {
            "service name is duplicated",
            R"([{"name":"Test","version":"1.0.0"},{"name":"Test","version":"1.0.1"}])",
        },
    };
    for (const auto &invalid_case : INVALID_CASES) {
        parsed = parse_manifest_case(
                     temporary_directory.get_path(),
                     make_package_manifest_json(invalid_case.services_json)
                 );
        if (parsed) {
            std::cerr << "Invalid manifest case unexpectedly passed: " << invalid_case.name << '\n';
            return false;
        }
    }
    return true;
}

bool verify_requirement_evaluator(ServiceManager &manager)
{
    const auto service_info = manager.get_service_info(std::string(TEST_RPC_NAME));
    if (!require(service_info.has_value(), "Requirement test service info was not found") ||
            !require(
                service_info->state == ManagerService::ServiceState::Stopped,
                "Requirement test service was not initially stopped"
            )) {
        return false;
    }

    constexpr std::string_view COMPATIBLE_VERSIONS[] = {
        "0.8.1",
        "0.8.3",
    };
    for (const auto version : COMPATIBLE_VERSIONS) {
        auto failures = evaluate_service_requirements(
                            make_manifest("compatible-evaluator", {make_test_requirement(std::string(version))}),
                            manager
                        );
        if (!require(failures.empty(), "Compatible service version was rejected: " + std::string(version))) {
            return false;
        }
    }

    struct RejectedVersion {
        const char *version;
        const char *description;
    };
    constexpr RejectedVersion REJECTED_VERSIONS[] = {
        {"0.8.4", "higher patch"},
        {"0.9.0", "different minor"},
        {"1.8.0", "different major"},
    };
    for (const auto &test_case : REJECTED_VERSIONS) {
        auto failures = evaluate_service_requirements(
                            make_manifest("rejected-evaluator", {make_test_requirement(test_case.version)}),
                            manager
                        );
        if (!require(failures.size() == 1, std::string(test_case.description) + " version was accepted") ||
                !require(failures[0].registered, std::string(test_case.description) + " service was not matched") ||
                !require(failures[0].local_version == "0.8.3", "Rejected local version was not reported")) {
            return false;
        }
    }

    auto case_failures = evaluate_service_requirements(
                             make_manifest(
                                 "case-sensitive",
                                 {AppManifestService{
                                     .name = "requirementtestrpc",
                                     .version = "0.8.1",
                                 }}
                             ),
                             manager
                         );
    if (!require(case_failures.size() == 1, "Case-mismatched RPC name was accepted") ||
            !require(!case_failures[0].registered, "Case-mismatched RPC name matched a local service")) {
        return false;
    }

    auto missing_failures = evaluate_service_requirements(
                                make_manifest(
                                    "missing",
                                    {AppManifestService{
                                        .name = "MissingRpc",
                                        .version = "1.0.0",
                                    }}
                                ),
                                manager
                            );
    if (!require(missing_failures.size() == 1, "Missing RPC requirement was accepted") ||
            !require(!missing_failures[0].registered, "Missing RPC was reported as registered")) {
        return false;
    }

    auto invalid_local_failures = evaluate_service_requirements(
                                      make_manifest(
                                          "invalid-local-version",
                                          {AppManifestService{
                                              .name = std::string(INVALID_VERSION_RPC_NAME),
                                              .version = "0.8.1",
                                          }}
                                      ),
                                      manager
                                  );
    const auto invalid_required = is_service_version_compatible("invalid", "0.8.3");
    return require(!invalid_required.has_value(), "Invalid required version was accepted") &&
           require(invalid_local_failures.size() == 1, "Invalid local version was accepted") &&
           require(invalid_local_failures[0].registered, "Invalid-version service was not registered") &&
           require(
               invalid_local_failures[0].reason.find("Local service version") != std::string::npos,
               "Invalid local version reason was not reported"
           );
}

bool verify_compatible_version(SystemHostBridge &bridge, RequirementTestService &service)
{
    constexpr AppId APP_ID = 101;
    bridge.register_app_manifest(
        APP_ID,
        make_manifest("compatible", {make_test_requirement("0.8.1")})
    );
    const auto modules = bridge.get_modules_for_app(APP_ID);

    auto available = call_native(modules, "service_available", {std::string(TEST_RPC_NAME)});
    if (!require_bool_result(available, true, "compatible service_available")) {
        return false;
    }

    const int starts_before = service.get_start_count();
    auto started = call_native(modules, "start_service", {std::string(TEST_RPC_NAME)});
    if (!require(started.has_value(), "Compatible start_service failed") ||
            !require(std::holds_alternative<int64_t>(*started), "Compatible start_service returned no handle") ||
            !require(service.get_start_count() == (starts_before + 1), "Compatible service did not start")) {
        return false;
    }
    auto stopped = call_native(modules, "stop_service", {*started});
    return require_bool_result(stopped, true, "compatible stop_service");
}

bool verify_version_rejections(SystemHostBridge &bridge)
{
    struct VersionCase {
        AppId app_id;
        const char *app_name;
        const char *required_version;
    };
    constexpr VersionCase CASES[] = {
        {102, "higher-patch", "0.8.4"},
        {103, "different-minor", "0.9.0"},
        {104, "different-major", "1.8.0"},
    };

    for (const auto &test_case : CASES) {
        bridge.register_app_manifest(
            test_case.app_id,
            make_manifest(test_case.app_name, {make_test_requirement(test_case.required_version)})
        );
        const auto modules = bridge.get_modules_for_app(test_case.app_id);
        auto available = call_native(modules, "service_available", {std::string(TEST_RPC_NAME)});
        if (!require_bool_result(available, false, test_case.app_name)) {
            return false;
        }
    }
    return true;
}

bool verify_named_call_paths_rejected(
    const std::vector<NativeModule> &modules,
    std::string_view rpc_name,
    RequirementTestService &service,
    int &event_dispatch_count,
    std::string_view context,
    std::string_view expected_reason
)
{
    const auto rpc = std::string(rpc_name);
    const auto label = std::string(context);
    const int calls_before = service.get_call_count();
    const int starts_before = service.get_start_count();
    const int events_before = event_dispatch_count;

    auto available = call_native(modules, "service_available", {rpc});
    if (!require_bool_result(available, false, label + " service_available")) {
        return false;
    }

    auto started = call_native(modules, "start_service", {rpc});
    if (!require_unavailable_error(started, label + " start_service", expected_reason)) {
        return false;
    }

    auto sync_result = call_native(
        modules,
        "call_service_function",
        {rpc, std::string("touch"), std::string("{}")}
    );
    if (!require_unavailable_error(sync_result, label + " call_service_function", expected_reason)) {
        return false;
    }

    const auto *async_function = find_native_function(modules, "call_service_function_async");
    if (!require(async_function != nullptr, "Async native function was not found") ||
            !require(static_cast<bool>(async_function->async_function), "Async native handler was not set")) {
        return false;
    }
    int callback_count = 0;
    std::optional<NativeResult> async_result;
    auto async_callback = [&callback_count, &async_result](NativeResult &&result) {
        ++callback_count;
        async_result.emplace(std::move(result));
    };
    async_function->async_function(
        {rpc, std::string("touch"), std::string("{}")},
        std::move(async_callback)
    );
    if (!require(callback_count == 1, label + " async call did not complete exactly once") ||
            !require(async_result.has_value(), label + " async call returned no result") ||
            !require_unavailable_error(
                *async_result,
                label + " call_service_function_async",
                expected_reason
            )) {
        return false;
    }

    auto batch_result = call_native(
        modules,
        "call_service_functions",
        {
            rpc,
            std::string(R"([{"name":"touch","params":{}}])"),
        }
    );
    if (!require_unavailable_error(batch_result, label + " call_service_functions", expected_reason)) {
        return false;
    }

    auto subscription = call_native(
        modules,
        "subscribe_service_event",
        {rpc, std::string("changed")}
    );
    if (!require_unavailable_error(subscription, label + " subscribe_service_event", expected_reason)) {
        return false;
    }

    return require(service.get_call_count() == calls_before, label + " function handler executed") &&
           require(service.get_start_count() == starts_before, label + " service was started") &&
           require(event_dispatch_count == events_before, label + " service event was dispatched");
}

bool verify_incompatible_call_paths(
    SystemHostBridge &bridge,
    RequirementTestService &service,
    int &event_dispatch_count
)
{
    constexpr AppId APP_ID = 102;
    return verify_named_call_paths_rejected(
               bridge.get_modules_for_app(APP_ID),
               TEST_RPC_NAME,
               service,
               event_dispatch_count,
               "incompatible requirement",
               "local version is incompatible"
           );
}

bool verify_undeclared_service(
    SystemHostBridge &bridge,
    RequirementTestService &service,
    int &event_dispatch_count
)
{
    constexpr AppId APP_ID = 105;
    bridge.register_app_manifest(APP_ID, make_manifest("undeclared-service", {}));
    return verify_named_call_paths_rejected(
               bridge.get_modules_for_app(APP_ID),
               TEST_RPC_NAME,
               service,
               event_dispatch_count,
               "undeclared service",
               "service is not declared in app manifest"
           );
}

bool verify_missing_manifest(
    SystemHostBridge &bridge,
    RequirementTestService &system_core_service,
    int &event_dispatch_count
)
{
    constexpr AppId APP_ID = 107;
    return verify_named_call_paths_rejected(
               bridge.get_modules_for_app(APP_ID),
               SystemCoreHelper::get_name(),
               system_core_service,
               event_dispatch_count,
               "unregistered manifest",
               "runtime app manifest is not registered"
           );
}

bool verify_implicit_system_services(
    SystemHostBridge &bridge,
    const std::array<std::shared_ptr<RequirementTestService>, 3> &system_services
)
{
    constexpr AppId APP_ID = 108;
    bridge.register_app_manifest(APP_ID, make_manifest("implicit-system-services", {}));
    const auto modules = bridge.get_modules_for_app(APP_ID);

    for (const auto &service : system_services) {
        const auto &rpc_name = service->get_attributes().name;
        auto available = call_native(modules, "service_available", {rpc_name});
        if (!require_bool_result(available, true, rpc_name + " implicit service_available")) {
            return false;
        }

        auto started = call_native(modules, "start_service", {rpc_name});
        if (!require(started.has_value(), rpc_name + " implicit start_service failed") ||
                !require(std::holds_alternative<int64_t>(*started), rpc_name + " returned no handle")) {
            return false;
        }

        const int calls_before = service->get_call_count();
        auto called = call_native(
            modules,
            "call_function",
            {*started, std::string("touch"), std::string("{}")}
        );
        if (!require(called.has_value(), rpc_name + " handle call failed") ||
                !require(service->get_call_count() == (calls_before + 1), rpc_name + " handler did not run")) {
            return false;
        }

        auto stopped = call_native(modules, "stop_service", {*started});
        if (!require_bool_result(stopped, true, rpc_name + " implicit stop_service")) {
            return false;
        }
    }
    return true;
}

bool verify_case_mismatched_system_service(
    SystemHostBridge &bridge,
    RequirementTestService &case_mismatched_service,
    int &event_dispatch_count
)
{
    constexpr AppId APP_ID = 109;
    bridge.register_app_manifest(APP_ID, make_manifest("case-mismatched-system-service", {}));
    return verify_named_call_paths_rejected(
               bridge.get_modules_for_app(APP_ID),
               CASE_MISMATCHED_SYSTEM_RPC_NAME,
               case_mismatched_service,
               event_dispatch_count,
               "case-mismatched system service",
               "service is not declared in app manifest"
           );
}

bool verify_explicit_system_service_version(
    SystemHostBridge &bridge,
    RequirementTestService &system_core_service,
    int &event_dispatch_count
)
{
    constexpr AppId APP_ID = 110;
    bridge.register_app_manifest(
        APP_ID,
        make_manifest(
            "explicit-system-version",
            {AppManifestService{
                .name = std::string(SystemCoreHelper::get_name()),
                .version = "0.8.4",
            }}
        )
    );
    return verify_named_call_paths_rejected(
               bridge.get_modules_for_app(APP_ID),
               SystemCoreHelper::get_name(),
               system_core_service,
               event_dispatch_count,
               "explicit system service version",
               "local version is incompatible"
           );
}

bool verify_handle_ownership(SystemHostBridge &bridge, RequirementTestService &service)
{
    constexpr AppId OWNER_APP_ID = 111;
    constexpr AppId OTHER_APP_ID = 112;
    bridge.register_app_manifest(
        OWNER_APP_ID,
        make_manifest("handle-owner", {make_test_requirement("0.8.1")})
    );
    bridge.register_app_manifest(
        OTHER_APP_ID,
        make_manifest("handle-other", {make_test_requirement("0.8.1")})
    );
    const auto owner_modules = bridge.get_modules_for_app(OWNER_APP_ID);
    const auto other_modules = bridge.get_modules_for_app(OTHER_APP_ID);

    auto started = call_native(owner_modules, "start_service", {std::string(TEST_RPC_NAME)});
    if (!require(started.has_value(), "Handle owner could not start service") ||
            !require(std::holds_alternative<int64_t>(*started), "Handle owner received no handle")) {
        return false;
    }

    const int calls_before = service.get_call_count();
    auto owner_call = call_native(
        owner_modules,
        "call_function",
        {*started, std::string("touch"), std::string("{}")}
    );
    if (!require(owner_call.has_value(), "Handle owner could not call its service") ||
            !require(service.get_call_count() == (calls_before + 1), "Handle owner call did not execute")) {
        return false;
    }

    auto other_call = call_native(
        other_modules,
        "call_function",
        {*started, std::string("touch"), std::string("{}")}
    );
    if (!require(!other_call.has_value(), "Another app called a foreign service handle") ||
            !require(
                other_call.error().find("does not belong") != std::string::npos,
                "Foreign service handle call returned an unexpected error"
            )) {
        return false;
    }

    auto other_stop = call_native(other_modules, "stop_service", {*started});
    if (!require(!other_stop.has_value(), "Another app stopped a foreign service handle") ||
            !require(
                other_stop.error().find("does not belong") != std::string::npos,
                "Foreign service handle stop returned an unexpected error"
            )) {
        return false;
    }

    auto owner_stop = call_native(owner_modules, "stop_service", {*started});
    return require_bool_result(owner_stop, true, "handle owner stop_service");
}

bool verify_unregister_fail_closed(
    SystemHostBridge &bridge,
    RequirementTestService &service,
    int &event_dispatch_count
)
{
    constexpr AppId APP_ID = 113;
    bridge.register_app_manifest(
        APP_ID,
        make_manifest("unregistered-after-registration", {make_test_requirement("0.8.1")})
    );
    const auto modules = bridge.get_modules_for_app(APP_ID);
    auto available = call_native(modules, "service_available", {std::string(TEST_RPC_NAME)});
    if (!require_bool_result(available, true, "registered service_available before unregister")) {
        return false;
    }

    bridge.unregister_app_manifest(APP_ID);
    return verify_named_call_paths_rejected(
               modules,
               TEST_RPC_NAME,
               service,
               event_dispatch_count,
               "service after manifest unregister",
               "runtime app manifest is not registered"
           );
}

bool verify_missing_service_probe(SystemHostBridge &bridge)
{
    constexpr AppId APP_ID = 106;
    bridge.register_app_manifest(
        APP_ID,
        make_manifest(
            "missing-service",
            {AppManifestService{
                .name = "MissingRpc",
                .version = "1.0.0",
            }}
        )
    );
    const auto modules = bridge.get_modules_for_app(APP_ID);

    auto available = call_native(modules, "service_available", {std::string("MissingRpc")});
    if (!require_bool_result(available, false, "missing service_available")) {
        return false;
    }
    available = call_native(modules, "service_available", {std::string("MissingRpc")});
    return require_bool_result(available, false, "repeated missing service_available");
}

bool run_tests()
{
    auto &manager = ServiceManager::get_instance();
    if (!require(manager.init(), "Failed to initialize ServiceManager")) {
        return false;
    }
    auto service = std::make_shared<RequirementTestService>();
    auto invalid_version_service = std::make_shared<RequirementTestService>(
                                       std::string(INVALID_VERSION_RPC_NAME),
                                       "invalid"
                                   );
    auto system_core_service = std::make_shared<RequirementTestService>(
                                   std::string(SystemCoreHelper::get_name())
                               );
    auto system_gui_service = std::make_shared<RequirementTestService>(
                                  std::string(SystemGuiHelper::get_name())
                              );
    auto system_timer_service = std::make_shared<RequirementTestService>(
                                    std::string(SystemTimerHelper::get_name())
                                );
    auto case_mismatched_system_service = std::make_shared<RequirementTestService>(
                                              std::string(CASE_MISMATCHED_SYSTEM_RPC_NAME)
                                          );
    const std::array system_services{
        system_core_service,
        system_gui_service,
        system_timer_service,
    };
    auto storage = std::make_shared<HostStorageService>();
    if (!require(manager.add_service(storage), "Failed to add host Storage service") ||
            !require(
                manager.add_service(invalid_version_service),
                "Failed to add invalid-version service"
            ) ||
            !require(manager.add_service(system_core_service), "Failed to add host SystemCore service") ||
            !require(manager.add_service(system_gui_service), "Failed to add host SystemGui service") ||
            !require(manager.add_service(system_timer_service), "Failed to add host SystemTimer service") ||
            !require(
                manager.add_service(case_mismatched_system_service),
                "Failed to add case-mismatched system service"
            ) ||
            !require(manager.add_service(service), "Failed to add requirement test service") ||
            !require(manager.start(), "Failed to start ServiceManager")) {
        manager.deinit();
        return false;
    }

    int event_dispatch_count = 0;
    auto event_dispatcher = [&event_dispatch_count](AppId, std::string, std::string, std::string) {
        ++event_dispatch_count;
        return std::expected<void, std::string>{};
    };
    auto function_bridge = std::make_shared<esp_brookesia::runtime::RuntimeFunctionBridge>();
    SystemHostBridge bridge(
        std::move(function_bridge),
        nullptr,
        {},
        std::move(event_dispatcher)
    );

    const bool passed = verify_manifest_parser() &&
                        verify_requirement_evaluator(manager) &&
                        verify_compatible_version(bridge, *service) &&
                        verify_version_rejections(bridge) &&
                        verify_incompatible_call_paths(bridge, *service, event_dispatch_count) &&
                        verify_undeclared_service(bridge, *service, event_dispatch_count) &&
                        verify_missing_manifest(bridge, *system_core_service, event_dispatch_count) &&
                        verify_implicit_system_services(bridge, system_services) &&
                        verify_case_mismatched_system_service(
                            bridge,
                            *case_mismatched_system_service,
                            event_dispatch_count
                        ) &&
                        verify_explicit_system_service_version(
                            bridge,
                            *system_core_service,
                            event_dispatch_count
                        ) &&
                        verify_handle_ownership(bridge, *service) &&
                        verify_unregister_fail_closed(bridge, *service, event_dispatch_count) &&
                        verify_missing_service_probe(bridge);

    bridge.release_all_app_resources();
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
        std::cout << "brookesia_system_core service requirement host test passed" << '\n';
        return EXIT_SUCCESS;
    } catch (const std::exception &error) {
        std::cerr << "Unhandled host test exception: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
