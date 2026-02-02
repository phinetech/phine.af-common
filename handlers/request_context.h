/**
 * @file request_context.h
 * @brief Standardized request context across all services
 *
 * @details Common request context structure used by all CAMARA API services
 * to extract and pass authentication, authorization, and tracing information.
 */

#pragma once

#include <string>
#include <optional>
#include <map>
#include <vector>
#include <algorithm>

namespace af {
namespace common {
namespace handlers {

/**
 * @brief Standardized request context for CAMARA services
 *
 * @details Contains authentication, authorization, and tracing information
 * extracted from request metadata/headers. Used consistently across all
 * service handlers.
 */
struct RequestContext {
    /// Unique correlation ID for request tracing (x-correlator header)
    std::string correlation_id;

    /// API consumer identifier (from OAuth client_id or API key)
    std::string api_consumer_id;

    /// Whether request uses 3-legged OAuth (user token) vs 2-legged (client token)
    bool is_three_legged = false;

    /// Device identifier extracted from 3-legged access token
    std::optional<std::string> device_from_token;

    /// OAuth scopes granted to this request
    std::vector<std::string> scopes;

    /// Additional metadata from request (transport-specific)
    std::map<std::string, std::string> additional_metadata;

    /**
     * @brief Check if a specific scope is granted
     *
     * @param scope Scope to check
     * @return true if scope is present
     */
    bool has_scope(const std::string& scope) const {
        return std::find(scopes.begin(), scopes.end(), scope) != scopes.end();
    }
};

} // namespace handlers
} // namespace common
} // namespace af
