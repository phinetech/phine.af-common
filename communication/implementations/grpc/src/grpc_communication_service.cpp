/**
 * @file grpc_communication_service.cpp
 * @brief Implementation of the gRPC communication service
 *
 * This file implements the GrpcCommunicationService class, which provides
 * communication between AF components using gRPC as the transport mechanism.
 *
 * The implementation includes:
 * - A gRPC server that handles incoming messages (optional)
 * - gRPC client connections to other services
 * - Message routing based on registered handlers and callbacks
 * - Subscription management for streaming updates
 */

#include "grpc_communication_service.h"
#include "cpp_utils/error.h"
#include <grpcpp/grpcpp.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include <chrono>
#include <random>
#include <sstream>
#include <iostream>

// Include generated protobuf/gRPC code
#include "message.grpc.pb.h"

namespace af {
namespace communication {
namespace grpc {

// Implementation of the internal gRPC service
class GrpcCommunicationService::InternalGrpcServiceImpl final
    : public af::proto::InternalCommunication::Service {
public:
    InternalGrpcServiceImpl(GrpcCommunicationService* parent)
        : parent_(parent) {}

    ::grpc::Status SendMessage(
        ::grpc::ServerContext* context,
        const af::proto::InternalMessage* request,
        af::proto::InternalMessage* response) override {

        // Convert protobuf message to internal Message format
        MessagePtr req_msg = std::make_shared<Message>();
        req_msg->message_type = request->message_type();
        req_msg->correlation_id = request->correlation_id();
        req_msg->payload = std::vector<uint8_t>(
            request->payload().begin(), request->payload().end());

        // Copy metadata
        for (const auto& entry : request->metadata()) {
            req_msg->metadata[entry.first] = entry.second;
        }

        // Process message based on registered handlers/callbacks
        MessagePtr resp_msg = nullptr;

        // Try handlers first
        bool handled = false;
        {
            std::lock_guard<std::mutex> lock(parent_->handlers_mutex_);
            auto handler_it = parent_->message_handlers_.find(req_msg->message_type);
            if (handler_it != parent_->message_handlers_.end()) {
                resp_msg = handler_it->second->handle_message(req_msg);
                handled = true;
            } else {
                auto callback_it = parent_->message_callbacks_.find(req_msg->message_type);
                if (callback_it != parent_->message_callbacks_.end()) {
                    resp_msg = callback_it->second(req_msg);
                    handled = true;
                }
            }
        }

        if (!handled) {
            return ::grpc::Status(::grpc::StatusCode::UNIMPLEMENTED,
                "No handler registered for message type: " + req_msg->message_type);
        }

        if (!resp_msg) {
            // Create empty response if handler didn't provide one
            resp_msg = std::make_shared<Message>();
            resp_msg->correlation_id = req_msg->correlation_id;
        }

        // Convert response to protobuf format
        response->set_message_type(resp_msg->message_type);
        response->set_correlation_id(resp_msg->correlation_id);
        response->set_payload(resp_msg->payload.data(), resp_msg->payload.size());

        // Copy metadata
        for (const auto& entry : resp_msg->metadata) {
            (*response->mutable_metadata())[entry.first] = entry.second;
        }

        // Determine gRPC status code from metadata.status (hrpc::StatusCode)
        auto status_it = resp_msg->metadata.find("status");
        if (status_it != resp_msg->metadata.end()) {
            ::grpc::StatusCode grpc_code = parent_->map_status_code(status_it->second);
            if (grpc_code != ::grpc::StatusCode::OK) {
                return ::grpc::Status(grpc_code, "Status from metadata: " + status_it->second);
            }
        }

        return ::grpc::Status::OK;
    }

    ::grpc::Status StreamMessages(
        ::grpc::ServerContext* context,
        ::grpc::ServerReaderWriter<af::proto::InternalMessage, af::proto::InternalMessage>* stream) override {

        // For simplicity, we'll implement a basic version here
        // A full implementation would handle bidirectional streaming properly

        af::proto::InternalMessage request;
        while (stream->Read(&request)) {
            // Convert and process message similar to SendMessage
            MessagePtr req_msg = std::make_shared<Message>();
            req_msg->message_type = request.message_type();
            req_msg->correlation_id = request.correlation_id();
            req_msg->payload = std::vector<uint8_t>(
                request.payload().begin(), request.payload().end());

            for (const auto& entry : request.metadata()) {
                req_msg->metadata[entry.first] = entry.second;
            }

            // Process message and get response
            MessagePtr resp_msg = nullptr;

            {
                std::lock_guard<std::mutex> lock(parent_->handlers_mutex_);
                auto handler_it = parent_->message_handlers_.find(req_msg->message_type);
                if (handler_it != parent_->message_handlers_.end()) {
                    resp_msg = handler_it->second->handle_message(req_msg);
                } else {
                    auto callback_it = parent_->message_callbacks_.find(req_msg->message_type);
                    if (callback_it != parent_->message_callbacks_.end()) {
                        resp_msg = callback_it->second(req_msg);
                    }
                }
            }

            if (!resp_msg) {
                resp_msg = std::make_shared<Message>();
                resp_msg->correlation_id = req_msg->correlation_id;
                resp_msg->message_type = "error.no_handler";
                std::string error = "No handler for message type: " + req_msg->message_type;
                resp_msg->payload = std::vector<uint8_t>(error.begin(), error.end());
            }

            // Send response
            af::proto::InternalMessage response;
            response.set_message_type(resp_msg->message_type);
            response.set_correlation_id(resp_msg->correlation_id);
            response.set_payload(resp_msg->payload.data(), resp_msg->payload.size());

            for (const auto& entry : resp_msg->metadata) {
                (*response.mutable_metadata())[entry.first] = entry.second;
            }

            // Determine gRPC status code from metadata.status (hrpc::StatusCode)
            auto status_it = resp_msg->metadata.find("status");
            if (status_it != resp_msg->metadata.end()) {
                ::grpc::StatusCode grpc_code = parent_->map_status_code(status_it->second);
                if (grpc_code != ::grpc::StatusCode::OK) {
                    // End the stream with the error status
                    return ::grpc::Status(grpc_code, "Status from metadata: " + status_it->second);
                }
            }

            if (!stream->Write(response)) {
                return ::grpc::Status(::grpc::StatusCode::UNAVAILABLE,
                    "Failed to write response to stream");
            }
        }

        return ::grpc::Status::OK;
    }

private:
    GrpcCommunicationService* parent_;
};

// GrpcCommunicationService implementation

GrpcCommunicationService::GrpcCommunicationService()
    : is_running_(false), client_only_(false) {
}

GrpcCommunicationService::~GrpcCommunicationService() {
    stop();
}

bool GrpcCommunicationService::initialize(
    const std::string& service_name,
    const std::unordered_map<std::string, std::string>& config,
    bool client_only) {

    service_name_ = service_name;
    client_only_ = client_only;

    // If client_only mode, we skip server creation
    if (client_only_) {
        std::cout << "GrpcCommunicationService initialized in client-only mode." << std::endl;
        return true;
    }

    std::cout << "Initializing GrpcCommunicationService: " << service_name_ << std::endl;

    // Extract configuration parameters
    std::string server_address = "0.0.0.0";  // Default to all interfaces
    int server_port = 0;  // Default to auto-assigned port
    int max_threads = 4;  // Default thread count

    auto it = config.find("server_address");
    if (it != config.end()) {
        server_address = it->second;
    }

    it = config.find("server_port");
    if (it != config.end()) {
        try {
            server_port = std::stoi(it->second);
        } catch (const std::exception& e) {
            return false;
        }
    }

    it = config.find("max_threads");
    if (it != config.end()) {
        try {
            max_threads = std::stoi(it->second);
        } catch (const std::exception& e) {
            return false;
        }
    }

    // Create server address string
    server_address_ = server_address + ":" + std::to_string(server_port);

    // Create service implementation
    service_impl_ = std::make_unique<InternalGrpcServiceImpl>(this);

    // Create server builder
    ::grpc::ServerBuilder builder;
    builder.AddListeningPort(server_address_, ::grpc::InsecureServerCredentials(), &server_port);
    builder.RegisterService(service_impl_.get());
    builder.SetMaxReceiveMessageSize(-1);  // Unlimited message size
    builder.SetMaxSendMessageSize(-1);     // Unlimited message size

    // Set up thread pool
    if (max_threads > 0) {
        builder.SetSyncServerOption(::grpc::ServerBuilder::SyncServerOption::NUM_CQS, max_threads);
        builder.SetSyncServerOption(::grpc::ServerBuilder::SyncServerOption::MIN_POLLERS, max_threads);
        builder.SetSyncServerOption(::grpc::ServerBuilder::SyncServerOption::MAX_POLLERS, max_threads);
    }

    // Enable reflection
    ::grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    // Build the server
    server_ = builder.BuildAndStart();
    if (!server_) {
        return false;
    }

    // Update server address with actual port if it was auto-assigned
    if (server_port == 0) {
        server_address_ = server_address + ":" + std::to_string(server_port);
    }

    return true;
}

MessagePtr GrpcCommunicationService::send_request(
    const std::string& destination,
    const MessagePtr& message) {

    if (!message) {
        return nullptr;
    }

    try {
        // Get or create connection to destination
        ClientConnection& connection = get_or_create_connection(destination);

        // Create stub
        auto stub = af::proto::InternalCommunication::NewStub(connection.channel);

        // Create gRPC request
        af::proto::InternalMessage request;
        request.set_message_type(message->message_type);
        request.set_correlation_id(message->correlation_id);
        request.set_payload(message->payload.data(), message->payload.size());

        // Add metadata
        for (const auto& entry : message->metadata) {
            (*request.mutable_metadata())[entry.first] = entry.second;
        }

        // Set up context with timeout
        ::grpc::ClientContext context;
        std::chrono::system_clock::time_point deadline =
            std::chrono::system_clock::now() + std::chrono::seconds(30);
        context.set_deadline(deadline);

         // Send request
        af::proto::InternalMessage response;
        ::grpc::Status status = stub->SendMessage(&context, request, &response);

        if (!status.ok()) {
            // Handle error
            std::cerr << "gRPC error: " << status.error_message() << std::endl;
            return nullptr;
        }

        // Convert response to internal format
        MessagePtr resp_msg = std::make_shared<Message>();
        resp_msg->message_type = response.message_type();
        resp_msg->correlation_id = response.correlation_id();
        resp_msg->payload = std::vector<uint8_t>(
            response.payload().begin(), response.payload().end());

        // Copy metadata
        for (const auto& entry : response.metadata()) {
            resp_msg->metadata[entry.first] = entry.second;
        }

        return resp_msg;
    } catch (const std::exception& e) {
        std::cerr << "Exception in send_request: " << e.what() << std::endl;
        return nullptr;
    }
}

bool GrpcCommunicationService::send_async(
    const std::string& destination,
    const MessagePtr& message,
    const MessageCallback& callback) {

    if (!message) {
        return false;
    }

    // For simplicity, we'll implement this as a new thread that calls send_request
    // A more efficient implementation would use gRPC's async API
    std::thread([this, destination, message, callback]() {
        MessagePtr response = this->send_request(destination, message);
        if (callback && response) {
            callback(response);
        }
    }).detach();

    return true;
}

bool GrpcCommunicationService::register_handler(
    const std::string& message_type,
    const MessageHandlerPtr& handler) {

    if (!handler) {
        return false;
    }

    // In client-only mode, we don't expect to receive messages
    if (client_only_) {
        std::cerr << "Warning: Registering handler in client-only mode" << std::endl;
    }

    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_handlers_[message_type] = handler;
    return true;
}

bool GrpcCommunicationService::register_callback(
    const std::string& message_type,
    const MessageCallback& callback) {

    if (!callback) {
        return false;
    }

    // In client-only mode, we don't expect to receive messages
    if (client_only_) {
        std::cerr << "Warning: Registering callback in client-only mode" << std::endl;
    }

    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_callbacks_[message_type] = callback;
    return true;
}

std::string GrpcCommunicationService::subscribe(
    const std::string& source,
    const std::string& message_type,
    const MessageCallback& callback) {

    if (!callback) {
        return "";
    }

    // Generate a unique subscription ID
    std::string subscription_id = generate_subscription_id();

    // Create subscription
    Subscription subscription;
    subscription.source = source;
    subscription.message_type = message_type;
    subscription.callback = callback;

    // Store subscription
    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_[subscription_id] = subscription;
    }

    // TODO: Set up streaming connection to source
    // This would involve creating a gRPC streaming call and handling responses

    return subscription_id;
}

bool GrpcCommunicationService::unsubscribe(const std::string& subscription_id) {
    std::lock_guard<std::mutex> lock(subscriptions_mutex_);

    auto it = subscriptions_.find(subscription_id);
    if (it == subscriptions_.end()) {
        return false;
    }

    // TODO: Cancel any active streaming calls for this subscription

    // Remove subscription
    subscriptions_.erase(it);
    return true;
}

bool GrpcCommunicationService::start() {
    if (is_running_) {
        return true;  // Already running
    }

    // In client-only mode, there's no server to start
    if (client_only_) {
        is_running_ = true;
        return true;
    }

    if (!server_) {
        return false;  // Server not initialized
    }

    // Start server in a separate thread
    is_running_ = true;
    server_thread_ = std::thread([this]() {
        // This will block until stop() is called
        server_->Wait();
    });

    return true;
}

bool GrpcCommunicationService::stop() {
    if (!is_running_) {
        return true;  // Already stopped
    }

    is_running_ = false;

    // Shutdown server and wait for it to complete (if not in client-only mode)
    if (!client_only_ && server_) {
        server_->Shutdown();

        // Wait for server thread to exit
        if (server_thread_.joinable()) {
            server_thread_.join();
        }
    }

    // Close all client connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        client_connections_.clear();
    }

    // Clear handlers and subscriptions
    {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        message_handlers_.clear();
        message_callbacks_.clear();
    }

    {
        std::lock_guard<std::mutex> lock(subscriptions_mutex_);
        subscriptions_.clear();
    }

    return true;
}

GrpcCommunicationService::ClientConnection& GrpcCommunicationService::get_or_create_connection(
    const std::string& destination) {

    std::lock_guard<std::mutex> lock(connections_mutex_);

    auto it = client_connections_.find(destination);
    if (it != client_connections_.end()) {
        return it->second;
    }

    // Create new connection
    ClientConnection connection;
    connection.channel = ::grpc::CreateChannel(
        destination, ::grpc::InsecureChannelCredentials());

    // Store and return the new connection
    auto result = client_connections_.emplace(destination, std::move(connection));
    return result.first->second;
}

std::string GrpcCommunicationService::generate_subscription_id() {
    // Generate a random UUID-like string for subscription IDs
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex_chars = "0123456789abcdef";

    std::stringstream ss;
    ss << service_name_ << "-sub-";
    for (int i = 0; i < 16; ++i) {
        ss << hex_chars[dis(gen)];
    }

    return ss.str();
}

::grpc::StatusCode GrpcCommunicationService::map_status_code(const std::string& status_str) {
    // hrpc::StatusCode is assumed to be an enum with the same values as grpc::StatusCode
    int code = 0;
    try {
        code = static_cast<int>(std::stoi(status_str));
    } catch (...) {
        code = static_cast<int>(::grpc::StatusCode::UNKNOWN);
    }
    ::grpc::StatusCode grpc_code = ::grpc::StatusCode::UNKNOWN;
    switch (code) {
        case 200: grpc_code = ::grpc::StatusCode::OK; break;
        case 201: grpc_code = ::grpc::StatusCode::OK; break;
        case 400: grpc_code = ::grpc::StatusCode::INVALID_ARGUMENT; break;
        case 401: grpc_code = ::grpc::StatusCode::UNAUTHENTICATED; break;
        case 403: grpc_code = ::grpc::StatusCode::PERMISSION_DENIED; break;
        case 404: grpc_code = ::grpc::StatusCode::NOT_FOUND; break;
        case 409: grpc_code = ::grpc::StatusCode::ALREADY_EXISTS; break;
        case 429: grpc_code = ::grpc::StatusCode::RESOURCE_EXHAUSTED; break;
        case 499: grpc_code = ::grpc::StatusCode::CANCELLED; break;
        case 500: grpc_code = ::grpc::StatusCode::INTERNAL; break;
        case 501: grpc_code = ::grpc::StatusCode::UNIMPLEMENTED; break;
        case 503: grpc_code = ::grpc::StatusCode::UNAVAILABLE; break;
        case 504: grpc_code = ::grpc::StatusCode::DEADLINE_EXCEEDED; break;
        default: grpc_code = ::grpc::StatusCode::UNKNOWN; break;
    }
    return grpc_code;
}

} // namespace grpc
} // namespace communication
} // namespace af