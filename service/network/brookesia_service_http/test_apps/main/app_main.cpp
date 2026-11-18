/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */
#if !defined(ESP_PLATFORM)
#define BROOKESIA_LIB_UTILS_TEST_ADAPTER_MAIN
#endif
#include "general.hpp"

#if defined(ESP_PLATFORM)
#include "esp_system.h"
#include "unity_test_utils.h"
#if TEST_HTTP_USE_WIFI
#include "esp_event.h"
#include "esp_netif.h"
#endif

#define TEST_DEFAULT_LEAK_THRESHOLD (32)

void setUp(void)
{
    // Per-test default; network tests bump this from inside start_network_services()
    // to absorb the ~40 B/cycle lwIP cyclic-timer churn that survives netif teardown.
    unity_utils_set_leak_level(TEST_DEFAULT_LEAK_THRESHOLD);
    unity_utils_record_free_mem();
}

void tearDown(void)
{
    cleanup_http_test();
    esp_reent_cleanup();
    unity_utils_evaluate_leaks();
}

extern "C" void app_main(void)
{
#if TEST_HTTP_USE_WIFI
    // These SDK resources live for the process, outside each test's heap baseline.
    ESP_ERROR_CHECK(esp_netif_init());
    const auto event_loop_result = esp_event_loop_create_default();
    if (event_loop_result != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(event_loop_result);
    }
#endif
    unity_run_menu();
}
#else

namespace {

class HttpTestObserver : public boost::unit_test::test_observer {
public:
    void test_unit_finish(const boost::unit_test::test_unit &unit, unsigned long) override
    {
        if (unit.p_type == boost::unit_test::TUT_CASE) {
            cleanup_http_test();
        }
    }
};

bool init_unit_test()
{
    static HttpTestObserver observer;
    boost::unit_test::framework::register_observer(observer);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    return boost::unit_test::unit_test_main(&init_unit_test, argc, argv);
}
#endif
