/**
 * @file service_handler_helpers.cpp
 * @brief Implementation of service handler helper utilities
 */

#include "service_handler_helpers.h"
#ifdef SPDLOG_VERSION
#include <spdlog/spdlog.h>
#endif

namespace af {
namespace common {
namespace handlers {

af::communication::MessagePtr ServiceHandlerHelpers::create_error_response(
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& correlation_id,
    std::shared_ptr<spdlog::logger> logger,
    std::function<nlohmann::json()> build_extensions) {

    // Build error response parameters
    response::ErrorResponseParams params;
    params.status = status;
    params.code = code;
    params.message = message;
    params.correlation_id = correlation_id;

    // Add service-specific extensions if provided
    if (build_extensions) {
        params.extensions = build_extensions();
    }

    // Build transport-agnostic response
    auto response_data = response::ResponseBuilder::build_error_response(params);

    // Log if logger provided
    if (logger) {
#ifdef SPDLOG_VERSION
        logger->debug("Error response: {} - {} - {}", status, code, message);
#endif
    }

    // Convert to gRPC format
    return common::communication::adapters::GrpcResponseAdapter::to_grpc_message(response_data);
}

af::communication::MessagePtr ServiceHandlerHelpers::create_success_response(
    const nlohmann::json& data,
    int status,
    const std::string& correlation_id,
    std::shared_ptr<spdlog::logger> logger,
    std::function<void(nlohmann::json&)> enrich_payload) {

    // Build success response parameters
    response::SuccessResponseParams params;
    params.data = data;
    params.status = status;
    params.correlation_id = correlation_id;

    // Build transport-agnostic response
    auto response_data = response::ResponseBuilder::build_success_response(params);

    // Enrich payload if provided
    if (enrich_payload) {
        enrich_payload(response_data.payload);
    }

    // Log if logger provided
    if (logger) {
#ifdef SPDLOG_VERSION
        logger->debug("Success response: status={}", status);
#endif
    }

    // Convert to gRPC format
    return common::communication::adapters::GrpcResponseAdapter::to_grpc_message(response_data);
}

RequestContext ServiceHandlerHelpers::extract_context(
    const af::communication::MessagePtr& message,
    std::shared_ptr<spdlog::logger> logger) {

    RequestContext context;

    // Extract correlation ID
    context.correlation_id = message->correlation_id;
    if (context.correlation_id.empty()) {
        context.correlation_id = generate_correlation_id();
    }

    // Extract metadata
    if (!message->metadata.empty()) {
        try {
            // Convert unordered_map<string,string> to JSON for easier parsing
            nlohmann::json metadata_json = nlohmann::json::object();
            for (const auto& [key, value] : message->metadata) {
                metadata_json[key] = value;
            }

            // Extract API consumer ID
            if (metadata_json.contains("api_consumer_id")) {
                context.api_consumer_id = metadata_json["api_consumer_id"];
            } else {
                context.api_consumer_id = "default_consumer";
            }

            // Determine authentication type
            if (metadata_json.contains("auth_type")) {
                context.is_three_legged = (metadata_json["auth_type"] == "3-legged" ||
                                          metadata_json["auth_type"] == "user");
            } else if (metadata_json.contains("token_type")) {
                context.is_three_legged = (metadata_json["token_type"] == "user");
            }

            // Extract device from token if 3-legged
            if (context.is_three_legged && metadata_json.contains("device_id")) {
                context.device_from_token = metadata_json["device_id"];
            }

            // Extract x-correlator header if provided
            if (metadata_json.contains("x-correlator")) {
                context.correlation_id = metadata_json["x-correlator"];
            }

            // Extract OAuth scopes
            if (metadata_json.contains("scopes") && metadata_json["scopes"].is_array()) {
                for (const auto& scope : metadata_json["scopes"]) {
                    context.scopes.push_back(scope.get<std::string>());
                }
            }

            // Store all additional metadata
            for (const auto& [key, value] : message->metadata) {
                context.additional_metadata[key] = value;
            }

        } catch (const std::exception& e) {
            if (logger) {
#ifdef SPDLOG_VERSION
                logger->warn("Failed to parse message metadata: {}", e.what());
#endif
            }
        }
    }

    return context;
}

std::string ServiceHandlerHelpers::generate_correlation_id(const std::string& prefix) {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return prefix + "-" + std::to_string(now);
}

std::string ServiceHandlerHelpers::extract_path_param(
    const af::communication::MessagePtr& message,
    const std::string& param_name) {

    if (message->metadata.empty()) {
        return "";
    }

    // Try direct lookup first
    auto it = message->metadata.find(param_name);
    if (it != message->metadata.end()) {
        return it->second;
    }

    // Try path_params.param_name format
    std::string path_params_key = "path_params." + param_name;
    it = message->metadata.find(path_params_key);
    if (it != message->metadata.end()) {
        return it->second;
    }

    return "";
}

} // namespace handlers
} // namespace common
} // namespace af
