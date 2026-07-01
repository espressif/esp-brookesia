/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "boost/json.hpp"
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interface.hpp"

/**
 * @file http_client.hpp
 * @brief Declares the HTTP client HAL interface.
 */

namespace esp_brookesia::hal::network {

/**
 * @brief Platform HTTP/HTTPS client interface.
 */
class HttpClientIface: public Interface {
public:
    static constexpr const char *NAME = "NetworkHttpClient";  ///< Interface registry name.

    /**
     * @brief HTTP request method.
     */
    enum class Method {
        Get,
        Post,
        Put,
        Patch,
        Delete,
        Head,
        Max,
    };

    /**
     * @brief TLS certificate verification policy for HTTPS requests.
     */
    enum class TlsVerifyMode {
        Default,
        Verify,
        Skip,
        Max,
    };

    /**
     * @brief HTTP platform error code.
     */
    enum class ErrorCode {
        Ok,
        InvalidArgument,
        NotStarted,
        UnsupportedTransport,
        BodyTooLarge,
        FileTooLarge,
        FileOpenFailed,
        ClientInitFailed,
        RequestFailed,
        Canceled,
        Timeout,
        Max,
    };

    /**
     * @brief HTTP request payload.
     */
    struct Request {
        std::string url;                         ///< Request URL.
        Method method = Method::Get;             ///< HTTP method.
        boost::json::object headers = {};        ///< Request headers.
        std::string body;                        ///< Optional request body.
        uint32_t timeout_ms = 10000;             ///< Per-attempt timeout in milliseconds.
        TlsVerifyMode tls_verify = TlsVerifyMode::Default; ///< HTTPS certificate verification policy.
        std::string cert_pem;                    ///< Optional PEM certificate.
        bool use_crt_bundle = true;              ///< Use platform certificate bundle when available.
        std::string download_path;               ///< Service-level output file path.
        uint32_t max_response_size = 0;          ///< Service-level memory response limit.
        uint32_t max_file_size = 0;              ///< Service-level file response limit.
        uint32_t retry_count = 0;                ///< Service-level retry count.
        std::vector<int> retry_on_status_codes = {}; ///< Service-level retryable HTTP status codes.
    };

    /**
     * @brief HTTP response payload.
     */
    struct Response {
        uint64_t request_id = 0;                 ///< Service-level request id.
        int status_code = 0;                     ///< HTTP status code.
        boost::json::object headers = {};        ///< Response headers.
        std::string body;                        ///< In-memory response body.
        std::string file_path;                   ///< Service-level downloaded file path.
        ErrorCode error = ErrorCode::Ok;         ///< Final error code.
        std::string error_message;               ///< Human-readable error.
    };

    /**
     * @brief Single HTTP request/response transaction.
     */
    class Transaction {
    public:
        Transaction() = default;
        virtual ~Transaction() = default;

        Transaction(const Transaction &) = delete;
        Transaction &operator=(const Transaction &) = delete;

        /**
         * @brief Open and send an HTTP request.
         *
         * @param[in] request Request payload.
         * @param[out] response Response metadata and error text.
         * @param[out] content_length Advertised response body length, or 0 if unknown.
         * @return Platform error code.
         */
        virtual ErrorCode open(const Request &request, Response &response, uint32_t &content_length) = 0;

        /**
         * @brief Read response body bytes.
         *
         * @param[out] buffer Destination buffer.
         * @param[in] size Destination size.
         * @param[out] error_message Human-readable error on failure.
         * @return Positive read length, 0 for EOF, or negative on failure.
         */
        virtual int read(char *buffer, size_t size, std::string &error_message) = 0;

        /**
         * @brief Check whether the full response body has been received.
         *
         * @return true if complete, otherwise false.
         */
        virtual bool is_complete() const = 0;

        /**
         * @brief Cancel the current request and close the transport.
         */
        virtual void cancel() = 0;
    };

    /**
     * @brief Construct an HTTP protocol interface.
     */
    HttpClientIface()
        : Interface(NAME)
    {
    }

    /**
     * @brief Virtual destructor for polymorphic protocol interfaces.
     */
    ~HttpClientIface() override = default;

    /**
     * @brief Create an independent HTTP transaction.
     *
     * @return Transaction instance, or nullptr on failure.
     */
    virtual std::shared_ptr<Transaction> create_transaction() = 0;

    /**
     * @brief Get the most recent backend error.
     *
     * @return Human-readable error string.
     */
    const std::string &get_last_error() const
    {
        return last_error_;
    }

protected:
    std::string last_error_;
};

BROOKESIA_DESCRIBE_ENUM(HttpClientIface::Method, Get, Post, Put, Patch, Delete, Head, Max);
BROOKESIA_DESCRIBE_ENUM(HttpClientIface::TlsVerifyMode, Default, Verify, Skip, Max);
BROOKESIA_DESCRIBE_ENUM(
    HttpClientIface::ErrorCode, Ok, InvalidArgument, NotStarted, UnsupportedTransport, BodyTooLarge, FileTooLarge,
    FileOpenFailed, ClientInitFailed, RequestFailed, Canceled, Timeout, Max
);
BROOKESIA_DESCRIBE_STRUCT(
    HttpClientIface::Request, (),
    (
        url, method, headers, body, timeout_ms, tls_verify, cert_pem, use_crt_bundle, download_path,
        max_response_size, max_file_size, retry_count, retry_on_status_codes
    )
);
BROOKESIA_DESCRIBE_STRUCT(
    HttpClientIface::Response, (),
    (request_id, status_code, headers, body, file_path, error, error_message)
);

} // namespace esp_brookesia::hal::network
