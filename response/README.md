# Response Handling Architecture

## Overview

This directory contains the transport-agnostic response handling utilities for all CAMARA API services in the AF. The design follows these principles:

1. **Composition over Inheritance** - Utilities are static helpers, not base classes
2. **Transport Agnostic** - Response building is separated from transport serialization
3. **Minimal Override Surface** - Extension hooks for service-specific customization
4. **RFC 7807 Compliance** - Error responses follow Problem Details standard

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    Service Handler                       │
│                   (e.g., QodHandler)                     │
└─────────────────┬───────────────────────┬───────────────┘
                  │                       │
                  ▼                       ▼
         ┌────────────────┐      ┌──────────────────┐
         │ ResponseBuilder│      │ ServiceHandler   │
         │   (static)     │      │    Helpers       │
         │                │      │   (optional)     │
         └────────┬───────┘      └────────┬─────────┘
                  │                       │
                  └───────────┬───────────┘
                              │
                              ▼
                    ┌─────────────────┐
                    │  ResponseData   │
                    │  (transport-    │
                    │   agnostic)     │
                    └────────┬────────┘
                             │
                   ┌─────────┴──────────┐
                   │                    │
                   ▼                    ▼
         ┌──────────────────┐  ┌──────────────────┐
         │  gRPC Adapter    │  │  REST Adapter    │
         │  (MessagePtr)    │  │  (HttpResponse)  │
         └──────────────────┘  └──────────────────┘
```

## Components

### 1. Core Response Builder

**Location:** `common/response/`

- `response_data.h` - Transport-agnostic response structures
- `response_builder.h/cpp` - Static utility for building responses

**Usage:**
```cpp
// Build error response
response::ErrorResponseParams params;
params.status = 400;
params.code = "INVALID_ARGUMENT";
params.message = "Invalid duration value";
params.correlation_id = "req-123";

auto response_data = response::ResponseBuilder::build_error_response(params);
```

### 2. Request Context

**Location:** `common/handlers/request_context.h`

Standardized context structure for all CAMARA services:
- `correlation_id` - Request tracing ID (x-correlator)
- `api_consumer_id` - OAuth client_id or API key
- `is_three_legged` - 3-legged (user) vs 2-legged (client) OAuth
- `device_from_token` - Device ID from 3-legged token
- `scopes` - OAuth scopes granted

**Usage:**
```cpp
RequestContext context;
context.correlation_id = "req-123";
context.is_three_legged = true;
context.device_from_token = "device-456";
```

### 3. Service Handler Helpers

**Location:** `common/handlers/service_handler_helpers.h/cpp`

Optional composition-based utilities that handlers can use:

- `create_error_response()` - Build error with optional extensions
- `create_success_response()` - Build success with optional enrichment
- `extract_context()` - Parse request metadata to RequestContext
- `generate_correlation_id()` - Generate unique correlation ID
- `extract_path_param()` - Extract path parameters from metadata

**Usage:**
```cpp
// Simple usage - no customization
auto response = ServiceHandlerHelpers::create_error_response(
    400,
    "INVALID_ARGUMENT",
    "Invalid parameter",
    "req-123",
    logger_
);

// Advanced usage - with service-specific extensions
auto response = ServiceHandlerHelpers::create_error_response(
    422,
    "QOD.DURATION_OUT_OF_RANGE",
    "Duration must be between 1 and 86400 seconds",
    "req-123",
    logger_,
    []() -> nlohmann::json {
        // Add QoD-specific error fields
        return {
            {"min_duration", 1},
            {"max_duration", 86400}
        };
    }
);
```

### 4. Transport Adapters

**Location:** `common/communication/adapters/`

Map transport-agnostic `ResponseData` to protocol-specific formats:

#### gRPC Adapter
```cpp
auto grpc_message = GrpcResponseAdapter::to_grpc_message(response_data);
// Returns MessagePtr for AF communication layer
```

#### REST Adapter
```cpp
auto http_response = RestResponseAdapter::to_http_response(response_data);
// Returns HttpResponse with status, headers, body
```

## Service Implementation Patterns

### Pattern 1: Direct Helper Usage (Recommended)

QodHandler example:
```cpp
#include "handlers/service_handler_helpers.h"

MessagePtr QodHandler::create_error_response(
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& correlation_id) {
    
    return common::handlers::ServiceHandlerHelpers::create_error_response(
        status, code, message, correlation_id, logger_
    );
}

MessagePtr QodHandler::create_success_response(
    const nlohmann::json& data,
    int status,
    const std::string& correlation_id) {
    
    return common::handlers::ServiceHandlerHelpers::create_success_response(
        data, status, correlation_id, logger_
    );
}
```

### Pattern 2: With Service-Specific Extensions

For services needing additional error fields:
```cpp
MessagePtr DeviceLocationHandler::create_error_response(
    int status,
    const std::string& code,
    const std::string& message,
    const std::string& correlation_id) {
    
    return ServiceHandlerHelpers::create_error_response(
        status, code, message, correlation_id, logger_,
        [&]() -> nlohmann::json {
            // Add location-specific error details
            if (code == "LOCATION.ACCURACY_UNAVAILABLE") {
                return {
                    {"requested_accuracy", requested_accuracy_},
                    {"available_accuracy", available_accuracy_}
                };
            }
            return {};
        }
    );
}
```

### Pattern 3: Payload Enrichment for Success Responses

```cpp
MessagePtr SessionHandler::create_session_response(
    const SessionData& session,
    const std::string& correlation_id) {
    
    nlohmann::json data = session.to_json();
    
    return ServiceHandlerHelpers::create_success_response(
        data, 201, correlation_id, logger_,
        [&](nlohmann::json& payload) {
            // Add computed fields
            payload["expiresAt"] = calculate_expiry(session);
            payload["_links"] = {
                {"self", {{"href", "/sessions/" + session.id}}}
            };
        }
    );
}
```

## Error Response Format (RFC 7807)

All error responses follow RFC 7807 Problem Details:

```json
{
  "status": 400,
  "code": "INVALID_ARGUMENT",
  "message": "Device identifier is required for 3-legged tokens",
  "detail": "When using 3-legged OAuth, the device parameter must be provided or inferable from the token"
}
```

Service-specific extensions:
```json
{
  "status": 422,
  "code": "QOD.DURATION_OUT_OF_RANGE",
  "message": "Duration value is outside acceptable range",
  "min_duration": 1,
  "max_duration": 86400
}
```

## Future CAMARA Services

This architecture is designed for all CAMARA API services:

- ✅ **QualityOnDemand** - Implemented
- ⏳ **DeviceLocation** - Use same patterns
- ⏳ **SIMSwap** - Use same patterns
- ⏳ **NumberVerification** - Use same patterns
- ⏳ **CarrierBilling** - Use same patterns

Each service can:
1. Use helpers directly (no inheritance needed)
2. Add service-specific error codes in `include/<service>/<service>_error_codes.h`
3. Provide extension hooks for additional fields when needed
4. Work with both gRPC and REST transports

## Testing

Example test for custom error response:
```cpp
TEST(ResponseBuilderTest, ServiceSpecificExtensions) {
    response::ErrorResponseParams params;
    params.status = 422;
    params.code = "QOD.DURATION_OUT_OF_RANGE";
    params.message = "Invalid duration";
    params.correlation_id = "test-123";
    params.extensions = nlohmann::json{
        {"min_duration", 1},
        {"max_duration", 86400}
    };
    
    auto response = response::ResponseBuilder::build_error_response(params);
    
    EXPECT_EQ(response.status_code, 422);
    EXPECT_EQ(response.payload["code"], "QOD.DURATION_OUT_OF_RANGE");
    EXPECT_EQ(response.payload["min_duration"], 1);
}
```

## References

- [RFC 7807 - Problem Details for HTTP APIs](https://datatracker.ietf.org/doc/html/rfc7807)
- [CAMARA API Design Guidelines](https://github.com/camaraproject/Commonalities)
- [gRPC Status Codes](https://grpc.io/docs/guides/error/)
