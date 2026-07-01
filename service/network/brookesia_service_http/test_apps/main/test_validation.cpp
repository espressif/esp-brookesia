/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

BROOKESIA_TEST_CASE(service_not_started, "Test ServiceHttp - service not started", "[service][http][lifecycle]")
{
    TEST_ASSERT_FALSE_MESSAGE(HttpHelper::is_available(), "HTTP service should not be available before binding");

    HttpHelper::HttpRequest request;
    request.url = "https://example.com";
    auto async_result = HttpHelper::call_function_sync<double>(
                            HttpHelper::FunctionId::RequestAsync,
                            BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                        );
    TEST_ASSERT_FALSE_MESSAGE(async_result, "RequestAsync should fail when service is not started");

    auto sync_result = HttpHelper::call_function_sync<boost::json::object>(
                           HttpHelper::FunctionId::Request,
                           BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                       );
    TEST_ASSERT_FALSE_MESSAGE(sync_result, "Request should fail when service is not started");
}

BROOKESIA_TEST_CASE(reject_unsupported_url, "Test ServiceHttp - reject unsupported URL", "[service][http][validation]")
{
    TEST_ASSERT_TRUE_MESSAGE(start_http_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    HttpHelper::HttpRequest request;
    request.url = "ftp://example.com/file.txt";
    auto result = HttpHelper::call_function_sync<double>(
                      HttpHelper::FunctionId::RequestAsync,
                      BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                  );
    TEST_ASSERT_FALSE_MESSAGE(result, "Unsupported URL should be rejected");
}

BROOKESIA_TEST_CASE(reject_invalid_requests_and_reset_statistics, "Test ServiceHttp - reject invalid requests and reset statistics", "[service][http][validation]")
{
    TEST_ASSERT_TRUE_MESSAGE(start_http_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    HttpHelper::HttpRequest request;
    auto async_result = HttpHelper::call_function_sync<double>(
                            HttpHelper::FunctionId::RequestAsync,
                            BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                        );
    TEST_ASSERT_FALSE_MESSAGE(async_result, "Empty URL should be rejected");

    auto sync_result = HttpHelper::call_function_sync<boost::json::object>(
                           HttpHelper::FunctionId::Request,
                           BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                       );
    TEST_ASSERT_FALSE_MESSAGE(sync_result, "Empty URL should be rejected by sync Request");

    auto cancel_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::CancelRequest, 999999);
    TEST_ASSERT_FALSE_MESSAGE(cancel_result, "Unknown request cancellation should be rejected");

    auto reset_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::ResetStatistics);
    TEST_ASSERT_TRUE_MESSAGE(reset_result, "ResetStatistics should succeed");
}

BROOKESIA_TEST_CASE(accept_http_and_https_url_schemes, "Test ServiceHttp - accept HTTP and HTTPS URL schemes", "[service][http][validation]")
{
    TEST_ASSERT_TRUE_MESSAGE(start_http_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    std::vector<std::string> urls = {
        "http://127.0.0.1/",
        "https://127.0.0.1/",
    };
    for (const auto &url : urls) {
        HttpHelper::HttpRequest request;
        request.url = url;
        request.timeout_ms = 1000;
        request.tls_verify = HttpHelper::TlsVerifyMode::Skip;
        auto result = HttpHelper::call_function_sync<double>(
                          HttpHelper::FunctionId::RequestAsync,
                          BROOKESIA_DESCRIBE_TO_JSON(request).as_object()
                      );
        TEST_ASSERT_TRUE_MESSAGE(result, "HTTP/HTTPS scheme should pass request validation");

        auto cancel_result = HttpHelper::call_function_sync(HttpHelper::FunctionId::CancelRequest, result.value());
        TEST_ASSERT_TRUE_MESSAGE(cancel_result, "CancelRequest should accept the submitted request id");
    }
}

BROOKESIA_TEST_CASE(unknown_request_state, "Test ServiceHttp - unknown request state", "[service][http][state]")
{
    TEST_ASSERT_TRUE_MESSAGE(start_http_service(), "Failed to startup");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });

    auto result = HttpHelper::call_function_sync<boost::json::object>(
                      HttpHelper::FunctionId::GetRequestState, 999999
                  );
    TEST_ASSERT_FALSE_MESSAGE(result, "Unknown request state should be rejected");
}
