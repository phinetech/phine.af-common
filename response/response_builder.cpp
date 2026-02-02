/**
 * @file response_builder.cpp
 * @brief Implementation of transport-agnostic response builder
 */

#include "response_builder.h"

namespace af {
namespace common {
namespace response {

ResponseData ResponseBuilder::build_error_response(const ErrorResponseParams& params) {
    ResponseData response;
    response.status_code = params.status;
    response.correlation_id = params.correlation_id;
    response.is_error = true;

    // Build RFC 7807 Problem Details payload
    response.payload = create_problem_details(
        params.status,
        params.code,
        params.message,
        params.detail
    );

    // Merge service-specific extensions if provided
    if (params.extensions) {
        response.payload.update(*params.extensions);
    }

    // Add standard metadata
    add_standard_metadata(response.metadata, params.correlation_id, params.status);

    return response;
}

ResponseData ResponseBuilder::build_success_response(const SuccessResponseParams& params) {
    ResponseData response;
    response.status_code = params.status;
    response.correlation_id = params.correlation_id;
    response.is_error = false;
    response.payload = params.data;

    // Add standard metadata
    add_standard_metadata(response.metadata, params.correlation_id, params.status);

    // Merge additional metadata
    response.metadata.insert(params.extra_metadata.begin(), params.extra_metadata.end());

    return response;
}

nlohmann::json ResponseBuilder::create_problem_details(
    int status,
    const std::string& code,
    const std::string& message,
    const std::optional<std::string>& detail) {

    nlohmann::json problem;
    problem["status"] = status;
    problem["code"] = code;
    problem["message"] = message;

    if (detail) {
        problem["detail"] = *detail;
    }

    return problem;
}

void ResponseBuilder::add_standard_metadata(
    std::map<std::string, std::string>& metadata,
    const std::string& correlation_id,
    int status) {

    metadata["status"] = std::to_string(status);
    metadata["x-correlator"] = correlation_id;
    metadata["content-type"] = "application/json";
}

} // namespace response
} // namespace common
} // namespace af
