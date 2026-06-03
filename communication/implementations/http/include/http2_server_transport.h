/**
 * @file http2_server_transport.h
 * @brief Reusable HTTP/2 server transport using nghttp2 and Boost.Asio
 *
 * This class provides low-level HTTP/2 server transport functionality for
 * accepting inbound connections and dispatching requests to registered handlers.
 * It is used by HttpCommunicationService to receive messages from other AF
 * components over HTTP/2.
 *
 * Features:
 * - Accept multiple concurrent HTTP/2 client connections
 * - Route inbound requests by path to registered handlers
 * - Asynchronous connection handling via Boost.Asio
 * - Graceful shutdown support
 */

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <boost/asio.hpp>
#include <nghttp2/nghttp2.h>

namespace af {
namespace communication {
namespace http {

/**
 * @brief Inbound HTTP request structure
 */
struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
    int32_t stream_id{0};
};

/**
 * @brief HTTP response to send back to the client
 */
struct HttpServerResponse {
    int status_code{200};
    std::string body;
    std::map<std::string, std::string> headers;
};

/**
 * @brief Handler function type for inbound HTTP requests
 */
using HttpRequestHandler = std::function<HttpServerResponse(const HttpRequest&)>;

/**
 * @brief Configuration for the HTTP/2 server transport
 */
struct Http2ServerConfig {
    std::string listen_address{"0.0.0.0"};    ///< Address to bind
    uint16_t listen_port{8080};                ///< Port to bind
    int max_concurrent_connections{64};         ///< Max concurrent client connections
    int max_concurrent_streams{100};           ///< Max concurrent streams per connection
};

/**
 * @brief Per-connection session state
 */
struct ConnectionSession {
    nghttp2_session* session{nullptr};
    boost::asio::ip::tcp::socket socket;
    std::map<int32_t, HttpRequest> pending_requests;
    std::mutex mutex;
    std::atomic<bool> active{true};

    explicit ConnectionSession(boost::asio::io_context& io)
        : socket(io) {}

    ~ConnectionSession() {
        if (session) {
            nghttp2_session_del(session);
            session = nullptr;
        }
    }
};

/**
 * @brief Low-level HTTP/2 server transport using nghttp2
 *
 * Accepts inbound TCP connections, performs HTTP/2 session setup,
 * and dispatches received requests to registered handlers.
 */
class Http2ServerTransport {
public:
    Http2ServerTransport();
    ~Http2ServerTransport();

    // Non-copyable
    Http2ServerTransport(const Http2ServerTransport&) = delete;
    Http2ServerTransport& operator=(const Http2ServerTransport&) = delete;

    /**
     * @brief Initialize the server transport
     * @param config Server configuration
     * @return true if initialization succeeded
     */
    bool initialize(const Http2ServerConfig& config);

    /**
     * @brief Register a handler for a specific path
     * @param path The URL path to handle (e.g., "/internal/messages")
     * @param handler The handler function
     */
    void register_route(const std::string& path, HttpRequestHandler handler);

    /**
     * @brief Register a default handler for unmatched paths
     * @param handler The default handler function
     */
    void set_default_handler(HttpRequestHandler handler);

    /**
     * @brief Start accepting connections
     * @return true if server started successfully
     */
    bool start();

    /**
     * @brief Stop the server and close all connections
     * @return true if server stopped successfully
     */
    bool stop();

    /**
     * @brief Check if the server is running
     */
    bool is_running() const { return running_.load(); }

    /**
     * @brief Get the actual port the server is listening on
     */
    uint16_t listening_port() const { return actual_port_; }

private:
    Http2ServerConfig config_;
    std::atomic<bool> running_{false};
    uint16_t actual_port_{0};

    // Boost.Asio
    boost::asio::io_context io_context_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::vector<std::thread> worker_threads_;

    // Route handlers
    std::map<std::string, HttpRequestHandler> route_handlers_;
    HttpRequestHandler default_handler_;
    std::mutex routes_mutex_;

    // Active connections
    std::vector<std::shared_ptr<ConnectionSession>> connections_;
    std::mutex connections_mutex_;

    // --- Internal methods ---

    /**
     * @brief Start accepting the next connection
     */
    void do_accept();

    /**
     * @brief Handle a new client connection
     */
    void handle_connection(std::shared_ptr<ConnectionSession> conn);

    /**
     * @brief Read data from connection and feed to nghttp2
     */
    void read_from_connection(std::shared_ptr<ConnectionSession> conn);

    /**
     * @brief Send pending nghttp2 data to the client
     */
    bool flush_connection(std::shared_ptr<ConnectionSession> conn);

    /**
     * @brief Dispatch a completed request to the appropriate handler
     */
    HttpServerResponse dispatch_request(const HttpRequest& request);

    /**
     * @brief Submit an HTTP/2 response on the given connection and stream
     */
    bool submit_response(
        std::shared_ptr<ConnectionSession> conn,
        int32_t stream_id,
        const HttpServerResponse& response);

    /**
     * @brief Initialize nghttp2 server session for a connection
     */
    bool initialize_server_session(std::shared_ptr<ConnectionSession> conn);

    // --- nghttp2 server callbacks ---

    static int on_frame_recv_callback(
        nghttp2_session* session,
        const nghttp2_frame* frame,
        void* user_data);

    static int on_data_chunk_recv_callback(
        nghttp2_session* session,
        uint8_t flags,
        int32_t stream_id,
        const uint8_t* data,
        size_t len,
        void* user_data);

    static int on_header_callback(
        nghttp2_session* session,
        const nghttp2_frame* frame,
        const uint8_t* name, size_t namelen,
        const uint8_t* value, size_t valuelen,
        uint8_t flags,
        void* user_data);

    static int on_begin_headers_callback(
        nghttp2_session* session,
        const nghttp2_frame* frame,
        void* user_data);

    static int on_stream_close_callback(
        nghttp2_session* session,
        int32_t stream_id,
        uint32_t error_code,
        void* user_data);

    static ssize_t on_data_source_read_callback(
        nghttp2_session* session,
        int32_t stream_id,
        uint8_t* buf,
        size_t length,
        uint32_t* data_flags,
        nghttp2_data_source* source,
        void* user_data);
};

} // namespace http
} // namespace communication
} // namespace af
