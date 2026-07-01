/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "boost/json.hpp"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"

#include "brookesia/hal_adaptor/macro_configs.h"
#if !BROOKESIA_HAL_ADAPTOR_NETWORK_HTTP_CLIENT_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "http_client_impl.hpp"

#if BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL

namespace esp_brookesia::hal {
namespace {

using ErrorCode = network::HttpClientIface::ErrorCode;
using Request = network::HttpClientIface::Request;
using Response = network::HttpClientIface::Response;
using TlsVerifyMode = network::HttpClientIface::TlsVerifyMode;

esp_http_client_method_t to_esp_method(network::HttpClientIface::Method method)
{
    switch (method) {
    case network::HttpClientIface::Method::Get:
        return HTTP_METHOD_GET;
    case network::HttpClientIface::Method::Post:
        return HTTP_METHOD_POST;
    case network::HttpClientIface::Method::Put:
        return HTTP_METHOD_PUT;
    case network::HttpClientIface::Method::Patch:
        return HTTP_METHOD_PATCH;
    case network::HttpClientIface::Method::Delete:
        return HTTP_METHOD_DELETE;
    case network::HttpClientIface::Method::Head:
        return HTTP_METHOD_HEAD;
    default:
        return HTTP_METHOD_GET;
    }
}

std::string json_value_to_header_string(const boost::json::value &value)
{
    if (value.is_string()) {
        return std::string(value.as_string().c_str());
    }
    return boost::json::serialize(value);
}

bool is_https_url(const std::string &url)
{
    return url.rfind("https://", 0) == 0;
}

class EspHttpTransaction: public network::HttpClientIface::Transaction {
public:
    EspHttpTransaction() = default;
    ~EspHttpTransaction() override
    {
        if (client_ != nullptr) {
            esp_http_client_cleanup(client_);
            client_ = nullptr;
        }
    }

    ErrorCode open(const Request &request, Response &response, uint32_t &content_length) override;
    int read(char *buffer, size_t size, std::string &error_message) override;
    bool is_complete() const override;
    void cancel() override;

private:
    static esp_err_t event_handler(esp_http_client_event_t *evt);

    esp_http_client_handle_t client_ = nullptr;
    std::vector<std::pair<std::string, std::string>> headers_;
};

esp_err_t EspHttpTransaction::event_handler(esp_http_client_event_t *evt)
{
    if (evt == nullptr || evt->event_id != HTTP_EVENT_ON_HEADER) {
        return ESP_OK;
    }

    auto *client = static_cast<EspHttpTransaction *>(evt->user_data);
    if (client == nullptr || evt->header_key == nullptr || evt->header_value == nullptr) {
        return ESP_OK;
    }

    client->headers_.emplace_back(evt->header_key, evt->header_value);
    return ESP_OK;
}

network::HttpClientIface::ErrorCode EspHttpTransaction::open(
    const Request &request, Response &response, uint32_t &content_length
)
{
    const bool is_https = is_https_url(request.url);
    const bool should_verify = request.tls_verify == TlsVerifyMode::Default ?
                               BROOKESIA_HAL_ADAPTOR_NETWORK_HTTP_CLIENT_REQUIRE_TLS_VERIFY :
                               request.tls_verify == TlsVerifyMode::Verify;

    headers_.clear();
    esp_http_client_config_t config = {};
    config.url = request.url.c_str();
    config.timeout_ms = request.timeout_ms;
    config.disable_auto_redirect = false;
    config.event_handler = event_handler;
    config.user_data = this;
    if (is_https) {
        if (!request.cert_pem.empty()) {
            config.cert_pem = request.cert_pem.c_str();
        } else if (request.use_crt_bundle || !should_verify) {
            // ESP-IDF mbedTLS rejects HTTPS without a CA source, even when common-name verification is skipped.
            config.crt_bundle_attach = esp_crt_bundle_attach;
        }
        if (!should_verify) {
            config.skip_cert_common_name_check = true;
        }
    }

    client_ = esp_http_client_init(&config);
    if (client_ == nullptr) {
        response.error_message = "Failed to initialize esp_http_client";
        return ErrorCode::ClientInitFailed;
    }

    esp_http_client_set_method(client_, to_esp_method(request.method));
    for (const auto &header : request.headers) {
        auto value = json_value_to_header_string(header.value());
        std::string key(header.key().data(), header.key().size());
        esp_http_client_set_header(client_, key.c_str(), value.c_str());
    }

    const int write_len = request.body.size();
    auto err = esp_http_client_open(client_, write_len);
    if (err != ESP_OK) {
        response.error_message = esp_err_to_name(err);
        return ErrorCode::RequestFailed;
    }

    if (write_len > 0) {
        auto written = esp_http_client_write(client_, request.body.data(), write_len);
        if (written < 0 || written != write_len) {
            response.error_message = "Failed to write HTTP request body";
            return ErrorCode::RequestFailed;
        }
    }

    auto fetched_length = esp_http_client_fetch_headers(client_);
    if (fetched_length < 0) {
        response.error_message = "Failed to fetch HTTP response headers";
        return ErrorCode::RequestFailed;
    }

    response.status_code = esp_http_client_get_status_code(client_);
    for (const auto &header : headers_) {
        response.headers[header.first] = header.second;
    }
    content_length = static_cast<uint32_t>(std::max<int64_t>(fetched_length, 0));

    return ErrorCode::Ok;
}

int EspHttpTransaction::read(char *buffer, size_t size, std::string &error_message)
{
    if (client_ == nullptr) {
        error_message = "HTTP transaction is not opened";
        return -1;
    }

    const int read_len = esp_http_client_read(client_, buffer, size);
    if (read_len < 0) {
        error_message = "Failed to read HTTP response body";
    }
    return read_len;
}

bool EspHttpTransaction::is_complete() const
{
    return (client_ != nullptr) && esp_http_client_is_complete_data_received(client_);
}

void EspHttpTransaction::cancel()
{
    if (client_ != nullptr) {
        esp_http_client_close(client_);
    }
}

} // namespace

std::shared_ptr<network::HttpClientIface::Transaction> HttpClientAdaptorIface::create_transaction()
{
    std::shared_ptr<Transaction> transaction;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        transaction = std::make_shared<EspHttpTransaction>(), nullptr, "Failed to create HTTP transaction"
    );
    last_error_.clear();
    return transaction;
}

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_ADAPTOR_NETWORK_ENABLE_HTTP_CLIENT_IMPL
