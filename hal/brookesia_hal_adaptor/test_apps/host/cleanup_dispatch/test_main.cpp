/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <mutex>
#include <new>

#ifdef TEST_STARTUP_REGISTRATION_FAILURE
#include <sys/resource.h>
#endif

#include "brookesia/hal_adaptor/board_manager.h"

namespace {

bool fail_allocations;
int lock_depth;
int manager_calls;
int retry_calls;
int error_queries;
int cleanup_error;
int second_error;
int manager_error;
int published_error;
int lookup_error;
bool live_handle;

static std::recursive_mutex &lifecycle_mutex();
static esp_err_t get_error();
static esp_err_t get_second_error();
static esp_err_t retry();
static esp_err_t retry_other();
static void register_devices();
static void run_case(const char *name);

} // namespace

void *operator new (std::size_t size)
{
    if (fail_allocations) {
        throw std::bad_alloc();
    }
    if (auto *memory = std::malloc(size ? size : 1)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete (void *memory) noexcept
{
    std::free(memory);
}

void operator delete (void *memory, std::size_t) noexcept
{
    std::free(memory);
}

extern "C" void brookesia_hal_lifecycle_lock()
{
    lifecycle_mutex().lock();
    ++lock_depth;
}

extern "C" void brookesia_hal_lifecycle_unlock()
{
    assert(lock_depth > 0);
    --lock_depth;
    lifecycle_mutex().unlock();
}

extern "C" esp_err_t esp_board_manager_init()
{
    assert(lock_depth > 0);
    return manager_error;
}

extern "C" esp_err_t esp_board_manager_init_device_by_name(const char *)
{
    assert(lock_depth > 0);
    return manager_error;
}

extern "C" esp_err_t esp_board_manager_get_device_handle(const char *, void **handle)
{
    assert(lock_depth > 0);
    *handle = live_handle ? &manager_calls : nullptr;
    return lookup_error;
}

extern "C" esp_err_t esp_board_manager_deinit_device_by_name(const char *)
{
    assert(lock_depth > 0);
    ++manager_calls;
    cleanup_error = published_error;
    live_handle = false;
    return manager_error;
}

extern "C" esp_err_t esp_board_manager_deinit()
{
    assert(lock_depth > 0);
    ++manager_calls;
    return manager_error;
}

// Unrelated facade methods still link against ordinary Board Manager entry points.
extern "C" esp_err_t esp_board_manager_get_periph_handle(const char *, void **)
{
    return ESP_OK;
}
extern "C" bool esp_board_manager_check_name(const char *)
{
    return true;
}
extern "C" esp_err_t esp_board_manager_get_device_config(const char *, void **)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_manager_get_periph_config(const char *, void **)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_manager_get_board_info(esp_board_info_t *)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_periph_init(const char *)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_periph_get_handle(const char *, void **)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_periph_ref_handle(const char *, void **)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_periph_unref_handle(const char *)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_periph_get_config(const char *, void **)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_periph_deinit(const char *)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_device_get_handle(const char *, void **)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_device_get_i2c_effective_addr(const char *, uint16_t *)
{
    return ESP_OK;
}
extern "C" esp_err_t esp_board_device_show(const char *)
{
    return ESP_OK;
}

#ifdef TEST_MOSAICO_REGISTRATION
extern "C" esp_err_t fs_nand_cleanup_error()
{
    assert(lock_depth > 0);
    return 11;
}
extern "C" esp_err_t fs_nand_cleanup()
{
    assert(lock_depth > 0);
    return 111;
}
extern "C" esp_err_t bq27220_fuel_gauge_cleanup_error()
{
    assert(lock_depth > 0);
    return 22;
}
extern "C" esp_err_t bq27220_fuel_gauge_cleanup()
{
    assert(lock_depth > 0);
    return 222;
}
extern "C" esp_err_t esp_mosaico_expansion_cleanup_error()
{
    assert(lock_depth > 0);
    return 33;
}
extern "C" esp_err_t esp_mosaico_expansion_cleanup()
{
    assert(lock_depth > 0);
    return 333;
}
#endif

#ifdef TEST_STARTUP_REGISTRATION_FAILURE
extern "C" esp_err_t __real_brookesia_hal_board_manager_register_device_cleanup(
    const brookesia_hal_board_device_cleanup_t *cleanup);

extern "C" esp_err_t __wrap_brookesia_hal_board_manager_register_device_cleanup(
    const brookesia_hal_board_device_cleanup_t *cleanup)
{
    static int registrations;
    if (++registrations == 2) {
        // Inject a partial-registration failure without leaving an expected core dump.
        const rlimit limit = {0, 0};
        if (setrlimit(RLIMIT_CORE, &limit) != 0) {
            std::_Exit(EXIT_FAILURE);
        }
        return ESP_ERR_NO_MEM;
    }
    return __real_brookesia_hal_board_manager_register_device_cleanup(cleanup);
}
#endif

int main(int argc, char **argv)
{
    try {
        assert(argc == 2);
        if (std::strcmp(argv[1], "startup_failure") == 0) {
            // Reaching main means the registrar failed to stop startup. Do not
            // assert here: the runner must distinguish this from the required abort.
            return EXIT_FAILURE;
        }
        run_case(argv[1]);
        assert(lock_depth == 0);
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Unexpected exception: %s\n", error.what());
        return 1;
    } catch (...) {
        return 1;
    }
}

namespace {

static std::recursive_mutex &lifecycle_mutex()
{
    static std::recursive_mutex mutex;
    return mutex;
}

static esp_err_t get_error()
{
    assert(lock_depth > 0);
    ++error_queries;
    return cleanup_error;
}

static esp_err_t get_second_error()
{
    assert(lock_depth > 0);
    ++error_queries;
    return second_error;
}

static esp_err_t retry()
{
    assert(lock_depth > 0);
    ++retry_calls;
    cleanup_error = ESP_OK;
    return ESP_OK;
}

static esp_err_t retry_other()
{
    assert(lock_depth > 0);
    ++retry_calls;
    return cleanup_error; // Unknown release outcome cannot be recovered by retrying.
}

static void register_devices()
{
    const brookesia_hal_board_device_cleanup_t first = {"arbitrary_device", get_error, retry};
    const brookesia_hal_board_device_cleanup_t second = {"another_device", get_second_error, retry_other};
    assert(brookesia_hal_board_manager_register_device_cleanup(&first) == ESP_OK);
    assert(brookesia_hal_board_manager_register_device_cleanup(&second) == ESP_OK);
}

static void run_case(const char *name)
{
#ifdef TEST_MOSAICO_REGISTRATION
    if (std::strcmp(name, "mosaico_registration") == 0) {
        // Real registrar is discovered from an archive before main(), without a
        // source-level reference from the generic facade to any board symbol.
        assert(brookesia_hal_board_manager_init() == ESP_OK);
        fail_allocations = true;
        assert(brookesia_hal_board_manager_deinit_device_by_name("fs_nand") == 111);
        assert(brookesia_hal_board_manager_deinit_device_by_name("bq27220_fuel_gauge") == 222);
        assert(brookesia_hal_board_manager_deinit_device_by_name("expansion_runtime_pin") == 333);
        assert(brookesia_hal_board_manager_deinit_device_by_name("expansion_module_manager") == 333);
        assert(manager_calls == 0);
        assert(brookesia_hal_board_manager_deinit() == 11);
        return;
    }
#endif
    if (std::strcmp(name, "passthrough") == 0) {
        manager_error = 23;
        assert(brookesia_hal_board_manager_init() == 23);
        assert(brookesia_hal_board_manager_deinit_device_by_name("ordinary_device") == 23);
        assert(brookesia_hal_board_manager_deinit() == 23);
        assert(manager_calls == 2 && error_queries == 0 && retry_calls == 0);
        return;
    }
    if (std::strcmp(name, "allocation_failure") == 0) {
        const brookesia_hal_board_device_cleanup_t descriptor = {"arbitrary_device", get_error, retry};
        fail_allocations = true;
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_NO_MEM);
        fail_allocations = false;
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_OK);
        assert(brookesia_hal_board_manager_deinit() == ESP_OK);
        assert(error_queries == 1);
        return;
    }
    if (std::strcmp(name, "registration") == 0) {
        assert(brookesia_hal_board_manager_register_device_cleanup(nullptr) == ESP_ERR_INVALID_ARG);
        brookesia_hal_board_device_cleanup_t descriptor = {nullptr, get_error, retry};
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_INVALID_ARG);
        descriptor.device_name = "";
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_INVALID_ARG);
        descriptor = {"arbitrary_device", nullptr, retry};
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_INVALID_ARG);
        descriptor = {"arbitrary_device", get_error, nullptr};
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_INVALID_ARG);
        register_devices();
        descriptor = {"arbitrary_device", get_error, retry};
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_OK);
        descriptor.retry = retry_other;
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_INVALID_STATE);
        assert(brookesia_hal_board_manager_deinit() == ESP_OK);
        assert(error_queries == 2); // Duplicate registration must not add another callback.
        return;
    }
    if (std::strcmp(name, "restart_required") == 0) {
        const brookesia_hal_board_device_cleanup_t descriptor = {"arbitrary_device", get_error, retry_other};
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_OK);
        cleanup_error = 17;
        fail_allocations = true;
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == 17);
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == 17);
        assert(manager_calls == 0 && retry_calls == 2);
        return;
    }
    register_devices();
    if (std::strcmp(name, "late_registration") == 0) {
        const brookesia_hal_board_device_cleanup_t descriptor = {"late_device", get_error, retry};
        assert(brookesia_hal_board_manager_init_device_by_name("arbitrary_device") == ESP_OK);
        assert(brookesia_hal_board_manager_register_device_cleanup(&descriptor) == ESP_ERR_INVALID_STATE);
        assert(brookesia_hal_board_manager_deinit() == ESP_OK);
        assert(brookesia_hal_board_manager_init() == ESP_OK);
        cleanup_error = 17;
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == ESP_OK);
        assert(retry_calls == 1); // Registration survives manager restart.
        return;
    }
    fail_allocations = true; // Error handling must not allocate after registration.
    if (std::strcmp(name, "first_failure") == 0) {
        live_handle = true;
        published_error = 17;
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == 17);
        assert(manager_calls == 1 && retry_calls == 0);
        void *handle = nullptr;
        assert(brookesia_hal_board_manager_get_device_handle("arbitrary_device", &handle) == ESP_OK);
        assert(!handle && retry_calls == 0);
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == ESP_OK);
        assert(manager_calls == 1 && retry_calls == 1);
    } else if (std::strcmp(name, "retained_retry") == 0 || std::strcmp(name, "failed_lookup") == 0) {
        cleanup_error = 17;
        lookup_error = std::strcmp(name, "failed_lookup") == 0 ? ESP_FAIL : ESP_OK;
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == ESP_OK);
        assert(manager_calls == 0 && retry_calls == 1);
    } else if (std::strcmp(name, "live_handle") == 0) {
        cleanup_error = 17;
        live_handle = true;
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == ESP_OK);
        assert(manager_calls == 1 && retry_calls == 0);
    } else if (std::strcmp(name, "error_priority") == 0) {
        manager_error = 23;
        published_error = 17;
        assert(brookesia_hal_board_manager_deinit_device_by_name("arbitrary_device") == 23);
        assert(manager_calls == 1 && retry_calls == 0);
    } else if (std::strcmp(name, "global_error") == 0 || std::strcmp(name, "global_priority") == 0) {
        cleanup_error = 17;
        second_error = 19;
        manager_error = std::strcmp(name, "global_priority") == 0 ? 23 : ESP_OK;
        assert(brookesia_hal_board_manager_deinit() == (manager_error ? manager_error : 17));
        assert(manager_calls == 1 && retry_calls == 0 && error_queries == 2);
    } else {
        assert(false);
    }
    assert(brookesia_hal_board_manager_deinit_device_by_name(nullptr) == ESP_ERR_INVALID_ARG);
}

} // namespace
