/**
 * @file camara_error_codes.h
 * @brief Common CAMARA API error codes shared across all services
 *
 * @details Standard error codes as defined in the CAMARA API Design Guidelines.
 * These codes are used across multiple CAMARA services (QoD, Device Location,
 * SIM Swap, Number Verification, etc.) for consistent error handling following
 * RFC 7807 Problem Details format.
 *
 * @see https://github.com/camaraproject/Commonalities/blob/main/documentation/CAMARA-API-Design-Guide.md#31-standardized-use-of-camara-error-responses
 *
 * @note The values of status and code fields are normative as defined in CAMARA_common.yaml
 */

#pragma once

namespace af {
namespace camara {

/**
 * @brief Common CAMARA error codes (from CAMARA Commonalities)
 *
 * @details These error codes follow RFC 7807 Problem Details format and are shared
 * across all CAMARA API services. They are grouped by error category for easier
 * reference.
 *
 * @note Status codes and error codes are normative - implementations MUST use these
 * exact values as defined in the CAMARA API Design Guidelines.
 */
namespace CommonErrorCode {

    // ============================================================================
    // SYNTAX EXCEPTIONS (400-level errors related to request format/validation)
    // ============================================================================

    /**
     * @brief Client specified an invalid argument, request body or query param
     * @http_status 400
     */
    constexpr const char* INVALID_ARGUMENT = "INVALID_ARGUMENT";

    /**
     * @brief Client specified an invalid range
     * @http_status 400
     * @details Used when a given field has a pre-defined range or invalid filter criteria
     */
    constexpr const char* OUT_OF_RANGE = "OUT_OF_RANGE";

    /**
     * @brief Client does not have sufficient permissions to perform this action
     * @http_status 403
     * @details OAuth2 token access does not have the required scope or when the user
     * fails operational security
     */
    constexpr const char* PERMISSION_DENIED = "PERMISSION_DENIED";

    /**
     * @brief Field is not consistent with access token
     * @http_status 403
     * @details Reflects inconsistency between information in some field of the API
     * and the related OAuth2 Token. This error SHOULD be used only when the scope
     * of the API allows it to explicitly confirm whether or not the supplied identity
     * matches that bound to the Three-Legged Access Token.
     */
    constexpr const char* INVALID_TOKEN_CONTEXT = "INVALID_TOKEN_CONTEXT";

    /**
     * @brief Concurrency conflict
     * @http_status 409
     * @details Concurrency of processes of the same nature/scope
     */
    constexpr const char* ABORTED = "ABORTED";

    /**
     * @brief The resource that a client tried to create already exists
     * @http_status 409
     */
    constexpr const char* ALREADY_EXISTS = "ALREADY_EXISTS";

    /**
     * @brief A specified resource duplicate entry found
     * @http_status 409
     * @details Duplication of an existing resource
     */
    constexpr const char* CONFLICT = "CONFLICT";

    // ============================================================================
    // SERVICE EXCEPTIONS (Authentication, Resource Access, Business Logic)
    // ============================================================================

    /**
     * @brief Request not authenticated due to missing, invalid, or expired credentials
     * @http_status 401
     * @details A new authentication is required. The request cannot be authenticated
     * and a new authentication is required.
     */
    constexpr const char* UNAUTHENTICATED = "UNAUTHENTICATED";

    /**
     * @brief The specified resource is not found
     * @http_status 404
     */
    constexpr const char* NOT_FOUND = "NOT_FOUND";

    /**
     * @brief Device identifier not found
     * @http_status 404
     * @details Some identifier cannot be matched to a device
     */
    constexpr const char* IDENTIFIER_NOT_FOUND = "IDENTIFIER_NOT_FOUND";

    /**
     * @brief The identifier provided is not supported
     * @http_status 422
     * @details None of the provided identifiers is supported by the implementation
     */
    constexpr const char* UNSUPPORTED_IDENTIFIER = "UNSUPPORTED_IDENTIFIER";

    /**
     * @brief The device is already identified by the access token
     * @http_status 422
     * @details An explicit identifier is provided when a device or phone number has
     * already been identified from the access token
     */
    constexpr const char* UNNECESSARY_IDENTIFIER = "UNNECESSARY_IDENTIFIER";

    /**
     * @brief The service is not available for the provided identifier
     * @http_status 422
     * @details Service not applicable for the provided identifier
     */
    constexpr const char* SERVICE_NOT_APPLICABLE = "SERVICE_NOT_APPLICABLE";

    /**
     * @brief The device cannot be identified
     * @http_status 422
     * @details An identifier is not included in the request and the device or phone
     * number identification cannot be derived from the 3-legged access token
     */
    constexpr const char* MISSING_IDENTIFIER = "MISSING_IDENTIFIER";

    /**
     * @brief Out of resource quota
     * @http_status 429
     * @details Request is rejected due to exceeding a business quota limit
     */
    constexpr const char* QUOTA_EXCEEDED = "QUOTA_EXCEEDED";

    /**
     * @brief Rate limit reached
     * @http_status 429
     * @details Access to the API has been temporarily blocked due to rate or spike
     * arrest limits being reached
     */
    constexpr const char* TOO_MANY_REQUESTS = "TOO_MANY_REQUESTS";

    // ============================================================================
    // NOTIFICATION/CALLBACK RELATED ERRORS
    // ============================================================================

    /**
     * @brief Invalid notification sink URL or configuration
     * @http_status 400
     * @details The notification sink (callback URL) provided is invalid or malformed.
     * This may occur when the URL format is incorrect or the sink configuration
     * is not properly specified.
     */
    constexpr const char* INVALID_SINK = "INVALID_SINK";

    /**
     * @brief Invalid credential for notification authentication
     * @http_status 400
     * @details The credential provided for authenticating notifications to the sink
     * is invalid or incorrectly formatted.
     */
    constexpr const char* INVALID_CREDENTIAL = "INVALID_CREDENTIAL";

    /**
     * @brief Invalid token provided
     * @http_status 401
     * @details The token (e.g., access token, refresh token) provided is invalid,
     * malformed, or does not meet the expected format requirements.
     */
    constexpr const char* INVALID_TOKEN = "INVALID_TOKEN";

    // ============================================================================
    // SERVER EXCEPTIONS (5xx errors and method/content negotiation)
    // ============================================================================

    /**
     * @brief The requested method is not allowed/supported on the target resource
     * @http_status 405
     * @details Invalid HTTP verb used with a given endpoint
     */
    constexpr const char* METHOD_NOT_ALLOWED = "METHOD_NOT_ALLOWED";

    /**
     * @brief The server cannot produce a response matching the content requested
     * @http_status 406
     * @details API Server does not accept the media type (Accept-* header) indicated
     * by API client
     */
    constexpr const char* NOT_ACCEPTABLE = "NOT_ACCEPTABLE";

    /**
     * @brief Access to the target resource is no longer available
     * @http_status 410
     * @details Use in notifications flow to allow API Consumer to indicate that its
     * callback is no longer available
     */
    constexpr const char* GONE = "GONE";

    /**
     * @brief Request cannot be executed in the current system state
     * @http_status 412
     * @details Indication by the API Server that the request cannot be processed in
     * current system state
     */
    constexpr const char* FAILED_PRECONDITION = "FAILED_PRECONDITION";

    /**
     * @brief The server refuses to accept the request (unsupported payload format)
     * @http_status 415
     * @details Payload format of the request is in an unsupported format by the Server
     */
    constexpr const char* UNSUPPORTED_MEDIA_TYPE = "UNSUPPORTED_MEDIA_TYPE";

    /**
     * @brief Unknown server error. Typically a server bug
     * @http_status 500
     * @details Problem in Server side. Regular Server Exception
     */
    constexpr const char* INTERNAL = "INTERNAL";

    /**
     * @brief This functionality is not implemented yet
     * @http_status 501
     * @details Service not implemented. The use of this code SHOULD be avoided as far
     * as possible to reach aligned implementations
     */
    constexpr const char* NOT_IMPLEMENTED = "NOT_IMPLEMENTED";

    /**
     * @brief An upstream internal service cannot be reached
     * @http_status 502
     * @details Internal routing problem in the Server side that blocks management of
     * the service properly
     */
    constexpr const char* BAD_GATEWAY = "BAD_GATEWAY";

    /**
     * @brief Service Unavailable
     * @http_status 503
     * @details Service is not available. Temporary situation usually related to
     * maintenance process in the server side
     */
    constexpr const char* UNAVAILABLE = "UNAVAILABLE";

    /**
     * @brief Request timeout exceeded
     * @http_status 504
     * @details API Server Timeout
     */
    constexpr const char* TIMEOUT = "TIMEOUT";

} // namespace CommonErrorCode

} // namespace camara
} // namespace af
