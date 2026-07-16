/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <string>

#include "boost/json.hpp"
#include "brookesia/lib_utils/function_guard.hpp"
#include "brookesia/lib_utils/test_adapter.hpp"
#include "brookesia/service_helper/media/picodet.hpp"
#include "brookesia/service_manager.hpp"
#include "brookesia/service_picodet.hpp"

#if defined(ESP_PLATFORM)
#include "esp_err.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#endif

using namespace esp_brookesia;
using PicoDetHelper = service::helper::PicoDet;

#if defined(ESP_PLATFORM)
namespace {

constexpr const char *LITTLEFS_BASE_PATH = "/littlefs";
constexpr const char *LITTLEFS_LABEL = "littlefs_data";
constexpr const char *MODEL_DIR = "/littlefs/models/picodet_cat";
constexpr const char *IMAGE_PATH = "/littlefs/test/cat.jpg";
constexpr const char *TAG = "PicoDetTest";

bool mount_littlefs()
{
    const esp_vfs_littlefs_conf_t conf = {
        .base_path = LITTLEFS_BASE_PATH,
        .partition_label = LITTLEFS_LABEL,
        .partition = nullptr,
        .format_if_mount_failed = false,
        .read_only = true,
        .dont_mount = false,
        .grow_on_mount = false,
    };
    return esp_vfs_littlefs_register(&conf) == ESP_OK;
}

} // namespace

BROOKESIA_TEST_CASE(
    test_service_picodet_detect_image,
    "PicoDet loads a model and detects the cat image",
    "[service][picodet][inference]"
)
{
    TEST_ASSERT_TRUE(mount_littlefs());
    lib_utils::FunctionGuard fs_guard([]() {
        (void)esp_vfs_littlefs_unregister(LITTLEFS_LABEL);
    });

    auto &manager = service::ServiceManager::get_instance();
    TEST_ASSERT_TRUE(manager.init());
    lib_utils::FunctionGuard manager_guard([&manager]() {
        manager.deinit();
        service::ServiceRegistry::release_all_instances();
    });
    TEST_ASSERT_TRUE(manager.start());

    auto binding = manager.bind(PicoDetHelper::get_name().data());
    TEST_ASSERT_TRUE(binding.is_valid());

    boost::json::object open_config;
    open_config["model_dir"] = MODEL_DIR;
    auto open_result = PicoDetHelper::call_function_sync<boost::json::object>(
                           PicoDetHelper::FunctionId::Open, open_config
                       );
    TEST_ASSERT_TRUE(open_result.has_value());

    auto detect_result = PicoDetHelper::call_function_sync<boost::json::array>(
                             PicoDetHelper::FunctionId::Detect, std::string(IMAGE_PATH)
                         );
    TEST_ASSERT_TRUE(detect_result.has_value());
    TEST_ASSERT_GREATER_THAN(0, detect_result->size());

    const std::string detection_json = boost::json::serialize(*detect_result);
    ESP_LOGI(
        TAG, "Detected %u object(s): %s",
        static_cast<unsigned>(detect_result->size()), detection_json.c_str()
    );

    const auto &detected = detect_result->front().as_object();
    TEST_ASSERT_EQUAL_INT(0, detected.at("category").as_int64());
    TEST_ASSERT_EQUAL_STRING("cat", detected.at("label").as_string().c_str());
    TEST_ASSERT_TRUE(detected.at("score").as_double() > 0.0);
    TEST_ASSERT_EQUAL_size_t(4, detected.at("box").as_array().size());

    TEST_ASSERT_TRUE(
        PicoDetHelper::call_function_sync<void>(PicoDetHelper::FunctionId::Close).has_value()
    );
}
#endif
