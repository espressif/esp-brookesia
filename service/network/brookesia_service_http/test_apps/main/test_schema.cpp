/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "general.hpp"

namespace {

template <typename Enum>
bool enum_value_string_roundtrip(Enum value)
{
    Enum parsed;
    const auto str = BROOKESIA_DESCRIBE_TO_STR(value);
    return BROOKESIA_DESCRIBE_STR_TO_ENUM(str, parsed) && (parsed == value);
}

bool has_function_schema(HttpHelper::FunctionId function_id)
{
    const auto expected_name = BROOKESIA_DESCRIBE_TO_STR(function_id);
    for (const auto &schema : HttpHelper::get_function_schemas()) {
        if (schema.name == expected_name) {
            return true;
        }
    }
    return false;
}

bool has_event_schema(HttpHelper::EventId event_id)
{
    const auto expected_name = BROOKESIA_DESCRIBE_TO_STR(event_id);
    for (const auto &schema : HttpHelper::get_event_schemas()) {
        if (schema.name == expected_name) {
            return true;
        }
    }
    return false;
}

bool function_schema_has_param(HttpHelper::FunctionId function_id, std::string_view param_name)
{
    const auto expected_function_name = BROOKESIA_DESCRIBE_TO_STR(function_id);
    for (const auto &schema : HttpHelper::get_function_schemas()) {
        if (schema.name != expected_function_name) {
            continue;
        }
        for (const auto &param : schema.parameters) {
            if (param.name == param_name) {
                return true;
            }
        }
    }
    return false;
}

bool event_schema_has_item(HttpHelper::EventId event_id, HttpHelper::EventRequestParam item)
{
    const auto expected_event_name = BROOKESIA_DESCRIBE_TO_STR(event_id);
    const auto expected_item_name = BROOKESIA_DESCRIBE_TO_STR(item);
    for (const auto &schema : HttpHelper::get_event_schemas()) {
        if (schema.name != expected_event_name) {
            continue;
        }
        for (const auto &item_schema : schema.items) {
            if (item_schema.name == expected_item_name) {
                return true;
            }
        }
    }
    return false;
}

bool event_schema_has_item(HttpHelper::EventId event_id, HttpHelper::EventGeneralStateChangedParam item)
{
    const auto expected_event_name = BROOKESIA_DESCRIBE_TO_STR(event_id);
    const auto expected_item_name = BROOKESIA_DESCRIBE_TO_STR(item);
    for (const auto &schema : HttpHelper::get_event_schemas()) {
        if (schema.name != expected_event_name) {
            continue;
        }
        for (const auto &item_schema : schema.items) {
            if (item_schema.name == expected_item_name) {
                return true;
            }
        }
    }
    return false;
}

} // namespace

BROOKESIA_TEST_CASE(enum_string_roundtrip, "Test ServiceHttp - enum string roundtrip", "[service][http][schema]")
{
    const std::vector<HttpHelper::HttpMethod> methods = {
        HttpHelper::HttpMethod::Get,
        HttpHelper::HttpMethod::Post,
        HttpHelper::HttpMethod::Put,
        HttpHelper::HttpMethod::Patch,
        HttpHelper::HttpMethod::Delete,
        HttpHelper::HttpMethod::Head,
    };
    for (auto value : methods) {
        TEST_ASSERT_TRUE(enum_value_string_roundtrip(value));
    }

    const std::vector<HttpHelper::TlsVerifyMode> tls_modes = {
        HttpHelper::TlsVerifyMode::Default,
        HttpHelper::TlsVerifyMode::Verify,
        HttpHelper::TlsVerifyMode::Skip,
    };
    for (auto value : tls_modes) {
        TEST_ASSERT_TRUE(enum_value_string_roundtrip(value));
    }

    const std::vector<HttpHelper::RequestState> request_states = {
        HttpHelper::RequestState::Queued,
        HttpHelper::RequestState::Running,
        HttpHelper::RequestState::Completed,
        HttpHelper::RequestState::Failed,
        HttpHelper::RequestState::Canceled,
    };
    for (auto value : request_states) {
        TEST_ASSERT_TRUE(enum_value_string_roundtrip(value));
    }

    const std::vector<HttpHelper::ErrorCode> error_codes = {
        HttpHelper::ErrorCode::Ok,
        HttpHelper::ErrorCode::InvalidArgument,
        HttpHelper::ErrorCode::NotStarted,
        HttpHelper::ErrorCode::UnsupportedTransport,
        HttpHelper::ErrorCode::BodyTooLarge,
        HttpHelper::ErrorCode::FileTooLarge,
        HttpHelper::ErrorCode::FileOpenFailed,
        HttpHelper::ErrorCode::ClientInitFailed,
        HttpHelper::ErrorCode::RequestFailed,
        HttpHelper::ErrorCode::Canceled,
        HttpHelper::ErrorCode::Timeout,
    };
    for (auto value : error_codes) {
        TEST_ASSERT_TRUE(enum_value_string_roundtrip(value));
    }

    const std::vector<HttpHelper::GeneralState> general_states = {
        HttpHelper::GeneralState::Idle,
        HttpHelper::GeneralState::Initing,
        HttpHelper::GeneralState::Inited,
        HttpHelper::GeneralState::TimeSyncing,
        HttpHelper::GeneralState::Starting,
        HttpHelper::GeneralState::Started,
        HttpHelper::GeneralState::Stopping,
        HttpHelper::GeneralState::Stopped,
        HttpHelper::GeneralState::Deiniting,
    };
    for (auto value : general_states) {
        TEST_ASSERT_TRUE(enum_value_string_roundtrip(value));
    }
}

BROOKESIA_TEST_CASE(helper_schema_and_serialization, "Test ServiceHttp - helper schema and serialization", "[service][http][schema]")
{
    HttpHelper::HttpRequest request;
    request.url = "https://example.com";
    request.method = HttpHelper::HttpMethod::Post;
    request.headers = {{"Accept", "application/json"}};
    request.body = "{\"hello\":\"world\"}";
    request.timeout_ms = 5000;
    request.tls_verify = HttpHelper::TlsVerifyMode::Verify;
    request.cert_pem = "test-cert";
    request.use_crt_bundle = false;
    request.download_path = "/littlefs/file.bin";
    request.max_response_size = 1024;
    request.max_file_size = 4096;
    request.retry_count = 2;
    request.retry_on_status_codes = {502, 503, 504};

    auto request_json = BROOKESIA_DESCRIBE_TO_JSON(request);
    HttpHelper::HttpRequest parsed_request;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(request_json, parsed_request));
    TEST_ASSERT_EQUAL_STRING(request.url.c_str(), parsed_request.url.c_str());
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::HttpMethod::Post),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(parsed_request.method)
    );
    TEST_ASSERT_EQUAL_STRING(request.body.c_str(), parsed_request.body.c_str());
    TEST_ASSERT_EQUAL_UINT32(request.timeout_ms, parsed_request.timeout_ms);
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(HttpHelper::TlsVerifyMode::Verify),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(parsed_request.tls_verify)
    );
    TEST_ASSERT_EQUAL_STRING(request.cert_pem.c_str(), parsed_request.cert_pem.c_str());
    TEST_ASSERT_FALSE(parsed_request.use_crt_bundle);
    TEST_ASSERT_EQUAL_STRING(request.download_path.c_str(), parsed_request.download_path.c_str());
    TEST_ASSERT_EQUAL_UINT32(request.max_response_size, parsed_request.max_response_size);
    TEST_ASSERT_EQUAL_UINT32(request.max_file_size, parsed_request.max_file_size);
    TEST_ASSERT_EQUAL_UINT32(request.retry_count, parsed_request.retry_count);
    TEST_ASSERT_EQUAL_size_t(request.retry_on_status_codes.size(), parsed_request.retry_on_status_codes.size());
    TEST_ASSERT_EQUAL_INT(request.retry_on_status_codes[0], parsed_request.retry_on_status_codes[0]);

    HttpHelper::HttpResponse response{
        .request_id = 7,
        .status_code = 200,
        .headers = {{"Content-Type", "text/plain"}},
        .body = "ok",
        .file_path = "/littlefs/out",
        .error = HttpHelper::ErrorCode::Ok,
        .error_message = "none",
    };
    HttpHelper::HttpResponse parsed_response;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(BROOKESIA_DESCRIBE_TO_JSON(response), parsed_response));
    TEST_ASSERT_EQUAL_UINT32(response.request_id, parsed_response.request_id);
    TEST_ASSERT_EQUAL(response.status_code, parsed_response.status_code);
    TEST_ASSERT_TRUE(parsed_response.headers.contains("Content-Type"));
    TEST_ASSERT_EQUAL_STRING(
        response.headers["Content-Type"].as_string().c_str(),
        parsed_response.headers["Content-Type"].as_string().c_str()
    );
    TEST_ASSERT_EQUAL_STRING(response.body.c_str(), parsed_response.body.c_str());
    TEST_ASSERT_EQUAL_STRING(response.file_path.c_str(), parsed_response.file_path.c_str());
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(response.error), BROOKESIA_DESCRIBE_ENUM_TO_NUM(parsed_response.error)
    );
    TEST_ASSERT_EQUAL_STRING(response.error_message.c_str(), parsed_response.error_message.c_str());

    HttpHelper::RequestProgress progress{.request_id = 7, .downloaded = 12, .total = 99};
    HttpHelper::RequestProgress parsed_progress;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(BROOKESIA_DESCRIBE_TO_JSON(progress), parsed_progress));
    TEST_ASSERT_EQUAL_UINT32(progress.downloaded, parsed_progress.downloaded);
    TEST_ASSERT_EQUAL_UINT32(progress.total, parsed_progress.total);

    HttpHelper::RequestStateInfo state_info{
        .request_id = 7,
        .state = HttpHelper::RequestState::Running,
        .error = HttpHelper::ErrorCode::Ok,
        .status_code = 200,
        .downloaded = 12,
        .total = 99,
    };
    HttpHelper::RequestStateInfo parsed_state_info;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(BROOKESIA_DESCRIBE_TO_JSON(state_info), parsed_state_info));
    TEST_ASSERT_EQUAL_UINT32(state_info.request_id, parsed_state_info.request_id);
    TEST_ASSERT_EQUAL(
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(state_info.state),
        BROOKESIA_DESCRIBE_ENUM_TO_NUM(parsed_state_info.state)
    );

    HttpHelper::Statistics statistics{
        .submitted_count = 1,
        .completed_count = 2,
        .failed_count = 3,
        .canceled_count = 4,
    };
    HttpHelper::Statistics parsed_statistics;
    TEST_ASSERT_TRUE(BROOKESIA_DESCRIBE_FROM_JSON(BROOKESIA_DESCRIBE_TO_JSON(statistics), parsed_statistics));
    TEST_ASSERT_EQUAL_UINT32(statistics.submitted_count, parsed_statistics.submitted_count);
    TEST_ASSERT_EQUAL_UINT32(statistics.completed_count, parsed_statistics.completed_count);
    TEST_ASSERT_EQUAL_UINT32(statistics.failed_count, parsed_statistics.failed_count);
    TEST_ASSERT_EQUAL_UINT32(statistics.canceled_count, parsed_statistics.canceled_count);

    TEST_ASSERT_EQUAL(5, HttpHelper::get_function_schemas().size());
    TEST_ASSERT_EQUAL(6, HttpHelper::get_event_schemas().size());
    for (const auto &schema : HttpHelper::get_function_schemas()) {
        TEST_ASSERT_NOT_EQUAL(0, schema.description.size());
        TEST_ASSERT_FALSE(schema.require_scheduler);
    }
    for (const auto &schema : HttpHelper::get_event_schemas()) {
        TEST_ASSERT_NOT_EQUAL(0, schema.description.size());
        TEST_ASSERT_NOT_EQUAL(0, schema.items.size());
    }

    TEST_ASSERT_TRUE(has_function_schema(HttpHelper::FunctionId::Request));
    TEST_ASSERT_TRUE(has_function_schema(HttpHelper::FunctionId::RequestAsync));
    TEST_ASSERT_TRUE(has_function_schema(HttpHelper::FunctionId::CancelRequest));
    TEST_ASSERT_TRUE(has_function_schema(HttpHelper::FunctionId::GetRequestState));
    TEST_ASSERT_TRUE(has_function_schema(HttpHelper::FunctionId::ResetStatistics));
    TEST_ASSERT_TRUE(
        function_schema_has_param(
            HttpHelper::FunctionId::Request, BROOKESIA_DESCRIBE_TO_STR(HttpHelper::FunctionRequestParam::Request)
        )
    );
    TEST_ASSERT_TRUE(
        function_schema_has_param(
            HttpHelper::FunctionId::RequestAsync,
            BROOKESIA_DESCRIBE_TO_STR(HttpHelper::FunctionRequestAsyncParam::Request)
        )
    );

    TEST_ASSERT_TRUE(has_event_schema(HttpHelper::EventId::RequestStarted));
    TEST_ASSERT_TRUE(has_event_schema(HttpHelper::EventId::RequestProgress));
    TEST_ASSERT_TRUE(has_event_schema(HttpHelper::EventId::RequestCompleted));
    TEST_ASSERT_TRUE(has_event_schema(HttpHelper::EventId::RequestFailed));
    TEST_ASSERT_TRUE(has_event_schema(HttpHelper::EventId::RequestCanceled));
    TEST_ASSERT_TRUE(has_event_schema(HttpHelper::EventId::GeneralStateChanged));
    TEST_ASSERT_TRUE(
        event_schema_has_item(HttpHelper::EventId::RequestStarted, HttpHelper::EventRequestParam::RequestId)
    );
    TEST_ASSERT_TRUE(event_schema_has_item(HttpHelper::EventId::RequestStarted, HttpHelper::EventRequestParam::State));
    TEST_ASSERT_TRUE(
        event_schema_has_item(HttpHelper::EventId::RequestProgress, HttpHelper::EventRequestParam::Progress)
    );
    TEST_ASSERT_TRUE(
        event_schema_has_item(HttpHelper::EventId::RequestCompleted, HttpHelper::EventRequestParam::Response)
    );
    TEST_ASSERT_TRUE(
        event_schema_has_item(HttpHelper::EventId::RequestFailed, HttpHelper::EventRequestParam::Response)
    );
    TEST_ASSERT_TRUE(
        event_schema_has_item(HttpHelper::EventId::RequestCanceled, HttpHelper::EventRequestParam::Response)
    );
    TEST_ASSERT_TRUE(
        event_schema_has_item(
            HttpHelper::EventId::GeneralStateChanged, HttpHelper::EventGeneralStateChangedParam::State
        )
    );
}
