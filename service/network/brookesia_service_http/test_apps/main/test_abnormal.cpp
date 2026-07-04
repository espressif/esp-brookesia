/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

#if TEST_HTTP_USE_WIFI
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

void assert_failed_state(const HttpHelper::RequestStateInfo &state_info)
{
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::RequestState::Failed),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_info.state)
    );
    TEST_ASSERT_NOT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_info.error)
    );
}

HttpHelper::HttpRequest make_short_failure_request()
{
    auto request = make_test_request();
    request.timeout_ms = 3000;
    request.retry_count = 0;
    return request;
}

} // namespace

BROOKESIA_TEST_CASE(request_without_wifi_fails_and_recovers, "Test ServiceHttp - request without WiFi fails and recovers", "[service][http][network][abnormal]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_http_service(), "Failed to start HTTP service");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    uint64_t request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(
        call_http_request_async(make_short_failure_request(), request_id), "Failed to submit disconnected request"
    );

    HttpHelper::RequestStateInfo state_info;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_terminal_state(request_id, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS),
        "Disconnected request did not reach terminal state"
    );
    assert_failed_state(state_info);

    HttpHelper::HttpResponse failed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestFailed after disconnected request"
    );
    TEST_ASSERT_NOT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(failed_response.error)
    );

    TEST_ASSERT_TRUE_MESSAGE(bind_wifi_service(), "Failed to bind WiFi service");
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA after disconnected failure");

    HttpHelper::HttpResponse recovery_response;
    TEST_ASSERT_TRUE_MESSAGE(
        call_http_request_sync(make_test_request(), recovery_response), "Failed recovery sync request"
    );
    assert_ok_response(recovery_response);
}

BROOKESIA_TEST_CASE(in_flight_request_fails_on_wifi_disconnect_and_recovers, "Test ServiceHttp - in-flight request fails on WiFi disconnect and recovers", "[service][http][network][abnormal]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    auto request = make_test_request();
    request.url = make_echo_url("/delay/10");
    request.timeout_ms = 15000;
    request.retry_count = 0;

    uint64_t request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_async(request, request_id), "Failed to submit delayed request");
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_started(request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), "Missing RequestStarted before disconnect"
    );

    TEST_ASSERT_TRUE_MESSAGE(disconnect_wifi_sta(), "Failed to disconnect WiFi STA during request");

    HttpHelper::RequestStateInfo state_info;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_terminal_state(request_id, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS),
        "In-flight disconnected request did not reach terminal state"
    );
    assert_failed_state(state_info);

    HttpHelper::HttpResponse failed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestFailed after in-flight disconnect"
    );
    TEST_ASSERT_NOT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(failed_response.error)
    );

    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to reconnect WiFi STA after in-flight disconnect");

    HttpHelper::HttpResponse recovery_response;
    TEST_ASSERT_TRUE_MESSAGE(
        call_http_request_sync(make_test_request(), recovery_response), "Failed recovery sync request"
    );
    assert_ok_response(recovery_response);
}

BROOKESIA_TEST_CASE(reset_and_reuse_after_abnormal_network_failure, "Test ServiceHttp - reset and reuse after abnormal network failure", "[service][http][network][abnormal]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");
    TEST_ASSERT_TRUE_MESSAGE(disconnect_wifi_sta(), "Failed to disconnect WiFi STA before failure");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    uint64_t failed_request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(
        call_http_request_async(make_short_failure_request(), failed_request_id),
        "Failed to submit disconnected failure request"
    );

    HttpHelper::RequestStateInfo failed_state;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_terminal_state(failed_request_id, failed_state, HTTP_WAIT_REQUEST_TIMEOUT_MS),
        "Disconnected failure request did not reach terminal state"
    );
    assert_failed_state(failed_state);

    HttpHelper::HttpResponse failed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(failed_request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestFailed before reset"
    );

    auto reset_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::ResetStatistics);
    TEST_ASSERT_TRUE_MESSAGE(reset_result, "ResetStatistics should succeed after abnormal failure");

    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to reconnect WiFi STA after reset");

    HttpHelper::HttpResponse sync_response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(make_test_request(), sync_response), "Failed reuse sync request");
    assert_ok_response(sync_response);

    uint64_t async_request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(
        call_http_request_async(make_test_request(), async_request_id), "Failed reuse async request"
    );
    HttpHelper::RequestStateInfo async_state;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_state(
            async_request_id, HttpHelper::RequestState::Completed, async_state, HTTP_WAIT_REQUEST_TIMEOUT_MS
        ),
        "Reuse async request did not complete"
    );

    HttpHelper::HttpResponse completed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_completed(async_request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestCompleted after reset"
    );
    assert_ok_response(completed_response);
}
#endif

#if TEST_HTTP_ENABLE_NETWORK && !TEST_HTTP_USE_WIFI
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

void assert_failed_state(const HttpHelper::RequestStateInfo &state_info)
{
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::RequestState::Failed),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_info.state)
    );
    TEST_ASSERT_NOT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_info.error)
    );
}

HttpHelper::HttpRequest make_invalid_host_request()
{
    auto request = make_test_request();
    request.url = "https://127.0.0.1:9/get";
    request.timeout_ms = 1000;
    request.retry_count = 0;
    request.tls_verify = HttpHelper::TlsVerifyMode::Skip;
    return request;
}

} // namespace

BROOKESIA_TEST_CASE(
    invalid_host_failure_and_recovery,
    "Test ServiceHttp - invalid host failure and recovery",
    "[service][http][network][abnormal]"
)
{
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    uint64_t request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_async(make_invalid_host_request(), request_id), "Failed invalid request");

    HttpHelper::RequestStateInfo state_info;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_terminal_state(request_id, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS),
        "Invalid host request did not reach terminal state"
    );
    assert_failed_state(state_info);

    HttpHelper::HttpResponse failed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestFailed after invalid host request"
    );

    auto reset_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::ResetStatistics);
    TEST_ASSERT_TRUE_MESSAGE(reset_result, "ResetStatistics should succeed after invalid host failure");

    HttpHelper::HttpResponse recovery_response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(make_test_request(), recovery_response), "Failed recovery request");
    assert_ok_response(recovery_response);
}
#endif
