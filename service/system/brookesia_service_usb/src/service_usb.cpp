/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_set>
#include <utility>
#include <vector>

#include "boost/json.hpp"
#include "transport.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "mbedtls/md.h"
#include "esp_log_write.h"
#include "unistd.h"

#include "brookesia/lib_utils/plugin.hpp"
#include "brookesia/service_usb/macro_configs.h"
#include "brookesia/service_usb/service_usb.hpp"
#include "private/utils.hpp"
#include "protocol.hpp"

namespace esp_brookesia::service {

namespace {

using Helper = Usb::Helper;
using FrameType = usb_internal::FrameType;
using BinaryFrame = usb_internal::BinaryFrame;

constexpr size_t COMMAND_BUFFER_LIMIT = 4096;
constexpr size_t TRANSPORT_READ_SIZE = 512;
constexpr size_t TRANSPORT_READ_BUDGET = 8;

struct RxMessage {
    size_t size = 0;
    uint8_t data[TRANSPORT_READ_SIZE] = {};
};

std::optional<std::string> get_string(const boost::json::object &object, std::string_view name)
{
    const auto *value = object.if_contains(name);
    if (value == nullptr || !value->is_string()) {
        return std::nullopt;
    }
    return std::string(value->as_string());
}

std::optional<uint64_t> get_unsigned(const boost::json::object &object, std::string_view name)
{
    const auto *value = object.if_contains(name);
    if (value == nullptr) {
        return std::nullopt;
    }
    if (value->is_uint64()) {
        return value->as_uint64();
    }
    if (value->is_int64() && value->as_int64() >= 0) {
        return static_cast<uint64_t>(value->as_int64());
    }
    if (value->is_double()) {
        const double number = value->as_double();
        const double max_exclusive = std::ldexp(1.0, std::numeric_limits<uint64_t>::digits);
        if (std::isfinite(number) && number >= 0 && number < max_exclusive && number == std::floor(number)) {
            return static_cast<uint64_t>(number);
        }
    }
    return std::nullopt;
}

bool get_bool_or_default(const boost::json::object &object, std::string_view name, bool default_value)
{
    const auto *value = object.if_contains(name);
    if (value == nullptr || !value->is_bool()) {
        return default_value;
    }
    return value->as_bool();
}

bool is_hex_sha256(std::string_view value)
{
    if (value.size() != 64) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char ch) {
        return std::isxdigit(static_cast<unsigned char>(ch)) != 0;
    });
}

std::string lower_copy(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool path_is_same_or_child(const std::filesystem::path &path, const std::filesystem::path &root)
{
    const auto normalized_path = path.lexically_normal();
    const auto normalized_root = root.lexically_normal();
    auto path_it = normalized_path.begin();
    for (auto root_it = normalized_root.begin(); root_it != normalized_root.end(); ++root_it, ++path_it) {
        if (path_it == normalized_path.end() || *path_it != *root_it) {
            return false;
        }
    }
    return true;
}

bool path_has_parent_reference(const std::filesystem::path &path)
{
    return std::any_of(path.begin(), path.end(), [](const auto & part) {
        return part == "..";
    });
}

bool path_contains_symlink(const std::filesystem::path &path, const std::filesystem::path &root)
{
    (void)root;
    std::error_code error_code;
    auto current = path.root_path();
    for (const auto &part : path.relative_path()) {
        current /= part;
        const auto status = std::filesystem::symlink_status(current, error_code);
        if (error_code) {
            error_code.clear();
            continue;
        }
        if (std::filesystem::is_symlink(status)) {
            return true;
        }
    }
    return false;
}

std::expected<std::filesystem::path, std::string> validate_upload_path(std::string_view remote_path)
{
    if (remote_path.empty()) {
        return std::unexpected("path is empty");
    }
    const std::filesystem::path relative(remote_path);
    if (relative.is_absolute() || path_has_parent_reference(relative)) {
        return std::unexpected("path must be relative and must not contain '..'");
    }

    const std::filesystem::path root(BROOKESIA_SERVICE_USB_UPLOAD_ROOT);
    const auto candidate = (root / relative).lexically_normal();
    if (!path_is_same_or_child(candidate, root)) {
        return std::unexpected("path escapes upload root");
    }
    if (path_contains_symlink(candidate, root)) {
        return std::unexpected("path contains a symbolic link");
    }
    return candidate;
}

boost::json::object make_response(std::string_view op, uint32_t request_id, bool ok)
{
    return {
        {"version", BROOKESIA_SERVICE_USB_PROTOCOL_VERSION},
        {"op", op},
        {"request_id", request_id},
        {"ok", ok},
    };
}

} // namespace

class Usb::Impl {
public:
    explicit Impl(Usb &owner)
        : owner_(owner)
        , transport_(usb_internal::make_transport())
    {
    }

    ~Impl()
    {
        stop();
    }

    bool start(std::shared_ptr<lib_utils::TaskScheduler> scheduler)
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_NULL_RETURN(scheduler, false, "Task scheduler is unavailable");

        rx_queue_ = xQueueCreate(BROOKESIA_SERVICE_USB_RX_QUEUE_LENGTH, sizeof(RxMessage));
        BROOKESIA_CHECK_NULL_RETURN(rx_queue_, false, "Failed to create USB RX queue");

        if (auto err = transport_->install(); err != ESP_OK) {
            BROOKESIA_LOGE("Failed to install transport: %s", esp_err_to_name(err));
            stop();
            return false;
        }

        scheduler_ = std::move(scheduler);
        auto pump_task = [this]() -> bool {
            pump_rx_queue();
            return true;
        };
        if (!scheduler_->post_periodic(
                    std::move(pump_task),
                    1,
                    &pump_task_id_,
                    owner_.get_call_task_group()
                )) {
            BROOKESIA_LOGE("Failed to schedule USB RX pump");
            stop();
            return false;
        }

        {
            std::lock_guard lock(mutex_);
            status_.port_state = Helper::PortState::Ready;
            status_.serial_jtag_connected = transport_->is_connected();
            status_.session_state = Helper::SessionState::Idle;
            status_.logs_suppressed = false;
            status_.max_frame_payload = BROOKESIA_SERVICE_USB_FRAME_PAYLOAD_SIZE;
            status_.transport = transport_->name();
        }
        publish_port_state();
        BROOKESIA_LOGI("USB service ready");
        return true;
    }

    void stop()
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        if (scheduler_ && pump_task_id_ != 0) {
            scheduler_->cancel(pump_task_id_);
        }
        pump_task_id_ = 0;
        scheduler_.reset();

        (void)abort_transfer_internal(0, "service stopped", false);
        if (rx_queue_ != nullptr) {
            vQueueDelete(rx_queue_);
            rx_queue_ = nullptr;
        }

        transport_->uninstall();

        release_log_suppression();
        {
            std::lock_guard lock(mutex_);
            status_.port_state = Helper::PortState::Disabled;
            status_.serial_jtag_connected = false;
            status_.session_state = Helper::SessionState::Idle;
            status_.logs_suppressed = false;
            status_.active_request_id = 0;
            transfer_status_ = {};
        }
    }

    bool register_bridge(Usb::HostCommandHandler handler)
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_RETURN(static_cast<bool>(handler), false, "Host command bridge is empty");
        std::lock_guard lock(mutex_);
        bridge_ = std::move(handler);
        return true;
    }

    bool register_service_call_bridge(Usb::ServiceCallHandler handler)
    {
        BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
        BROOKESIA_CHECK_FALSE_RETURN(static_cast<bool>(handler), false, "Service call bridge is empty");
        std::lock_guard lock(mutex_);
        service_call_bridge_ = std::move(handler);
        return true;
    }

    void clear_bridge()
    {
        std::lock_guard lock(mutex_);
        bridge_ = {};
    }

    void clear_service_call_bridge()
    {
        std::lock_guard lock(mutex_);
        service_call_bridge_ = {};
    }

    void mark_starting()
    {
        std::lock_guard lock(mutex_);
        status_.port_state = Helper::PortState::Starting;
    }

    Helper::Status get_status() const
    {
        std::lock_guard lock(mutex_);
        return status_;
    }

    Helper::TransferStatus get_transfer_status() const
    {
        std::lock_guard lock(mutex_);
        return transfer_status_;
    }

    std::expected<void, std::string> abort_transfer(uint32_t request_id)
    {
        return abort_transfer_internal(request_id, "aborted", true);
    }

private:
    struct Transfer {
        Helper::HostCommand command = Helper::HostCommand::PutFile;
        uint32_t request_id = 0;
        uint64_t expected_size = 0;
        uint64_t received_size = 0;
        uint32_t expected_sequence = 0;
        bool overwrite = false;
        std::string destination;
        std::string expected_sha256;
        std::string temporary_path;
        int file_descriptor = -1;
        mbedtls_md_context_t sha256_context;
        std::chrono::steady_clock::time_point last_activity = std::chrono::steady_clock::now();

        Transfer()
        {
            mbedtls_md_init(&sha256_context);
        }

        ~Transfer()
        {
            if (file_descriptor >= 0) {
                close(file_descriptor);
            }
            mbedtls_md_free(&sha256_context);
        }

        Transfer(const Transfer &) = delete;
        Transfer &operator=(const Transfer &) = delete;
    };

    static int suppress_log_vprintf(const char *format, va_list args)
    {
        (void)format;
        (void)args;
        return 0;
    }

    bool is_control_session_active() const
    {
        std::lock_guard lock(mutex_);
        return status_.session_state == Helper::SessionState::Active;
    }

    void activate_control_session()
    {
        if (!log_suppression_active_) {
            previous_vprintf_ = esp_log_set_vprintf(&Impl::suppress_log_vprintf);
            log_suppression_active_ = true;
        }
        {
            std::lock_guard lock(mutex_);
            status_.session_state = Helper::SessionState::Active;
            status_.logs_suppressed = true;
        }
        publish_port_state();
    }

    void release_log_suppression()
    {
        if (!log_suppression_active_) {
            return;
        }
        (void)esp_log_set_vprintf(previous_vprintf_);
        previous_vprintf_ = nullptr;
        log_suppression_active_ = false;
    }

    void release_control_session()
    {
        release_log_suppression();
        {
            std::lock_guard lock(mutex_);
            status_.session_state = Helper::SessionState::Idle;
            status_.logs_suppressed = false;
        }
        publish_port_state();
    }

    void poll_transport()
    {
        if (rx_queue_ == nullptr) {
            return;
        }
        for (size_t read_count = 0; read_count < TRANSPORT_READ_BUDGET; ++read_count) {
            RxMessage message;
            const int rx_size = transport_->read(message.data, sizeof(message.data));
            if (rx_size <= 0) {
                return;
            }
            message.size = static_cast<size_t>(rx_size);
            if (xQueueSend(rx_queue_, &message, 0) != pdTRUE) {
                if (transfer_) {
                    fail_transfer("bad_frame", "Transport RX queue is full");
                }
                return;
            }
        }
    }

    void update_connection_state()
    {
        const bool connected = transport_->is_connected();
        bool changed = false;
        {
            std::lock_guard lock(mutex_);
            changed = status_.serial_jtag_connected != connected;
            status_.serial_jtag_connected = connected;
            if (!connected && status_.session_state == Helper::SessionState::Active) {
                disconnect_pending_ = true;
            }
        }
        if (changed) {
            publish_port_state();
        }
    }

    void pump_rx_queue()
    {
        update_connection_state();
        bool disconnected = false;
        {
            std::lock_guard lock(mutex_);
            disconnected = disconnect_pending_;
            disconnect_pending_ = false;
        }
        if (disconnected) {
            (void)abort_transfer_internal(0, "USB control port disconnected", false);
            request_ids_.clear();
            rx_buffer_.clear();
            frame_parser_.clear();
            release_control_session();
        }
        check_transfer_timeout();
        check_session_timeout();
        poll_transport();
        if (rx_queue_ == nullptr) {
            return;
        }
        RxMessage message;
        while (xQueueReceive(rx_queue_, &message, 0) == pdTRUE) {
            rx_buffer_.insert(rx_buffer_.end(), message.data, message.data + message.size);
            process_rx_buffer();
        }
    }

    void check_transfer_timeout()
    {
        if (!transfer_) {
            return;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - transfer_->last_activity
                             ).count();
        if (elapsed > BROOKESIA_SERVICE_USB_COMMAND_TIMEOUT_MS) {
            fail_transfer("timeout", "USB transfer timed out");
            request_ids_.clear();
            rx_buffer_.clear();
            frame_parser_.clear();
            release_control_session();
        }
    }

    void check_session_timeout()
    {
        if (!is_control_session_active()) {
            return;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - session_last_activity_
                             ).count();
        if (elapsed <= BROOKESIA_SERVICE_USB_COMMAND_TIMEOUT_MS) {
            return;
        }
        if (transfer_) {
            fail_transfer("timeout", "USB control session timed out");
        } else {
            send_error(0, "timeout", "USB control session timed out");
        }
        request_ids_.clear();
        rx_buffer_.clear();
        frame_parser_.clear();
        release_control_session();
    }

    void process_rx_buffer()
    {
        if (transfer_) {
            // Keep a small out-of-band escape hatch for the standalone
            // `brookesia-usb abort` command. Binary payloads are never
            // inspected as text while a partial frame is buffered.
            if (frame_parser_.empty() && !rx_buffer_.empty() && rx_buffer_.front() == '{') {
                const auto line_end = std::find(
                                          rx_buffer_.begin(), rx_buffer_.end(), static_cast<uint8_t>('\n')
                                      );
                if (line_end != rx_buffer_.end() &&
                        static_cast<size_t>(std::distance(rx_buffer_.begin(), line_end)) <= COMMAND_BUFFER_LIMIT) {
                    std::string line(rx_buffer_.begin(), line_end);
                    auto command = usb_internal::parse_command_line(line);
                    const auto operation = command ? get_string(*command, "op") : std::nullopt;
                    if (operation && *operation == "abort") {
                        rx_buffer_.erase(rx_buffer_.begin(), line_end + 1);
                        handle_command(*command);
                        return;
                    }
                }
            }
            frame_parser_.append(rx_buffer_);
            rx_buffer_.clear();
            while (transfer_) {
                auto frame_result = frame_parser_.next(BROOKESIA_SERVICE_USB_FRAME_PAYLOAD_SIZE);
                if (!frame_result) {
                    fail_transfer("bad_frame", frame_result.error());
                    return;
                }
                if (!frame_result->has_value()) {
                    return;
                }
                handle_frame(frame_result->value());
            }
            return;
        }

        while (true) {
            const auto line_end = std::find(rx_buffer_.begin(), rx_buffer_.end(), static_cast<uint8_t>('\n'));
            if (line_end == rx_buffer_.end()) {
                if (rx_buffer_.size() > COMMAND_BUFFER_LIMIT) {
                    if (transport_->name() != std::string_view("uart")) {
                        send_error(0, "invalid_command", "command line is too long");
                    }
                    rx_buffer_.clear();
                }
                return;
            }
            std::string line(rx_buffer_.begin(), line_end);
            rx_buffer_.erase(rx_buffer_.begin(), line_end + 1);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            auto command = usb_internal::parse_command_line(line);
            if (!command) {
                if (transport_->name() == std::string_view("uart")) {
                    continue;
                }
                send_error(0, "invalid_command", command.error());
                continue;
            }
            handle_command(*command);
            if (transfer_) {
                if (!rx_buffer_.empty()) {
                    frame_parser_.append(rx_buffer_);
                    rx_buffer_.clear();
                }
                return;
            }
        }
    }

    void handle_command(const boost::json::object &command)
    {
        const auto request_id = get_unsigned(command, "request_id");
        if (!request_id || *request_id > UINT32_MAX) {
            send_error(0, "invalid_command", "request_id is required");
            return;
        }
        const auto op = get_string(command, "op");
        if (!op) {
            send_error(*request_id, "invalid_command", "op is required");
            return;
        }
        const auto version = get_unsigned(command, "version");
        if (!version || *version != BROOKESIA_SERVICE_USB_PROTOCOL_VERSION) {
            send_error(static_cast<uint32_t>(*request_id), "invalid_command", "unsupported protocol version");
            return;
        }
        if (*op != "abort" && !request_ids_.insert(static_cast<uint32_t>(*request_id)).second) {
            send_error(static_cast<uint32_t>(*request_id), "invalid_command", "request_id has already been used");
            return;
        }

        if (*op == "hello") {
            if (is_control_session_active()) {
                send_error(static_cast<uint32_t>(*request_id), "busy", "a control session is already active");
                return;
            }
            activate_control_session();
            session_last_activity_ = std::chrono::steady_clock::now();
            auto response = make_response("hello", *request_id, true);
            response["protocol_version"] = BROOKESIA_SERVICE_USB_PROTOCOL_VERSION;
            response["transport"] = transport_->name();
            response["session"] = BROOKESIA_SERVICE_USB_SESSION_MODE;
            response["control_cdc"] = 0;
            response["max_frame_payload"] = BROOKESIA_SERVICE_USB_FRAME_PAYLOAD_SIZE;
            send_json(response);
            return;
        }
        if (*op == "abort") {
            auto result = abort_transfer(static_cast<uint32_t>(*request_id));
            if (!result) {
                send_error(static_cast<uint32_t>(*request_id), "aborted", result.error());
            }
            return;
        }
        if (!is_control_session_active()) {
            send_error(static_cast<uint32_t>(*request_id), "invalid_command", "hello is required first");
            return;
        }
        session_last_activity_ = std::chrono::steady_clock::now();
        if (*op == "status") {
            auto response = make_response("status", *request_id, true);
            response["status"] = BROOKESIA_DESCRIBE_TO_JSON(get_status());
            response["transfer"] = BROOKESIA_DESCRIBE_TO_JSON(get_transfer_status());
            send_json(response);
            return;
        }
        if (*op == "goodbye") {
            (void)abort_transfer_internal(0, "control session ended", false);
            auto response = make_response("done", static_cast<uint32_t>(*request_id), true);
            response["command"] = "goodbye";
            send_json(response);
            request_ids_.clear();
            rx_buffer_.clear();
            frame_parser_.clear();
            release_control_session();
            return;
        }
        if (*op == "call") {
            handle_service_call(*request_id, command);
            return;
        }
        if (*op == "put" || *op == "install_bpk") {
            begin_transfer(
                static_cast<uint32_t>(*request_id),
                *op == "put" ? Helper::HostCommand::PutFile : Helper::HostCommand::InstallBpk,
                command
            );
            return;
        }
        send_error(static_cast<uint32_t>(*request_id), "invalid_command", "unsupported operation");
    }

    void handle_service_call(uint64_t request_id, const boost::json::object &command)
    {
        const auto service_name = get_string(command, "service");
        const auto function_name = get_string(command, "function");
        const auto *args_value = command.if_contains("args");
        if (!service_name || !function_name || args_value == nullptr || !args_value->is_object()) {
            send_error(
                static_cast<uint32_t>(request_id),
                "invalid_command",
                "call requires service, function and args"
            );
            return;
        }
        if (*service_name == Helper::get_name()) {
            send_error(static_cast<uint32_t>(request_id), "invalid_command", "recursive Usb calls are not allowed");
            return;
        }
        uint32_t timeout_ms = BROOKESIA_SERVICE_USB_COMMAND_TIMEOUT_MS;
        if (const auto timeout = get_unsigned(command, "timeout_ms"); timeout && *timeout <= UINT32_MAX) {
            timeout_ms = static_cast<uint32_t>(*timeout);
        }
        Usb::ServiceCallHandler service_call_bridge;
        {
            std::lock_guard lock(mutex_);
            service_call_bridge = service_call_bridge_;
        }
        if (!service_call_bridge) {
            send_error(static_cast<uint32_t>(request_id), "path_denied", "service call bridge is not registered");
            return;
        }
        auto result = service_call_bridge(*service_name, *function_name, args_value->as_object(), timeout_ms);
        auto response = make_response("call", static_cast<uint32_t>(request_id), result.has_value());
        if (result) {
            response["result"] = *result;
        } else {
            response["error_code"] = result.error().code;
            response["error"] = result.error().message;
        }
        send_json(response);
    }

    void begin_transfer(uint32_t request_id, Helper::HostCommand command, const boost::json::object &params)
    {
        if (transfer_) {
            send_error(request_id, "busy", "another transfer is active");
            return;
        }
        const auto size = get_unsigned(params, "size");
        const auto sha256 = get_string(params, "sha256");
        if (!size || !sha256 || !is_hex_sha256(*sha256)) {
            send_error(request_id, "invalid_command", "size or sha256 is invalid");
            return;
        }
        if (*size > BROOKESIA_SERVICE_USB_MAX_TRANSFER_SIZE) {
            send_error(request_id, "size_limit", "transfer exceeds the configured maximum size");
            return;
        }

        auto transfer = std::make_unique<Transfer>();
        transfer->command = command;
        transfer->request_id = request_id;
        transfer->expected_size = *size;
        transfer->expected_sha256 = lower_copy(*sha256);

        if (command == Helper::HostCommand::PutFile) {
            const auto remote_path = get_string(params, "path");
            if (!remote_path) {
                send_error(request_id, "invalid_command", "put requires path");
                return;
            }
            const auto target = validate_upload_path(*remote_path);
            if (!target) {
                send_error(request_id, "path_denied", target.error());
                return;
            }
            transfer->destination = target->generic_string();
            transfer->overwrite = get_bool_or_default(params, "overwrite", false);
            std::error_code error_code;
            if (!transfer->overwrite) {
                const bool target_exists = std::filesystem::exists(*target, error_code);
                if (error_code) {
                    send_error(request_id, "path_denied", "failed to inspect upload destination");
                    return;
                }
                if (target_exists) {
                    send_error(request_id, "path_denied", "destination already exists");
                    return;
                }
            }
        }

        const std::filesystem::path root(BROOKESIA_SERVICE_USB_UPLOAD_ROOT);
        const std::filesystem::path temp_directory = root / ".brookesia_usb";
        std::error_code error_code;
        std::filesystem::create_directories(temp_directory, error_code);
        if (error_code) {
            send_error(request_id, "storage_full", "failed to create USB temporary directory");
            return;
        }
        transfer->temporary_path = (temp_directory / ("transfer_" + std::to_string(request_id) + ".part")).string();
        transfer->file_descriptor = open(
                                        transfer->temporary_path.c_str(),
                                        O_WRONLY | O_CREAT | O_TRUNC,
                                        0600
                                    );
        if (transfer->file_descriptor < 0) {
            send_error(request_id, "storage_full", std::strerror(errno));
            return;
        }
        const auto *sha256_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
        if (sha256_info == nullptr ||
                mbedtls_md_setup(&transfer->sha256_context, sha256_info, 0) != 0 ||
                mbedtls_md_starts(&transfer->sha256_context) != 0) {
            std::error_code cleanup_error;
            std::filesystem::remove(transfer->temporary_path, cleanup_error);
            if (cleanup_error) {
                BROOKESIA_LOGW(
                    "Failed to remove temporary USB transfer file: path(%1%), error(%2%)",
                    transfer->temporary_path,
                    cleanup_error.message()
                );
            }
            send_error(request_id, "storage_full", "failed to initialize SHA-256");
            return;
        }

        transfer_status_ = {
            .state = Helper::TransferState::Receiving,
            .request_id = request_id,
            .received_bytes = 0,
            .total_bytes = *size,
            .error = {},
        };
        status_.port_state = Helper::PortState::Busy;
        status_.active_request_id = request_id;
        transfer_ = std::move(transfer);
        frame_parser_.clear();

        auto response = make_response("ready", request_id, true);
        response["max_frame_payload"] = BROOKESIA_SERVICE_USB_FRAME_PAYLOAD_SIZE;
        send_json(response);
        publish_port_state();
        publish_transfer_progress();
    }

    void handle_frame(const BinaryFrame &frame)
    {
        if (!transfer_ || frame.request_id != transfer_->request_id) {
            send_error(frame.request_id, "bad_frame", "request id does not match active transfer");
            return;
        }
        session_last_activity_ = std::chrono::steady_clock::now();
        if (frame.type == FrameType::Cancel) {
            (void)abort_transfer_internal(frame.request_id, "cancelled by host", true);
            return;
        }
        transfer_->last_activity = std::chrono::steady_clock::now();
        if (frame.sequence < transfer_->expected_sequence) {
            send_ack(frame.request_id, frame.sequence);
            return;
        }
        if (frame.sequence != transfer_->expected_sequence) {
            fail_transfer("bad_frame", "unexpected frame sequence");
            return;
        }
        if (frame.type == FrameType::Data) {
            if (!frame.payload.empty()) {
                const auto *data = frame.payload.data();
                size_t remaining = frame.payload.size();
                while (remaining > 0) {
                    const ssize_t written = write(transfer_->file_descriptor, data, remaining);
                    if (written <= 0) {
                        fail_transfer("storage_full", std::strerror(errno));
                        return;
                    }
                    data += written;
                    remaining -= static_cast<size_t>(written);
                }
                if (mbedtls_md_update(
                            &transfer_->sha256_context,
                            frame.payload.data(),
                            frame.payload.size()
                        ) != 0) {
                    fail_transfer("storage_full", "failed to update SHA-256");
                    return;
                }
                transfer_->received_size += frame.payload.size();
                if (transfer_->received_size > transfer_->expected_size) {
                    fail_transfer("size_mismatch", "received more bytes than declared");
                    return;
                }
            }
            ++transfer_->expected_sequence;
            send_ack(frame.request_id, frame.sequence);
            update_transfer_progress();
            return;
        }
        if (frame.type == FrameType::End) {
            finish_transfer(frame);
        }
    }

    void finish_transfer(const BinaryFrame &frame)
    {
        if (frame.sequence != transfer_->expected_sequence) {
            fail_transfer("bad_frame", "unexpected end frame sequence");
            return;
        }
        if (transfer_->received_size != transfer_->expected_size) {
            fail_transfer("size_mismatch", "received byte count does not match declared size");
            return;
        }
        std::array<uint8_t, 32> digest = {};
        if (mbedtls_md_finish(&transfer_->sha256_context, digest.data()) != 0) {
            fail_transfer("hash_mismatch", "failed to finalize SHA-256");
            return;
        }
        const auto actual_sha256 = usb_internal::sha256_to_hex(digest);
        if (actual_sha256 != transfer_->expected_sha256) {
            fail_transfer("hash_mismatch", "SHA-256 does not match declared digest");
            return;
        }
        close(transfer_->file_descriptor);
        transfer_->file_descriptor = -1;
        transfer_status_.state = Helper::TransferState::Handling;
        publish_transfer_progress();

        Usb::HostCommandHandler bridge;
        {
            std::lock_guard lock(mutex_);
            bridge = bridge_;
        }
        if (!bridge) {
            fail_transfer("install_failed", "system host command bridge is not registered");
            return;
        }

        const auto command = transfer_->command;
        const auto destination = transfer_->destination;
        const auto request_id = transfer_->request_id;
        const Helper::TransferArtifact artifact{
            .command = command,
            .request_id = transfer_->request_id,
            .temporary_path = transfer_->temporary_path,
            .size = transfer_->expected_size,
            .sha256 = actual_sha256,
            .overwrite = transfer_->overwrite,
        };
        auto result = bridge(command, artifact, destination);
        if (!result) {
            fail_transfer("install_failed", result.error());
            return;
        }

        std::error_code error_code;
        std::filesystem::remove(transfer_->temporary_path, error_code);
        transfer_status_.state = Helper::TransferState::Completed;
        transfer_status_.error.clear();
        publish_transfer_finished();
        auto response = make_response("done", request_id, true);
        response["size"] = transfer_->expected_size;
        response["sha256"] = actual_sha256;
        send_json(response);
        transfer_.reset();
        status_.active_request_id = 0;
        status_.port_state = Helper::PortState::Ready;
        publish_port_state();
        (void)frame;
    }

    std::expected<void, std::string> abort_transfer_internal(
        uint32_t request_id, std::string_view reason, bool notify_host
    )
    {
        if (!transfer_) {
            if (request_id == 0) {
                return {};
            }
            return std::unexpected("no active transfer");
        }
        if (request_id != 0 && request_id != transfer_->request_id) {
            return std::unexpected("request id does not match active transfer");
        }
        const auto active_request_id = transfer_->request_id;
        std::error_code error_code;
        if (transfer_->file_descriptor >= 0) {
            close(transfer_->file_descriptor);
            transfer_->file_descriptor = -1;
        }
        std::filesystem::remove(transfer_->temporary_path, error_code);
        transfer_status_.state = Helper::TransferState::Aborted;
        transfer_status_.error = std::string(reason);
        transfer_.reset();
        status_.active_request_id = 0;
        status_.port_state = Helper::PortState::Ready;
        frame_parser_.clear();
        rx_buffer_.clear();
        publish_transfer_finished();
        publish_port_state();
        if (notify_host) {
            send_error(active_request_id, "aborted", reason);
        }
        return {};
    }

    void fail_transfer(std::string_view error_code, std::string_view message)
    {
        const uint32_t request_id = transfer_ ? transfer_->request_id : 0;
        if (transfer_) {
            std::error_code file_error;
            if (transfer_->file_descriptor >= 0) {
                close(transfer_->file_descriptor);
                transfer_->file_descriptor = -1;
            }
            std::filesystem::remove(transfer_->temporary_path, file_error);
            transfer_status_.state = Helper::TransferState::Failed;
            transfer_status_.error = std::string(message);
            transfer_.reset();
        }
        status_.active_request_id = 0;
        status_.port_state = Helper::PortState::Ready;
        frame_parser_.clear();
        rx_buffer_.clear();
        publish_transfer_finished();
        publish_port_state();
        send_error(request_id, error_code, message);
    }

    void update_transfer_progress()
    {
        if (!transfer_) {
            return;
        }
        transfer_status_.received_bytes = transfer_->received_size;
        publish_transfer_progress();
        auto response = make_response("progress", transfer_->request_id, true);
        response["received"] = transfer_->received_size;
        response["total"] = transfer_->expected_size;
        send_json(response);
    }

    void send_ack(uint32_t request_id, uint32_t sequence)
    {
        auto response = make_response("ack", request_id, true);
        response["sequence"] = sequence;
        send_json(response);
    }

    void send_error(uint32_t request_id, std::string_view error_code, std::string_view message)
    {
        auto response = make_response("error", request_id, false);
        response["error_code"] = error_code;
        response["error"] = message;
        const boost::json::object event_data = {
            {"RequestId", request_id},
            {"ErrorCode", error_code},
            {"Message", message},
        };
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::Error),
            event_data,
            false
        );
        send_json(response);
    }

    void send_json(const boost::json::object &response)
    {
        const auto serialized = boost::json::serialize(response) + "\n";
        std::lock_guard tx_lock(transport_tx_mutex_);
        size_t offset = 0;
        while (offset < serialized.size()) {
            const int written = transport_->write(
                                    reinterpret_cast<const uint8_t *>(serialized.data()) + offset,
                                    serialized.size() - offset,
                                    1000
                                );
            if (written <= 0) {
                BROOKESIA_LOGW("Transport response write failed at %1%/%2% bytes", offset, serialized.size());
                return;
            }
            offset += static_cast<size_t>(written);
        }
        transport_->wait_tx_done(1000);
    }

    void publish_port_state()
    {
        const boost::json::object event_data = {
            {"Status", BROOKESIA_DESCRIBE_TO_JSON(get_status()).as_object()}
        };
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::PortStateChanged),
            event_data,
            false
        );
    }

    void publish_transfer_progress()
    {
        const boost::json::object event_data = {
            {"Status", BROOKESIA_DESCRIBE_TO_JSON(get_transfer_status()).as_object()}
        };
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::TransferProgress),
            event_data,
            false
        );
    }

    void publish_transfer_finished()
    {
        const boost::json::object event_data = {
            {"Status", BROOKESIA_DESCRIBE_TO_JSON(get_transfer_status()).as_object()}
        };
        owner_.publish_event(
            BROOKESIA_DESCRIBE_TO_STR(Helper::EventId::TransferFinished),
            event_data,
            false
        );
    }

    Usb &owner_;
    mutable std::mutex mutex_;
    std::mutex transport_tx_mutex_;
    std::shared_ptr<lib_utils::TaskScheduler> scheduler_;
    lib_utils::TaskScheduler::TaskId pump_task_id_ = 0;
    QueueHandle_t rx_queue_ = nullptr;
    std::unique_ptr<usb_internal::Transport> transport_;
    bool log_suppression_active_ = false;
    vprintf_like_t previous_vprintf_ = nullptr;
    bool disconnect_pending_ = false;
    std::chrono::steady_clock::time_point session_last_activity_ = std::chrono::steady_clock::now();
    std::vector<uint8_t> rx_buffer_;
    usb_internal::FrameParser frame_parser_;
    std::unique_ptr<Transfer> transfer_;
    Helper::Status status_;
    Helper::TransferStatus transfer_status_;
    Usb::HostCommandHandler bridge_;
    Usb::ServiceCallHandler service_call_bridge_;
    std::unordered_set<uint32_t> request_ids_;
};

Usb &Usb::get_instance()
{
    static Usb instance;
    return instance;
}

Usb::Usb()
    : ServiceBase({
    .name = Helper::get_name().data(),
    .description = "Expose the USB control port for host commands and file transfers.",
    .version = get_component_version(),
    .dependencies = {},
    .scheduler_type = SchedulerType::Main,
})
, impl_(std::make_unique<Impl>(*this))
{
}

std::string Usb::get_component_version()
{
    return make_version(
               BROOKESIA_SERVICE_USB_VER_MAJOR,
               BROOKESIA_SERVICE_USB_VER_MINOR,
               BROOKESIA_SERVICE_USB_VER_PATCH
           );
}

bool Usb::register_host_command_bridge(HostCommandHandler handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    return impl_->register_bridge(std::move(handler));
}

bool Usb::register_service_call_bridge(ServiceCallHandler handler)
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    return impl_->register_service_call_bridge(std::move(handler));
}

void Usb::clear_host_command_bridge()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    impl_->clear_bridge();
}

void Usb::clear_service_call_bridge()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    impl_->clear_service_call_bridge();
}

bool Usb::on_init()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    BROOKESIA_LOGI(
        "Version: %1%.%2%.%3%",
        BROOKESIA_SERVICE_USB_VER_MAJOR,
        BROOKESIA_SERVICE_USB_VER_MINOR,
        BROOKESIA_SERVICE_USB_VER_PATCH
    );
    return true;
}

void Usb::on_deinit()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    impl_->stop();
    BROOKESIA_LOGI("Deinitialized");
}

bool Usb::on_start()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    impl_->mark_starting();
    return impl_->start(get_task_scheduler());
}

void Usb::on_stop()
{
    BROOKESIA_LOG_TRACE_GUARD_WITH_THIS();
    impl_->stop();
}

std::vector<FunctionSchema> Usb::get_function_schemas()
{
    const auto schemas = Helper::get_function_schemas();
    return {schemas.begin(), schemas.end()};
}

std::vector<EventSchema> Usb::get_event_schemas()
{
    const auto schemas = Helper::get_event_schemas();
    return {schemas.begin(), schemas.end()};
}

ServiceBase::FunctionHandlerMap Usb::get_function_handlers()
{
    return {
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetStatus,
            function_get_status()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_0(
            Helper, Helper::FunctionId::GetTransferStatus,
            function_get_transfer_status()
        ),
        BROOKESIA_SERVICE_HELPER_FUNC_HANDLER_1(
            Helper, Helper::FunctionId::AbortTransfer, double,
            function_abort_transfer(static_cast<uint32_t>(PARAM))
        ),
    };
}

std::expected<boost::json::object, std::string> Usb::function_get_status()
{
    return BROOKESIA_DESCRIBE_TO_JSON(impl_->get_status()).as_object();
}

std::expected<boost::json::object, std::string> Usb::function_get_transfer_status()
{
    return BROOKESIA_DESCRIBE_TO_JSON(impl_->get_transfer_status()).as_object();
}

std::expected<void, std::string> Usb::function_abort_transfer(uint32_t request_id)
{
    return impl_->abort_transfer(request_id);
}

#if BROOKESIA_SERVICE_USB_ENABLE_AUTO_REGISTER
BROOKESIA_PLUGIN_REGISTER_SINGLETON_WITH_SYMBOL(
    ServiceBase, Usb, Usb::get_instance().get_attributes().name, Usb::get_instance(),
    BROOKESIA_SERVICE_USB_PLUGIN_SYMBOL
);
#endif

} // namespace esp_brookesia::service
