/**
 * @file service_handler_helpers.h
 * @brief Optional helper utilities for service handlers
 *
 * @details Provides optional helper functions that service handlers can use
 * via composition rather than inheritance. Services can pick and choose which
 * utilities they need.
 */

#pragma once

#include "request_context.h"
#include "response/response_builder.h"
#include "communication/include/message.h"
#include "communication/adapters/grpc_response_adapter.h"
#include <nlohmann/json.hpp>
#include <memory>
#include <chrono>
#include <functional>

// Forward declare spdlog to avoid header dependency
namespace spdlog {
    class logger;
}

namespace af {
namespace common {
namespace handlers {

/**
 * @brief Helper utilities for service handlers (composition approach)
 *
 * @details Provides static utility methods that handlers can use without
 * inheritance. Handlers can create response payloads with service-specific
 * extensions via hooks.
 */
class ServiceHandlerHelpers {
public:
    /**
     * @brief Create error response with optional payload customization
     *
     * @param status HTTP status code
     * @param code Error code
     * @param message Error message
     * @param correlation_id Correlation ID
     * @param logger Optional logger for debug output
     * @param build_extensions Optional function to add service-specific fields
     * @return gRPC message
     */
    static af::communication::MessagePtr create_error_response(
        int status,
        const std::string& code,
        const std::string& message,
        const std::string& correlation_id,
        std::shared_ptr<spdlog::logger> logger = nullptr,
        std::function<nlohmann::json()> build_extensions = nullptr
    );

    /**
     * @brief Create success response with optional payload customization
     *
     * @param data Response data
     * @param status HTTP status code (default 200)
     * @param correlation_id Correlation ID
     * @param logger Optional logger for debug output
     * @param build_extensions Optional function to add service-specific fields
     * @return gRPC message
     */
    static af::communication::MessagePtr create_success_response(
        const nlohmann::json& data,
        int status,
        const std::string& correlation_id,
        std::shared_ptr<spdlog::logger> logger = nullptr,
        std::function<void(nlohmann::json&)> enrich_payload = nullptr
    );

    /**
     * @brief Extract request context from gRPC message
     *
     * @param message gRPC message
     * @param logger Optional logger for warnings
     * @return Standardized request context
     */
    static RequestContext extract_context(
        const af::communication::MessagePtr& message,
        std::shared_ptr<spdlog::logger> logger = nullptr
    );

    /**
     * @brief Generate correlation ID if not provided
     *
     * @param prefix Prefix for generated ID
     * @return Generated correlation ID
     */
    static std::string generate_correlation_id(const std::string& prefix = "req");

    /**
     * @brief Extract path parameter from message metadata
     *
     * @param message gRPC message
     * @param param_name Parameter name
     * @return Parameter value or empty string
     */
    static std::string extract_path_param(
        const af::communication::MessagePtr& message,
        const std::string& param_name
    );
};

} // namespace handlers
} // namespace common
} // namespace af
