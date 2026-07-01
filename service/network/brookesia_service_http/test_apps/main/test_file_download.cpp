/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

#include <cstdio>

#if TEST_HTTP_ENABLE_NETWORK
BROOKESIA_TEST_CASE(download_response_to_file, "Test ServiceHttp - download response to file", "[service][http][network][file]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(true), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    std::remove(HTTP_DOWNLOAD_FILE_PATH.data());

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    auto request = make_test_request();
    request.url = make_echo_url("/bytes/102400");
    request.download_path = HTTP_DOWNLOAD_FILE_PATH;

    HttpHelper::HttpResponse response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(request, response), "Failed HTTP file download request");
    TEST_HTTP_RETURN_IF_TRANSIENT(response);
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(response.error)
    );
    TEST_ASSERT_EQUAL_STRING(HTTP_DOWNLOAD_FILE_PATH.data(), response.file_path.c_str());
    TEST_ASSERT_EQUAL(0, response.body.size());

    size_t file_size = 0;
    TEST_ASSERT_TRUE_MESSAGE(get_file_size(HTTP_DOWNLOAD_FILE_PATH, file_size), "Failed to read downloaded file");
    TEST_ASSERT_GREATER_THAN(0, file_size);

    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_started(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), "Missing RequestStarted"
    );
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_progress(response.request_id, HTTP_WAIT_EVENT_TIMEOUT_MS), "Missing RequestProgress"
    );
    HttpHelper::HttpResponse completed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_completed(response.request_id, completed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing RequestCompleted"
    );
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::Ok),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(completed_response.error)
    );
}

BROOKESIA_TEST_CASE(file_download_limit_and_open_failure, "Test ServiceHttp - file download limit and open failure", "[service][http][network][file]")
{
    TEST_HTTP_ASSERT_WIFI_CREDENTIALS();
    TEST_ASSERT_TRUE_MESSAGE(start_network_services(true), "Failed to start network services");
    lib_utils::FunctionGuard shutdown_guard([]() {
        stop_services();
    });
    TEST_ASSERT_TRUE_MESSAGE(connect_wifi_sta(), "Failed to connect WiFi STA");

    std::remove(HTTP_DOWNLOAD_LIMIT_FILE_PATH.data());

    HttpEventCollector collector;
    TEST_ASSERT_TRUE_MESSAGE(collector.start(), "Failed to start HTTP event collector");

    auto limit_request = make_test_request();
    limit_request.url = make_echo_url("/bytes/512");
    limit_request.download_path = HTTP_DOWNLOAD_LIMIT_FILE_PATH;
    limit_request.max_file_size = HTTP_DOWNLOAD_FILE_LIMIT;

    HttpHelper::HttpResponse limit_response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(limit_request, limit_response), "Failed file limit request");
    TEST_HTTP_RETURN_IF_TRANSIENT(limit_response);
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::FileTooLarge),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(limit_response.error)
    );
    HttpHelper::HttpResponse failed_response;
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(limit_response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing FileTooLarge RequestFailed"
    );
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::FileTooLarge),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(failed_response.error)
    );

    auto open_fail_request = make_test_request();
    open_fail_request.url = make_echo_url("/bytes/16");
    open_fail_request.download_path = std::string(TEST_HTTP_DOWNLOAD_DIR) + "/missing-dir/http_download.bin";

    HttpHelper::HttpResponse open_fail_response;
    TEST_ASSERT_TRUE_MESSAGE(call_http_request_sync(open_fail_request, open_fail_response), "Failed open failure request");
    TEST_HTTP_RETURN_IF_TRANSIENT(open_fail_response);
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::FileOpenFailed),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(open_fail_response.error)
    );
    TEST_ASSERT_TRUE_MESSAGE(
        collector.wait_for_failed(open_fail_response.request_id, failed_response, HTTP_WAIT_EVENT_TIMEOUT_MS),
        "Missing FileOpenFailed RequestFailed"
    );
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::ErrorCode::FileOpenFailed),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(failed_response.error)
    );

    std::remove(HTTP_DOWNLOAD_LIMIT_FILE_PATH.data());
}
#endif
