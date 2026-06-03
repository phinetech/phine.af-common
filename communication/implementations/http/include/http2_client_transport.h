/**
 * @file http2_client_transport.h
 * @brief Reusable HTTP/2 client transport using nghttp2 and Boost.Asio
 *
 * This class provides low-level HTTP/2 client transport functionality that can
 * be used by both internal AF communication and external PCF SBI calls.
 * It manages the nghttp2 session, TCP/TLS connection, and HTTP/2 frame processing.
 *
 * Features:
 * - HTTP/2 multiplexed streams over a single TCP connection
 * - Automatic connection management with reconnection
 * - Configurable timeouts and concurrency limits
 * - Support for cleartext HTTP/2 (h2c) with prior knowledge
 * - TLS/ALPN support (optional)
 */

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <boost/asio.hpp>
#include <nghttp2/nghttp2.h>

namespace af {
namespace communication {
namespace http {

/**
 * @brief HTTP response structure
 */
struct HttpResponse {
    int status_code{0};
    std::string body;
    std::map<std::string, std::string> headers;
    bool completed{false};
    bool error{false};
    std::string error_message;
};

/**
 * @brief Configuration for the HTTP/2 client transport
 */
struct Http2ClientConfig {
    std::string base_url;                          ///< Target URL (e.g., "http://host:port/path")
    bool use_tls{false};                           ///< Enable TLS (ALPN h2)
    int timeout_ms{5000};                          ///< Request timeout in milliseconds
    int connect_timeout_ms{3000};                  ///< Connection timeout in milliseconds
    int max_concurrent_streams{100};               ///< Max concurrent HTTP/2 streams
    bool http2_prior_knowledge{true};              ///< Use HTTP/2 prior knowledge (h2c)
    int max_connect_attempts{3};                   ///< Max connection retry attempts
    int max_request_attempts{3};                   ///< Max request retry attempts (reconnect on refused/GOAWAY)
    int retry_base_delay_ms{50};                   ///< Base delay for exponential backoff
};

/**
 * @brief Low-level HTTP/2 client transport using nghttp2
 *
 * This class handles the raw HTTP/2 transport layer: connection management,
 * frame processing, request submission, and response accumulation.
 */
class Http2ClientTransport {
public:
    Http2ClientTransport();
    ~Http2ClientTransport();

    // Non-copyable
    Http2ClientTransport(const Http2ClientTransport&) = delete;
    Http2ClientTransport& operator=(const Http2ClientTransport&) = delete;

    /**
     * @brief Initialize the transport with the given configuration
     * @param config Client transport configuration
     * @return true if initialization succeeded
     */
    bool initialize(const Http2ClientConfig& config);

    /**
     * @brief Establish TCP connection and perform HTTP/2 handshake
     * @return true if connection succeeded
     */
    bool connect();

    /**
     * @brief Disconnect from the server
     */
    void disconnect();

    /**
     * @brief Check if the transport is connected
     * @return true if connected
     */
    bool is_connected() const;

    /**
     * @brief Send an HTTP/2 request and wait for response
     * @param method HTTP method (GET, POST, PUT, PATCH, DELETE)
     * @param path Request path
     * @param headers Additional HTTP headers
     * @param body Request body (for POST/PUT/PATCH)
     * @return HttpResponse containing status, headers, and body
     */
    HttpResponse send_request(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& headers = {},
        const std::string& body = "");

    /**
     * @brief Get the configured host
     */
    const std::string& host() const { return host_; }

    /**
     * @brief Get the configured port
     */
    const std::string& port() const { return port_; }

    /**
     * @brief Get the configured base path
     */
    const std::string& base_path() const { return base_path_; }

private:
    // URL components
    std::string host_;
    std::string port_;
    std::string base_path_;

    // Configuration
    Http2ClientConfig config_;

    // nghttp2 session
    nghttp2_session* session_{nullptr};

    // Boost.Asio networking
    boost::asio::io_context io_context_;
    boost::asio::ip::tcp::socket socket_{io_context_};

    // Connection state
    std::atomic<bool> connected_{false};
    std::atomic<bool> goaway_received_{false};  ///< Set when peer sends GOAWAY; session is unusable
    std::mutex session_mutex_;

    // Per-stream response accumulation
    std::map<int32_t, HttpResponse> responses_;
    std::mutex responses_mutex_;
    std::condition_variable response_cv_;

    // --- Internal methods ---

    /**
     * @brief Parse URL into host, port, and base_path
     */
    bool parse_url(const std::string& url);

    /**
     * @brief Initialize nghttp2 client session with callbacks
     */
    bool initialize_nghttp2();

    /**
     * @brief Clean up nghttp2 session
     */
    void cleanup_nghttp2();

    /**
     * @brief Check if the TCP connection is still alive
     */
    bool is_connection_alive();

    /**
     * @brief Submit an HTTP/2 request to the session
     * @return Stream ID or -1 on error
     */
    int32_t submit_request(
        const std::string& method,
        const std::string& path,
        const std::map<std::string, std::string>& headers,
        const std::string& body);

    /**
     * @brief Send pending session data over the socket
     */
    bool flush_session();

    /**
     * @brief Read and process data from the socket
     */
    bool receive_and_process();

    /**
     * @brief Wait for a specific stream's response to complete
     * @param stream_id The stream to wait for
     * @param timeout_ms Timeout in milliseconds
     * @return true if response completed within timeout
     */
    bool wait_for_response(int32_t stream_id, int timeout_ms);

    // --- nghttp2 static callbacks ---

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

    static int on_stream_close_callback(
        nghttp2_session* session,
        int32_t stream_id,
        uint32_t error_code,
        void* user_data);
};

} // namespace http
} // namespace communication
} // namespace af
