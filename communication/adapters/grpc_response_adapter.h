/**
 * @file grpc_response_adapter.h
 * @brief Adapter for converting transport-agnostic responses to gRPC format
 *
 * @details Maps ResponseData structures to gRPC MessagePtr format used by
 * the AF communication layer.
 */

#pragma once

#include "response/response_data.h"
#include "communication/include/message.h"
#include <memory>

namespace af {
namespace common {
namespace communication {
namespace adapters {

/**
 * @brief Adapter for gRPC transport
 *
 * @details Converts transport-agnostic ResponseData to gRPC MessagePtr format.
 * Handles:
 * - Message type selection based on error/success
 * - Metadata mapping
 * - Payload serialization
 */
class GrpcResponseAdapter {
public:
    /**
     * @brief Convert ResponseData to gRPC MessagePtr
     *
     * @param response_data Transport-agnostic response data
     * @return gRPC message pointer
     */
    static af::communication::MessagePtr to_grpc_message(const response::ResponseData& response_data);

    /**
     * @brief Map HTTP status code to gRPC status code
     *
     * @param http_status HTTP status code
     * @return Equivalent gRPC status code
     */
    static int map_http_to_grpc_status(int http_status);
};

} // namespace adapters
} // namespace communication
} // namespace common
} // namespace af
