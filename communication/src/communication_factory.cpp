/**
 * @file communication_factory.cpp
 * @brief Implementation of the communication factory
 *
 * This file implements the CommunicationFactory methods for creating different
 * types of communication services. It loads the appropriate implementation based
 * on the requested type and configuration.
 */

 #include "communication_factory.h"
 #include "../implementations/direct/include/direct_communication_service.h"
 #include "cpp_utils/error.h"
 #include <algorithm>
 #include <cctype>
 #include <string>

 // Check if gRPC is available
 #if __has_include("../implementations/grpc/include/grpc_communication_service.h")
 #include "../implementations/grpc/include/grpc_communication_service.h"
 #define GRPC_AVAILABLE 1
 #else
 #define GRPC_AVAILABLE 0
 #endif

 // Check if HTTP/2 is available
 #if __has_include("../implementations/http/include/http_communication_service.h")
 #include "../implementations/http/include/http_communication_service.h"
 #define HTTP_AVAILABLE 1
 #else
 #define HTTP_AVAILABLE 0
 #endif

 namespace af {
 namespace communication {

 CommunicationServicePtr CommunicationFactory::create_service(
     CommunicationType type,
     const std::string& service_name,
     const std::unordered_map<std::string, std::string>& config) {

     CommunicationServicePtr service;

     // Create the appropriate service based on type
     switch (type) {
         case CommunicationType::GRPC:
 #if GRPC_AVAILABLE
             service = std::make_shared<grpc::GrpcCommunicationService>();
 #else
             throw utils::ConfigurationError("gRPC communication service not available - rebuild with gRPC support");
 #endif
             break;
         case CommunicationType::DIRECT:
             service = std::make_shared<direct::DirectCommunicationService>();
             break;
         case CommunicationType::ZEROMQ:
             // Not implemented yet
             throw utils::ConfigurationError("ZeroMQ communication service not implemented yet");
         case CommunicationType::REST:
             // Not implemented yet
             throw utils::ConfigurationError("REST communication service not implemented yet");        case CommunicationType::HTTP:
#if HTTP_AVAILABLE
            service = std::make_shared<http::HttpCommunicationService>();
#else
            throw utils::ConfigurationError("HTTP/2 communication service not available - rebuild with nghttp2 support");
#endif
            break;         default:
             throw utils::ConfigurationError("Unknown communication type");
     }

     // Initialize the service
     bool client_only = false;
     auto it = config.find("client_only");
     if (it != config.end() && it->second == "true")
        client_only = true;
     if (!service->initialize(service_name, config, client_only)) {
         throw utils::ConfigurationError("Failed to initialize communication service");
     }

     return service;
 }

 CommunicationServicePtr CommunicationFactory::create_service(
     const std::string& type_str,
     const std::string& service_name,
     const std::unordered_map<std::string, std::string>& config) {

     // Convert string to lowercase for case-insensitive comparison
     std::string type_lower = type_str;
     std::transform(type_lower.begin(), type_lower.end(), type_lower.begin(),
                   [](unsigned char c){ return std::tolower(c); });

     // Map string to enum
     CommunicationType type;
     if (type_lower == "grpc") {
         type = CommunicationType::GRPC;
     } else if (type_lower == "direct") {
         type = CommunicationType::DIRECT;
     } else if (type_lower == "zeromq") {
         type = CommunicationType::ZEROMQ;
     } else if (type_lower == "rest") {
         type = CommunicationType::REST;
    } else if (type_lower == "http" || type_lower == "http2") {
        type = CommunicationType::HTTP;
     }

     return create_service(type, service_name, config);
 }

 } // namespace communication
 } // namespace af