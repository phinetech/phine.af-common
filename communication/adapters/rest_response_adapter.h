/**
 * @file rest_response_adapter.h
 * @brief Adapter for converting transport-agnostic responses to REST format
 *
 * @details Maps ResponseData structures to REST HTTP format (status, headers, body).
 * Can be used with HTTP frameworks like Pistache, Beast, or custom REST implementations.
 */

#pragma once

#include "../../response/response_data.h"
#include <string>

namespace af {
namespace common {
namespace communication {
namespace adapters {

/**
 * @brief HTTP response structure for REST APIs
 */
struct HttpResponse {
    /// HTTP status code (200, 400, 500, etc.)
    int status_code;

    /// Response body (JSON string)
    std::string body;

    /// HTTP headers
    std::map<std::string, std::string> headers;
};

/**
 * @brief Adapter for REST/HTTP transport
 *
 * @details Converts transport-agnostic ResponseData to HTTP format.
 * Handles:
 * - Status code mapping
 * - Header population (Content-Type, x-correlator, etc.)
 * - Body serialization
 */
class RestResponseAdapter {
public:
    /**
     * @brief Convert ResponseData to HTTP response
     *
     * @param response_data Transport-agnostic response data
     * @return HTTP response structure
     */
    static HttpResponse to_http_response(const response::ResponseData& response_data);

    /**
     * @brief Set standard HTTP headers
     *
     * @param headers Headers map to populate
     * @param response_data Source response data
     */
    static void set_standard_headers(
        std::map<std::string, std::string>& headers,
        const response::ResponseData& response_data
    );
};

} // namespace adapters
} // namespace communication
} // namespace common
} // namespace af
