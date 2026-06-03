/**
 * @file http_communication_service.h
 * @brief HTTP/2 implementation of the CommunicationService interface
 *
 * This class implements the CommunicationService interface using HTTP/2 transport
 * via nghttp2. It supports both internal AF messaging (message envelope over
 * POST /internal/messages) and raw HTTP/2 requests for external service calls.
 *
 * Modes:
 * - Client-only: Only outbound HTTP/2 requests (e.g., af_core → pcf_handler)
 * - Server+Client: Inbound handler dispatch + outbound requests
 *
 * Internal message contract:
 *   POST /internal/messages
 *   Body: JSON-serialized Message {message_type, correlation_id, payload, metadata}
 *   Response: JSON-serialized Message
 *
 * For external calls (e.g., PCF SBI), use send_http() directly with explicit
 * HTTP method/path/headers/body.
 */

#pragma once

#include "communication_interface.h"
#include "http2_client_transport.h"
#include "http2_server_transport.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

namespace af {
namespace communication {
namespace http {

/**
 * @brief HTTP/2 implementation of the CommunicationService interface
 *
 * Composes Http2ClientTransport and Http2ServerTransport to provide full
 * bidirectional communication compatible with the AF DI framework.
 */
class HttpCommunicationService : public CommunicationService {
public:
    HttpCommunicationService();
    ~HttpCommunicationService() override;

    // --- CommunicationService interface ---

    /**
     * @brief Initialize the HTTP communication service
     *
     * @param service_name Name of this service instance
     * @param config Configuration parameters:
     *   - server_address: Address to bind (default "0.0.0.0")
     *   - server_port: Port to bind (default "8080")
     *   - base_url: Target URL for outbound requests
     *   - timeout_ms: Request timeout (default "5000")
     *   - use_tls: Enable TLS ("true"/"false", default "false")
     *   - max_concurrent_streams: HTTP/2 streams (default "100")
     *   - client_only: Only act as client ("true"/"false", default "false")
     * @param client_only If true, skip server initialization
     * @return bool True if initialization succeeded
     */
    bool initialize(const std::string& service_name,
                   const std::unordered_map<std::string, std::string>& config,
                   bool client_only = false) override;

    /**
     * @brief Send a message and wait for response (internal envelope)
     */
    MessagePtr send_request(const std::string& destination,
                           const MessagePtr& message) override;

    /**
     * @brief Send a message asynchronously
     */
    bool send_async(const std::string& destination,
                   const MessagePtr& message,
                   const MessageCallback& callback = nullptr) override;

    /**
     * @brief Register a message handler for a specific message type
     */
    bool register_handler(const std::string& message_type,
                         const MessageHandlerPtr& handler) override;

    /**
     * @brief Register a callback for a specific message type
     */
    bool register_callback(const std::string& message_type,
                          const MessageCallback& callback) override;

    /**
     * @brief Subscribe to messages (not supported in HTTP client mode)
     */
    std::string subscribe(const std::string& source,
                         const std::string& message_type,
                         const MessageCallback& callback) override;

    /**
     * @brief Unsubscribe (not supported in HTTP client mode)
     */
    bool unsubscribe(const std::string& subscription_id) override;

    /**
     * @brief Start the HTTP communication service
     */
    bool start() override;

    /**
     * @brief Stop the HTTP communication service
     */
    bool stop() override;

    // --- Extended HTTP interface (for external/raw HTTP calls) ---

    /**
     * @brief Send a raw HTTP/2 request (for external service calls like PCF SBI)
     *
     * @param method HTTP method (GET, POST, PUT, PATCH, DELETE)
     * @param path Request path (appended to base_url)
     * @param headers HTTP headers
     * @param body Request body
     * @return HttpResponse with status, headers, body
     */
    HttpResponse send_http(const std::string& method,
                          const std::string& path,
                          const std::map<std::string, std::string>& headers = {},
                          const std::string& body = "");

private:
    // Service identity
    std::string service_name_;
    bool client_only_{false};
    std::atomic<bool> is_running_{false};

    // Transport layers
    std::unique_ptr<Http2ClientTransport> client_transport_;
    std::unique_ptr<Http2ServerTransport> server_transport_;

    // Message handlers and callbacks
    std::unordered_map<std::string, MessageHandlerPtr> message_handlers_;
    std::unordered_map<std::string, MessageCallback> message_callbacks_;
    std::mutex handlers_mutex_;

    // Configuration
    std::string base_url_;

    // --- Internal helpers ---

    /**
     * @brief Serialize a Message to JSON string
     */
    static std::string serialize_message(const MessagePtr& message);

    /**
     * @brief Deserialize a JSON string to Message
     */
    static MessagePtr deserialize_message(const std::string& json_str);

    /**
     * @brief Handle inbound internal message request
     * Routes to the registered handler by message_type
     */
    HttpServerResponse handle_internal_message(const HttpRequest& request);

    /**
     * @brief Dispatch message to the appropriate registered handler
     */
    MessagePtr dispatch_to_handler(const MessagePtr& message);
};

} // namespace http
} // namespace communication
} // namespace af
