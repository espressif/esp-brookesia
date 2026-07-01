/*
 * SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <array>
#include <span>
#include <string>
#include <vector>
#include "brookesia/lib_utils/describe_helpers.hpp"
#include "brookesia/hal_interface/interfaces/network/http_client.hpp"
#include "brookesia/service_manager/helper/base.hpp"

namespace esp_brookesia::service::helper {

/**
 * @brief Helper schema definitions for the HTTP service.
 */
class Http: public Base<Http> {
public:
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////// The following are the service specific types and enumerations ///////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    using HttpMethod = hal::network::HttpClientIface::Method;
    using TlsVerifyMode = hal::network::HttpClientIface::TlsVerifyMode;
    using ErrorCode = hal::network::HttpClientIface::ErrorCode;
    using HttpRequest = hal::network::HttpClientIface::Request;
    using HttpResponse = hal::network::HttpClientIface::Response;

    /**
     * @brief Lifecycle state of an individual HTTP request.
     */
    enum class RequestState {
        Queued,
        Running,
        Completed,
        Failed,
        Canceled,
        Max,
    };

    /**
     * @brief General lifecycle state of the HTTP service.
     */
    enum class GeneralState {
        Idle,
        Initing,
        Inited,
        TimeSyncing,
        Starting,
        Started,
        Stopping,
        Stopped,
        Deiniting,
        Max,
    };

    /**
     * @brief Progress payload for a running HTTP request.
     */
    struct RequestProgress {
        uint64_t request_id = 0;
        uint32_t downloaded = 0;
        uint32_t total = 0;
    };

    /**
     * @brief Snapshot of the current state and progress of an HTTP request.
     */
    struct RequestStateInfo {
        uint64_t request_id = 0;
        RequestState state = RequestState::Queued;
        ErrorCode error = ErrorCode::Ok;
        int status_code = 0;
        uint32_t downloaded = 0;
        uint32_t total = 0;
    };

    /**
     * @brief HTTP service request counters.
     */
    struct Statistics {
        uint64_t submitted_count = 0;
        uint64_t completed_count = 0;
        uint64_t failed_count = 0;
        uint64_t canceled_count = 0;
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the types required by the Base class /////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionId {
        Request,
        RequestAsync,
        CancelRequest,
        GetRequestState,
        ResetStatistics,
        Max,
    };

    enum class EventId {
        RequestStarted,
        RequestProgress,
        RequestCompleted,
        RequestFailed,
        RequestCanceled,
        GeneralStateChanged,
        Max,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the function parameter types ////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class FunctionRequestParam {
        Request,
    };

    enum class FunctionRequestAsyncParam {
        Request,
    };

    enum class FunctionCancelRequestParam {
        RequestId,
    };

    enum class FunctionGetRequestStateParam {
        RequestId,
    };

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// The following are the event parameter types ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    enum class EventRequestParam {
        RequestId,
        State,
        Response,
        Progress,
        Error,
    };

    enum class EventGeneralStateChangedParam {
        State,
    };

private:
    static FunctionSchema function_schema_request()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::Request),
            .description = "Submit an HTTP request and wait for the response.",
            .parameters = {{
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRequestParam::Request),
                    .description = "HTTP request object.",
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((HttpResponse{
                    .request_id = 1,
                    .status_code = 200,
                    .body = "{\"ok\":true}",
                    .file_path = "",
                    .error = ErrorCode::Ok,
                    .error_message = "",
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_request_async()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::RequestAsync),
            .description = "Submit an HTTP request asynchronously.",
            .parameters = {{
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionRequestAsyncParam::Request),
                    .description = "HTTP request object.",
                    .type = FunctionValueType::Object
                }
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Number,
                .description = "Request id number.",
            },
        };
    }

    static FunctionSchema function_schema_cancel_request()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::CancelRequest),
            .description = "Cancel a pending or running HTTP request.",
            .parameters = {{
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionCancelRequestParam::RequestId),
                    .description = "Request id returned by RequestAsync.",
                    .type = FunctionValueType::Number
                }
            },
            .require_scheduler = false
        };
    }

    static FunctionSchema function_schema_get_request_state()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::GetRequestState),
            .description = "Get HTTP request state.",
            .parameters = {{
                    .name = BROOKESIA_DESCRIBE_TO_STR(FunctionGetRequestStateParam::RequestId),
                    .description = "Request id returned by RequestAsync.",
                    .type = FunctionValueType::Number
                }
            },
            .require_scheduler = false,
            .return_value = FunctionReturnSchema{
                .type = FunctionValueType::Object,
                .description = (boost::format("Example: %1%")
                % BROOKESIA_DESCRIBE_JSON_SERIALIZE((RequestStateInfo{
                    .request_id = 1,
                    .state = RequestState::Completed,
                    .error = ErrorCode::Ok,
                    .status_code = 200,
                    .downloaded = 128,
                    .total = 128,
                }))).str(),
            },
        };
    }

    static FunctionSchema function_schema_reset_statistics()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(FunctionId::ResetStatistics),
            .description = "Reset HTTP service statistics.",
            .require_scheduler = false
        };
    }

    static EventSchema event_schema_request_started()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::RequestStarted),
            .description = "HTTP request started.",
            .items = request_id_state_items(),
        };
    }

    static EventSchema event_schema_request_progress()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::RequestProgress),
            .description = "HTTP request progress updated.",
            .items = {{
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventRequestParam::RequestId),
                    .description = "Request id.",
                    .type = EventItemType::Number
                },
                {
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventRequestParam::Progress),
                    .description = "Progress object.",
                    .type = EventItemType::Object
                }
            },
        };
    }

    static EventSchema event_schema_request_completed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::RequestCompleted),
            .description = "HTTP request completed.",
            .items = request_id_response_items(),
        };
    }

    static EventSchema event_schema_request_failed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::RequestFailed),
            .description = "HTTP request failed.",
            .items = request_id_response_items(),
        };
    }

    static EventSchema event_schema_request_canceled()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::RequestCanceled),
            .description = "HTTP request canceled.",
            .items = request_id_response_items(),
        };
    }

    static EventSchema event_schema_general_state_changed()
    {
        return {
            .name = BROOKESIA_DESCRIBE_TO_STR(EventId::GeneralStateChanged),
            .description = "HTTP service general state changed.",
            .items = {{
                    .name = BROOKESIA_DESCRIBE_TO_STR(EventGeneralStateChangedParam::State),
                    .description = "General state.",
                    .type = EventItemType::String
                }
            },
        };
    }

    static std::vector<EventItemSchema> request_id_state_items()
    {
        return {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventRequestParam::RequestId),
                .description = "Request id.",
                .type = EventItemType::Number
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventRequestParam::State),
                .description = "Request state.",
                .type = EventItemType::String
            },
        };
    }

    static std::vector<EventItemSchema> request_id_response_items()
    {
        return {
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventRequestParam::RequestId),
                .description = "Request id.",
                .type = EventItemType::Number
            },
            {
                .name = BROOKESIA_DESCRIBE_TO_STR(EventRequestParam::Response),
                .description = "HTTP response object.",
                .type = EventItemType::Object
            },
        };
    }

public:
    static constexpr std::string_view get_name()
    {
        return "Http";
    }

    static std::span<const FunctionSchema> get_function_schemas()
    {
        static const std::array<FunctionSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(FunctionId::Max)> FUNCTION_SCHEMAS = {{
                function_schema_request(),
                function_schema_request_async(),
                function_schema_cancel_request(),
                function_schema_get_request_state(),
                function_schema_reset_statistics(),
            }
        };
        return std::span<const FunctionSchema>(FUNCTION_SCHEMAS);
    }

    static std::span<const EventSchema> get_event_schemas()
    {
        static const std::array<EventSchema, BROOKESIA_DESCRIBE_ENUM_TO_NUM(EventId::Max)> EVENT_SCHEMAS = {{
                event_schema_request_started(),
                event_schema_request_progress(),
                event_schema_request_completed(),
                event_schema_request_failed(),
                event_schema_request_canceled(),
                event_schema_general_state_changed(),
            }
        };
        return std::span<const EventSchema>(EVENT_SCHEMAS);
    }
};

BROOKESIA_DESCRIBE_ENUM(Http::RequestState, Queued, Running, Completed, Failed, Canceled, Max);
BROOKESIA_DESCRIBE_ENUM(
    Http::GeneralState, Idle, Initing, Inited, TimeSyncing, Starting, Started, Stopping, Stopped, Deiniting, Max
);
BROOKESIA_DESCRIBE_ENUM(Http::FunctionId, Request, RequestAsync, CancelRequest, GetRequestState, ResetStatistics, Max);
BROOKESIA_DESCRIBE_ENUM(
    Http::EventId, RequestStarted, RequestProgress, RequestCompleted, RequestFailed, RequestCanceled,
    GeneralStateChanged, Max
);
BROOKESIA_DESCRIBE_ENUM(Http::FunctionRequestParam, Request);
BROOKESIA_DESCRIBE_ENUM(Http::FunctionRequestAsyncParam, Request);
BROOKESIA_DESCRIBE_ENUM(Http::FunctionCancelRequestParam, RequestId);
BROOKESIA_DESCRIBE_ENUM(Http::FunctionGetRequestStateParam, RequestId);
BROOKESIA_DESCRIBE_ENUM(Http::EventRequestParam, RequestId, State, Response, Progress, Error);
BROOKESIA_DESCRIBE_ENUM(Http::EventGeneralStateChangedParam, State);
BROOKESIA_DESCRIBE_STRUCT(Http::RequestProgress, (), (request_id, downloaded, total));
BROOKESIA_DESCRIBE_STRUCT(Http::RequestStateInfo, (), (request_id, state, error, status_code, downloaded, total));
BROOKESIA_DESCRIBE_STRUCT(
    Http::Statistics, (), (submitted_count, completed_count, failed_count, canceled_count)
);

} // namespace esp_brookesia::service::helper
