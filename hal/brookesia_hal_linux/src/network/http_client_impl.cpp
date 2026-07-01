/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "boost/asio/ip/tcp.hpp"
#include "boost/asio/ssl.hpp"
#include "boost/asio/ssl/host_name_verification.hpp"
#include "boost/beast/core.hpp"
#include "boost/beast/http.hpp"
#include "boost/json.hpp"
#include "openssl/ssl.h"

#include "brookesia/hal_linux/macro_configs.h"
#if !BROOKESIA_HAL_LINUX_NETWORK_HTTP_CLIENT_IMPL_ENABLE_DEBUG_LOG
#   define BROOKESIA_LOG_DISABLE_DEBUG_TRACE 1
#endif
#include "private/utils.hpp"
#include "http_client_impl.hpp"

#if BROOKESIA_HAL_LINUX_NETWORK_ENABLE_HTTP_CLIENT_IMPL

namespace esp_brookesia::hal {
namespace {

namespace asio = boost::asio;
namespace beast = boost::beast;
namespace http = beast::http;
namespace ssl = asio::ssl;
using tcp = asio::ip::tcp;
using ErrorCode = network::HttpClientIface::ErrorCode;
using Request = network::HttpClientIface::Request;
using Response = network::HttpClientIface::Response;
using TlsVerifyMode = network::HttpClientIface::TlsVerifyMode;

constexpr uint64_t HTTP_CLIENT_READ_THROTTLE_BYTES_PER_SEC =
    BROOKESIA_HAL_LINUX_NETWORK_HTTP_CLIENT_READ_THROTTLE_BYTES_PER_SEC;
constexpr auto HTTP_CLIENT_READ_THROTTLE_SLEEP_SLICE = std::chrono::milliseconds(10);

struct ParsedUrl {
    bool is_https = false;
    std::string host;
    std::string port;
    std::string target;
};

http::verb to_beast_method(network::HttpClientIface::Method method)
{
    switch (method) {
    case network::HttpClientIface::Method::Get:
        return http::verb::get;
    case network::HttpClientIface::Method::Post:
        return http::verb::post;
    case network::HttpClientIface::Method::Put:
        return http::verb::put;
    case network::HttpClientIface::Method::Patch:
        return http::verb::patch;
    case network::HttpClientIface::Method::Delete:
        return http::verb::delete_;
    case network::HttpClientIface::Method::Head:
        return http::verb::head;
    default:
        return http::verb::get;
    }
}

std::string json_value_to_header_string(const boost::json::value &value)
{
    if (value.is_string()) {
        return std::string(value.as_string().c_str());
    }
    return boost::json::serialize(value);
}

bool parse_url(const std::string &url, ParsedUrl &parsed, std::string &error_message)
{
    constexpr std::string_view http_scheme = "http://";
    constexpr std::string_view https_scheme = "https://";

    size_t offset = 0;
    if (url.rfind(http_scheme, 0) == 0) {
        parsed.is_https = false;
        parsed.port = "80";
        offset = http_scheme.size();
    } else if (url.rfind(https_scheme, 0) == 0) {
        parsed.is_https = true;
        parsed.port = "443";
        offset = https_scheme.size();
    } else {
        error_message = "Unsupported URL scheme";
        return false;
    }

    auto path_pos = url.find_first_of("/?", offset);
    std::string authority = path_pos == std::string::npos ?
                            url.substr(offset) :
                            url.substr(offset, path_pos - offset);
    parsed.target = path_pos == std::string::npos ? "/" : url.substr(path_pos);
    if (authority.empty()) {
        error_message = "URL host is empty";
        return false;
    }

    if (authority.front() == '[') {
        auto end = authority.find(']');
        if (end == std::string::npos) {
            error_message = "Invalid IPv6 URL host";
            return false;
        }
        parsed.host = authority.substr(1, end - 1);
        if ((end + 1) < authority.size()) {
            if (authority[end + 1] != ':') {
                error_message = "Invalid URL authority";
                return false;
            }
            parsed.port = authority.substr(end + 2);
        }
    } else {
        auto port_pos = authority.rfind(':');
        if (port_pos != std::string::npos) {
            parsed.host = authority.substr(0, port_pos);
            parsed.port = authority.substr(port_pos + 1);
        } else {
            parsed.host = authority;
        }
    }

    if (parsed.host.empty() || parsed.port.empty()) {
        error_message = "URL host or port is empty";
        return false;
    }

    return true;
}

template<typename Operation>
void run_error_code_operation(Operation &&operation)
{
    if constexpr (std::is_void_v<std::invoke_result_t<Operation>>) {
        std::forward<Operation>(operation)();
    } else {
        [[maybe_unused]] auto result = std::forward<Operation>(operation)();
    }
}

std::shared_ptr<ssl::context> get_cached_tls_context(bool should_verify, beast::error_code &ec)
{
    static std::mutex context_mutex;
    static std::shared_ptr<ssl::context> verify_context;
    static std::shared_ptr<ssl::context> no_verify_context;

    std::lock_guard<std::mutex> lock(context_mutex);
    auto &context = should_verify ? verify_context : no_verify_context;
    if (context != nullptr) {
        ec = {};
        return context;
    }

    auto new_context = std::make_shared<ssl::context>(ssl::context::tls_client);
    if (should_verify) {
        run_error_code_operation([&]() {
            return new_context->set_default_verify_paths(ec);
        });
        if (ec) {
            return nullptr;
        }
    }

    context = std::move(new_context);
    ec = {};
    return context;
}

std::shared_ptr<ssl::context> create_tls_context(
    const network::HttpClientIface::Request &request, bool should_verify, beast::error_code &ec
)
{
    if (request.cert_pem.empty()) {
        return get_cached_tls_context(should_verify, ec);
    }

    auto context = std::make_shared<ssl::context>(ssl::context::tls_client);
    if (should_verify) {
        run_error_code_operation([&]() {
            return context->add_certificate_authority(
                       asio::buffer(request.cert_pem.data(), request.cert_pem.size()), ec
                   );
        });
        if (ec) {
            return nullptr;
        }
    }

    ec = {};
    return context;
}

class LinuxHttpTransaction: public network::HttpClientIface::Transaction {
public:
    LinuxHttpTransaction() = default;
    ~LinuxHttpTransaction() override
    {
        cancel();
    }

    ErrorCode open(const Request &request, Response &response, uint32_t &content_length) override;
    int read(char *buffer, size_t size, std::string &error_message) override;
    bool is_complete() const override;
    void cancel() override;

private:
    using SslStream = ssl::stream<beast::tcp_stream>;

    ErrorCode perform_http(
        const ParsedUrl &url, const Request &request, http::request<http::string_body> &request_message,
        Response &response
    );
    ErrorCode perform_https(
        const ParsedUrl &url, const Request &request, http::request<http::string_body> &request_message,
        Response &response
    );
    template<typename Stream>
    ErrorCode write_and_read_response(
        Stream &stream, const Request &request, http::request<http::string_body> &request_message,
        Response &response
    );
    void fill_response(const http::response<http::dynamic_body> &message, Response &response);
    void throttle_after_read(size_t copy_size);

    asio::io_context io_context_;
    std::mutex stream_mutex_;
    std::shared_ptr<beast::tcp_stream> tcp_stream_;
    std::shared_ptr<ssl::context> ssl_context_;
    std::shared_ptr<SslStream> ssl_stream_;
    std::string body_;
    size_t body_offset_ = 0;
    uint64_t throttled_bytes_ = 0;
    std::chrono::steady_clock::time_point throttle_start_time_;
    std::atomic_bool canceled_{false};
    bool response_complete_ = false;
};

network::HttpClientIface::ErrorCode LinuxHttpTransaction::open(
    const Request &request, Response &response, uint32_t &content_length
)
{
    canceled_.store(false);

    ParsedUrl url;
    if (!parse_url(request.url, url, response.error_message)) {
        return ErrorCode::InvalidArgument;
    }

    http::request<http::string_body> request_message{to_beast_method(request.method), url.target, 11};
    request_message.set(http::field::host, url.host);
    request_message.set(http::field::user_agent, "ESP-Brookesia-Http-PC");
    for (const auto &header : request.headers) {
        auto value = json_value_to_header_string(header.value());
        std::string key(header.key().data(), header.key().size());
        request_message.set(key, value);
    }
    request_message.body() = request.body;
    request_message.prepare_payload();

    const auto result = url.is_https ?
                        perform_https(url, request, request_message, response) :
                        perform_http(url, request, request_message, response);
    if (result != ErrorCode::Ok) {
        return result;
    }

    body_offset_ = 0;
    throttled_bytes_ = 0;
    throttle_start_time_ = std::chrono::steady_clock::now();
    response_complete_ = true;
    content_length = static_cast<uint32_t>(
                         std::min<uint64_t>(body_.size(), std::numeric_limits<uint32_t>::max())
                     );

    return ErrorCode::Ok;
}

int LinuxHttpTransaction::read(char *buffer, size_t size, std::string &error_message)
{
    if (buffer == nullptr || size == 0) {
        error_message = "Invalid response read buffer";
        return -1;
    }

    if (body_offset_ >= body_.size()) {
        return 0;
    }

    const auto copy_size = std::min(size, body_.size() - body_offset_);
    std::copy_n(body_.data() + body_offset_, copy_size, buffer);
    body_offset_ += copy_size;
    throttle_after_read(copy_size);

    return static_cast<int>(copy_size);
}

bool LinuxHttpTransaction::is_complete() const
{
    return response_complete_ && (body_offset_ >= body_.size());
}

void LinuxHttpTransaction::cancel()
{
    canceled_.store(true);

    std::shared_ptr<beast::tcp_stream> tcp_stream;
    std::shared_ptr<SslStream> ssl_stream;
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        tcp_stream = tcp_stream_;
        ssl_stream = ssl_stream_;
    }

    beast::error_code ec;
    if (tcp_stream != nullptr) {
        tcp_stream->socket().shutdown(tcp::socket::shutdown_both, ec);
        ec = {};
        tcp_stream->socket().close(ec);
    }
    if (ssl_stream != nullptr) {
        ssl_stream->next_layer().socket().shutdown(tcp::socket::shutdown_both, ec);
        ec = {};
        ssl_stream->next_layer().socket().close(ec);
    }
}

network::HttpClientIface::ErrorCode LinuxHttpTransaction::perform_http(
    const ParsedUrl &url, const Request &request, http::request<http::string_body> &request_message,
    Response &response
)
{
    beast::error_code ec;
    tcp::resolver resolver(io_context_);
    auto results = resolver.resolve(url.host, url.port, ec);
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }

    auto stream = std::make_shared<beast::tcp_stream>(io_context_);
    stream->expires_after(std::chrono::milliseconds(request.timeout_ms));
    stream->connect(results, ec);
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        tcp_stream_ = stream;
    }

    return write_and_read_response(*stream, request, request_message, response);
}

network::HttpClientIface::ErrorCode LinuxHttpTransaction::perform_https(
    const ParsedUrl &url, const Request &request, http::request<http::string_body> &request_message,
    Response &response
)
{
    const bool should_verify = request.tls_verify == TlsVerifyMode::Default ?
                               BROOKESIA_HAL_LINUX_NETWORK_HTTP_CLIENT_REQUIRE_TLS_VERIFY :
                               request.tls_verify == TlsVerifyMode::Verify;

    beast::error_code ec;
    auto context = create_tls_context(request, should_verify, ec);
    if (context == nullptr) {
        response.error_message = ec ? ec.message() : "Failed to create TLS context";
        return ErrorCode::ClientInitFailed;
    }
    if (!should_verify) {
        context->set_verify_mode(ssl::verify_none);
    }

    tcp::resolver resolver(io_context_);
    auto results = resolver.resolve(url.host, url.port, ec);
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }

    auto stream = std::make_shared<SslStream>(io_context_, *context);
    SSL_set_tlsext_host_name(stream->native_handle(), url.host.c_str());
    if (should_verify) {
        stream->set_verify_mode(ssl::verify_peer);
        stream->set_verify_callback(ssl::host_name_verification(url.host));
    }
    beast::get_lowest_layer(*stream).expires_after(std::chrono::milliseconds(request.timeout_ms));
    beast::get_lowest_layer(*stream).connect(results, ec);
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }
    stream->handshake(ssl::stream_base::client, ec);
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }
    {
        std::lock_guard<std::mutex> lock(stream_mutex_);
        ssl_context_ = context;
        ssl_stream_ = stream;
    }

    return write_and_read_response(*stream, request, request_message, response);
}

template<typename Stream>
network::HttpClientIface::ErrorCode LinuxHttpTransaction::write_and_read_response(
    Stream &stream, const Request &request, http::request<http::string_body> &request_message, Response &response
)
{
    beast::error_code ec;
    beast::get_lowest_layer(stream).expires_after(std::chrono::milliseconds(request.timeout_ms));
    http::write(stream, request_message, ec);
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }

    beast::flat_buffer buffer;
    http::response<http::dynamic_body> message;
    if (request.method == network::HttpClientIface::Method::Head) {
        http::response_parser<http::dynamic_body> parser;
        parser.skip(true);
        http::read(stream, buffer, parser, ec);
        if (!ec) {
            message = parser.release();
        }
    } else {
        http::read(stream, buffer, message, ec);
    }
    if (ec) {
        response.error_message = ec.message();
        return ErrorCode::RequestFailed;
    }

    fill_response(message, response);
    beast::get_lowest_layer(stream).expires_never();

    return ErrorCode::Ok;
}

void LinuxHttpTransaction::fill_response(const http::response<http::dynamic_body> &message, Response &response)
{
    response.status_code = message.result_int();
    response.headers = {};
    for (const auto &header : message.base()) {
        response.headers[std::string(header.name_string())] = std::string(header.value());
    }
    body_ = beast::buffers_to_string(message.body().data());
}

void LinuxHttpTransaction::throttle_after_read(size_t copy_size)
{
    if constexpr (HTTP_CLIENT_READ_THROTTLE_BYTES_PER_SEC == 0) {
        return;
    }

    if (copy_size == 0 || canceled_.load()) {
        return;
    }

    throttled_bytes_ += copy_size;
    const auto target_elapsed = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                    std::chrono::duration<long double>(
                                        static_cast<long double>(throttled_bytes_) /
                                        static_cast<long double>(HTTP_CLIENT_READ_THROTTLE_BYTES_PER_SEC)
                                    )
                                );
    const auto target_time = throttle_start_time_ + target_elapsed;
    while (!canceled_.load()) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= target_time) {
            break;
        }
        const auto sleep_duration = std::min(
                                        target_time - now,
                                        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                            HTTP_CLIENT_READ_THROTTLE_SLEEP_SLICE
                                        )
                                    );
        std::this_thread::sleep_for(sleep_duration);
    }
}

} // namespace

std::shared_ptr<network::HttpClientIface::Transaction> HttpClientLinuxIface::create_transaction()
{
    std::shared_ptr<Transaction> transaction;
    BROOKESIA_CHECK_EXCEPTION_RETURN(
        transaction = std::make_shared<LinuxHttpTransaction>(), nullptr, "Failed to create HTTP transaction"
    );
    last_error_.clear();
    return transaction;
}

} // namespace esp_brookesia::hal

#endif // BROOKESIA_HAL_LINUX_NETWORK_ENABLE_HTTP_CLIENT_IMPL
