/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/runtime_manager.hpp"

using namespace esp_brookesia::runtime;

namespace {

constexpr char FAKE_BACKEND_NAME[] = "test_fake_backend";

class FakeBackend final: public IRuntimeBackend {
public:
    FakeBackend()
        : IRuntimeBackend(BackendType::Lua,
    {
        .name = FAKE_BACKEND_NAME
    })
    {}

    void set_native_modules(std::vector<NativeModule> modules) override
    {
        native_modules_ = std::move(modules);
        set_native_modules_count_++;
    }

    std::expected<void, std::string> init() override
    {
        initialized_ = true;
        init_count_++;
        return {};
    }

    void deinit() override
    {
        initialized_ = false;
        deinit_count_++;
        app_states_.clear();
    }

    std::expected<void, std::string> load_app(AppId id, const AppConfig &config) override
    {
        if (fail_next_load_) {
            fail_next_load_ = false;
            return std::unexpected("fake load failed");
        }
        loaded_configs_[id] = config;
        app_states_[id] = AppState::Loaded;
        return {};
    }

    std::expected<void, std::string> unload_app(AppId id) override
    {
        if (!app_states_.contains(id)) {
            return std::unexpected("fake app not loaded");
        }
        app_states_.erase(id);
        loaded_configs_.erase(id);
        return {};
    }

    std::expected<void, std::string> start_app(AppId id) override
    {
        if (!app_states_.contains(id)) {
            return std::unexpected("fake app not loaded");
        }
        if (fail_next_start_) {
            fail_next_start_ = false;
            app_states_[id] = AppState::Error;
            return std::unexpected("fake start failed");
        }
        app_states_[id] = AppState::Running;
        return {};
    }

    std::expected<void, std::string> stop_app(AppId id) override
    {
        if (!app_states_.contains(id)) {
            return std::unexpected("fake app not loaded");
        }
        if (fail_next_stop_) {
            fail_next_stop_ = false;
            app_states_[id] = AppState::Error;
            return std::unexpected("fake stop failed");
        }
        app_states_[id] = AppState::Stopped;
        return {};
    }

    std::expected<NativeValue, std::string> call_function(
        AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
    ) override
    {
        if (!app_states_.contains(id)) {
            return std::unexpected("fake app not loaded");
        }

        for (const auto &module : native_modules_) {
            if (module.name != module_name) {
                continue;
            }
            auto it = std::find_if(
                          module.functions.begin(), module.functions.end(),
            [&](const NativeFunctionSpec & function) {
                return function.name == function_name;
            }
                      );
            if (it == module.functions.end()) {
                return std::unexpected("fake native function not found");
            }
            if (!it->function) {
                return std::unexpected("fake native function is not sync-callable");
            }
            return it->function(args);
        }

        return std::unexpected("fake native module not found");
    }

    AppState get_app_state(AppId id) const override
    {
        auto it = app_states_.find(id);
        return (it == app_states_.end()) ? AppState::Unloaded : it->second;
    }

    size_t module_count() const
    {
        return native_modules_.size();
    }

    size_t init_count() const
    {
        return init_count_;
    }

    size_t deinit_count() const
    {
        return deinit_count_;
    }

    size_t set_native_modules_count() const
    {
        return set_native_modules_count_;
    }

    void fail_next_load()
    {
        fail_next_load_ = true;
    }

    void fail_next_start()
    {
        fail_next_start_ = true;
    }

    void fail_next_stop()
    {
        fail_next_stop_ = true;
    }

private:
    std::vector<NativeModule> native_modules_;
    std::map<AppId, AppState> app_states_;
    std::map<AppId, AppConfig> loaded_configs_;
    size_t init_count_ = 0;
    size_t deinit_count_ = 0;
    size_t set_native_modules_count_ = 0;
    bool initialized_ = false;
    bool fail_next_load_ = false;
    bool fail_next_start_ = false;
    bool fail_next_stop_ = false;
};

std::shared_ptr<FakeBackend> register_fake_backend()
{
    RuntimeBackendRegistry::remove_all_plugins();
    RuntimeBackendRegistry::register_plugin<FakeBackend>(FAKE_BACKEND_NAME, []() {
        return std::make_shared<FakeBackend>();
    });
    auto backend = std::dynamic_pointer_cast<FakeBackend>(RuntimeBackendRegistry::get_instance(FAKE_BACKEND_NAME));
    TEST_ASSERT_NOT_NULL(backend);
    return backend;
}

NativeModule make_math_module()
{
    NativeModule module;
    module.name = "math";

    module.functions.push_back({
        .name = "add",
        .function = [](const NativeArgs & args) -> NativeResult {
            if (args.size() != 2)
            {
                return std::unexpected("add requires two args");
            }
            auto lhs = std::get_if<int64_t>(&args[0]);
            auto rhs = std::get_if<int64_t>(&args[1]);
            if ((lhs == nullptr) || (rhs == nullptr))
            {
                return std::unexpected("add args must be int64");
            }
            return NativeValue{*lhs + *rhs};
        },
        .async_function = nullptr,
    });

    return module;
}

std::shared_ptr<RuntimeHostBridge> make_host_bridge_with_math()
{
    auto bridge = std::make_shared<RuntimeHostBridge>();
    bridge->add_module(make_math_module());
    return bridge;
}

AppConfig make_app_config()
{
    return {
        .type = BackendType::Unknown,
        .app_path = "/tmp/fake_runtime_app",
        .entry = "main",
        .resource_dir = "/tmp/fake_runtime_app/assets",
        .arguments = {"--test"},
    };
}

} // namespace

BROOKESIA_TEST_CASE(
    test_function_bridge_validation_and_context,
    "RuntimeFunctionBridge validates modules and scopes app context",
    "[runtime][manager][function_bridge]"
)
{
    RuntimeFunctionBridge bridge;

    auto empty_module = bridge.add_module({});
    TEST_ASSERT_FALSE(empty_module.has_value());

    NativeModule invalid_function_module;
    invalid_function_module.name = "invalid";
    invalid_function_module.functions.push_back({.name = "", .function = nullptr, .async_function = nullptr});
    auto invalid_function = bridge.add_module(std::move(invalid_function_module));
    TEST_ASSERT_FALSE(invalid_function.has_value());

    auto add_result = bridge.add_module(make_math_module());
    TEST_ASSERT_TRUE(add_result.has_value());
    auto duplicate_result = bridge.add_module(make_math_module());
    TEST_ASSERT_FALSE(duplicate_result.has_value());

    auto no_context = bridge.get_current_app_id();
    TEST_ASSERT_FALSE(no_context.has_value());

    {
        auto context = bridge.make_app_context_guard(42);
        (void)context;
        auto current = bridge.get_current_app_id();
        TEST_ASSERT_TRUE(current.has_value());
        TEST_ASSERT_EQUAL_UINT32(42, current.value());

        const auto token = bridge.get_or_create_context_token(42);
        bridge.detach_context();
        TEST_ASSERT_FALSE(bridge.get_current_app_id().has_value());
        auto attach_result = bridge.attach_context(token);
        TEST_ASSERT_TRUE(attach_result.has_value());
        TEST_ASSERT_EQUAL_UINT32(42, bridge.get_current_app_id().value());
    }

    TEST_ASSERT_FALSE(bridge.get_current_app_id().has_value());
    bridge.release_all_app_contexts();
    TEST_ASSERT_FALSE(bridge.attach_context(1).has_value());
}

BROOKESIA_TEST_CASE(
    test_runtime_lifecycle_with_fake_backend,
    "Runtime manages fake backend app lifecycle and native calls",
    "[runtime][manager][lifecycle]"
)
{
    auto backend = register_fake_backend();
    Runtime runtime(make_host_bridge_with_math());

    auto init_result = runtime.init();
    TEST_ASSERT_TRUE(init_result.has_value());
    TEST_ASSERT_EQUAL(BackendType::Lua, runtime.get_type());
    TEST_ASSERT_GREATER_OR_EQUAL(2, backend->module_count());
    TEST_ASSERT_EQUAL_size_t(1, backend->init_count());

    auto app_id = runtime.load_app(make_app_config());
    TEST_ASSERT_TRUE(app_id.has_value());
    TEST_ASSERT_TRUE(runtime.get_app(app_id.value()).has_value());
    TEST_ASSERT_EQUAL_size_t(1, runtime.list_apps().size());

    auto start_result = runtime.start_app(app_id.value());
    TEST_ASSERT_TRUE(start_result.has_value());
    TEST_ASSERT_EQUAL(AppState::Running, runtime.get_app(app_id.value())->state);

    auto add_result = runtime.call_function(app_id.value(), "math", "add", {NativeValue{int64_t{2}}, NativeValue{int64_t{3}}});
    TEST_ASSERT_TRUE(add_result.has_value());
    TEST_ASSERT_EQUAL(int64_t{5}, std::get<int64_t>(add_result.value()));

    auto context_result = runtime.call_function(app_id.value(), "brookesia", "current_app_context");
    TEST_ASSERT_TRUE(context_result.has_value());
    TEST_ASSERT_TRUE(std::get<int64_t>(context_result.value()) > 0);

    auto stop_result = runtime.stop_app(app_id.value());
    TEST_ASSERT_TRUE(stop_result.has_value());
    TEST_ASSERT_EQUAL(AppState::Stopped, runtime.get_app(app_id.value())->state);

    auto unload_result = runtime.unload_app(app_id.value());
    TEST_ASSERT_TRUE(unload_result.has_value());
    TEST_ASSERT_FALSE(runtime.get_app(app_id.value()).has_value());

    runtime.deinit();
    TEST_ASSERT_EQUAL_size_t(1, backend->deinit_count());
}

BROOKESIA_TEST_CASE(
    test_runtime_finish_request_stops_app_after_native_call,
    "Runtime consumes finish requests after native calls",
    "[runtime][manager][finish]"
)
{
    register_fake_backend();
    Runtime runtime(make_host_bridge_with_math());
    TEST_ASSERT_TRUE(runtime.init().has_value());
    auto app_id = runtime.load_app(make_app_config());
    TEST_ASSERT_TRUE(app_id.has_value());
    TEST_ASSERT_TRUE(runtime.start_app(app_id.value()).has_value());

    auto finish_result = runtime.call_function(app_id.value(), "brookesia", "finish_app");
    TEST_ASSERT_TRUE(finish_result.has_value());
    TEST_ASSERT_EQUAL(AppState::Stopped, runtime.get_app(app_id.value())->state);
}

BROOKESIA_TEST_CASE(
    test_runtime_error_paths,
    "Runtime reports missing backend and lifecycle errors",
    "[runtime][manager][error]"
)
{
    RuntimeBackendRegistry::remove_all_plugins();
    Runtime runtime_without_backend;
    TEST_ASSERT_FALSE(runtime_without_backend.init().has_value());
    TEST_ASSERT_FALSE(runtime_without_backend.load_app(make_app_config()).has_value());

    auto backend = register_fake_backend();
    Runtime runtime(make_host_bridge_with_math());
    TEST_ASSERT_TRUE(runtime.init().has_value());

    backend->fail_next_load();
    TEST_ASSERT_FALSE(runtime.load_app(make_app_config()).has_value());
    TEST_ASSERT_FALSE(runtime.start_app(999).has_value());

    auto app_id = runtime.load_app(make_app_config());
    TEST_ASSERT_TRUE(app_id.has_value());

    backend->fail_next_start();
    auto start_result = runtime.start_app(app_id.value());
    TEST_ASSERT_FALSE(start_result.has_value());
    TEST_ASSERT_EQUAL(AppState::Error, runtime.get_app(app_id.value())->state);

    TEST_ASSERT_TRUE(runtime.start_app(app_id.value()).has_value());
    backend->fail_next_stop();
    auto stop_result = runtime.stop_app(app_id.value());
    TEST_ASSERT_FALSE(stop_result.has_value());
    TEST_ASSERT_EQUAL(AppState::Error, runtime.get_app(app_id.value())->state);
}
