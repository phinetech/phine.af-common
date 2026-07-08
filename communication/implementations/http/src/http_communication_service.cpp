/**
 * @file http_communication_service.cpp
 * @brief Implementation of the HTTP/2 communication service
 *
 * Implements the CommunicationService interface over HTTP/2.
 * Internal AF messaging uses a JSON envelope over POST /internal/messages.
 * External calls (PCF SBI, etc.) can use send_http() directly.
 */

#include "http_communication_service.h"
#include "cpp_utils/error.h"

#include <nlohmann/json.hpp>
#include <sstream>
#include <thread>
#include <chrono>
#include <random>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace af {
namespace communication {
namespace http {

namespace {
// Lazily-created named logger for the HTTP communication service.
spdlog::logger& tlog() {
    static std::shared_ptr<spdlog::logger> logger = [] {
        auto l = spdlog::get("http_comm");
        if (!l) {
            l = spdlog::stdout_color_mt("http_comm");
        }
        l->set_level(spdlog::level::trace);
        return l;
    }();
    return *logger;
}
}  // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

HttpCommunicationService::HttpCommunicationService() = default;

HttpCommunicationService::~HttpCommunicationService() {
    stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// CommunicationService Interface — Initialization
// ─────────────────────────────────────────────────────────────────────────────

bool HttpCommunicationService::initialize(
    const std::string& service_name,
    const std::unordered_map<std::string, std::string>& config,
    bool client_only) {

    service_name_ = service_name;
    client_only_ = client_only;

    tlog().debug("initialize: service='{}' client_only={} ({} config entries)",
                 service_name, client_only, config.size());

    // Also check config for client_only override
    auto it = config.find("client_only");
    if (it != config.end() && it->second == "true") {
        client_only_ = true;
    }

    // --- Client transport setup ---
    // Look for base_url or construct from server_address + server_port for outbound
    auto base_url_it = config.find("base_url");
    auto addr_it = config.find("server_address");
    auto port_it = config.find("server_port");

    if (base_url_it != config.end() && !base_url_it->second.empty()) {
        base_url_ = base_url_it->second;
    } else if (addr_it != config.end() && port_it != config.end()) {
        // For client mode connecting to a remote, construct URL
        if (client_only_) {
            std::string scheme = "http";
            auto tls_it = config.find("use_tls");
            if (tls_it != config.end() && tls_it->second == "true") {
                scheme = "https";
            }
            base_url_ = scheme + "://" + addr_it->second + ":" + port_it->second;
        }
    }

    // Initialize client transport if we have a target
    if (!base_url_.empty()) {
        Http2ClientConfig client_config;
        client_config.base_url = base_url_;

        auto tls_it = config.find("use_tls");
        if (tls_it != config.end() && tls_it->second == "true") {
            client_config.use_tls = true;
        }

        auto timeout_it = config.find("timeout_ms");
        if (timeout_it != config.end()) {
            try {
                client_config.timeout_ms = std::stoi(timeout_it->second);
            } catch (...) {}
        }

        auto streams_it = config.find("max_concurrent_streams");
        if (streams_it != config.end()) {
            try {
                client_config.max_concurrent_streams = std::stoi(streams_it->second);
            } catch (...) {}
        }

        tlog().debug("initialize: client transport target base_url='{}' use_tls={}",
                     client_config.base_url, client_config.use_tls);

        client_transport_ = std::make_unique<Http2ClientTransport>();
        if (!client_transport_->initialize(client_config)) {
            tlog().error("initialize: client transport initialization failed (base_url='{}')",
                         client_config.base_url);
            return false;
        }
    } else {
        tlog().debug("initialize: no base_url/target configured, client transport disabled");
    }

    // --- Server transport setup (if not client-only) ---
    if (!client_only_) {
        Http2ServerConfig server_config;

        if (addr_it != config.end()) {
            server_config.listen_address = addr_it->second;
        }
        if (port_it != config.end()) {
            try {
                server_config.listen_port = static_cast<uint16_t>(std::stoi(port_it->second));
            } catch (...) {}
        }

        auto streams_it = config.find("max_concurrent_streams");
        if (streams_it != config.end()) {
            try {
                server_config.max_concurrent_streams = std::stoi(streams_it->second);
            } catch (...) {}
        }

        server_transport_ = std::make_unique<Http2ServerTransport>();
        if (!server_transport_->initialize(server_config)) {
            return false;
        }

        // Register the internal message endpoint. REST endpoints registered
        // later via register_http_endpoint() are keyed by their own distinct
        // paths, so both coexist on the same route table without conflict.
        server_transport_->register_route("/internal/messages",
            [this](const HttpRequest& req) {
                return handle_internal_message(req);
            });
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CommunicationService Interface — Messaging
// ─────────────────────────────────────────────────────────────────────────────

MessagePtr HttpCommunicationService::send_request(
    const std::string& /*destination*/,
    const MessagePtr& message) {

    if (!client_transport_) {
        tlog().error("send_request: client transport not initialized");
        return nullptr;
    }

    // Ensure connection
    if (!client_transport_->is_connected()) {
        if (!client_transport_->connect()) {
            tlog().error("send_request: failed to connect client transport");
            return nullptr;
        }
    }

    // Serialize message to JSON
    std::string body = serialize_message(message);

    // Send as POST to /internal/messages
    std::map<std::string, std::string> headers;
    headers["content-type"] = "application/json";

    tlog().debug("send_request: POST /internal/messages message_type='{}' correlation_id='{}'",
                 message ? message->message_type : "", message ? message->correlation_id : "");

    HttpResponse response = client_transport_->send_request(
        "POST", "/internal/messages", headers, body);

    if (response.error || response.status_code < 200 || response.status_code >= 300) {
        tlog().error("send_request: POST /internal/messages failed (status={}, error={})",
                     response.status_code,
                     response.error_message.empty() ? response.body : response.error_message);
        auto error_msg = std::make_shared<Message>();
        error_msg->message_type = "error";
        error_msg->correlation_id = message ? message->correlation_id : "";
        error_msg->metadata["status_code"] = std::to_string(response.status_code);
        error_msg->metadata["error"] = response.error_message.empty()
            ? response.body : response.error_message;
        return error_msg;
    }

    // Deserialize response
    return deserialize_message(response.body);
}

bool HttpCommunicationService::send_async(
    const std::string& destination,
    const MessagePtr& message,
    const MessageCallback& callback) {

    // Fire on a detached thread
    std::thread([this, destination, message, callback]() {
        auto response = send_request(destination, message);
        if (callback) {
            callback(response);
        }
    }).detach();

    return true;
}

bool HttpCommunicationService::register_handler(
    const std::string& message_type,
    const MessageHandlerPtr& handler) {

    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_handlers_[message_type] = handler;
    return true;
}

bool HttpCommunicationService::register_callback(
    const std::string& message_type,
    const MessageCallback& callback) {

    std::lock_guard<std::mutex> lock(handlers_mutex_);
    message_callbacks_[message_type] = callback;
    return true;
}

std::string HttpCommunicationService::subscribe(
    const std::string& /*source*/,
    const std::string& /*message_type*/,
    const MessageCallback& /*callback*/) {

    // Subscriptions not supported in HTTP mode (would require webhooks or SSE)
    return "";
}

bool HttpCommunicationService::unsubscribe(const std::string& /*subscription_id*/) {
    // Not supported
    return false;
}

bool HttpCommunicationService::register_http_endpoint(
    const std::string& method,
    const std::string& path,
    af::communication::HttpRequestHandler handler) {

    tlog().debug("register_http_endpoint: method={} path={}", method, path);

    if (!server_transport_) {
        tlog().warn("register_http_endpoint: server transport not initialized (client_only mode?)");
        return false;
    }

    // Wrap handler to:
    // 1. Filter by HTTP method
    // 2. Convert between af::communication::http types (used by transport) and
    //    af::communication types (used by the public interface)
    auto filtered_handler = [method, handler](const http::HttpRequest& req) -> http::HttpServerResponse {
        // Convert from http:: namespace to base namespace
        af::communication::HttpRequest base_req;
        base_req.method = req.method;
        base_req.path = req.path;
        base_req.headers = req.headers;
        base_req.body = req.body;

        if (base_req.method != method) {
            http::HttpServerResponse resp;
            resp.status_code = 405;
            resp.body = R"({"error": "Method Not Allowed"})";
            resp.headers["content-type"] = "application/json";
            resp.headers["allow"] = method;
            return resp;
        }

        // Call handler with base namespace type
        af::communication::HttpServerResponse base_resp = handler(base_req);

        // Convert response back to http:: namespace
        http::HttpServerResponse resp;
        resp.status_code = base_resp.status_code;
        resp.headers = base_resp.headers;
        resp.body = base_resp.body;
        return resp;
    };

    server_transport_->register_route(path, filtered_handler);
    tlog().debug("register_http_endpoint: registered {} {}", method, path);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// CommunicationService Interface — Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

bool HttpCommunicationService::start() {
    if (is_running_) {
        return true;
    }

    // Start server transport (if not client-only)
    if (server_transport_) {
        if (!server_transport_->start()) {
            return false;
        }
    }

    // Connect client transport
    if (client_transport_) {
        // Attempt connection but don't fail start() if remote isn't available yet
        // (lazy connect on first send_request is fine)
        client_transport_->connect();
    }

    is_running_ = true;
    return true;
}

bool HttpCommunicationService::stop() {
    if (!is_running_) {
        return true;
    }

    is_running_ = false;

    if (server_transport_) {
        server_transport_->stop();
    }

    if (client_transport_) {
        client_transport_->disconnect();
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Extended HTTP Interface
// ─────────────────────────────────────────────────────────────────────────────

HttpResponse HttpCommunicationService::send_http(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    const std::string& body) {

    tlog().debug("send_http: {} {} ({} body byte(s))", method, path, body.size());

    if (!client_transport_) {
        HttpResponse error;
        error.error = true;
        error.error_message = "Client transport not initialized";
        tlog().error("send_http: {}", error.error_message);
        return error;
    }

    // Ensure connection
    if (!client_transport_->is_connected()) {
        tlog().trace("send_http: client transport not connected, connecting");
        if (!client_transport_->connect()) {
            HttpResponse error;
            error.error = true;
            error.error_message = "Failed to connect";
            tlog().error("send_http: {}", error.error_message);
            return error;
        }
    }

    auto response = client_transport_->send_request(method, path, headers, body);
    tlog().debug("send_http: {} {} -> status={} error={} ({} body byte(s))",
                 method, path, response.status_code, response.error, response.body.size());
    return response;
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal Helpers
// ─────────────────────────────────────────────────────────────────────────────

std::string HttpCommunicationService::serialize_message(const MessagePtr& message) {
    if (!message) {
        return "{}";
    }

    nlohmann::json j;
    j["message_type"] = message->message_type;
    j["correlation_id"] = message->correlation_id;

    // Encode payload as base64-like string (simple hex encoding for reliability)
    // Using raw string for JSON payloads which is the common case
    if (!message->payload.empty()) {
        // Try to treat payload as UTF-8 string first (most common case)
        std::string payload_str(message->payload.begin(), message->payload.end());
        // Check if it's valid JSON
        try {
            auto parsed = nlohmann::json::parse(payload_str);
            j["payload"] = parsed;
        } catch (...) {
            // Not JSON, encode as base64-like string
            j["payload_raw"] = payload_str;
        }
    }

    // Metadata
    if (!message->metadata.empty()) {
        j["metadata"] = message->metadata;
    }

    return j.dump();
}

MessagePtr HttpCommunicationService::deserialize_message(const std::string& json_str) {
    if (json_str.empty()) {
        return nullptr;
    }

    try {
        auto j = nlohmann::json::parse(json_str);
        auto msg = std::make_shared<Message>();

        if (j.contains("message_type")) {
            msg->message_type = j["message_type"].get<std::string>();
        }
        if (j.contains("correlation_id")) {
            msg->correlation_id = j["correlation_id"].get<std::string>();
        }

        // Decode payload
        if (j.contains("payload")) {
            std::string payload_str;
            if (j["payload"].is_string()) {
                payload_str = j["payload"].get<std::string>();
            } else {
                payload_str = j["payload"].dump();
            }
            msg->payload = std::vector<uint8_t>(payload_str.begin(), payload_str.end());
        } else if (j.contains("payload_raw")) {
            std::string raw = j["payload_raw"].get<std::string>();
            msg->payload = std::vector<uint8_t>(raw.begin(), raw.end());
        }

        // Metadata
        if (j.contains("metadata") && j["metadata"].is_object()) {
            for (auto& [key, val] : j["metadata"].items()) {
                msg->metadata[key] = val.is_string() ? val.get<std::string>() : val.dump();
            }
        }

        return msg;

    } catch (const std::exception&) {
        return nullptr;
    }
}

HttpServerResponse HttpCommunicationService::handle_internal_message(const HttpRequest& request) {
    HttpServerResponse response;

    tlog().debug("handle_internal_message: received {} {} ({} body byte(s))",
                 request.method, request.path, request.body.size());

    if (request.method != "POST") {
        tlog().warn("handle_internal_message: method {} not allowed", request.method);
        response.status_code = 405;
        response.body = R"({"error": "Method Not Allowed"})";
        return response;
    }

    tlog().trace("handle_internal_message: deserializing message body");
    // Deserialize inbound message
    auto message = deserialize_message(request.body);
    if (!message) {
        tlog().error("handle_internal_message: failed to deserialize message");
        response.status_code = 400;
        response.body = R"({"error": "Invalid message format"})";
        return response;
    }

    tlog().debug("handle_internal_message: dispatching message_type='{}' correlation_id='{}'",
                 message->message_type, message->correlation_id);

    // Dispatch to handler
    auto result = dispatch_to_handler(message);

    if (result) {
        tlog().debug("handle_internal_message: handler returned response message_type='{}'",
                     result->message_type);
        response.status_code = 200;
        response.body = serialize_message(result);
    } else {
        tlog().error("handle_internal_message: handler returned null response");
        response.status_code = 500;
        response.body = R"({"error": "Handler returned no response"})";
    }

    response.headers["content-type"] = "application/json";
    tlog().debug("handle_internal_message: returning response status={} ({} body byte(s))",
                 response.status_code, response.body.size());
    return response;
}

MessagePtr HttpCommunicationService::dispatch_to_handler(const MessagePtr& message) {
    std::lock_guard<std::mutex> lock(handlers_mutex_);

    // Try exact message type match
    auto handler_it = message_handlers_.find(message->message_type);
    if (handler_it != message_handlers_.end()) {
        return handler_it->second->handle_message(message);
    }

    // Try callback match
    auto callback_it = message_callbacks_.find(message->message_type);
    if (callback_it != message_callbacks_.end()) {
        return callback_it->second(message);
    }

    // Try wildcard handler
    auto wildcard_it = message_handlers_.find("*");
    if (wildcard_it != message_handlers_.end()) {
        return wildcard_it->second->handle_message(message);
    }

    // Try wildcard callback
    auto wildcard_cb_it = message_callbacks_.find("*");
    if (wildcard_cb_it != message_callbacks_.end()) {
        return wildcard_cb_it->second(message);
    }

    // No handler found
    auto error_msg = std::make_shared<Message>();
    error_msg->message_type = "error";
    error_msg->correlation_id = message->correlation_id;
    error_msg->metadata["error"] = "No handler registered for message type: " + message->message_type;
    return error_msg;
}

} // namespace http
} // namespace communication
} // namespace af
