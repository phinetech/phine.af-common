/**
 * @file grpc_response_adapter.cpp
 * @brief Implementation of gRPC response adapter
 */

#include "grpc_response_adapter.h"

namespace af {
namespace common {
namespace communication {
namespace adapters {

af::communication::MessagePtr GrpcResponseAdapter::to_grpc_message(const response::ResponseData& response_data) {
    auto message = std::make_shared<af::communication::Message>();

    // Set message type based on error/success
    if (response_data.is_error) {
        message->message_type = "error_response";
    } else {
        message->message_type = "success_response";
    }

    // Set correlation ID
    message->correlation_id = response_data.correlation_id;

    // Serialize payload
    std::string payload_str = response_data.payload.dump();
    message->payload.assign(payload_str.begin(), payload_str.end());

    // Copy all metadata (convert map to unordered_map)
    for (const auto& [key, value] : response_data.metadata) {
        message->metadata[key] = value;
    }

    // Add gRPC-specific status mapping if needed
    message->metadata["grpc_status"] = std::to_string(
        map_http_to_grpc_status(response_data.status_code)
    );

    return message;
}

int GrpcResponseAdapter::map_http_to_grpc_status(int http_status) {
    // Map common HTTP status codes to gRPC status codes
    // See: https://github.com/grpc/grpc/blob/master/doc/statuscodes.md
    switch (http_status) {
        case 200: return 0;  // OK
        case 400: return 3;  // INVALID_ARGUMENT
        case 401: return 16; // UNAUTHENTICATED
        case 403: return 7;  // PERMISSION_DENIED
        case 404: return 5;  // NOT_FOUND
        case 409: return 6;  // ALREADY_EXISTS / ABORTED
        case 412: return 9;  // FAILED_PRECONDITION
        case 422: return 3;  // INVALID_ARGUMENT (unprocessable entity)
        case 429: return 8;  // RESOURCE_EXHAUSTED
        case 500: return 13; // INTERNAL
        case 501: return 12; // UNIMPLEMENTED
        case 503: return 14; // UNAVAILABLE
        case 504: return 4;  // DEADLINE_EXCEEDED
        default:  return 2;  // UNKNOWN
    }
}

} // namespace adapters
} // namespace communication
} // namespace common
} // namespace af
