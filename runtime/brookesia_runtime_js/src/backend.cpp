/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_js/backend.hpp"

#include <map>
#include <deque>
#include <functional>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/lib_utils/task_scheduler.hpp"
#include "brookesia/runtime_manager/detail/native_utils.hpp"
#include "brookesia/runtime_js/macro_configs.h"
#include "brookesia/service_helper.hpp"
#if !BROOKESIA_RUNTIME_JS_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"

#if defined(BROOKESIA_RUNTIME_JS_HAS_QUICKJS)
extern "C" {
#include "quickjs.h"
#include "quickjs-libc.h"
}
#endif

namespace esp_brookesia::runtime::js {

#if defined(BROOKESIA_RUNTIME_JS_HAS_QUICKJS)
namespace {

constexpr uint32_t STORAGE_FS_TIMEOUT_MS = 5000;

constexpr size_t JS_MEMORY_LIMIT = 16 * 1024 * 1024;
#if defined(__EMSCRIPTEN__)
constexpr size_t JS_MAX_STACK_SIZE = 8 * 1024 * 1024;
#else
constexpr size_t JS_MAX_STACK_SIZE = 512 * 1024;
#endif

static bool qjs_is_ident_continue(unsigned char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '$';
}

static const uint8_t *qjs_skip_shebang_comments_ws(const uint8_t *p, size_t len)
{
    const uint8_t *end = p + len;

    while (p < end) {
        unsigned char c = *p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
            p++;
            continue;
        }
        if (p[0] == '#' && p + 1 < end && p[1] == '!') {
            p += 2;
            while (p < end && *p != '\n' && *p != '\r') {
                p++;
            }
            continue;
        }
        if (p + 1 < end && p[0] == '/' && p[1] == '*') {
            p += 2;
            while (p + 1 < end && !(p[0] == '*' && p[1] == '/')) {
                p++;
            }
            if (p + 1 < end) {
                p += 2;
            }
            continue;
        }
        if (p + 1 < end && p[0] == '/' && p[1] == '/') {
            p += 2;
            while (p < end && *p != '\n' && *p != '\r') {
                p++;
            }
            continue;
        }
        break;
    }
    return p;
}

static bool qjs_source_looks_like_es_module(const uint8_t *data, size_t len)
{
    const uint8_t *p = qjs_skip_shebang_comments_ws(data, len);
    const uint8_t *end = data + len;

    if (p + 6 > end) {
        return false;
    }
    if (std::memcmp(p, "import", 6) == 0 && !qjs_is_ident_continue(p[6])) {
        return true;
    }
    if (std::memcmp(p, "export", 6) == 0 && !qjs_is_ident_continue(p[6])) {
        return true;
    }
    return false;
}

std::string get_exception_string(JSContext *context);

void drain_microtasks(JSContext *context, AppId app_id = INVALID_APP_ID)
{
    JSRuntime *rt = JS_GetRuntime(context);
    while (JS_IsJobPending(rt)) {
        JSContext *ctx_ref = nullptr;
        const int result = JS_ExecutePendingJob(rt, &ctx_ref);
        if (result < 0) {
            JSContext *error_context = ctx_ref != nullptr ? ctx_ref : context;
            if (app_id != INVALID_APP_ID) {
                BROOKESIA_LOGW(
                    "JavaScript microtask failed: app_id(%1%), error(%2%)",
                    app_id, get_exception_string(error_context)
                );
            } else {
                BROOKESIA_LOGW("JavaScript microtask failed: %1%", get_exception_string(error_context));
            }
        }
    }
}

std::expected<std::string, std::string> read_all(const std::string &path)
{
    auto result = service::helper::Storage::fs_read_text(path, STORAGE_FS_TIMEOUT_MS);
    if (!result) {
        return std::unexpected("Failed to read JavaScript app: " + path + ", error: " + result.error());
    }
    return result.value();
}

std::string get_exception_string(JSContext *context)
{
    JSValue exception = JS_GetException(context);
    const char *message = JS_ToCString(context, exception);
    std::string error = message != nullptr ? message : "JavaScript exception";
    if (message != nullptr) {
        JS_FreeCString(context, message);
    }
    JS_FreeValue(context, exception);
    return error;
}

/** Default brookesia_app hooks when app script omits part or all of the module. */
void ensure_js_lifecycle_module(JSContext *context, AppId app_id)
{
    static const char kLifecycleShim[] =
        "(function(){"
        "  var g = globalThis;"
        "  if (!g.brookesia_app || typeof g.brookesia_app !== 'object') {"
        "    g.brookesia_app = {};"
        "  }"
        "  var app = g.brookesia_app;"
        "  var noop = function(){ return true; };"
        "  if (typeof app.on_install !== 'function') app.on_install = noop;"
        "  if (typeof app.on_start !== 'function') app.on_start = noop;"
        "  if (typeof app.on_stop !== 'function') app.on_stop = noop;"
        "  if (typeof app.on_pause !== 'function') app.on_pause = noop;"
        "  if (typeof app.on_resume !== 'function') app.on_resume = noop;"
        "  if (typeof app.on_uninstall !== 'function') app.on_uninstall = noop;"
        "  if (typeof app.on_action !== 'function') app.on_action = noop;"
        "  if (typeof app.on_event !== 'function') app.on_event = noop;"
        "  if (typeof app.on_timer !== 'function') app.on_timer = noop;"
        "})();";
    JSValue result = JS_Eval(
                         context,
                         kLifecycleShim,
                         std::strlen(kLifecycleShim),
                         "<brookesia-lifecycle-shim>",
                         JS_EVAL_TYPE_GLOBAL
                     );
    if (JS_IsException(result)) {
        BROOKESIA_LOGW(
            "JavaScript lifecycle shim failed: %1%",
            get_exception_string(context)
        );
    }
    JS_FreeValue(context, result);
    drain_microtasks(context, app_id);
}

NativeValue js_to_native(JSContext *context, JSValueConst value)
{
    if (JS_IsBool(value)) {
        return NativeValue{JS_ToBool(context, value) != 0};
    }
    if (JS_IsNumber(value)) {
        int64_t integer = 0;
        if (JS_ToInt64(context, &integer, value) == 0) {
            return NativeValue{integer};
        }
        double number = 0;
        if (JS_ToFloat64(context, &number, value) == 0) {
            return NativeValue{number};
        }
    }
    if (JS_IsString(value)) {
        const char *str = JS_ToCString(context, value);
        std::string result = str != nullptr ? str : "";
        if (str != nullptr) {
            JS_FreeCString(context, str);
        }
        return NativeValue{result};
    }
    return NativeValue{};
}

JSValue native_to_js(JSContext *context, const NativeValue &value)
{
    return std::visit(
    [context](const auto & item) -> JSValue {
        using ItemType = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<ItemType, std::monostate>)
        {
            return JS_UNDEFINED;
        } else if constexpr (std::is_same_v<ItemType, bool>)
        {
            return JS_NewBool(context, item);
        } else if constexpr (std::is_same_v<ItemType, int64_t>)
        {
            return JS_NewInt64(context, item);
        } else if constexpr (std::is_same_v<ItemType, double>)
        {
            return JS_NewFloat64(context, item);
        } else if constexpr (std::is_same_v<ItemType, std::string>)
        {
            return JS_NewStringLen(context, item.data(), item.size());
        }
    },
    value
           );
}

} // namespace

struct JsAppRecord {
    struct PendingPromise {
        JSValue resolve = JS_UNDEFINED;
        JSValue reject = JS_UNDEFINED;
    };

    struct AsyncCompletion {
        uint64_t promise_id = 0;
        NativeResult result;
    };

    AppId id = INVALID_APP_ID;
    AppConfig config;
    std::function < void(AppId, uint64_t, NativeResult &&) > enqueue_async_completion;
    JSRuntime *runtime = nullptr;
    JSContext *context = nullptr;
    std::vector<NativeModule> native_modules;
    std::vector<NativeFunctionSpec *> flat_functions;
    std::map<uint64_t, PendingPromise> pending_promises;
    std::deque<AsyncCompletion> async_completions;
    uint64_t next_promise_id = 1;
    AppState app_state = AppState::Unloaded;
};

class Backend::Impl {
public:
    static constexpr const char *ASYNC_TASK_GROUP = "RuntimeJsAsync";

    void reject_all_pending_promises(JsAppRecord &app, const std::string &message);
    void enqueue_async_completion(AppId id, uint64_t promise_id, NativeResult result);
    void flush_async_completions(AppId id);

    std::vector<NativeModule> native_modules_;
    std::map<AppId, JsAppRecord> apps_;
    std::recursive_mutex mutex_;
    lib_utils::TaskScheduler async_scheduler_;
    bool initialized_ = false;
};

namespace {

void reject_promise(JSContext *context, JSValue reject, const std::string &message)
{
    JSValue error = JS_NewStringLen(context, message.data(), message.size());
    JSValue result = JS_Call(context, reject, JS_UNDEFINED, 1, &error);
    if (JS_IsException(result)) {
        BROOKESIA_LOGW("JavaScript async promise reject handler failed: %1%", get_exception_string(context));
    }
    JS_FreeValue(context, result);
    JS_FreeValue(context, error);
}

void free_pending_promise(JSContext *context, JsAppRecord::PendingPromise &promise)
{
    JS_FreeValue(context, promise.resolve);
    JS_FreeValue(context, promise.reject);
    promise.resolve = JS_UNDEFINED;
    promise.reject = JS_UNDEFINED;
}

void flush_app_async_completions(JsAppRecord &app)
{
    if (app.context == nullptr) {
        app.async_completions.clear();
        return;
    }
    if (app.runtime != nullptr) {
        JS_UpdateStackTop(app.runtime);
    }
    bool resolved_any = false;
    while (!app.async_completions.empty()) {
        auto completion = std::move(app.async_completions.front());
        app.async_completions.pop_front();

        auto promise_it = app.pending_promises.find(completion.promise_id);
        if (promise_it == app.pending_promises.end()) {
            continue;
        }

        auto &promise = promise_it->second;
        if (completion.result) {
            JSValue value = native_to_js(app.context, completion.result.value());
            JSValue result = JS_Call(app.context, promise.resolve, JS_UNDEFINED, 1, &value);
            if (JS_IsException(result)) {
                BROOKESIA_LOGW(
                    "JavaScript async promise resolve handler failed: app_id(%1%), error(%2%)",
                    app.id, get_exception_string(app.context)
                );
            }
            JS_FreeValue(app.context, result);
            JS_FreeValue(app.context, value);
        } else {
            reject_promise(app.context, promise.reject, completion.result.error());
        }

        free_pending_promise(app.context, promise);
        app.pending_promises.erase(promise_it);
        resolved_any = true;
    }

    if (resolved_any) {
        drain_microtasks(app.context, app.id);
    }
}

JSValue native_function_thunk(
    JSContext *context, JSValueConst this_value, int argc, JSValueConst *argv, int magic
)
{
    (void)this_value;
    auto *app = static_cast<JsAppRecord *>(JS_GetContextOpaque(context));
    if ((app == nullptr) || (magic < 0) || (static_cast<size_t>(magic) >= app->flat_functions.size())) {
        return JS_ThrowInternalError(context, "native function is not available");
    }

    NativeArgs args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(js_to_native(context, argv[i]));
    }

    auto *function = app->flat_functions[static_cast<size_t>(magic)];
    if (!function->function) {
        return JS_ThrowInternalError(context, "native function is asynchronous; use its Promise-returning form");
    }
    auto result = function->function(args);
    if (!result) {
        return JS_ThrowInternalError(context, "%s", result.error().c_str());
    }
    return native_to_js(context, result.value());
}

JSValue native_async_function_thunk(
    JSContext *context, JSValueConst this_value, int argc, JSValueConst *argv, int magic
)
{
    (void)this_value;
    auto *app = static_cast<JsAppRecord *>(JS_GetContextOpaque(context));
    if ((app == nullptr) || (magic < 0) || (static_cast<size_t>(magic) >= app->flat_functions.size())) {
        return JS_ThrowInternalError(context, "native async function is not available");
    }

    auto *function = app->flat_functions[static_cast<size_t>(magic)];
    if (!function->async_function) {
        return JS_ThrowInternalError(context, "native function is synchronous; use the sync call form");
    }

    NativeArgs args;
    args.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        args.push_back(js_to_native(context, argv[i]));
    }

    JSValue resolving_functions[2] = {JS_UNDEFINED, JS_UNDEFINED};
    JSValue promise = JS_NewPromiseCapability(context, resolving_functions);
    if (JS_IsException(promise)) {
        JS_FreeValue(context, resolving_functions[0]);
        JS_FreeValue(context, resolving_functions[1]);
        return promise;
    }

    const uint64_t promise_id = app->next_promise_id++;
    app->pending_promises.emplace(
        promise_id,
    JsAppRecord::PendingPromise{
        .resolve = resolving_functions[0],
        .reject = resolving_functions[1],
    }
    );

    try {
        function->async_function(
            args,
        [enqueue = app->enqueue_async_completion, app_id = app->id, promise_id](NativeResult && result) mutable {
            if (!enqueue)
            {
                return;
            }
            enqueue(app_id, promise_id, std::move(result));
        }
        );
    } catch (const std::exception &e) {
        auto promise_it = app->pending_promises.find(promise_id);
        if (promise_it != app->pending_promises.end()) {
            reject_promise(context, promise_it->second.reject, e.what());
            free_pending_promise(context, promise_it->second);
            app->pending_promises.erase(promise_it);
        }
    } catch (...) {
        auto promise_it = app->pending_promises.find(promise_id);
        if (promise_it != app->pending_promises.end()) {
            reject_promise(context, promise_it->second.reject, "Native async function threw an unknown exception");
            free_pending_promise(context, promise_it->second);
            app->pending_promises.erase(promise_it);
        }
    }

    flush_app_async_completions(*app);
    return promise;
}

void register_native_modules(JSContext *context, JsAppRecord &app, std::vector<NativeModule> &modules)
{
    JSValue global = JS_GetGlobalObject(context);
    int magic = 0;
    for (auto &module : modules) {
        JSValue module_object = JS_NewObject(context);
        for (auto &function : module.functions) {
            app.flat_functions.push_back(&function);
            JSValue js_function = function.async_function ?
                                  JS_NewCFunctionMagic(
                                      context, native_async_function_thunk, function.name.c_str(), 0,
                                      JS_CFUNC_generic_magic, magic++
                                  ) :
                                  JS_NewCFunctionMagic(
                                      context, native_function_thunk, function.name.c_str(), 0,
                                      JS_CFUNC_generic_magic, magic++
                                  );
            JS_SetPropertyStr(context, module_object, function.name.c_str(), js_function);
        }
        JS_SetPropertyStr(context, global, module.name.c_str(), module_object);
    }
    JS_FreeValue(context, global);
}

} // namespace

void Backend::Impl::reject_all_pending_promises(JsAppRecord &app, const std::string &message)
{
    if (app.context == nullptr) {
        app.pending_promises.clear();
        app.async_completions.clear();
        return;
    }
    if (app.runtime != nullptr) {
        JS_UpdateStackTop(app.runtime);
    }
    for (auto &[promise_id, promise] : app.pending_promises) {
        (void)promise_id;
        reject_promise(app.context, promise.reject, message);
        free_pending_promise(app.context, promise);
    }
    app.pending_promises.clear();
    app.async_completions.clear();
    drain_microtasks(app.context, app.id);
}

void Backend::Impl::enqueue_async_completion(AppId id, uint64_t promise_id, NativeResult result)
{
    {
        std::lock_guard lock(mutex_);
        auto it = apps_.find(id);
        if (it == apps_.end()) {
            BROOKESIA_LOGD("Drop JavaScript async completion for unloaded app: id(%1%)", id);
            return;
        }
        it->second.async_completions.push_back(JsAppRecord::AsyncCompletion{
            .promise_id = promise_id,
            .result = std::move(result),
        });
    }

    if (!async_scheduler_.is_running()) {
        flush_async_completions(id);
        return;
    }
    auto post_result = async_scheduler_.post(
    [this, id]() {
        flush_async_completions(id);
    },
    nullptr,
    ASYNC_TASK_GROUP
                       );
    if (!post_result) {
        BROOKESIA_LOGW("Failed to post JavaScript async completion task: app_id(%1%)", id);
        flush_async_completions(id);
    }
}

void Backend::Impl::flush_async_completions(AppId id)
{
    std::lock_guard lock(mutex_);
    auto app_it = apps_.find(id);
    if (app_it == apps_.end()) {
        return;
    }
    auto &app = app_it->second;
    if (app.context == nullptr) {
        return;
    }
    flush_app_async_completions(app);
}

Backend::Backend()
    : IRuntimeBackend(BackendType::JavaScript,
{
    .name = BROOKESIA_RUNTIME_JS_BACKEND_NAME,
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
        "Version: %1%.%2%.%3%", BROOKESIA_RUNTIME_JS_VER_MAJOR, BROOKESIA_RUNTIME_JS_VER_MINOR,
        BROOKESIA_RUNTIME_JS_VER_PATCH
    );
    std::lock_guard lock(impl_->mutex_);
    BROOKESIA_LOGD("Params: initialized(%1%)", impl_->initialized_);
    if (impl_->initialized_) {
        return {};
    }
    lib_utils::TaskScheduler::StartConfig scheduler_config;
    scheduler_config.worker_configs = {
        lib_utils::ThreadConfig{
            .name = "RuntimeJsAsync",
            .stack_size = 8 * 1024,
        }
    };
    if (!impl_->async_scheduler_.start(scheduler_config)) {
        BROOKESIA_LOGW("JavaScript backend init failed: failed to start async completion scheduler");
        return std::unexpected("Failed to start JavaScript async completion scheduler");
    }
    impl_->async_scheduler_.configure_group(
        Impl::ASYNC_TASK_GROUP,
    lib_utils::TaskScheduler::GroupConfig{
        .enable_serial_execution = true,
    }
    );
    impl_->initialized_ = true;
    BROOKESIA_LOGD("JavaScript backend initialized");
    return {};
}

void Backend::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    {
        std::lock_guard lock(impl_->mutex_);
        BROOKESIA_LOGD("Params: app_count(%1%), initialized(%2%)", impl_->apps_.size(), impl_->initialized_);
        for (auto &[id, app] : impl_->apps_) {
            impl_->reject_all_pending_promises(app, "JavaScript backend is deinitializing");
            if (app.context != nullptr) {
                JSRuntime *rt = JS_GetRuntime(app.context);
                JS_FreeContext(app.context);
                app.context = nullptr;
                if (app.runtime != nullptr) {
                    js_std_free_handlers(rt);
                    JS_FreeRuntime(app.runtime);
                    app.runtime = nullptr;
                }
            } else if (app.runtime != nullptr) {
                js_std_free_handlers(app.runtime);
                JS_FreeRuntime(app.runtime);
                app.runtime = nullptr;
            }
        }
        impl_->apps_.clear();
        impl_->initialized_ = false;
    }
    if (impl_->async_scheduler_.is_running()) {
        impl_->async_scheduler_.stop();
    }
    BROOKESIA_LOGD("JavaScript backend deinitialized");
}

std::expected<void, std::string> Backend::load_app(AppId id, const AppConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    std::lock_guard lock(impl_->mutex_);
    BROOKESIA_LOGD(
        "Params: app_id(%1%), config(%2%), module_count(%3%)",
        id, config, impl_->native_modules_.size()
    );
    if (!impl_->initialized_) {
        BROOKESIA_LOGW("JavaScript app load failed: backend is not initialized, id(%1%)", id);
        return std::unexpected("JavaScript backend is not initialized");
    }
    if (impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("JavaScript app load failed: id(%1%) already exists", id);
        return std::unexpected("JavaScript app id already exists");
    }

    JsAppRecord app;
    app.id = id;
    app.config = config;
    app.enqueue_async_completion = [impl = impl_.get()](AppId app_id, uint64_t promise_id, NativeResult && result) {
        impl->enqueue_async_completion(app_id, promise_id, std::move(result));
    };
    app.native_modules = impl_->native_modules_;
    app.runtime = JS_NewRuntime();
    if (app.runtime == nullptr) {
        BROOKESIA_LOGE("JavaScript app load failed: create runtime failed, id(%1%)", id);
        return std::unexpected("Failed to create JavaScript runtime");
    }
    js_std_init_handlers(app.runtime);
    JS_SetMemoryLimit(app.runtime, JS_MEMORY_LIMIT);
    JS_SetMaxStackSize(app.runtime, JS_MAX_STACK_SIZE);

    app.context = JS_NewContext(app.runtime);
    if (app.context == nullptr) {
        js_std_free_handlers(app.runtime);
        JS_FreeRuntime(app.runtime);
        BROOKESIA_LOGE("JavaScript app load failed: create context failed, id(%1%)", id);
        return std::unexpected("Failed to create JavaScript context");
    }
    js_std_add_helpers(app.context, 0, nullptr);
    /* quickjs-ng: js_module_loader matches JSModuleLoaderFunc2 (import attributes). */
    JS_SetModuleLoaderFunc2(app.runtime, nullptr, js_module_loader, js_module_check_attributes, nullptr);
    app.app_state = AppState::Loaded;
    auto [it, inserted] = impl_->apps_.emplace(id, std::move(app));
    if (!inserted) {
        BROOKESIA_LOGW("JavaScript app load failed: id(%1%) already exists after emplace", id);
        return std::unexpected("JavaScript app id already exists");
    }
    JS_SetContextOpaque(it->second.context, &it->second);
    register_native_modules(it->second.context, it->second, it->second.native_modules);
    BROOKESIA_LOGD("JavaScript app loaded: id(%1%), path(%2%)", id, config.app_path);
    return {};
}

std::expected<void, std::string> Backend::unload_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    std::lock_guard lock(impl_->mutex_);
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("JavaScript app unload failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }
    impl_->reject_all_pending_promises(it->second, "JavaScript app is unloading");
    JSContext *ctx = it->second.context;
    JSRuntime *rt = JS_GetRuntime(ctx);
    JS_FreeContext(ctx);
    js_std_free_handlers(rt);
    JS_FreeRuntime(rt);
    impl_->apps_.erase(it);
    BROOKESIA_LOGD("JavaScript app unloaded: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::start_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    std::lock_guard lock(impl_->mutex_);
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("JavaScript app start failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }

    const std::string entry_path = detail::make_entry_path(it->second.config);
    BROOKESIA_LOGD("JavaScript app starting: id(%1%), entry(%2%)", id, entry_path);
    JS_UpdateStackTop(it->second.runtime);
    const auto code = read_all(entry_path);
    if (!code) {
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("JavaScript app start failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, code.error());
        return std::unexpected(code.error());
    }

    const char *ep = code.value().data();
    size_t elen = code.value().size();
    if (elen >= 3 && static_cast<unsigned char>(ep[0]) == 0xEFU && static_cast<unsigned char>(ep[1]) == 0xBBU &&
            static_cast<unsigned char>(ep[2]) == 0xBFU) {
        ep += 3;
        elen -= 3;
    }

    const int eval_type =
        qjs_source_looks_like_es_module(reinterpret_cast<const uint8_t *>(ep), elen) ? JS_EVAL_TYPE_MODULE :
        JS_EVAL_TYPE_GLOBAL;
    JSValue result =
        JS_Eval(it->second.context, ep, elen, entry_path.c_str(), eval_type);
    if (JS_IsException(result)) {
        std::string error = get_exception_string(it->second.context);
        JS_FreeValue(it->second.context, result);
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("JavaScript app start failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, error);
        return std::unexpected(error);
    }
    JS_FreeValue(it->second.context, result);
    drain_microtasks(it->second.context, id);
    ensure_js_lifecycle_module(it->second.context, id);
    it->second.app_state = AppState::Running;
    BROOKESIA_LOGD("JavaScript app started: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::stop_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    std::lock_guard lock(impl_->mutex_);
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("JavaScript app stop failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }
    impl_->reject_all_pending_promises(it->second, "JavaScript app is stopping");
    it->second.app_state = AppState::Stopped;
    BROOKESIA_LOGD("JavaScript app stopped: id(%1%)", id);
    return {};
}

std::expected<NativeValue, std::string> Backend::call_function(
    AppId id, std::string_view module_name, std::string_view function_name, const NativeArgs &args
)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    std::lock_guard lock(impl_->mutex_);
    BROOKESIA_LOGD(
        "Params: app_id(%1%), module(%2%), function(%3%), arg_count(%4%)",
        id, module_name, function_name, args.size()
    );
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("JavaScript function call failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }

    flush_app_async_completions(it->second);

    JSContext *context = it->second.context;
    JS_UpdateStackTop(it->second.runtime);
    JSValue global = JS_GetGlobalObject(context);
    JSValue module = JS_GetPropertyStr(context, global, std::string(module_name).c_str());
    JS_FreeValue(context, global);
    if (!JS_IsObject(module)) {
        JS_FreeValue(context, module);
        BROOKESIA_LOGW("JavaScript function call failed: id(%1%), module(%2%) not found", id, module_name);
        return std::unexpected("JavaScript module not found");
    }
    JSValue function = JS_GetPropertyStr(context, module, std::string(function_name).c_str());
    if (!JS_IsFunction(context, function)) {
        JS_FreeValue(context, function);
        JS_FreeValue(context, module);
        BROOKESIA_LOGW(
            "JavaScript function call failed: id(%1%), module(%2%), function(%3%) not found",
            id, module_name, function_name
        );
        return std::unexpected("JavaScript function not found");
    }

    std::vector<JSValue> js_args;
    js_args.reserve(args.size());
    for (const auto &arg : args) {
        js_args.push_back(native_to_js(context, arg));
    }
    JSValue result = JS_Call(context, function, module, static_cast<int>(js_args.size()), js_args.data());
    for (auto &arg : js_args) {
        JS_FreeValue(context, arg);
    }
    JS_FreeValue(context, function);
    JS_FreeValue(context, module);

    if (JS_IsException(result)) {
        std::string error = get_exception_string(context);
        JS_FreeValue(context, result);
        BROOKESIA_LOGE(
            "JavaScript function call failed: id(%1%), module(%2%), function(%3%), error(%4%)",
            id, module_name, function_name, error
        );
        return std::unexpected(error);
    }
    drain_microtasks(context, id);
    NativeValue native_result = js_to_native(context, result);
    JS_FreeValue(context, result);
    return native_result;
}

AppState Backend::get_app_state(AppId id) const
{
    std::lock_guard lock(impl_->mutex_);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        return AppState::Unloaded;
    }
    return it->second.app_state;
}

#else

class Backend::Impl {
public:
    std::vector<NativeModule> native_modules_;
    std::map<AppId, AppConfig> apps_;
    bool initialized_ = false;
};

Backend::Backend()
    : IRuntimeBackend(BackendType::JavaScript,
{
    .name = BROOKESIA_RUNTIME_JS_BACKEND_NAME,
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
        "Version: %1%.%2%.%3%", BROOKESIA_RUNTIME_JS_VER_MAJOR, BROOKESIA_RUNTIME_JS_VER_MINOR,
        BROOKESIA_RUNTIME_JS_VER_PATCH
    );
    BROOKESIA_LOGD("Params: initialized(%1%)", impl_->initialized_);
    impl_->initialized_ = true;
    BROOKESIA_LOGD("JavaScript fallback backend initialized");
    return {};
}

void Backend::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_count(%1%), initialized(%2%)", impl_->apps_.size(), impl_->initialized_);
    impl_->apps_.clear();
    impl_->initialized_ = false;
    BROOKESIA_LOGD("JavaScript fallback backend deinitialized");
}

std::expected<void, std::string> Backend::load_app(AppId id, const AppConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: app_id(%1%), config(%2%), module_count(%3%)",
        id, config, impl_->native_modules_.size()
    );
    if (!impl_->initialized_) {
        BROOKESIA_LOGW("JavaScript fallback app load failed: backend is not initialized, id(%1%)", id);
        return std::unexpected("JavaScript backend is not initialized");
    }
    impl_->apps_.emplace(id, config);
    BROOKESIA_LOGD("JavaScript fallback app loaded: id(%1%), path(%2%)", id, config.app_path);
    return {};
}

std::expected<void, std::string> Backend::unload_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    impl_->apps_.erase(id);
    BROOKESIA_LOGD("JavaScript fallback app unloaded: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::start_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    if (!impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("JavaScript fallback app start failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }
    BROOKESIA_LOGW("JavaScript fallback app start failed: id(%1%), QuickJS not configured", id);
    return std::unexpected("QuickJS is not configured for this build");
}

std::expected<void, std::string> Backend::stop_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    if (!impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("JavaScript fallback app stop failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }
    BROOKESIA_LOGD("JavaScript fallback app stopped: id(%1%)", id);
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
    if (!impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("JavaScript fallback function call failed: id(%1%) not found", id);
        return std::unexpected("JavaScript app not found");
    }
    for (const auto &module : impl_->native_modules_) {
        if (module.name != module_name) {
            continue;
        }
        for (const auto &function : module.functions) {
            if (function.name == function_name) {
                if (!function.function) {
                    return std::unexpected("Asynchronous native function is not supported without QuickJS");
                }
                return function.function(args);
            }
        }
    }
    BROOKESIA_LOGW(
        "JavaScript fallback function call failed: id(%1%), module(%2%), function(%3%) not found",
        id, module_name, function_name
    );
    return std::unexpected("JavaScript host function not found");
}

AppState Backend::get_app_state(AppId id) const
{
    return impl_->apps_.contains(id) ? AppState::Loaded : AppState::Unloaded;
}

#endif

#if BROOKESIA_RUNTIME_JS_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    IRuntimeBackend, Backend, Backend::get_instance().get_attributes().name, Backend::get_instance(),
    BROOKESIA_RUNTIME_JS_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::runtime::js
