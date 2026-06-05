/**
 * @file http2_server_transport.cpp
 * @brief Implementation of the HTTP/2 server transport
 *
 * Provides nghttp2-based HTTP/2 server transport with Boost.Asio for networking.
 * Accepts inbound connections and dispatches requests to registered handlers.
 */

#include "http2_server_transport.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <thread>

#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace af {
namespace communication {
namespace http {

namespace {
// Lazily-created named logger for the HTTP/2 server transport.
// Set to trace level so the diagnostic logs below are visible while debugging.
spdlog::logger& tlog() {
    static std::shared_ptr<spdlog::logger> logger = [] {
        auto l = spdlog::get("http2_server");
        if (!l) {
            l = spdlog::stdout_color_mt("http2_server");
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

Http2ServerTransport::Http2ServerTransport() = default;

Http2ServerTransport::~Http2ServerTransport() {
    stop();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool Http2ServerTransport::initialize(const Http2ServerConfig& config) {
    config_ = config;
    return true;
}

void Http2ServerTransport::register_route(const std::string& path, HttpRequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    route_handlers_[path] = std::move(handler);
}

void Http2ServerTransport::set_default_handler(HttpRequestHandler handler) {
    std::lock_guard<std::mutex> lock(routes_mutex_);
    default_handler_ = std::move(handler);
}

bool Http2ServerTransport::start() {
    if (running_) {
        return true;
    }

    try {
        // Create acceptor
        auto endpoint = boost::asio::ip::tcp::endpoint(
            boost::asio::ip::make_address(config_.listen_address),
            config_.listen_port);

        acceptor_ = std::make_unique<boost::asio::ip::tcp::acceptor>(
            io_context_, endpoint);

        actual_port_ = acceptor_->local_endpoint().port();
        running_ = true;

        tlog().info("start: HTTP/2 server listening on {}:{}",
                    config_.listen_address, actual_port_);

        // Start accepting connections
        do_accept();

        // Start worker threads for io_context
        int num_threads = std::max(1, std::min(4, config_.max_concurrent_connections / 16));
        for (int i = 0; i < num_threads; ++i) {
            worker_threads_.emplace_back([this]() {
                while (running_) {
                    try {
                        io_context_.run_for(std::chrono::milliseconds(100));
                    } catch (const std::exception&) {
                        // Continue unless stopped
                    }
                }
            });
        }

        return true;

    } catch (const std::exception& e) {
        tlog().error("start: failed to start server on {}:{} - {}",
                     config_.listen_address, config_.listen_port, e.what());
        running_ = false;
        return false;
    }
}

bool Http2ServerTransport::stop() {
    if (!running_) {
        return true;
    }

    running_ = false;

    // Close acceptor
    if (acceptor_ && acceptor_->is_open()) {
        boost::system::error_code ec;
        acceptor_->close(ec);
    }

    // Close all connections
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        for (auto& conn : connections_) {
            conn->active = false;
            if (conn->socket.is_open()) {
                boost::system::error_code ec;
                conn->socket.close(ec);
            }
        }
        connections_.clear();
    }

    // Stop io_context
    io_context_.stop();

    // Join worker threads
    for (auto& t : worker_threads_) {
        if (t.joinable()) {
            t.join();
        }
    }
    worker_threads_.clear();

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection Handling
// ─────────────────────────────────────────────────────────────────────────────

void Http2ServerTransport::do_accept() {
    if (!running_ || !acceptor_) {
        return;
    }

    auto conn = std::make_shared<ConnectionSession>(io_context_);

    acceptor_->async_accept(conn->socket,
        [this, conn](const boost::system::error_code& ec) {
            if (!ec && running_) {
                boost::system::error_code rep_ec;
                auto remote = conn->socket.remote_endpoint(rep_ec);
                tlog().debug("do_accept: accepted connection from {}",
                             rep_ec ? "unknown" : remote.address().to_string() +
                                 ":" + std::to_string(remote.port()));

                // Set socket options
                boost::asio::ip::tcp::no_delay no_delay(true);
                conn->socket.set_option(no_delay);

                // Store connection
                {
                    std::lock_guard<std::mutex> lock(connections_mutex_);
                    connections_.push_back(conn);
                }

                // Initialize and handle connection
                handle_connection(conn);
            }

            // Accept next connection
            if (running_) {
                do_accept();
            }
        });
}

void Http2ServerTransport::handle_connection(std::shared_ptr<ConnectionSession> conn) {
    // Initialize nghttp2 server session
    if (!initialize_server_session(conn)) {
        conn->active = false;
        return;
    }

    // Send server connection preface (SETTINGS)
    nghttp2_settings_entry iv[] = {
        {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
         static_cast<uint32_t>(config_.max_concurrent_streams)}
    };

    nghttp2_submit_settings(conn->session, NGHTTP2_FLAG_NONE, iv,
                           sizeof(iv) / sizeof(iv[0]));

    flush_connection(conn);

    // Start reading from connection
    read_from_connection(conn);
}

void Http2ServerTransport::read_from_connection(std::shared_ptr<ConnectionSession> conn) {
    if (!conn->active || !conn->socket.is_open()) {
        tlog().trace("read_from_connection: connection not active or socket closed");
        return;
    }

    auto buffer = std::make_shared<std::vector<uint8_t>>(16384);

    conn->socket.async_read_some(
        boost::asio::buffer(*buffer),
        [this, conn, buffer](const boost::system::error_code& ec, size_t bytes_read) {
            if (ec || bytes_read == 0) {
                if (ec && ec != boost::asio::error::eof &&
                    ec != boost::asio::error::operation_aborted) {
                    tlog().debug("read_from_connection: read error - {}", ec.message());
                }
                conn->active = false;
                return;
            }

            tlog().trace("read_from_connection: received {} byte(s)", bytes_read);

            // Feed data to nghttp2
            std::lock_guard<std::mutex> lock(conn->mutex);
            tlog().trace("read_from_connection: feeding {} byte(s) to nghttp2_session_mem_recv",
                         bytes_read);

            ssize_t rv = nghttp2_session_mem_recv(
                conn->session, buffer->data(), bytes_read);

            if (rv < 0) {
                tlog().warn("read_from_connection: nghttp2_session_mem_recv failed rv={} ({})",
                            rv, nghttp2_strerror(static_cast<int>(rv)));
                conn->active = false;
                return;
            }

            tlog().trace("read_from_connection: nghttp2_session_mem_recv consumed {} byte(s)", rv);

            // Flush any pending outbound data
            tlog().trace("read_from_connection: flushing pending outbound data");
            bool flush_ok = flush_connection(conn);
            tlog().trace("read_from_connection: flush_connection returned {}", flush_ok);

            // Continue reading
            if (conn->active && running_) {
                read_from_connection(conn);
            } else {
                tlog().debug("read_from_connection: not continuing (active={} running={})",
                             conn->active, running_);
            }
        });
}

bool Http2ServerTransport::flush_connection(std::shared_ptr<ConnectionSession> conn) {
    const uint8_t* data_ptr;
    ssize_t len;

    while ((len = nghttp2_session_mem_send(conn->session, &data_ptr)) > 0) {
        boost::system::error_code ec;
        boost::asio::write(conn->socket, boost::asio::buffer(data_ptr, len), ec);
        if (ec) {
            conn->active = false;
            return false;
        }
    }

    return len >= 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Request Dispatch
// ─────────────────────────────────────────────────────────────────────────────

HttpServerResponse Http2ServerTransport::dispatch_request(const HttpRequest& request) {
    std::lock_guard<std::mutex> lock(routes_mutex_);

    tlog().debug("dispatch_request: {} {} ({} body byte(s))",
                 request.method, request.path, request.body.size());

    // Try exact path match
    auto it = route_handlers_.find(request.path);
    if (it != route_handlers_.end()) {
        tlog().trace("dispatch_request: matched route '{}'", request.path);
        return it->second(request);
    }

    // Try default handler
    if (default_handler_) {
        tlog().trace("dispatch_request: no exact route for '{}', using default handler",
                     request.path);
        return default_handler_(request);
    }

    // No handler found
    tlog().warn("dispatch_request: no handler registered for '{}' -> 404", request.path);
    HttpServerResponse response;
    response.status_code = 404;
    response.body = R"({"error": "Not Found"})";
    response.headers["content-type"] = "application/json";
    return response;
}

bool Http2ServerTransport::submit_response(
    std::shared_ptr<ConnectionSession> conn,
    int32_t stream_id,
    const HttpServerResponse& response) {

    // Build response headers in an owning container first to avoid dangling pointers.
    // nghttp2_submit_response uses NGHTTP2_NV_FLAG_NONE so it copies the bytes, but
    // the name/value pointers must stay valid until that call returns.
    std::vector<std::pair<std::string, std::string>> all_headers;
    all_headers.reserve(1 + response.headers.size() + 1);

    // Pseudo-header
    all_headers.emplace_back(":status", std::to_string(response.status_code));

    // Regular headers
    bool has_content_type = false;
    for (const auto& [key, value] : response.headers) {
        all_headers.emplace_back(key, value);
        if (key == "content-type") {
            has_content_type = true;
        }
    }

    // Add default content-type if body present and not already set
    if (!has_content_type && !response.body.empty()) {
        all_headers.emplace_back("content-type", "application/json");
    }

    // Build nghttp2_nv array from the owning container
    std::vector<nghttp2_nv> nva;
    nva.reserve(all_headers.size());
    for (const auto& [name, value] : all_headers) {
        nghttp2_nv nv;
        nv.name = reinterpret_cast<uint8_t*>(const_cast<char*>(name.c_str()));
        nv.namelen = name.size();
        nv.value = reinterpret_cast<uint8_t*>(const_cast<char*>(value.c_str()));
        nv.valuelen = value.size();
        nv.flags = NGHTTP2_NV_FLAG_NONE;
        nva.push_back(nv);
    }

    tlog().debug("submit_response: stream_id={} status={} ({} body byte(s))",
                 stream_id, response.status_code, response.body.size());

    // Data provider for response body
    int rv;
    if (!response.body.empty()) {
        // Store the body for the callback
        auto* body_copy = new std::string(response.body);

        nghttp2_data_provider data_prd;
        data_prd.source.ptr = body_copy;
        data_prd.read_callback = on_data_source_read_callback;

        rv = nghttp2_submit_response(conn->session, stream_id,
                               nva.data(), nva.size(), &data_prd);
    } else {
        rv = nghttp2_submit_response(conn->session, stream_id,
                               nva.data(), nva.size(), nullptr);
    }

    if (rv != 0) {
        tlog().error("submit_response: nghttp2_submit_response failed for stream_id={} rv={} ({})",
                     stream_id, rv, nghttp2_strerror(rv));
        return false;
    }

    return flush_connection(conn);
}

// ─────────────────────────────────────────────────────────────────────────────
// nghttp2 Server Session Initialization
// ─────────────────────────────────────────────────────────────────────────────

bool Http2ServerTransport::initialize_server_session(std::shared_ptr<ConnectionSession> conn) {
    nghttp2_session_callbacks* callbacks = nullptr;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_frame_recv_callback(
        callbacks, on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
        callbacks, on_data_chunk_recv_callback);
    nghttp2_session_callbacks_set_on_header_callback(
        callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_begin_headers_callback(
        callbacks, on_begin_headers_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(
        callbacks, on_stream_close_callback);

    // Pass a pair of (server_transport, connection) as user_data
    // We use a simple struct stored on the connection
    struct SessionUserData {
        Http2ServerTransport* server;
        std::shared_ptr<ConnectionSession> conn;
    };

    auto* user_data = new SessionUserData{this, conn};

    int rv = nghttp2_session_server_new(&conn->session, callbacks, user_data);
    nghttp2_session_callbacks_del(callbacks);

    return rv == 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// nghttp2 Server Callbacks
// ─────────────────────────────────────────────────────────────────────────────

struct SessionUserData {
    Http2ServerTransport* server;
    std::shared_ptr<ConnectionSession> conn;
};

int Http2ServerTransport::on_begin_headers_callback(
    nghttp2_session* /*session*/,
    const nghttp2_frame* frame,
    void* user_data) {

    auto* sud = static_cast<SessionUserData*>(user_data);

    tlog().trace("on_begin_headers: frame_type={} stream_id={} category={}",
                 static_cast<int>(frame->hd.type), frame->hd.stream_id,
                 frame->hd.type == NGHTTP2_HEADERS ? static_cast<int>(frame->headers.cat) : -1);

    if (frame->hd.type == NGHTTP2_HEADERS &&
        frame->headers.cat == NGHTTP2_HCAT_REQUEST) {
        // New request stream
        // NOTE: conn->mutex is already held by read_from_connection caller
        tlog().debug("on_begin_headers: new request on stream_id={}", frame->hd.stream_id);
        sud->conn->pending_requests[frame->hd.stream_id] = HttpRequest{};
        sud->conn->pending_requests[frame->hd.stream_id].stream_id = frame->hd.stream_id;
    }

    return 0;
}

int Http2ServerTransport::on_header_callback(
    nghttp2_session* /*session*/,
    const nghttp2_frame* frame,
    const uint8_t* name, size_t namelen,
    const uint8_t* value, size_t valuelen,
    uint8_t /*flags*/,
    void* user_data) {

    auto* sud = static_cast<SessionUserData*>(user_data);

    if (frame->hd.type != NGHTTP2_HEADERS) {
        return 0;
    }

    std::string header_name(reinterpret_cast<const char*>(name), namelen);
    std::string header_value(reinterpret_cast<const char*>(value), valuelen);

    tlog().trace("on_header: stream_id={} {}: {}",
                 frame->hd.stream_id, header_name, header_value);

    // NOTE: conn->mutex is already held by read_from_connection caller
    auto it = sud->conn->pending_requests.find(frame->hd.stream_id);
    if (it == sud->conn->pending_requests.end()) {
        tlog().warn("on_header: stream_id={} not found in pending_requests", frame->hd.stream_id);
        return 0;
    }

    if (header_name == ":method") {
        it->second.method = header_value;
    } else if (header_name == ":path") {
        it->second.path = header_value;
    } else {
        it->second.headers[header_name] = header_value;
    }

    return 0;
}

int Http2ServerTransport::on_data_chunk_recv_callback(
    nghttp2_session* /*session*/,
    uint8_t /*flags*/,
    int32_t stream_id,
    const uint8_t* data,
    size_t len,
    void* user_data) {

    auto* sud = static_cast<SessionUserData*>(user_data);

    tlog().trace("on_data_chunk_recv: stream_id={} received {} byte(s)", stream_id, len);

    // NOTE: conn->mutex is already held by read_from_connection caller
    auto it = sud->conn->pending_requests.find(stream_id);
    if (it != sud->conn->pending_requests.end()) {
        it->second.body.append(reinterpret_cast<const char*>(data), len);
        tlog().trace("on_data_chunk_recv: stream_id={} total body size now {} byte(s)",
                     stream_id, it->second.body.size());
    } else {
        tlog().warn("on_data_chunk_recv: stream_id={} not found in pending_requests", stream_id);
    }

    return 0;
}

int Http2ServerTransport::on_frame_recv_callback(
    nghttp2_session* /*session*/,
    const nghttp2_frame* frame,
    void* user_data) {

    auto* sud = static_cast<SessionUserData*>(user_data);

    tlog().trace("on_frame_recv: frame_type={} stream_id={} flags=0x{:02x}",
                 static_cast<int>(frame->hd.type), frame->hd.stream_id, frame->hd.flags);

    // When we receive END_STREAM on a HEADERS or DATA frame, the request is complete
    if ((frame->hd.type == NGHTTP2_HEADERS || frame->hd.type == NGHTTP2_DATA) &&
        (frame->hd.flags & NGHTTP2_FLAG_END_STREAM)) {

        tlog().debug("on_frame_recv: END_STREAM received on stream_id={}", frame->hd.stream_id);

        HttpRequest request;
        // NOTE: conn->mutex is already held by read_from_connection caller
        auto it = sud->conn->pending_requests.find(frame->hd.stream_id);
        if (it == sud->conn->pending_requests.end()) {
            tlog().warn("on_frame_recv: stream_id={} not found in pending_requests",
                        frame->hd.stream_id);
            return 0;
        }
        request = std::move(it->second);
        sud->conn->pending_requests.erase(it);

        tlog().debug("on_frame_recv: dispatching request on stream_id={} ({} {} with {} body byte(s))",
                     frame->hd.stream_id, request.method, request.path, request.body.size());

        // Dispatch the request and send response
        HttpServerResponse response = sud->server->dispatch_request(request);

        tlog().debug("on_frame_recv: submitting response on stream_id={} (status={})",
                     frame->hd.stream_id, response.status_code);

        sud->server->submit_response(sud->conn, frame->hd.stream_id, response);

        tlog().debug("on_frame_recv: response submitted successfully on stream_id={}",
                     frame->hd.stream_id);
    }

    return 0;
}

int Http2ServerTransport::on_stream_close_callback(
    nghttp2_session* /*session*/,
    int32_t stream_id,
    uint32_t error_code,
    void* user_data) {

    auto* sud = static_cast<SessionUserData*>(user_data);

    if (error_code != 0) {
        tlog().warn("on_stream_close: stream_id={} closed with error_code={} ({})",
                    stream_id, error_code, nghttp2_http2_strerror(error_code));
    } else {
        tlog().trace("on_stream_close: stream_id={} closed cleanly", stream_id);
    }

    // Clean up any pending request for this stream
    // NOTE: conn->mutex is already held by read_from_connection caller
    sud->conn->pending_requests.erase(stream_id);

    return 0;
}

ssize_t Http2ServerTransport::on_data_source_read_callback(
    nghttp2_session* /*session*/,
    int32_t /*stream_id*/,
    uint8_t* buf,
    size_t length,
    uint32_t* data_flags,
    nghttp2_data_source* source,
    void* /*user_data*/) {

    auto* body = static_cast<std::string*>(source->ptr);

    size_t to_copy = std::min(length, body->size());
    if (to_copy > 0) {
        std::memcpy(buf, body->data(), to_copy);
    }

    // We send the entire body in one shot since it's already in memory
    *data_flags |= NGHTTP2_DATA_FLAG_EOF;

    // Free the body copy
    delete body;

    return static_cast<ssize_t>(to_copy);
}

} // namespace http
} // namespace communication
} // namespace af
