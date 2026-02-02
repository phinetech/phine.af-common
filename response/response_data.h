/**
 * @file response_data.h
 * @brief Transport-agnostic response data structures
 *
 * @details Defines neutral response structures that can be adapted to any
 * transport protocol (gRPC, REST, etc.). Follows RFC 7807 Problem Details
 * format for errors and provides flexible success response structure.
 */

#pragma once

#include <string>
#include <map>
#include <optional>
#include <nlohmann/json.hpp>

namespace af {
namespace common {
namespace response {

/**
 * @brief Transport-agnostic response data
 *
 * @details Contains all information needed to construct a response in any
 * transport protocol. Transport adapters map this to protocol-specific formats.
 */
struct ResponseData {
    /// HTTP-style status code (can be mapped to gRPC status)
    int status_code;

    /// Response body as JSON
    nlohmann::json payload;

    /// Additional metadata/headers (transport-agnostic)
    std::map<std::string, std::string> metadata;

    /// Correlation ID for request tracing
    std::string correlation_id;

    /// Whether this is an error response
    bool is_error;
};

/**
 * @brief Builder parameters for error responses
 *
 * @details Input parameters for constructing RFC 7807 Problem Details
 * error responses. Services can extend with additional fields.
 */
struct ErrorResponseParams {
    /// HTTP status code
    int status;

    /// Error code (e.g., "INVALID_ARGUMENT", "QOD.DURATION_OUT_OF_RANGE")
    std::string code;

    /// Human-readable error message
    std::string message;

    /// Correlation ID for tracing
    std::string correlation_id;

    /// Optional detailed explanation
    std::optional<std::string> detail;

    /// Service-specific additional fields
    std::optional<nlohmann::json> extensions;
};

/**
 * @brief Builder parameters for success responses
 *
 * @details Input parameters for constructing success responses.
 */
struct SuccessResponseParams {
    /// Response data
    nlohmann::json data;

    /// HTTP status code (default 200)
    int status = 200;

    /// Correlation ID for tracing
    std::string correlation_id;

    /// Additional metadata to include
    std::map<std::string, std::string> extra_metadata;
};

} // namespace response
} // namespace common
} // namespace af
