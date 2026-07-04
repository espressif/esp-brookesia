/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

#include <algorithm>

#if TEST_HTTP_ENABLE_NETWORK
namespace {

void assert_ok_response(const HttpHelper::HttpResponse &response)
{
    TEST_HTTP_RETURN_IF_TRANSIENT(response);
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(response.error)
    );
    TEST_ASSERT_EQUAL(200, response.status_code);
    TEST_ASSERT_NOT_EQUAL(0, response.body.size());
}

HttpHelper::HttpRequest make_stress_request(std::string_view path)
{
    auto request = make_test_request();
    request.url = make_echo_url(path);
    request.timeout_ms = 15000;
    request.retry_count = 1;
    return request;
}

size_t get_concurrent_batch_size()
{
    const auto service_limit = static_cast<size_t>(BROOKESIA_SERVICE_HTTP_MAX_CONCURRENT_REQUESTS);
    return std::min(service_limit, HTTP_STRESS_CONCURRENT_LIMIT);
}

void assert_async_completed(HttpEventCollector &collector, uint64_t request_id)
{
    HttpHelper::RequestStateInfo state_info;
    const bool completed = wait_for_http_request_state(
                               request_id, HttpHelper::RequestState::Completed, state_info,
                               HTTP_WAIT_REQUEST_TIMEOUT_MS
                           );
    if (!completed) {
        HttpHelper::HttpResponse failed_response;
        const bool failed_event_seen = collector.wait_for_failed(request_id, failed_response, HTTP_POLL_INTERVAL_MS);
        if (failed_event_seen && is_transient_network_response(failed_response)) {
            stop_services();
            TEST_IGNORE_MESSAGE("Transient HTTP endpoint failure");
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(
        completed, "Async stress request did not complete"
    );

    HttpHelper::HttpResponse completed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_completed(request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestCompleted in stress test"
    );
    assert_ok_response(completed_response);
}

} // namespace

BROOKESIA_TEST_CASE(sequential_request_stress, "Test ServiceHttp - sequential request stress", "[service][http][network][stress]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    auto reset_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::ResetStatistics);
    TEST_ASSERT_TRUE_MESSAGE(reset_result, "ResetStatistics should succeed before stress");

    for (size_t i = 0; i < HTTP_STRESS_SEQUENTIAL_COUNT; i++) {
        if ((i % 10) == 0) {
            BROOKESIA_LOGI("Sequential HTTP stress iteration %1%/%2%", i + 1, HTTP_STRESS_SEQUENTIAL_COUNT);
        }

        HttpHelper::HttpResponse response;
        TEST_ASSERT_TRUE_MESSAGE(
            call_http_request_sync(make_stress_request("/base64/ZXNwLWJyb29rZXNpYQ=="), response),
            "Failed sequential stress request"
        );
        assert_ok_response(response);
    }
}

BROOKESIA_TEST_CASE(concurrent_async_request_stress, "Test ServiceHttp - concurrent async request stress", "[service][http][network][stress]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    const auto batch_size = get_concurrent_batch_size();
    TEST_ASSERT_GREATER_THAN(0, batch_size);

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    for (size_t round = 0; round < HTTP_STRESS_CONCURRENT_ROUNDS; round++) {
        BROOKESIA_LOGI(
            "Concurrent HTTP stress round %1%/%2%, batch=%3%",
            round + 1, HTTP_STRESS_CONCURRENT_ROUNDS, batch_size
        );

        std::vector<uint64_t> request_ids;
        request_ids.reserve(batch_size);
        for (size_t i = 0; i < batch_size; i++) {
            uint64_t request_id = 0;
            TEST_ASSERT_TRUE_MESSAGE(
                call_http_request_async(make_stress_request("/delay/1"), request_id),
                "Failed concurrent stress request submission"
            );
            request_ids.push_back(request_id);
        }

        for (auto request_id : request_ids) {
            assert_async_completed(collector, request_id);
        }
    }
}

BROOKESIA_TEST_CASE(concurrent_request_limit_stress, "Test ServiceHttp - concurrent request limit stress", "[service][http][network][stress]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    const auto service_limit = static_cast<size_t>(BROOKESIA_SERVICE_HTTP_MAX_CONCURRENT_REQUESTS);
    std::vector<uint64_t> accepted_request_ids;
    accepted_request_ids.reserve(service_limit);
    size_t rejected_count = 0;

    auto request = make_stress_request("/delay/10");
    request.timeout_ms = 15000;
    request.retry_count = 0;

    for (size_t i = 0; i < service_limit + 1; i++) {
        auto result = HttpHelper::call_function_sync<double>(
                          HttpHelper::FunctionId::RequestAsync,
                          BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                      );
        if (result) {
            accepted_request_ids.push_back(static_cast<uint64_t>(result.value()));
            continue;
        }

        rejected_count++;
        TEST_ASSERT_NOT_EQUAL(
            std::string::npos, result.error().find("Too many concurrent HTTP requests")
        );
    }

    TEST_ASSERT_EQUAL(service_limit, accepted_request_ids.size());
    TEST_ASSERT_GREATER_OR_EQUAL(1, rejected_count);

    for (auto request_id : accepted_request_ids) {
        auto cancel_result = HttpHelper::call_function_sync(
                                 HttpHelper::FunctionId::CancelRequest, static_cast<double>(request_id)
                             );
        TEST_ASSERT_TRUE_MESSAGE(cancel_result, "CancelRequest should succeed during limit stress cleanup");
    }

    for (auto request_id : accepted_request_ids) {
        HttpHelper::RequestStateInfo state_info;
        TEST_ASSERT_TRUE_MESSAGE(
            wait_for_http_request_terminal_state(request_id, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS),
            "Accepted limit-stress request did not reach terminal state"
        );

        if (state_info.state == HttpHelper::RequestState::Canceled) {
            HttpHelper::HttpResponse canceled_response;
            TEST_ASSERT_TRUE_MESSAGE(
                collector.wait_for_canceled(request_id, canceled_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
                "Missing RequestCanceled during limit stress cleanup"
            );
            TEST_ASSERT_EQUAL(
                BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Canceled),
                BROOKESIA_DESCRIBE_ENUM_TO_NUM(canceled_response.error)
            );
        } else {
            TEST_ASSERT_EQUAL(
                BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::RequestState::Completed),
                BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_info.state)
            );
            HttpHelper::HttpResponse completed_response;
            TEST_ASSERT_TRUE_MESSAGE(
                collector.wait_for_completed(request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
                "Missing RequestCompleted during limit stress cleanup"
            );
            assert_ok_response(completed_response);
        }
    }
}
#endif
