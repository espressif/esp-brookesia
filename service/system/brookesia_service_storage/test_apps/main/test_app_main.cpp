/*
 * SPDX-FileCopyrightText: 2024-2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#if !defined(ESP_PLATFORM)
#define BROOKESIA_LIB_UTILS_TEST_ADAPTER_MAIN
#endif
#include "brookesia/lib_utils/test_adapter.hpp"

#if defined(ESP_PLATFORM)
#include "esp_system.h"
#include "unity_test_utils.h"

// Some resources are lazy allocated in the driver, the threshold is left for that case
#define TEST_MEMORY_LEAK_THRESHOLD (32)

void setUp(void)
{
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    esp_reent_cleanup();
    unity_utils_evaluate_leaks_direct(TEST_MEMORY_LEAK_THRESHOLD);
}

extern "C" void app_main(void)
{
    unity_run_menu();
}
#else

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

namespace {

bool is_subpath(
    const std::filesystem::path &candidate_path,
    const std::filesystem::path &parent_path
)
{
    auto candidate_it = candidate_path.begin();
    auto parent_it = parent_path.begin();
    for (; parent_it != parent_path.end(); ++parent_it, ++candidate_it) {
        if ((candidate_it == candidate_path.end()) || (*candidate_it != *parent_it)) {
            return false;
        }
    }
    return true;
}

bool clear_directory_children(const std::filesystem::path &directory_path, bool preserve_directories = false)
{
    std::error_code error_code;
    if (!std::filesystem::exists(directory_path, error_code)) {
        return true;
    }
    if (error_code) {
        std::fprintf(stderr, "Failed to inspect directory '%s': %s\n",
                     directory_path.c_str(), error_code.message().c_str());
        return false;
    }

    for (const auto &entry : std::filesystem::directory_iterator(directory_path, error_code)) {
        if (error_code) {
            std::fprintf(stderr, "Failed to iterate directory '%s': %s\n",
                         directory_path.c_str(), error_code.message().c_str());
            return false;
        }
        if (preserve_directories && entry.is_directory(error_code) && !entry.is_symlink(error_code)) {
            if (error_code) {
                std::fprintf(stderr, "Failed to inspect directory entry '%s': %s\n",
                             entry.path().c_str(), error_code.message().c_str());
                return false;
            }
            if (!clear_directory_children(entry.path(), false)) {
                return false;
            }
        } else {
            std::filesystem::remove_all(entry.path(), error_code);
            if (error_code) {
                std::fprintf(stderr, "Failed to clear '%s': %s\n",
                             entry.path().c_str(), error_code.message().c_str());
                return false;
            }
        }
    }

    return true;
}

bool prepare_storage_kv_dir()
{
    const char *kv_dir = std::getenv("BROOKESIA_HAL_LINUX_KV_DIR");
    if ((kv_dir == nullptr) || (kv_dir[0] == '\0')) {
#if defined(TEST_STORAGE_PC_KV_DIR)
        if (setenv("BROOKESIA_HAL_LINUX_KV_DIR", TEST_STORAGE_PC_KV_DIR, 1) != 0) {
            std::perror("setenv(BROOKESIA_HAL_LINUX_KV_DIR)");
            return false;
        }
        kv_dir = std::getenv("BROOKESIA_HAL_LINUX_KV_DIR");
#endif
    }

    if ((kv_dir == nullptr) || (kv_dir[0] == '\0')) {
        std::fprintf(stderr, "BROOKESIA_HAL_LINUX_KV_DIR is not set\n");
        return false;
    }

    const std::filesystem::path kv_dir_path(kv_dir);
    const char *fs_root = std::getenv("BROOKESIA_HAL_LINUX_FS_ROOT");
    const bool has_fs_root = (fs_root != nullptr) && (fs_root[0] != '\0');
    const std::filesystem::path fs_root_path = has_fs_root ? std::filesystem::path(fs_root) : std::filesystem::path();
    std::error_code error_code;
    std::filesystem::create_directories(kv_dir_path, error_code);
    if (error_code) {
        std::fprintf(stderr, "Failed to create storage KV dir '%s': %s\n",
                     kv_dir, error_code.message().c_str());
        return false;
    }

    if (has_fs_root && is_subpath(fs_root_path, kv_dir_path)) {
        for (const auto &entry : std::filesystem::directory_iterator(kv_dir_path, error_code)) {
            if (error_code) {
                std::fprintf(stderr, "Failed to iterate storage KV dir '%s': %s\n",
                             kv_dir, error_code.message().c_str());
                return false;
            }
            if (entry.path() == fs_root_path) {
                continue;
            }
            std::filesystem::remove_all(entry.path(), error_code);
            if (error_code) {
                std::fprintf(stderr, "Failed to clear storage KV entry '%s': %s\n",
                             entry.path().c_str(), error_code.message().c_str());
                return false;
            }
        }
        if (!clear_directory_children(fs_root_path, true)) {
            return false;
        }
        std::filesystem::create_directories(fs_root_path, error_code);
        if (error_code) {
            std::fprintf(stderr, "Failed to create file-system root '%s': %s\n",
                         fs_root_path.c_str(), error_code.message().c_str());
            return false;
        }
    } else {
        std::filesystem::remove_all(kv_dir_path, error_code);
        if (error_code) {
            std::fprintf(stderr, "Failed to clear storage KV dir '%s': %s\n",
                         kv_dir, error_code.message().c_str());
            return false;
        }
        std::filesystem::create_directories(kv_dir_path, error_code);
        if (error_code) {
            std::fprintf(stderr, "Failed to recreate storage KV dir '%s': %s\n",
                         kv_dir, error_code.message().c_str());
            return false;
        }
    }

    return true;
}

bool init_unit_test()
{
    return prepare_storage_kv_dir();
}

} // namespace

int main(int argc, char **argv)
{
    return boost::unit_test::unit_test_main(&init_unit_test, argc, argv);
}
#endif
