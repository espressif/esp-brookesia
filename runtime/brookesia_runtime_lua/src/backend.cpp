/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_lua/backend.hpp"

#include <map>
#include <string>
#include <type_traits>
#include <utility>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/runtime_manager/detail/native_utils.hpp"
#include "brookesia/runtime_lua/macro_configs.h"
#if !BROOKESIA_RUNTIME_LUA_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

extern "C" {
#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"
}

namespace esp_brookesia::runtime::lua {
namespace {

NativeValue lua_to_native(lua_State *state, int index)
{
    switch (lua_type(state, index)) {
    case LUA_TBOOLEAN:
        return NativeValue{lua_toboolean(state, index) != 0};
    case LUA_TNUMBER:
        if (lua_isinteger(state, index)) {
            return NativeValue{static_cast<int64_t>(lua_tointeger(state, index))};
        }
        return NativeValue{static_cast<double>(lua_tonumber(state, index))};
    case LUA_TSTRING:
        return NativeValue{std::string(lua_tostring(state, index))};
    default:
        return NativeValue{};
    }
}

void push_native_value(lua_State *state, const NativeValue &value)
{
    std::visit(
    [state](const auto & item) {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, std::monostate>) {
            lua_pushnil(state);
        } else if constexpr (std::is_same_v<ItemType, bool>) {
            lua_pushboolean(state, item);
        } else if constexpr (std::is_same_v<ItemType, int64_t>) {
            lua_pushinteger(state, static_cast<lua_Integer>(item));
        } else if constexpr (std::is_same_v<ItemType, double>) {
            lua_pushnumber(state, static_cast<lua_Number>(item));
        } else if constexpr (std::is_same_v<ItemType, std::string>) {
            lua_pushlstring(state, item.data(), item.size());
        }
    },
    value
    );
}

int native_function_thunk(lua_State *state)
{
    auto *function = static_cast<NativeFunctionSpec *>(lua_touserdata(state, lua_upvalueindex(1)));
    if (function == nullptr) {
        return luaL_error(state, "native function is not available");
    }
    if (!function->function) {
        return luaL_error(state, "asynchronous native function is not supported by Lua backend");
    }

    NativeArgs args;
    const int argc = lua_gettop(state);
    args.reserve(argc);
    for (int i = 1; i <= argc; ++i) {
        args.push_back(lua_to_native(state, i));
    }

    auto result = function->function(args);
    if (!result) {
        return luaL_error(state, "%s", result.error().c_str());
    }
    push_native_value(state, result.value());
    return 1;
}

void register_native_modules(lua_State *state, std::vector<NativeModule> &modules)
{
    for (auto &module : modules) {
        lua_newtable(state);
        for (auto &function : module.functions) {
            lua_pushlightuserdata(state, &function);
            lua_pushcclosure(state, native_function_thunk, 1);
            lua_setfield(state, -2, function.name.c_str());
        }
        lua_setglobal(state, module.name.c_str());
    }
}

} // namespace

class Backend::Impl {
public:
    struct AppRecord {
        AppId id = INVALID_APP_ID;
        AppConfig config;
        std::vector<NativeModule> native_modules;
        lua_State *state = nullptr;
        AppState app_state = AppState::Unloaded;
    };

    std::vector<NativeModule> native_modules_;
    std::map<AppId, AppRecord> apps_;
    bool initialized_ = false;
};

Backend::Backend()
    : IRuntimeBackend(BackendType::Lua,
{
    .name = BROOKESIA_RUNTIME_LUA_BACKEND_NAME,
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
        "Version: %1%.%2%.%3%", BROOKESIA_RUNTIME_LUA_VER_MAJOR, BROOKESIA_RUNTIME_LUA_VER_MINOR,
        BROOKESIA_RUNTIME_LUA_VER_PATCH
    );
    BROOKESIA_LOGD("Params: initialized(%1%)", impl_->initialized_);
    impl_->initialized_ = true;
    BROOKESIA_LOGD("Lua backend initialized");
    return {};
}

void Backend::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_count(%1%), initialized(%2%)", impl_->apps_.size(), impl_->initialized_);
    for (auto &[id, app] : impl_->apps_) {
        if (app.state != nullptr) {
            lua_close(app.state);
            app.state = nullptr;
        }
    }
    impl_->apps_.clear();
    impl_->initialized_ = false;
    BROOKESIA_LOGD("Lua backend deinitialized");
}

std::expected<void, std::string> Backend::load_app(AppId id, const AppConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: app_id(%1%), config(%2%), module_count(%3%)",
        id, config, impl_->native_modules_.size()
    );
    if (!impl_->initialized_) {
        BROOKESIA_LOGW("Lua app load failed: backend is not initialized, id(%1%)", id);
        return std::unexpected("Lua backend is not initialized");
    }
    if (impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("Lua app load failed: id(%1%) already exists", id);
        return std::unexpected("Lua app id already exists");
    }

    lua_State *state = luaL_newstate();
    if (state == nullptr) {
        BROOKESIA_LOGE("Lua app load failed: create state failed, id(%1%)", id);
        return std::unexpected("Failed to create Lua state");
    }
    luaL_openlibs(state);

    Impl::AppRecord record;
    record.id = id;
    record.config = config;
    record.native_modules = impl_->native_modules_;
    record.state = state;
    record.app_state = AppState::Loaded;
    register_native_modules(state, record.native_modules);
    impl_->apps_.emplace(id, std::move(record));
    BROOKESIA_LOGD("Lua app loaded: id(%1%), path(%2%)", id, config.app_path);
    return {};
}

std::expected<void, std::string> Backend::unload_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("Lua app unload failed: id(%1%) not found", id);
        return std::unexpected("Lua app not found");
    }
    lua_close(it->second.state);
    impl_->apps_.erase(it);
    BROOKESIA_LOGD("Lua app unloaded: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::start_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("Lua app start failed: id(%1%) not found", id);
        return std::unexpected("Lua app not found");
    }

    const std::string entry_path = detail::make_entry_path(it->second.config);
    BROOKESIA_LOGD("Lua app starting: id(%1%), entry(%2%)", id, entry_path);
    if (luaL_dofile(it->second.state, entry_path.c_str()) != LUA_OK) {
        std::string error = lua_tostring(it->second.state, -1);
        lua_pop(it->second.state, 1);
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("Lua app start failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, error);
        return std::unexpected(error);
    }
    it->second.app_state = AppState::Running;
    BROOKESIA_LOGD("Lua app started: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::stop_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("Lua app stop failed: id(%1%) not found", id);
        return std::unexpected("Lua app not found");
    }
    it->second.app_state = AppState::Stopped;
    BROOKESIA_LOGD("Lua app stopped: id(%1%)", id);
    return {};
}

std::expected<NativeValue, std::string> Backend::call_function(
    AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: app_id(%1%), module(%2%), function(%3%), arg_count(%4%)",
        id, module_name, function_name, args.size()
    );
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("Lua function call failed: id(%1%) not found", id);
        return std::unexpected("Lua app not found");
    }

    lua_State *state = it->second.state;
    lua_getglobal(state, std::string(module_name).c_str());
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        BROOKESIA_LOGW("Lua function call failed: id(%1%), module(%2%) not found", id, module_name);
        return std::unexpected("Lua module not found");
    }
    lua_getfield(state, -1, std::string(function_name).c_str());
    lua_remove(state, -2);
    if (!lua_isfunction(state, -1)) {
        lua_pop(state, 1);
        BROOKESIA_LOGW(
            "Lua function call failed: id(%1%), module(%2%), function(%3%) not found",
            id, module_name, function_name
        );
        return std::unexpected("Lua function not found");
    }
    for (const auto &arg : args) {
        push_native_value(state, arg);
    }
    if (lua_pcall(state, static_cast<int>(args.size()), 1, 0) != LUA_OK) {
        std::string error = lua_tostring(state, -1);
        lua_pop(state, 1);
        BROOKESIA_LOGE(
            "Lua function call failed: id(%1%), module(%2%), function(%3%), error(%4%)",
            id, module_name, function_name, error
        );
        return std::unexpected(error);
    }
    NativeValue result = lua_to_native(state, -1);
    lua_pop(state, 1);
    return result;
}

AppState Backend::get_app_state(AppId id) const
{
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        return AppState::Unloaded;
    }
    return it->second.app_state;
}

#if BROOKESIA_RUNTIME_LUA_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    IRuntimeBackend, Backend, Backend::get_instance().get_attributes().name, Backend::get_instance(),
    BROOKESIA_RUNTIME_LUA_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::runtime::lua
