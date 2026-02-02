/**
 * @file rest_response_adapter.cpp
 * @brief Implementation of REST response adapter
 */

#include "rest_response_adapter.h"

namespace af {
namespace common {
namespace communication {
namespace adapters {

HttpResponse RestResponseAdapter::to_http_response(const response::ResponseData& response_data) {
    HttpResponse http_response;

    // Set status code directly (already HTTP compatible)
    http_response.status_code = response_data.status_code;

    // Serialize payload to JSON string
    http_response.body = response_data.payload.dump();

    // Set headers
    set_standard_headers(http_response.headers, response_data);

    return http_response;
}

void RestResponseAdapter::set_standard_headers(
    std::map<std::string, std::string>& headers,
    const response::ResponseData& response_data) {

    // Content-Type
    headers["Content-Type"] = "application/json; charset=utf-8";

    // CAMARA required headers
    headers["x-correlator"] = response_data.correlation_id;

    // Copy all additional metadata as headers
    for (const auto& [key, value] : response_data.metadata) {
        // Skip internal metadata that shouldn't be headers
        if (key != "status" && key != "grpc_status") {
            headers[key] = value;
        }
    }

    // CORS headers (if needed)
    // headers["Access-Control-Allow-Origin"] = "*";
    // headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
    // headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, x-correlator";
}

} // namespace adapters
} // namespace communication
} // namespace common
} // namespace af
