/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include "brookesia/runtime_wasm/backend.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/runtime_wasm/macro_configs.h"
#if !BROOKESIA_RUNTIME_WASM_BACKEND_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/host_utils.hpp"
#include "private/import_descriptor.hpp"
#include "private/native_call_utils.hpp"
#include "private/utils.hpp"

#if (BROOKESIA_UTILS_LOG_LEVEL <= BROOKESIA_UTILS_LOG_LEVEL_DEBUG) && \
    BROOKESIA_RUNTIME_WASM_HOST_IMPORT_ENABLE_DEBUG_LOG
#   define BROOKESIA_RUNTIME_WASM_HOST_LOGD(format, ...) \
        BROOKESIA_LOGD_IMPL(esp_brookesia::lib_utils::TAG, format, ##__VA_ARGS__)
#else
#   define BROOKESIA_RUNTIME_WASM_HOST_LOGD(format, ...) ((void)0)
#endif

#if defined(BROOKESIA_RUNTIME_WASM_HAS_WASMTIME_C_API)
extern "C" {
#include "wasm.h"
#include "wasmtime.h"
}
#endif

#if defined(BROOKESIA_RUNTIME_WASM_HAS_WAMR)
#include "esp_heap_caps.h"
extern "C" {
#include "wasm_export.h"
#include "wasm_native.h"
}
#endif

namespace esp_brookesia::runtime::wasm {
namespace {

#if defined(BROOKESIA_RUNTIME_WASM_HAS_WASMTIME_C_API)
std::string get_wasmtime_error_message(wasmtime_error_t *error)
{
    wasm_byte_vec_t message;
    wasmtime_error_message(error, &message);
    std::string result(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasmtime_error_delete(error);
    return result;
}

std::string get_wasm_trap_message(wasm_trap_t *trap)
{
    wasm_byte_vec_t message;
    wasm_trap_message(trap, &message);
    std::string result(message.data, message.size);
    wasm_byte_vec_delete(&message);
    wasm_trap_delete(trap);
    return result;
}

std::expected<wasmtime_memory_t, std::string> get_caller_memory(wasmtime_caller_t *caller)
{
    wasmtime_extern_t item;
    if (!wasmtime_caller_export_get(caller, "memory", strlen("memory"), &item) ||
        (item.kind != WASMTIME_EXTERN_MEMORY)) {
        return std::unexpected("WASM memory export is not available");
    }
    return item.of.memory;
}

std::string read_caller_string(wasmtime_caller_t *caller, int32_t ptr, int32_t len)
{
    if ((ptr < 0) || (len < 0)) {
        return {};
    }
    auto memory = get_caller_memory(caller);
    if (!memory) {
        return {};
    }

    auto *context = wasmtime_caller_context(caller);
    auto *data = wasmtime_memory_data(context, &memory.value());
    const size_t data_size = wasmtime_memory_data_size(context, &memory.value());
    const size_t offset = static_cast<size_t>(ptr);
    const size_t size = static_cast<size_t>(len);
    if ((offset > data_size) || (size > (data_size - offset))) {
        return {};
    }
    return std::string(reinterpret_cast<const char *>(data + offset), size);
}

int32_t write_caller_bytes(wasmtime_caller_t *caller, int32_t ptr, int32_t len, std::string_view value)
{
    if ((ptr < 0) || (len <= 0)) {
        return 0;
    }
    auto memory = get_caller_memory(caller);
    if (!memory) {
        return 0;
    }

    auto *context = wasmtime_caller_context(caller);
    auto *data = wasmtime_memory_data(context, &memory.value());
    const size_t data_size = wasmtime_memory_data_size(context, &memory.value());
    const size_t offset = static_cast<size_t>(ptr);
    const size_t capacity = static_cast<size_t>(len);
    if ((offset > data_size) || (capacity > (data_size - offset))) {
        return 0;
    }

    const size_t copy_size = std::min(capacity, value.size());
    std::memcpy(data + offset, value.data(), copy_size);
    return static_cast<int32_t>(copy_size);
}

wasm_functype_t *make_functype(std::vector<wasm_valkind_t> params, std::vector<wasm_valkind_t> results)
{
    wasm_valtype_vec_t param_vec;
    wasm_valtype_vec_new_uninitialized(&param_vec, params.size());
    for (size_t i = 0; i < params.size(); ++i) {
        param_vec.data[i] = wasm_valtype_new(params[i]);
    }

    wasm_valtype_vec_t result_vec;
    wasm_valtype_vec_new_uninitialized(&result_vec, results.size());
    for (size_t i = 0; i < results.size(); ++i) {
        result_vec.data[i] = wasm_valtype_new(results[i]);
    }

    return wasm_functype_new(&param_vec, &result_vec);
}

struct WasmtimeImportBinding {
    HostContext *host_context = nullptr;
    const NativeImportDescriptor *descriptor = nullptr;
};

wasm_trap_t *dynamic_import_callback(
    void *env, wasmtime_caller_t *caller, const wasmtime_val_t *args, size_t nargs, wasmtime_val_t *results,
    size_t nresults
)
{
    if ((env == nullptr) || (nargs < 2) || (nresults < 1)) {
        return nullptr;
    }

    auto &binding = *static_cast<WasmtimeImportBinding *>(env);
    if ((binding.host_context == nullptr) || (binding.descriptor == nullptr)) {
        return nullptr;
    }

    auto &host_context = *binding.host_context;
    const std::string args_json = read_caller_string(caller, args[0].of.i32, args[1].of.i32);
    const std::string result_json = call_registered_native_function(
                                        *host_context.modules, binding.descriptor->module_name,
                                        binding.descriptor->function_name, args_json
                                    );
    results[0].kind = WASMTIME_I32;
    results[0].of.i32 = store_result(host_context, result_json);
    return nullptr;
}

wasm_trap_t *result_len_callback(
    void *env, wasmtime_caller_t *caller, const wasmtime_val_t *args, size_t nargs, wasmtime_val_t *results,
    size_t nresults
)
{
    (void)caller;
    if ((env == nullptr) || (nargs < 1) || (nresults < 1)) {
        return nullptr;
    }

    auto &host_context = *static_cast<HostContext *>(env);
    results[0].kind = WASMTIME_I32;
    auto *stored_result = host_context.results.find(args[0].of.i32);
    results[0].of.i32 = stored_result != nullptr ? static_cast<int32_t>(stored_result->size()) : 0;
    return nullptr;
}

wasm_trap_t *result_copy_callback(
    void *env, wasmtime_caller_t *caller, const wasmtime_val_t *args, size_t nargs, wasmtime_val_t *results,
    size_t nresults
)
{
    if ((env == nullptr) || (nargs < 3) || (nresults < 1)) {
        return nullptr;
    }

    auto &host_context = *static_cast<HostContext *>(env);
    auto *stored_result = host_context.results.find(args[0].of.i32);
    const int32_t copied = stored_result != nullptr ?
                           write_caller_bytes(caller, args[1].of.i32, args[2].of.i32, *stored_result) : 0;
    results[0].kind = WASMTIME_I32;
    results[0].of.i32 = copied;
    return nullptr;
}

wasm_trap_t *result_free_callback(
    void *env, wasmtime_caller_t *caller, const wasmtime_val_t *args, size_t nargs, wasmtime_val_t *results,
    size_t nresults
)
{
    (void)caller;
    (void)results;
    (void)nresults;
    if ((env != nullptr) && (nargs >= 1)) {
        static_cast<HostContext *>(env)->results.erase(args[0].of.i32);
    }
    return nullptr;
}

std::expected<void, std::string> define_import(
    wasmtime_linker_t *linker, const char *name, wasm_functype_t *type, wasmtime_func_callback_t callback, void *env
)
{
    BROOKESIA_RUNTIME_WASM_HOST_LOGD("Define host import: name(%1%)", name);
    wasmtime_error_t *error = wasmtime_linker_define_func(
        linker, "env", strlen("env"), name, strlen(name), type, callback, env, nullptr
    );
    wasm_functype_delete(type);
    if (error != nullptr) {
        return std::unexpected("Failed to define env." + std::string(name) + ": " + get_wasmtime_error_message(error));
    }
    return {};
}

std::expected<void, std::string> define_result_helper_imports(wasmtime_linker_t *linker, HostContext &host_context)
{
    BROOKESIA_RUNTIME_WASM_HOST_LOGD(
        "Define result helper imports: linker(%1%), host_context(%2%)", linker, &host_context
    );
    struct ImportDefinition {
        const char *name;
        std::vector<wasm_valkind_t> params;
        std::vector<wasm_valkind_t> results;
        wasmtime_func_callback_t callback;
    };

    const std::vector<ImportDefinition> imports = {
        {"brookesia_result_len", {WASM_I32}, {WASM_I32}, result_len_callback},
        {"brookesia_result_copy", {WASM_I32, WASM_I32, WASM_I32}, {WASM_I32}, result_copy_callback},
        {"brookesia_result_free", {WASM_I32}, {}, result_free_callback},
    };

    for (const auto &import : imports) {
        auto result = define_import(
            linker, import.name, make_functype(import.params, import.results), import.callback, &host_context
        );
        if (!result) {
            return result;
        }
    }
    BROOKESIA_LOGD("WASM result helper imports defined: count(%1%)", imports.size());
    return {};
}

std::expected<void, std::string> define_native_imports(
    wasmtime_linker_t *linker, HostContext &host_context, const NativeImportSet &import_set,
    std::vector<WasmtimeImportBinding> &bindings
)
{
    BROOKESIA_RUNTIME_WASM_HOST_LOGD(
        "Define native imports: linker(%1%), host_context(%2%), import_count(%3%)",
        linker, &host_context, import_set.descriptors.size()
    );
    bindings.reserve(import_set.descriptors.size());
    for (const auto &descriptor : import_set.descriptors) {
        bindings.push_back({
            .host_context = &host_context,
            .descriptor = &descriptor,
        });
        auto result = define_import(
            linker, descriptor.symbol_name.c_str(), make_functype({WASM_I32, WASM_I32}, {WASM_I32}),
            dynamic_import_callback, &bindings.back()
        );
        if (!result) {
            return result;
        }
    }
    auto helper_result = define_result_helper_imports(linker, host_context);
    if (!helper_result) {
        return helper_result;
    }
    return {};
}

std::expected<void, std::string> run_wasm_app(
    const std::string &entry_path, const std::vector<NativeModule> &native_modules, const NativeImportSet &import_set
)
{
    BROOKESIA_LOG_TRACE_GUARD();
    BROOKESIA_LOGD(
        "Params: entry(%1%), module_count(%2%), import_count(%3%)",
        entry_path, native_modules.size(), import_set.descriptors.size()
    );
    BROOKESIA_LOGD("WASM app running: entry(%1%)", entry_path);
    auto wasm_bytes = read_wasm_file(entry_path);
    if (!wasm_bytes) {
        BROOKESIA_LOGE("WASM app read failed: entry(%1%), error(%2%)", entry_path, wasm_bytes.error());
        return std::unexpected(wasm_bytes.error());
    }

    wasm_engine_t *engine = wasm_engine_new();
    if (engine == nullptr) {
        BROOKESIA_LOGE("WASM app run failed: create Wasmtime engine failed, entry(%1%)", entry_path);
        return std::unexpected("Failed to create Wasmtime engine");
    }

    wasmtime_store_t *store = wasmtime_store_new(engine, nullptr, nullptr);
    if (store == nullptr) {
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: create Wasmtime store failed, entry(%1%)", entry_path);
        return std::unexpected("Failed to create Wasmtime store");
    }
    wasmtime_context_t *context = wasmtime_store_context(store);

    wasi_config_t *wasi = wasi_config_new();
    if (wasi == nullptr) {
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: create WASI config failed, entry(%1%)", entry_path);
        return std::unexpected("Failed to create WASI config");
    }
    wasi_config_inherit_stdout(wasi);
    wasi_config_inherit_stderr(wasi);
    const char *guest_argv[] = {entry_path.c_str()};
    if (!wasi_config_set_argv(wasi, 1, guest_argv)) {
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: set WASI argv failed, entry(%1%)", entry_path);
        return std::unexpected("Failed to set WASI argv");
    }

    wasmtime_error_t *error = wasmtime_context_set_wasi(context, wasi);
    if (error != nullptr) {
        const std::string message = get_wasmtime_error_message(error);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: configure WASI failed, entry(%1%), error(%2%)", entry_path, message);
        return std::unexpected("Failed to configure WASI: " + message);
    }

    wasmtime_module_t *module = nullptr;
    error = wasmtime_module_new(engine, wasm_bytes.value().data(), wasm_bytes.value().size(), &module);
    if (error != nullptr) {
        const std::string message = get_wasmtime_error_message(error);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: compile module failed, entry(%1%), error(%2%)", entry_path, message);
        return std::unexpected("Failed to compile WASM module: " + message);
    }

    wasmtime_linker_t *linker = wasmtime_linker_new(engine);
    if (linker == nullptr) {
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: create Wasmtime linker failed, entry(%1%)", entry_path);
        return std::unexpected("Failed to create Wasmtime linker");
    }

    error = wasmtime_linker_define_wasi(linker);
    if (error != nullptr) {
        const std::string message = get_wasmtime_error_message(error);
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: define WASI imports failed, entry(%1%), error(%2%)", entry_path, message);
        return std::unexpected("Failed to define WASI imports: " + message);
    }

    HostContext host_context;
    host_context.modules = &native_modules;
    std::vector<WasmtimeImportBinding> bindings;
    auto define_result = define_native_imports(linker, host_context, import_set, bindings);
    if (!define_result) {
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: define host imports failed, entry(%1%), error(%2%)", entry_path, define_result.error());
        return define_result;
    }

    wasm_trap_t *trap = nullptr;
    wasmtime_instance_t instance;
    error = wasmtime_linker_instantiate(linker, context, module, &instance, &trap);
    if ((error != nullptr) || (trap != nullptr)) {
        std::string message = error != nullptr ? get_wasmtime_error_message(error) : get_wasm_trap_message(trap);
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: instantiate failed, entry(%1%), error(%2%)", entry_path, message);
        return std::unexpected("Failed to instantiate WASM module: " + message);
    }

    wasmtime_extern_t start;
    bool ok = wasmtime_instance_export_get(context, &instance, "_start", strlen("_start"), &start);
    if (!ok || (start.kind != WASMTIME_EXTERN_FUNC)) {
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: _start export missing, entry(%1%)", entry_path);
        return std::unexpected("Failed to find WASM _start export");
    }

    error = wasmtime_func_call(context, &start.of.func, nullptr, 0, nullptr, 0, &trap);
    if ((error != nullptr) || (trap != nullptr)) {
        std::string message = error != nullptr ? get_wasmtime_error_message(error) : get_wasm_trap_message(trap);
        wasmtime_linker_delete(linker);
        wasmtime_module_delete(module);
        wasmtime_store_delete(store);
        wasm_engine_delete(engine);
        BROOKESIA_LOGE("WASM app run failed: _start failed, entry(%1%), error(%2%)", entry_path, message);
        return std::unexpected("Failed to call WASM _start: " + message);
    }

    wasmtime_linker_delete(linker);
    wasmtime_module_delete(module);
    wasmtime_store_delete(store);
    wasm_engine_delete(engine);
    return {};
}
#endif

#if defined(BROOKESIA_RUNTIME_WASM_HAS_WAMR)
thread_local HostContext *current_wamr_host_context = nullptr;

struct WamrRegisteredImports {
    std::vector<NativeImportDescriptor> descriptors;
    std::vector<std::string> names;
    std::vector<std::string> signatures;
    std::vector<NativeSymbol> symbols;
};

int32_t write_wamr_bytes(wasm_exec_env_t exec_env, int32_t ptr, int32_t len, std::string_view value)
{
    if ((ptr < 0) || (len <= 0)) {
        return 0;
    }
    auto module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!wasm_runtime_validate_app_addr(module_inst, static_cast<uint32_t>(ptr), static_cast<uint32_t>(len))) {
        return 0;
    }
    auto *data = static_cast<char *>(wasm_runtime_addr_app_to_native(module_inst, static_cast<uint32_t>(ptr)));
    if (data == nullptr) {
        return 0;
    }
    const size_t copy_size = std::min(static_cast<size_t>(len), value.size());
    std::memcpy(data, value.data(), copy_size);
    return static_cast<int32_t>(copy_size);
}

std::string read_wamr_string(wasm_exec_env_t exec_env, int32_t ptr, int32_t len)
{
    if ((ptr < 0) || (len <= 0)) {
        return {};
    }
    auto module_inst = wasm_runtime_get_module_inst(exec_env);
    if (!wasm_runtime_validate_app_addr(module_inst, static_cast<uint32_t>(ptr), static_cast<uint32_t>(len))) {
        return {};
    }
    auto *data = static_cast<const char *>(wasm_runtime_addr_app_to_native(module_inst, static_cast<uint32_t>(ptr)));
    if (data == nullptr) {
        return {};
    }
    return std::string(data, static_cast<size_t>(len));
}

HostContext *get_wamr_host_context()
{
    return current_wamr_host_context;
}

void *wamr_malloc(unsigned int size)
{
#ifdef CONFIG_SPIRAM
    constexpr uint32_t caps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
#else
    constexpr uint32_t caps = MALLOC_CAP_8BIT;
#endif
    return heap_caps_aligned_alloc(8, size, caps);
}

void *wamr_realloc(void *ptr, unsigned int size)
{
    void *new_ptr = wamr_malloc(size);
    if ((new_ptr != nullptr) && (ptr != nullptr)) {
        const size_t old_size = heap_caps_get_allocated_size(ptr);
        std::memcpy(new_ptr, ptr, std::min<size_t>(size, old_size));
        heap_caps_free(ptr);
    }
    return new_ptr;
}

void wamr_free(void *ptr)
{
    heap_caps_free(ptr);
}

const NativeImportDescriptor *get_wamr_import_descriptor(wasm_exec_env_t exec_env)
{
    return static_cast<const NativeImportDescriptor *>(wasm_runtime_get_function_attachment(exec_env));
}

int32_t wamr_dynamic_import(wasm_exec_env_t exec_env, int32_t args_json_ptr, int32_t args_json_len)
{
    auto *host_context = get_wamr_host_context();
    auto *descriptor = get_wamr_import_descriptor(exec_env);
    if ((host_context == nullptr) || (descriptor == nullptr)) {
        return 0;
    }

    const std::string args_json = read_wamr_string(exec_env, args_json_ptr, args_json_len);
    const std::string_view args_json_view = !args_json.empty() ? std::string_view(args_json) : std::string_view("[]", 2);
    const std::string result_json = call_registered_native_function(
                                        *host_context->modules, descriptor->module_name, descriptor->function_name,
                                        args_json_view
                                    );
    return store_result(*host_context, result_json);
}

int32_t wamr_result_len(wasm_exec_env_t exec_env, int32_t result_id)
{
    (void)exec_env;
    auto *host_context = get_wamr_host_context();
    if (host_context == nullptr) {
        return 0;
    }
    auto *stored_result = host_context->results.find(result_id);
    return stored_result != nullptr ? static_cast<int32_t>(stored_result->size()) : 0;
}

int32_t wamr_result_copy(wasm_exec_env_t exec_env, int32_t result_id, int32_t out_ptr, int32_t out_len)
{
    auto *host_context = get_wamr_host_context();
    if (host_context == nullptr) {
        return 0;
    }
    auto *stored_result = host_context->results.find(result_id);
    return stored_result != nullptr ? write_wamr_bytes(exec_env, out_ptr, out_len, *stored_result) : 0;
}

void wamr_result_free(wasm_exec_env_t exec_env, int32_t result_id)
{
    (void)exec_env;
    auto *host_context = get_wamr_host_context();
    if (host_context != nullptr) {
        host_context->results.erase(result_id);
    }
}

std::expected<void, std::string> unregister_wamr_brookesia_imports(WamrRegisteredImports &registered_imports)
{
    if (registered_imports.symbols.empty()) {
        return {};
    }

    if (!wasm_runtime_unregister_natives("env", registered_imports.symbols.data())) {
        return std::unexpected("Failed to unregister WAMR brookesia host imports");
    }
    registered_imports.descriptors.clear();
    registered_imports.names.clear();
    registered_imports.signatures.clear();
    registered_imports.symbols.clear();
    BROOKESIA_LOGD("WAMR host imports unregistered");
    return {};
}

std::expected<void, std::string> register_wamr_brookesia_imports(
    WamrRegisteredImports &registered_imports, const NativeImportSet &import_set
)
{
    BROOKESIA_RUNTIME_WASM_HOST_LOGD(
        "Register WAMR host imports: import_count(%1%)", import_set.descriptors.size()
    );
    auto unregister_result = unregister_wamr_brookesia_imports(registered_imports);
    if (!unregister_result) {
        return unregister_result;
    }

    constexpr std::array<std::string_view, 3> helper_signatures = {
        "(i)i",
        "(iii)i",
        "(i)",
    };
    const std::array<void *, 3> helper_callbacks = {
        reinterpret_cast<void *>(wamr_result_len),
        reinterpret_cast<void *>(wamr_result_copy),
        reinterpret_cast<void *>(wamr_result_free),
    };

    registered_imports.descriptors = import_set.descriptors;
    const size_t total_symbol_count = registered_imports.descriptors.size() + RESULT_HELPER_IMPORT_NAMES.size();
    registered_imports.names.reserve(total_symbol_count);
    registered_imports.signatures.reserve(total_symbol_count);
    registered_imports.symbols.reserve(total_symbol_count);

    for (const auto &descriptor : registered_imports.descriptors) {
        registered_imports.names.push_back(descriptor.symbol_name);
        registered_imports.signatures.emplace_back("(ii)i");
    }
    for (size_t i = 0; i < RESULT_HELPER_IMPORT_NAMES.size(); ++i) {
        registered_imports.names.emplace_back(RESULT_HELPER_IMPORT_NAMES[i]);
        registered_imports.signatures.emplace_back(helper_signatures[i]);
    }

    for (size_t i = 0; i < registered_imports.descriptors.size(); ++i) {
        registered_imports.symbols.push_back({
            registered_imports.names[i].data(),
            reinterpret_cast<void *>(wamr_dynamic_import),
            registered_imports.signatures[i].data(),
            &registered_imports.descriptors[i],
        });
    }
    for (size_t i = 0; i < RESULT_HELPER_IMPORT_NAMES.size(); ++i) {
        const size_t index = registered_imports.descriptors.size() + i;
        registered_imports.symbols.push_back({
            registered_imports.names[index].data(),
            helper_callbacks[i],
            registered_imports.signatures[index].data(),
            nullptr,
        });
    }

    if (!wasm_runtime_register_natives("env", registered_imports.symbols.data(), registered_imports.symbols.size())) {
        registered_imports.descriptors.clear();
        registered_imports.names.clear();
        registered_imports.signatures.clear();
        registered_imports.symbols.clear();
        return std::unexpected("Failed to register WAMR brookesia host imports");
    }
    BROOKESIA_LOGD("WAMR host imports registered: count(%1%)", registered_imports.symbols.size());
    return {};
}

std::expected<void, std::string> run_wamr_app(
    const std::string &entry_path, const std::vector<NativeModule> &native_modules, const NativeImportSet &import_set,
    WamrRegisteredImports &registered_imports
)
{
    BROOKESIA_LOG_TRACE_GUARD();
    BROOKESIA_LOGD(
        "Params: entry(%1%), module_count(%2%), import_count(%3%)",
        entry_path, native_modules.size(), import_set.descriptors.size()
    );
    BROOKESIA_LOGD("WASM app running on WAMR: entry(%1%)", entry_path);
    auto wasm_bytes = read_wasm_file(entry_path);
    if (!wasm_bytes) {
        BROOKESIA_LOGE("WAMR app read failed: entry(%1%), error(%2%)", entry_path, wasm_bytes.error());
        return std::unexpected(wasm_bytes.error());
    }

    auto register_result = register_wamr_brookesia_imports(registered_imports, import_set);
    if (!register_result) {
        BROOKESIA_LOGE(
            "WAMR app run failed: register host imports failed, entry(%1%), error(%2%)",
            entry_path, register_result.error()
        );
        return register_result;
    }

    constexpr uint32_t wamr_stack_size = 64 * 1024;
    constexpr uint32_t wamr_heap_size = 256 * 1024;
    char error_buf[128] = {};
    wasm_module_t module = wasm_runtime_load(
        wasm_bytes.value().data(), static_cast<uint32_t>(wasm_bytes.value().size()), error_buf, sizeof(error_buf)
    );
    if (module == nullptr) {
        (void)unregister_wamr_brookesia_imports(registered_imports);
        BROOKESIA_LOGE("WAMR app run failed: load module failed, entry(%1%), error(%2%)", entry_path, error_buf);
        return std::unexpected(std::string("Failed to load WAMR module: ") + error_buf);
    }

    char *argv[] = {const_cast<char *>(entry_path.c_str())};
#if defined(CONFIG_WAMR_ENABLE_LIBC_WASI) && CONFIG_WAMR_ENABLE_LIBC_WASI != 0
    wasm_runtime_set_wasi_args(module, nullptr, 0, nullptr, 0, nullptr, 0, argv, 1);
#endif

    wasm_module_inst_t module_inst = wasm_runtime_instantiate(
        module, wamr_stack_size, wamr_heap_size, error_buf, sizeof(error_buf)
    );
    if (module_inst == nullptr) {
        (void)unregister_wamr_brookesia_imports(registered_imports);
        wasm_runtime_unload(module);
        BROOKESIA_LOGE("WAMR app run failed: instantiate failed, entry(%1%), error(%2%)", entry_path, error_buf);
        return std::unexpected(std::string("Failed to instantiate WAMR module: ") + error_buf);
    }

    HostContext host_context;
    host_context.modules = &native_modules;
    current_wamr_host_context = &host_context;
    const bool executed = wasm_application_execute_main(module_inst, 1, argv);
    current_wamr_host_context = nullptr;
    if (!executed) {
        const char *exception = wasm_runtime_get_exception(module_inst);
        std::string message = exception != nullptr ? exception : "WAMR main returned false";
        wasm_runtime_deinstantiate(module_inst);
        (void)unregister_wamr_brookesia_imports(registered_imports);
        wasm_runtime_unload(module);
        BROOKESIA_LOGE("WAMR app run failed: execute main failed, entry(%1%), error(%2%)", entry_path, message);
        return std::unexpected("Failed to execute WAMR module: " + message);
    }

    wasm_runtime_deinstantiate(module_inst);
    auto unregister_after_run_result = unregister_wamr_brookesia_imports(registered_imports);
    wasm_runtime_unload(module);
    if (!unregister_after_run_result) {
        BROOKESIA_LOGE(
            "WAMR app run failed: unregister host imports failed, entry(%1%), error(%2%)",
            entry_path, unregister_after_run_result.error()
        );
        return unregister_after_run_result;
    }
    return {};
}
#endif

} // namespace

class Backend::Impl {
public:
    struct AppRecord {
        AppConfig config;
        AppState app_state = AppState::Unloaded;
    };

    std::vector<NativeModule> native_modules_;
    std::map<AppId, AppRecord> apps_;
#if defined(BROOKESIA_RUNTIME_WASM_HAS_WAMR)
    WamrRegisteredImports wamr_registered_imports_;
#endif
    bool initialized_ = false;
};

Backend::Backend()
    : IRuntimeBackend(BackendType::Wasm, {
        .name = BROOKESIA_RUNTIME_WASM_BACKEND_NAME,
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
        "Version: %1%.%2%.%3%", BROOKESIA_RUNTIME_WASM_VER_MAJOR, BROOKESIA_RUNTIME_WASM_VER_MINOR,
        BROOKESIA_RUNTIME_WASM_VER_PATCH
    );
    BROOKESIA_LOGD("Params: initialized(%1%)", impl_->initialized_);
#if defined(BROOKESIA_RUNTIME_WASM_HAS_WAMR)
    RuntimeInitArgs init_args;
    memset(&init_args, 0, sizeof(RuntimeInitArgs));
    init_args.mem_alloc_type = Alloc_With_Allocator;
    init_args.mem_alloc_option.allocator.malloc_func = reinterpret_cast<void *>(
        static_cast<void *(*)(unsigned int)>(wamr_malloc)
    );
    init_args.mem_alloc_option.allocator.realloc_func = reinterpret_cast<void *>(
        static_cast<void *(*)(void *, unsigned int)>(wamr_realloc)
    );
    init_args.mem_alloc_option.allocator.free_func = reinterpret_cast<void *>(
        static_cast<void (*)(void *)>(wamr_free)
    );
    auto full_init_result = wasm_runtime_full_init(&init_args);
    if (!full_init_result) {
        BROOKESIA_LOGE("WASM backend init failed: full init failed");
        return std::unexpected("WASM backend full init failed");
    }

#endif
    impl_->initialized_ = true;
    BROOKESIA_LOGD("WASM backend initialized");
    return {};
}

void Backend::deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_count(%1%), initialized(%2%)", impl_->apps_.size(), impl_->initialized_);
#if defined(BROOKESIA_RUNTIME_WASM_HAS_WAMR)
    (void)unregister_wamr_brookesia_imports(impl_->wamr_registered_imports_);
#endif
    impl_->apps_.clear();
    impl_->initialized_ = false;
    BROOKESIA_LOGD("WASM backend deinitialized");
}

std::expected<void, std::string> Backend::load_app(AppId id, const AppConfig &config)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD(
        "Params: app_id(%1%), config(%2%), module_count(%3%)",
        id, config, impl_->native_modules_.size()
    );
    if (!impl_->initialized_) {
        BROOKESIA_LOGW("WASM app load failed: backend is not initialized, id(%1%)", id);
        return std::unexpected("WASM backend is not initialized");
    }
    if (impl_->apps_.contains(id)) {
        BROOKESIA_LOGW("WASM app load failed: id(%1%) already exists", id);
        return std::unexpected("WASM app id already exists");
    }

    Impl::AppRecord record;
    record.config = config;
    record.app_state = AppState::Loaded;
    impl_->apps_.emplace(id, std::move(record));
    BROOKESIA_LOGD("WASM app loaded: id(%1%), path(%2%)", id, config.app_path);
    return {};
}

std::expected<void, std::string> Backend::unload_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("WASM app unload failed: id(%1%) not found", id);
        return std::unexpected("WASM app not found");
    }
    impl_->apps_.erase(it);
    BROOKESIA_LOGD("WASM app unloaded: id(%1%)", id);
    return {};
}

std::expected<void, std::string> Backend::start_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("WASM app start failed: id(%1%) not found", id);
        return std::unexpected("WASM app not found");
    }
    auto import_set = build_native_import_set(impl_->native_modules_);
    if (!import_set) {
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("WASM app start failed: build imports failed, id(%1%), error(%2%)", id, import_set.error());
        return std::unexpected(import_set.error());
    }

#if defined(BROOKESIA_RUNTIME_WASM_HAS_WAMR)
    const std::string entry_path = detail::make_entry_path(it->second.config);
    BROOKESIA_LOGD("WASM app starting: id(%1%), entry(%2%)", id, entry_path);
    auto result = run_wamr_app(entry_path, impl_->native_modules_, import_set.value(), impl_->wamr_registered_imports_);
    if (!result) {
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("WASM app start failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, result.error());
        return result;
    }
    it->second.app_state = AppState::Running;
    BROOKESIA_LOGD("WASM app started: id(%1%)", id);
    return {};
#elif defined(BROOKESIA_RUNTIME_WASM_HAS_WASMTIME_C_API)
    const std::string entry_path = detail::make_entry_path(it->second.config);
    BROOKESIA_LOGD("WASM app starting: id(%1%), entry(%2%)", id, entry_path);
    auto result = run_wasm_app(entry_path, impl_->native_modules_, import_set.value());
    if (!result) {
        it->second.app_state = AppState::Error;
        BROOKESIA_LOGE("WASM app start failed: id(%1%), entry(%2%), error(%3%)", id, entry_path, result.error());
        return result;
    }
    it->second.app_state = AppState::Running;
    BROOKESIA_LOGD("WASM app started: id(%1%)", id);
    return {};
#else
    (void)it;
    BROOKESIA_LOGE("WASM app start failed: no WASM backend engine configured, id(%1%)", id);
    return std::unexpected("Wasmtime C API is not configured for this build");
#endif
}

std::expected<void, std::string> Backend::stop_app(AppId id)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGD("Params: app_id(%1%)", id);
    auto it = impl_->apps_.find(id);
    if (it == impl_->apps_.end()) {
        BROOKESIA_LOGW("WASM app stop failed: id(%1%) not found", id);
        return std::unexpected("WASM app not found");
    }
    it->second.app_state = AppState::Stopped;
    BROOKESIA_LOGD("WASM app stopped: id(%1%)", id);
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
        BROOKESIA_LOGW("WASM function call failed: id(%1%) not found", id);
        return std::unexpected("WASM app not found");
    }
    auto result = detail::call_native_module(impl_->native_modules_, module_name, function_name, args);
    if (!result) {
        BROOKESIA_LOGW(
            "WASM function call failed: id(%1%), module(%2%), function(%3%), error(%4%)",
            id, module_name, function_name, result.error()
        );
    }
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

#if BROOKESIA_RUNTIME_WASM_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    IRuntimeBackend, Backend, Backend::get_instance().get_attributes().name, Backend::get_instance(),
    BROOKESIA_RUNTIME_WASM_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::runtime::wasm
