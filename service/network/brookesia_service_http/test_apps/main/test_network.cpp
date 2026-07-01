/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

#if TEST_HTTP_ENABLE_NETWORK
namespace {

void assert_ok_response(const HttpHelper::HttpResponse &response)
{
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(response.error)
    );
    TEST_ASSERT_EQUAL(200, response.status_code);
}

} // namespace

BROOKESIA_TEST_CASE(sync_get_request_flow, "Test ServiceHttp - sync GET request flow", "[service][http][network][sync]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    HttpHelper::HttpResponse response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(make_test_request(), response), "Failed HTTP sync request");
    TEST_HTTP_RETURN_IF_TRANSIENT(response);
    assert_ok_response(response);
    TEST_ASSERT_NOT_EQUAL(0, response.body.size());
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_started(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), "Missing RequestStarted"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_progress(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), "Missing RequestProgress"
    );
}

BROOKESIA_TEST_CASE(http_method_variants, "Test ServiceHttp - HTTP method variants", "[service][http][network][methods]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    struct MethodCase {
        HttpHelper::HttpMethod method;
        std::string path;
        bool expect_body;
    };
    const std::vector<MethodCase> method_cases = {
        {HttpHelper::HttpMethod::Post, "/post", true},
        {HttpHelper::HttpMethod::Put, "/put", true},
        {HttpHelper::HttpMethod::Patch, "/patch", true},
        {HttpHelper::HttpMethod::Delete, "/delete", true},
        {HttpHelper::HttpMethod::Head, "/get", false},
    };
    for (const auto &test_case : method_cases) {
        auto method_request = make_test_request();
        method_request.url = make_echo_url(test_case.path);
        method_request.method = test_case.method;
        method_request.headers["Content-Type"] = "application/json";
        if (test_case.expect_body) {
            method_request.body = "{\"hello\":\"world\"}";
        }

        HttpHelper::HttpResponse method_response;
        TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(method_request, method_response), "Failed method request");
        TEST_HTTP_RETURN_IF_TRANSIENT(method_response);
        assert_ok_response(method_response);
        if (test_case.expect_body) {
            TEST_ASSERT_NOT_EQUAL(std::string::npos, method_response.body.find("world"));
        } else {
            TEST_ASSERT_EQUAL(0, method_response.body.size());
        }
    }
}

BROOKESIA_TEST_CASE(request_and_response_headers, "Test ServiceHttp - request and response headers", "[service][http][network][headers]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    auto header_request = make_test_request();
    header_request.url = make_echo_url("/response-headers?X-ESP-Brookesia=present");
    HttpHelper::HttpResponse header_response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(header_request, header_response), "Failed header request");
    TEST_HTTP_RETURN_IF_TRANSIENT(header_response);
    assert_ok_response(header_response);
    std::string header_value;
    TEST_ASSERT_TRUE_MESSAGE(
        get_response_header_case_insensitive(header_response, "X-ESP-Brookesia", header_value),
        "Missing custom header"
    );
    TEST_ASSERT_EQUAL_STRING("present", header_value.c_str());
}

BROOKESIA_TEST_CASE(tls_verify_modes, "Test ServiceHttp - TLS verify modes", "[service][http][network][tls]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    const std::vector<HttpHelper::TlsVerifyMode> tls_modes = {
        HttpHelper::TlsVerifyMode::Default,
        HttpHelper::TlsVerifyMode::Verify,
        HttpHelper::TlsVerifyMode::Skip,
    };
    for (auto mode : tls_modes) {
        auto tls_request = make_test_request();
        tls_request.tls_verify = mode;
        HttpHelper::HttpResponse tls_response;
        TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(tls_request, tls_response), "Failed TLS mode request");
        TEST_HTTP_RETURN_IF_TRANSIENT(tls_response);
        assert_ok_response(tls_response);
    }
}

BROOKESIA_TEST_CASE(body_size_limit, "Test ServiceHttp - body size limit", "[service][http][network][limit]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    auto limit_request = make_test_request();
    limit_request.max_response_size = HTTP_FAILURE_BODY_LIMIT;
    HttpHelper::HttpResponse limit_response;
    for (uint32_t attempt = 1; attempt <= HTTP_FAILURE_MAX_ATTEMPTS; attempt++) {
        TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(limit_request, limit_response), "Failed body-limit request");
        if (limit_response.error == HttpHelper::ErrorCode::BodyTooLarge) {
            break;
        }
        TEST_HTTP_RETURN_IF_TRANSIENT(limit_response);
        TEST_ASSERT_EQUAL(
            BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::RequestFailed),
            BROOKESIA_DESCRIBE_ENUM_TO_NUM(limit_response.error)
        );
    }
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::BodyTooLarge),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(limit_response.error)
    );
    HttpHelper::HttpResponse failed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(limit_response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestFailed"
    );
}

BROOKESIA_TEST_CASE(retry_on_5xx_status, "Test ServiceHttp - retry on 5xx status", "[service][http][network][retry]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    auto retry_request = make_test_request();
    retry_request.url = make_echo_url("/status/500");
    retry_request.retry_count = 2;
    retry_request.timeout_ms = 8000;
    HttpHelper::HttpResponse retry_response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(retry_request, retry_response), "Failed retry request");
    TEST_ASSERT_EQUAL(500, retry_response.status_code);
    TEST_ASSERT_TRUE(
        (retry_response.error == HttpHelper::ErrorCode::Ok) ||
        (retry_response.error == HttpHelper::ErrorCode::RequestFailed)
    );
    if (retry_response.error == HttpHelper::ErrorCode::RequestFailed) {
        HttpHelper::HttpResponse failed_response;
        TEST_ASSERT_TRUE_MESSAGE(
            collector.wait_for_failed(retry_response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
            "Missing retry RequestFailed"
        );
    }
}

BROOKESIA_TEST_CASE(async_request_lifecycle, "Test ServiceHttp - async request lifecycle", "[service][http][network][async]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    uint64_t request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_async(make_test_request(), request_id), "Failed HTTP async request");
    HttpHelper::RequestStateInfo state_info;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_state(
            request_id, HttpHelper::RequestState::Completed, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS
        ),
        "Async request did not complete"
    );
    HttpHelper::HttpResponse completed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_completed(request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestCompleted"
    );
    TEST_HTTP_RETURN_IF_TRANSIENT(completed_response);
    assert_ok_response(completed_response);
}

BROOKESIA_TEST_CASE(deterministic_async_cancel, "Test ServiceHttp - deterministic async cancel", "[service][http][network][cancel]")
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

    uint64_t request_id = 0;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_async(request, request_id), "Failed HTTP async cancel request");
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_started(request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), "Missing RequestStarted before cancel"
    );

    auto cancel_result = HttpHelper::call_function_sync(
                             HttpHelper::FunctionId::CancelRequest, static_cast<double>(request_id)
                         );
    TEST_ASSERT_TRUE_MESSAGE(cancel_result, "CancelRequest should succeed");

    HttpHelper::RequestStateInfo state_info;
    TEST_ASSERT_TRUE_MESSAGE(
        wait_for_http_request_state(
            request_id, HttpHelper::RequestState::Canceled, state_info, HTTP_WAIT_REQUEST_TIMEOUT_MS
        ),
        "Async request did not enter Canceled"
    );

    HttpHelper::HttpResponse canceled_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_canceled(request_id, canceled_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestCanceled"
    );
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Canceled),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(canceled_response.error)
    );
}
#endif
