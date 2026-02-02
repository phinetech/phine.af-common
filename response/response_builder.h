/**
 * @file response_builder.h
 * @brief Transport-agnostic response builder
 *
 * @details Core utility for building standardized responses following
 * RFC 7807 Problem Details format for errors. Transport-agnostic design
 * allows reuse across gRPC, REST, and other protocols.
 */

#pragma once

#include "response_data.h"
#include <nlohmann/json.hpp>

namespace af {
namespace common {
namespace response {

/**
 * @brief Pure utility class for building transport-agnostic responses
 *
 * @details This class provides static methods for constructing standardized
 * response data that can be adapted to any transport protocol. It handles:
 * - RFC 7807 Problem Details format for errors
 * - Consistent metadata/header management
 * - Correlation ID tracking
 *
 * Services can use this directly or through transport-specific adapters.
 */
class ResponseBuilder {
public:
    /**
     * @brief Build error response data following RFC 7807 Problem Details
     *
     * @param params Error response parameters
     * @return Transport-agnostic response data
     */
    static ResponseData build_error_response(const ErrorResponseParams& params);

    /**
     * @brief Build success response data
     *
     * @param params Success response parameters
     * @return Transport-agnostic response data
     */
    static ResponseData build_success_response(const SuccessResponseParams& params);

    /**
     * @brief Create RFC 7807 Problem Details JSON
     *
     * @param status HTTP status code
     * @param code Error code
     * @param message Human-readable message
     * @param detail Optional detailed explanation
     * @return Problem details JSON object
     */
    static nlohmann::json create_problem_details(
        int status,
        const std::string& code,
        const std::string& message,
        const std::optional<std::string>& detail = std::nullopt
    );

    /**
     * @brief Add standard metadata to response
     *
     * @param metadata Metadata map to populate
     * @param correlation_id Correlation ID
     * @param status HTTP status code
     */
    static void add_standard_metadata(
        std::map<std::string, std::string>& metadata,
        const std::string& correlation_id,
        int status
    );
};

} // namespace response
} // namespace common
} // namespace af
