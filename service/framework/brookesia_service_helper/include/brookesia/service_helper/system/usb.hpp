/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <cstdint>
#include <array>
#include <expected>
#include <span>
#include <string>
#include <string_view>

#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema and public data types for the USB host-control service.
 */
class Usb : public Base<Usb> {
public:
    enum class FunctionId {
        GetStatus,
        GetTransferStatus,
        AbortTransfer,
        Max,
    };

    enum class EventId {
        PortStateChanged,
        TransferProgress,
        TransferFinished,
        Error,
        Max,
    };

    enum class PortState {
        Disabled,
        Starting,
        Ready,
        Busy,
        Error,
        Max,
    };

    enum class SessionState {
        Idle,
        Active,
        Max,
    };

    enum class TransferState {
        Idle,
        Receiving,
        Verifying,
        Handling,
        Completed,
        Failed,
        Aborted,
        Max,
    };

    enum class HostCommand {
        PutFile,
        InstallBpk,
        Max,
    };

    struct Status {
        PortState port_state = PortState::Disabled;
        bool serial_jtag_connected = false;
        std::string transport;
        SessionState session_state = SessionState::Idle;
        bool logs_suppressed = false;
        uint32_t active_request_id = 0;
        uint32_t max_frame_payload = 0;
    };

    struct TransferStatus {
        TransferState state = TransferState::Idle;
        uint32_t request_id = 0;
        uint64_t received_bytes = 0;
        uint64_t total_bytes = 0;
        std::string error;
    };

    struct TransferArtifact {
        HostCommand command = HostCommand::PutFile;
        uint32_t request_id = 0;
        std::string temporary_path;
        uint64_t size = 0;
        std::string sha256;
        bool overwrite = false;
    };

    static constexpr std::string_view get_name()
    {
        return "Usb";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, static_cast<size_t>(FunctionId::Max)> schemas = {{
                {
                    .name = "GetStatus",
                    .description = "Get USB Serial/JTAG control-port status.",
                    .return_value = FunctionReturnSchema{
                        .type = FunctionValueType::Object,
                        .description = "USB port status.",
                    },
                },
                {
                    .name = "GetTransferStatus",
                    .description = "Get the active Serial/JTAG transfer status.",
                    .return_value = FunctionReturnSchema{
                        .type = FunctionValueType::Object,
                        .description = "Transfer status.",
                    },
                },
                {
                    .name = "AbortTransfer",
                    .description = "Abort the active Serial/JTAG transfer.",
                    .parameters = {
                        FunctionParameterSchema{
                            .name = "RequestId",
                            .description = "Active request identifier.",
                            .type = FunctionValueType::Number,
                        },
                    },
                },
            }
        };
        return schemas;
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, static_cast<size_t>(EventId::Max)> schemas = {{
                {
                    .name = "PortStateChanged",
                    .description = "The Serial/JTAG control port changed state.",
                    .items = {
                        {"Status", "Current port status.", EventItemType::Object},
                    },
                },
                {
                    .name = "TransferProgress",
                    .description = "A Serial/JTAG transfer made progress.",
                    .items = {
                        {"Status", "Current transfer status.", EventItemType::Object},
                    },
                },
                {
                    .name = "TransferFinished",
                    .description = "A Serial/JTAG transfer completed or failed.",
                    .items = {
                        {"Status", "Final transfer status.", EventItemType::Object},
                    },
                },
                {
                    .name = "Error",
                    .description = "The USB control service reported an error.",
                    .items = {
                        {"RequestId", "Related request identifier.", EventItemType::Number},
                        {"ErrorCode", "Machine-readable error code.", EventItemType::String},
                        {"Message", "Error description.", EventItemType::String},
                    },
                },
            }
        };
        return schemas;
    }

    static std::expected<Status, std::string> get_status(uint32_t timeout_ms = 0)
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Usb service");
        }
        auto result = call_function_sync<boost::json::object>(FunctionId::GetStatus, Timeout(timeout_ms));
        if (!result) {
            return std::unexpected(result.error());
        }
        Status status;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(*result, status)) {
            return std::unexpected("Failed to parse Usb service status");
        }
        return status;
    }

    static std::expected<TransferStatus, std::string> get_transfer_status(uint32_t timeout_ms = 0)
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Usb service");
        }
        auto result = call_function_sync<boost::json::object>(FunctionId::GetTransferStatus, Timeout(timeout_ms));
        if (!result) {
            return std::unexpected(result.error());
        }
        TransferStatus status;
        if (!BROOKESIA_DESCRIBE_FROM_JSON(*result, status)) {
            return std::unexpected("Failed to parse Usb transfer status");
        }
        return status;
    }

    static std::expected<void, std::string> abort_transfer(uint32_t request_id, uint32_t timeout_ms = 0)
    {
        auto binding = ServiceManager::get_instance().bind(get_name().data());
        if (!binding.is_valid()) {
            return std::unexpected("Failed to bind Usb service");
        }
        return call_function_sync<void>(FunctionId::AbortTransfer, request_id, Timeout(timeout_ms));
    }
};

BROOKESIA_DESCRIBE_ENUM(Usb::FunctionId, GetStatus, GetTransferStatus, AbortTransfer, Max)
BROOKESIA_DESCRIBE_ENUM(
    Usb::EventId, PortStateChanged, TransferProgress, TransferFinished, Error, Max
)
BROOKESIA_DESCRIBE_ENUM(Usb::PortState, Disabled, Starting, Ready, Busy, Error, Max)
BROOKESIA_DESCRIBE_ENUM(Usb::SessionState, Idle, Active, Max)
BROOKESIA_DESCRIBE_ENUM(
    Usb::TransferState, Idle, Receiving, Verifying, Handling, Completed, Failed, Aborted, Max
)
BROOKESIA_DESCRIBE_ENUM(Usb::HostCommand, PutFile, InstallBpk, Max)
BROOKESIA_DESCRIBE_STRUCT(
    Usb::Status, (), (port_state, serial_jtag_connected, transport, session_state, logs_suppressed, active_request_id,
                      max_frame_payload)
)
BROOKESIA_DESCRIBE_STRUCT(
    Usb::TransferStatus, (), (state, request_id, received_bytes, total_bytes, error)
)
BROOKESIA_DESCRIBE_STRUCT(
    Usb::TransferArtifact, (), (command, request_id, temporary_path, size, sha256, overwrite)
)

} // namespace esp_brookesia::service::helper
