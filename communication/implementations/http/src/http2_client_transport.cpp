/**
 * @file http2_client_transport.cpp
 * @brief Implementation of the HTTP/2 client transport
 *
 * Provides nghttp2-based HTTP/2 client transport with Boost.Asio for networking.
 * Patterns informed by the existing PcfClientWrapper implementation.
 */

#include "http2_client_transport.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iostream>
#include <regex>
#include <sstream>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/write.hpp>
#include <boost/asio/read.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>

namespace af {
namespace communication {
namespace http {

namespace {
// Lazily-created named logger for the HTTP/2 client transport.
// Set to trace level so the diagnostic logs below are visible while debugging;
// dial this back to debug/info once the transport is stable.
spdlog::logger& tlog() {
    static std::shared_ptr<spdlog::logger> logger = [] {
        auto l = spdlog::get("http2_client");
        if (!l) {
            l = spdlog::stdout_color_mt("http2_client");
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

Http2ClientTransport::Http2ClientTransport() = default;

Http2ClientTransport::~Http2ClientTransport() {
    disconnect();
    cleanup_nghttp2();
}

// ─────────────────────────────────────────────────────────────────────────────
// Public API
// ─────────────────────────────────────────────────────────────────────────────

bool Http2ClientTransport::initialize(const Http2ClientConfig& config) {
    config_ = config;

    tlog().debug("initialize: base_url='{}' use_tls={} timeout_ms={} max_streams={}",
                 config_.base_url, config_.use_tls, config_.timeout_ms,
                 config_.max_concurrent_streams);

    if (!parse_url(config_.base_url)) {
        tlog().error("initialize: failed to parse base_url '{}'", config_.base_url);
        return false;
    }

    tlog().debug("initialize: parsed host='{}' port='{}' base_path='{}'",
                 host_, port_, base_path_);

    if (!initialize_nghttp2()) {
        tlog().error("initialize: failed to initialize nghttp2 session");
        return false;
    }

    return true;
}

bool Http2ClientTransport::connect() {
    std::lock_guard<std::mutex> lock(session_mutex_);

    if (connected_ && !goaway_received_ && is_connection_alive()) {
        return true;
    }

    tlog().debug("connect: establishing HTTP/2 connection to {}:{} (max_attempts={})",
                 host_, port_, config_.max_connect_attempts);

    for (int attempt = 0; attempt < config_.max_connect_attempts; ++attempt) {
        if (attempt > 0) {
            int delay_ms = config_.retry_base_delay_ms * (1 << (attempt - 1));
            tlog().trace("connect: retry attempt {} after {}ms backoff", attempt, delay_ms);
            std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        }

        try {
            // Close any existing socket
            if (socket_.is_open()) {
                boost::system::error_code ec;
                socket_.close(ec);
            }

            // Re-initialize nghttp2 session for a fresh connection
            cleanup_nghttp2();
            if (!initialize_nghttp2()) {
                tlog().warn("connect: nghttp2 re-init failed on attempt {}", attempt);
                continue;
            }
            goaway_received_ = false;

            // Resolve host
            boost::asio::ip::tcp::resolver resolver(io_context_);
            boost::asio::ip::tcp::resolver::results_type endpoints;

            try {
                endpoints = resolver.resolve(host_, port_);
            } catch (const std::exception& e) {
                tlog().warn("connect: failed to resolve {}:{} - {}", host_, port_, e.what());
                continue;
            }

            // Connect
            boost::system::error_code connect_ec;
            boost::asio::connect(socket_, endpoints, connect_ec);

            if (connect_ec) {
                tlog().warn("connect: TCP connect to {}:{} failed - {}",
                            host_, port_, connect_ec.message());
                continue;
            }

            tlog().trace("connect: TCP connected to {}:{}, sending HTTP/2 preface + SETTINGS",
                         host_, port_);

            // Set socket options
            boost::asio::socket_base::keep_alive keep_alive(true);
            socket_.set_option(keep_alive);
            boost::asio::ip::tcp::no_delay no_delay(true);
            socket_.set_option(no_delay);

            // Submit HTTP/2 SETTINGS
            nghttp2_settings_entry iv[] = {
                {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
                 static_cast<uint32_t>(config_.max_concurrent_streams)},
                {NGHTTP2_SETTINGS_ENABLE_PUSH, 0},
                {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, 65535},
                {NGHTTP2_SETTINGS_MAX_FRAME_SIZE, 16384}
            };

            int rv = nghttp2_submit_settings(session_, NGHTTP2_FLAG_NONE, iv,
                                             sizeof(iv) / sizeof(iv[0]));
            if (rv != 0) {
                tlog().warn("connect: nghttp2_submit_settings failed rv={} ({})",
                            rv, nghttp2_strerror(rv));
                continue;
            }

            // NOTE: We deliberately do NOT flush the preface+SETTINGS here, and we
            // do NOT perform a separate blocking handshake round-trip before
            // returning. Doing either leaves a *streamless* HTTP/2 connection open
            // for a moment, which some servers (notably free5GC's Go HTTP/2 server)
            // respond to by gracefully sending GOAWAY(last_stream_id=0, NO_ERROR) —
            // after which our first request lands on stream 1 (> 0) and is rejected
            // with REFUSED_STREAM, surfacing as status_code=0.
            //
            // Normal HTTP/2 clients (curl --http2-prior-knowledge, the original
            // PcfClientWrapper, etc.) open a stream promptly: they queue the request
            // HEADERS right after SETTINGS and flush them together. We do the same —
            // connect() leaves preface+SETTINGS queued in the nghttp2 session, and
            // the first send_request() flushes preface+SETTINGS+HEADERS in one go.
            // nghttp2 handles the SETTINGS exchange (and SETTINGS ACK) as part of
            // normal frame flow when we read the response.
            connected_ = true;
            tlog().info("connect: HTTP/2 connection established to {}:{}", host_, port_);
            return true;

        } catch (const std::exception& e) {
            tlog().warn("connect: exception on attempt {} - {}", attempt, e.what());
            continue;
        }
    }

    tlog().error("connect: exhausted {} attempts, could not connect to {}:{}",
                 config_.max_connect_attempts, host_, port_);
    return false;
}

void Http2ClientTransport::disconnect() {
    std::lock_guard<std::mutex> lock(session_mutex_);

    connected_ = false;

    if (socket_.is_open()) {
        boost::system::error_code ec;
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
        socket_.close(ec);
    }
}

bool Http2ClientTransport::is_connected() const {
    return connected_.load();
}

HttpResponse Http2ClientTransport::send_request(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    const std::string& body) {

    HttpResponse error_response;
    error_response.error = true;

    tlog().debug("send_request: {} {} ({} header(s), {} body byte(s))",
                 method, path, headers.size(), body.size());

    // Retry loop: a stream that gets refused/reset (status_code=0, e.g. after a
    // peer GOAWAY) or a transport error is retried on a fresh connection. This
    // mirrors the resilience the original PcfClientWrapper had.
    for (int attempt = 0; attempt < config_.max_request_attempts; ++attempt) {

        // (Re)establish a usable connection. On a retry, force a brand new
        // session+socket so we don't submit onto a GOAWAY'd/dead connection.
        if (attempt > 0) {
            tlog().debug("send_request: retry {} - reconnecting with a fresh session", attempt);
            disconnect();
        }
        if (!connected_ || goaway_received_) {
            tlog().trace("send_request: establishing connection (attempt {})", attempt);
            if (!connect()) {
                error_response.error_message = "Failed to connect to " + host_ + ":" + port_;
                tlog().error("send_request: {} (attempt {})", error_response.error_message, attempt);
                if (attempt + 1 < config_.max_request_attempts) continue;
                return error_response;
            }
        }

        HttpResponse result;
        bool transport_error = false;
        int32_t stream_id = -1;

        {
            std::lock_guard<std::mutex> lock(session_mutex_);

            stream_id = submit_request(method, path, headers, body);
            if (stream_id < 0) {
                error_response.error_message = "Failed to submit HTTP/2 request";
                transport_error = true;
            } else {
                tlog().trace("send_request: submitted on stream_id={}, flushing", stream_id);

                if (!flush_session()) {
                    error_response.error_message = "Failed to send request data";
                    tlog().error("send_request: {} (stream_id={})",
                                 error_response.error_message, stream_id);
                    transport_error = true;
                } else if (!wait_for_response(stream_id, config_.timeout_ms)) {
                    error_response.error_message = "Request timed out";
                    tlog().error("send_request: timed out after {}ms waiting for response "
                                 "on stream_id={}", config_.timeout_ms, stream_id);
                    std::lock_guard<std::mutex> rlock(responses_mutex_);
                    responses_.erase(stream_id);
                    transport_error = true;
                } else {
                    std::lock_guard<std::mutex> rlock(responses_mutex_);
                    auto it = responses_.find(stream_id);
                    if (it == responses_.end()) {
                        error_response.error_message = "Response not found for stream";
                        tlog().error("send_request: {} (stream_id={})",
                                     error_response.error_message, stream_id);
                        transport_error = true;
                    } else {
                        result = std::move(it->second);
                        responses_.erase(it);
                    }
                }
            }
        }

        if (transport_error) {
            if (attempt + 1 < config_.max_request_attempts) {
                tlog().warn("send_request: transport error '{}', retrying on a fresh connection",
                            error_response.error_message);
                continue;
            }
            return error_response;
        }

        // status_code 0 with no transport error == the peer refused/reset the
        // stream (commonly REFUSED_STREAM after a GOAWAY). Reconnect and retry.
        if (result.status_code == 0) {
            if (attempt + 1 < config_.max_request_attempts) {
                tlog().warn("send_request: stream_id={} completed with status_code=0 "
                            "(stream refused/reset by peer), retrying on a fresh connection "
                            "(attempt {} -> {})", stream_id, attempt, attempt + 1);
                continue;
            }
            tlog().error("send_request: status_code=0 after {} attempt(s) - peer keeps "
                         "refusing the stream", config_.max_request_attempts);
            return result;
        }

        tlog().debug("send_request: stream_id={} -> status={} ({} body byte(s))",
                     stream_id, result.status_code, result.body.size());
        return result;
    }

    return error_response;
}

// ─────────────────────────────────────────────────────────────────────────────
// URL Parsing
// ─────────────────────────────────────────────────────────────────────────────

bool Http2ClientTransport::parse_url(const std::string& url) {
    std::regex url_regex("(https?)://([^:/]+)(?::([0-9]+))?(/.*)?");
    std::smatch matches;

    if (!std::regex_match(url, matches, url_regex)) {
        return false;
    }

    std::string protocol = matches[1].str();
    host_ = matches[2].str();
    port_ = matches[3].matched ? matches[3].str()
                               : (protocol == "https" ? "443" : "80");
    base_path_ = matches[4].matched ? matches[4].str() : "/";

    // Ensure base_path ends with /
    if (!base_path_.empty() && base_path_.back() != '/') {
        base_path_ += '/';
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// nghttp2 Session Management
// ─────────────────────────────────────────────────────────────────────────────

bool Http2ClientTransport::initialize_nghttp2() {
    nghttp2_session_callbacks* callbacks = nullptr;
    nghttp2_session_callbacks_new(&callbacks);

    nghttp2_session_callbacks_set_on_frame_recv_callback(
        callbacks, on_frame_recv_callback);
    nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
        callbacks, on_data_chunk_recv_callback);
    nghttp2_session_callbacks_set_on_header_callback(
        callbacks, on_header_callback);
    nghttp2_session_callbacks_set_on_stream_close_callback(
        callbacks, on_stream_close_callback);

    int rv = nghttp2_session_client_new(&session_, callbacks, this);
    nghttp2_session_callbacks_del(callbacks);

    return rv == 0;
}

void Http2ClientTransport::cleanup_nghttp2() {
    if (session_) {
        nghttp2_session_del(session_);
        session_ = nullptr;
    }
}

bool Http2ClientTransport::is_connection_alive() {
    if (!socket_.is_open()) {
        return false;
    }

    // Peek to detect closed connection
    boost::system::error_code ec;
    char buf;
    socket_.non_blocking(true);
    socket_.receive(boost::asio::buffer(&buf, 1), boost::asio::socket_base::message_peek, ec);
    socket_.non_blocking(false);

    if (ec == boost::asio::error::would_block) {
        return true; // No data but connection is alive
    }
    if (ec) {
        return false; // Error means connection is dead
    }
    return true; // Data available means connection is alive
}

// ─────────────────────────────────────────────────────────────────────────────
// Request Submission
// ─────────────────────────────────────────────────────────────────────────────

int32_t Http2ClientTransport::submit_request(
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& headers,
    const std::string& body) {

    // Build the full path
    std::string full_path = path;
    if (full_path.empty() || full_path[0] != '/') {
        full_path = base_path_ + full_path;
    }

    // Collect every header (pseudo-headers first, as required by HTTP/2) into a
    // stable owning container, then build the nghttp2_nv array from references
    // into it.
    //
    // This ownership matters and is subtle. We use NGHTTP2_NV_FLAG_NONE, so
    // nghttp2 copies the name/value bytes at nghttp2_submit_request() time — every
    // nv.name/nv.value pointer must therefore stay valid until that call returns.
    // The previous version built the nv array with a make_nv(":method", ...)
    // helper taking `const std::string&`; passing a string *literal* bound it to a
    // temporary std::string that was destroyed at the end of the push_back
    // statement, leaving the pseudo-header *names* dangling. nghttp2 then copied
    // garbage names (observed on the wire as a header named "x" instead of
    // ":method"), and the peer rejected the malformed HEADERS with PROTOCOL_ERROR.
    // Keeping all names and values in `all_headers` (alive for the whole function)
    // avoids that — mirroring the original PcfClientWrapper, which built its nv
    // array from a live std::map.
    std::vector<std::pair<std::string, std::string>> all_headers;
    all_headers.reserve(4 + headers.size() + 1);

    // Pseudo-headers (must come first)
    all_headers.emplace_back(":method", method);
    all_headers.emplace_back(":path", full_path);
    all_headers.emplace_back(":scheme", config_.use_tls ? "https" : "http");
    all_headers.emplace_back(":authority", host_ + ":" + port_);

    // Regular headers
    bool has_content_type = false;
    for (const auto& [key, value] : headers) {
        all_headers.emplace_back(key, value);
        if (key == "content-type") {
            has_content_type = true;
        }
    }

    // Add default content-type if body present and not already set
    if (!body.empty() && !has_content_type) {
        all_headers.emplace_back("content-type", "application/json");
    }

    tlog().trace("submit_request: :method={} :scheme={} :authority={} :path={}",
                 method, config_.use_tls ? "https" : "http", host_ + ":" + port_, full_path);

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

    // Prepare data provider for body
    struct BodyData {
        const std::string* body;
        size_t offset;
    };

    int32_t stream_id;

    if (!body.empty()) {
        // We need to store the body data for the data provider callback
        auto* body_data = new BodyData{&body, 0};

        nghttp2_data_provider data_prd;
        data_prd.source.ptr = body_data;
        data_prd.read_callback = [](nghttp2_session* /*session*/,
                                    int32_t /*stream_id*/,
                                    uint8_t* buf,
                                    size_t length,
                                    uint32_t* data_flags,
                                    nghttp2_data_source* source,
                                    void* /*user_data*/) -> ssize_t {
            auto* bd = static_cast<BodyData*>(source->ptr);
            size_t remaining = bd->body->size() - bd->offset;
            size_t to_copy = std::min(length, remaining);

            if (to_copy > 0) {
                std::memcpy(buf, bd->body->data() + bd->offset, to_copy);
                bd->offset += to_copy;
            }

            if (bd->offset >= bd->body->size()) {
                *data_flags |= NGHTTP2_DATA_FLAG_EOF;
                delete bd;
            }

            return static_cast<ssize_t>(to_copy);
        };

        stream_id = nghttp2_submit_request(
            session_, nullptr, nva.data(), nva.size(), &data_prd, nullptr);
    } else {
        stream_id = nghttp2_submit_request(
            session_, nullptr, nva.data(), nva.size(), nullptr, nullptr);
    }

    if (stream_id < 0) {
        tlog().error("submit_request: nghttp2_submit_request failed rv={} ({})",
                     stream_id, nghttp2_strerror(stream_id));
        return -1;
    }

    // Prepare response accumulator for this stream
    {
        std::lock_guard<std::mutex> rlock(responses_mutex_);
        responses_[stream_id] = HttpResponse{};
    }

    tlog().trace("submit_request: queued {} header(s) on stream_id={} (body={} bytes)",
                 nva.size(), stream_id, body.size());
    return stream_id;
}

// ─────────────────────────────────────────────────────────────────────────────
// Network I/O
// ─────────────────────────────────────────────────────────────────────────────

bool Http2ClientTransport::flush_session() {
    const uint8_t* data_ptr;
    ssize_t len;

    size_t total_sent = 0;
    while ((len = nghttp2_session_mem_send(session_, &data_ptr)) > 0) {
        boost::system::error_code ec;
        boost::asio::write(socket_, boost::asio::buffer(data_ptr, len), ec);
        if (ec) {
            tlog().warn("flush_session: socket write failed after {} bytes - {}",
                        total_sent, ec.message());
            connected_ = false;
            return false;
        }
        total_sent += static_cast<size_t>(len);
    }

    if (len < 0) {
        tlog().warn("flush_session: nghttp2_session_mem_send failed rv={} ({})",
                    len, nghttp2_strerror(static_cast<int>(len)));
        return false;
    }

    if (total_sent > 0) {
        tlog().trace("flush_session: wrote {} byte(s) to socket", total_sent);
    }
    return true;
}

bool Http2ClientTransport::receive_and_process() {
    std::vector<uint8_t> buffer(16384);
    boost::system::error_code ec;

    size_t bytes_read = socket_.read_some(boost::asio::buffer(buffer), ec);
    if (ec) {
        if (ec != boost::asio::error::would_block) {
            connected_ = false;
        }
        return false;
    }

    if (bytes_read == 0) {
        connected_ = false;
        return false;
    }

    ssize_t rv = nghttp2_session_mem_recv(session_, buffer.data(), bytes_read);
    if (rv < 0) {
        return false;
    }

    // Send any pending frames (e.g., WINDOW_UPDATE)
    flush_session();
    return true;
}

bool Http2ClientTransport::wait_for_response(int32_t stream_id, int timeout_ms) {
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        // Check if response is complete
        {
            std::lock_guard<std::mutex> rlock(responses_mutex_);
            auto it = responses_.find(stream_id);
            if (it != responses_.end() && it->second.completed) {
                return true;
            }
        }

        // Try to read more data
        try {
            // Check if we have data available
            boost::system::error_code avail_ec;
            socket_.non_blocking(true);

            std::vector<uint8_t> buffer(16384);
            boost::system::error_code ec;
            size_t bytes_read = socket_.read_some(boost::asio::buffer(buffer), ec);

            socket_.non_blocking(false);

            if (!ec && bytes_read > 0) {
                ssize_t rv = nghttp2_session_mem_recv(session_, buffer.data(), bytes_read);
                if (rv < 0) {
                    return false;
                }
                flush_session();
            } else if (ec == boost::asio::error::would_block) {
                // No data available, sleep briefly
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            } else if (ec) {
                connected_ = false;
                return false;
            }
        } catch (const std::exception&) {
            return false;
        }
    }

    return false; // Timeout
}

// ─────────────────────────────────────────────────────────────────────────────
// nghttp2 Callbacks
// ─────────────────────────────────────────────────────────────────────────────

int Http2ClientTransport::on_frame_recv_callback(
    nghttp2_session* /*session*/,
    const nghttp2_frame* frame,
    void* user_data) {

    auto* transport = static_cast<Http2ClientTransport*>(user_data);

    tlog().trace("on_frame_recv: stream_id={} type={} flags=0x{:02x}",
                 frame->hd.stream_id, static_cast<int>(frame->hd.type), frame->hd.flags);

    if (frame->hd.type == NGHTTP2_GOAWAY) {
        // The peer will not process any stream with id > last_stream_id. Mark the
        // session unusable so the next send_request reconnects on a fresh session
        // instead of submitting onto a connection that will refuse the stream.
        transport->goaway_received_ = true;
        tlog().warn("on_frame_recv: received GOAWAY from peer (last_stream_id={}, error_code={}) "
                    "- marking session for reconnect",
                    frame->goaway.last_stream_id, frame->goaway.error_code);
    }

    if (frame->hd.type == NGHTTP2_DATA) {
        if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
            tlog().trace("on_frame_recv: END_STREAM on DATA for stream_id={}, marking complete",
                         frame->hd.stream_id);
            std::lock_guard<std::mutex> lock(transport->responses_mutex_);
            auto it = transport->responses_.find(frame->hd.stream_id);
            if (it != transport->responses_.end()) {
                it->second.completed = true;
            }
            transport->response_cv_.notify_all();
        }
    }

    return 0;
}

int Http2ClientTransport::on_data_chunk_recv_callback(
    nghttp2_session* /*session*/,
    uint8_t /*flags*/,
    int32_t stream_id,
    const uint8_t* data,
    size_t len,
    void* user_data) {

    auto* transport = static_cast<Http2ClientTransport*>(user_data);

    std::lock_guard<std::mutex> lock(transport->responses_mutex_);
    auto it = transport->responses_.find(stream_id);
    if (it != transport->responses_.end()) {
        it->second.body.append(reinterpret_cast<const char*>(data), len);
    }

    return 0;
}

int Http2ClientTransport::on_header_callback(
    nghttp2_session* /*session*/,
    const nghttp2_frame* frame,
    const uint8_t* name, size_t namelen,
    const uint8_t* value, size_t valuelen,
    uint8_t /*flags*/,
    void* user_data) {

    auto* transport = static_cast<Http2ClientTransport*>(user_data);

    if (frame->hd.type != NGHTTP2_HEADERS) {
        return 0;
    }

    std::string header_name(reinterpret_cast<const char*>(name), namelen);
    std::string header_value(reinterpret_cast<const char*>(value), valuelen);

    std::lock_guard<std::mutex> lock(transport->responses_mutex_);
    auto it = transport->responses_.find(frame->hd.stream_id);
    if (it != transport->responses_.end()) {
        if (header_name == ":status") {
            it->second.status_code = std::stoi(header_value);
            tlog().trace("on_header: stream_id={} :status={}",
                         frame->hd.stream_id, header_value);
        } else {
            it->second.headers[header_name] = header_value;
            tlog().trace("on_header: stream_id={} {}: {}",
                         frame->hd.stream_id, header_name, header_value);
        }
    }

    return 0;
}

int Http2ClientTransport::on_stream_close_callback(
    nghttp2_session* /*session*/,
    int32_t stream_id,
    uint32_t error_code,
    void* user_data) {

    auto* transport = static_cast<Http2ClientTransport*>(user_data);

    // error_code is an HTTP/2 error code (NGHTTP2_NO_ERROR == 0 means a clean
    // close). A non-zero code here is the smoking gun for "status_code=0"
    // responses: the peer reset the stream before sending a response.
    if (error_code != 0) {
        tlog().warn("on_stream_close: stream_id={} closed with error_code={} ({})",
                    stream_id, error_code,
                    nghttp2_http2_strerror(error_code));
    } else {
        tlog().trace("on_stream_close: stream_id={} closed cleanly (NO_ERROR)", stream_id);
    }

    std::lock_guard<std::mutex> lock(transport->responses_mutex_);
    auto it = transport->responses_.find(stream_id);
    if (it != transport->responses_.end()) {
        it->second.completed = true;
    }
    transport->response_cv_.notify_all();

    return 0;
}

} // namespace http
} // namespace communication
} // namespace af
