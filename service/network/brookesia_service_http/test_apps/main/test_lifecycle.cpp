/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

BROOKESIA_TEST_CASE(service_lifecycle_and_availability, "Test ServiceHttp - service lifecycle and availability", "[service][http][lifecycle]")
{
    TEST_ASSERT_TRUE_MESSAGE(start_http_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    TEST_ASSERT_TRUE_MESSAGE(HttpHelper::is_available(), "HTTP service should be available after binding");
}

BROOKESIA_TEST_CASE(general_state_changed_event, "Test ServiceHttp - general state changed event", "[service][http][lifecycle][event]")
{
    TEST_ASSERT_TRUE_MESSAGE(start_service_manager(), "Failed to start service manager");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    auto binding = bind_http_service();
    TEST_ASSERT_TRUE_MESSAGE(binding.is_valid(), "Failed to bind HTTP service");
    TEST_ASSERT_TRUE_MESSAGE(HttpHelper::is_available(), "HTTP service should be available after binding");
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_general_state(
            BROOKESIA_DESCRIBE_TO_STR(HttpHelper::GeneralState::Started), HTTP_WAIT_EVENT_TIMEOUT_MS
        ),
        "Missing Started GeneralStateChanged event"
    );

    binding.release();
}
