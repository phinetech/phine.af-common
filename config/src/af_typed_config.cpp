/**
 * @file af_typed_config.cpp
 * @brief Typed AF configuration loader and validation helpers.
 */

#include "af_typed_config.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <yaml-cpp/yaml.h>

namespace af {
namespace config {
namespace {

std::string to_lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

template <typename T>
T read_or(const YAML::Node& node, const char* key, const T& default_value) {
    if (node && node[key]) {
        return node[key].as<T>();
    }

    return default_value;
}

EndpointConfig decode_endpoint(
    const YAML::Node& node,
    EndpointConfig defaults,
    const char* host_key,
    const char* port_key) {

    if (!node) {
        return defaults;
    }

    if (node[host_key]) {
        defaults.host = node[host_key].as<std::string>();
    }

    if (node[port_key]) {
        defaults.port = node[port_key].as<std::uint16_t>();
    }

    return defaults;
}

LoggingConfig decode_logging(const YAML::Node& node, LoggingConfig defaults) {
    if (!node) {
        return defaults;
    }

    if (node["level"]) {
        defaults.level = log_level_from_string(node["level"].as<std::string>());
    }

    defaults.console_output = read_or<bool>(node, "console_output", defaults.console_output);
    defaults.file_output = read_or<bool>(node, "file_output", defaults.file_output);

    if (node["file_path"]) {
        defaults.file_path = node["file_path"].as<std::string>();
    } else if (node["file"]) {
        defaults.file_path = node["file"].as<std::string>();
    }

    return defaults;
}

CommunicationConfig decode_communication(
    const YAML::Node& node,
    CommunicationConfig defaults,
    const char* legacy_remote_host_key = nullptr,
    const char* legacy_remote_port_key = nullptr) {

    if (!node) {
        return defaults;
    }

    if (node["kind"]) {
        defaults.kind = communication_kind_from_string(node["kind"].as<std::string>());
    } else if (node["type"]) {
        defaults.kind = communication_kind_from_string(node["type"].as<std::string>());
    }

    defaults.client_only = read_or<bool>(node, "client_only", defaults.client_only);

    if (node["listen"]) {
        defaults.listen = decode_endpoint(node["listen"], defaults.listen, "host", "port");
    }
    defaults.listen = decode_endpoint(node, defaults.listen, "server_address", "server_port");

    if (node["remote"]) {
        EndpointConfig remote_defaults = defaults.remote.value_or(EndpointConfig{});
        defaults.remote = decode_endpoint(node["remote"], remote_defaults, "host", "port");
    }

    if (legacy_remote_host_key != nullptr && legacy_remote_port_key != nullptr &&
        (node[legacy_remote_host_key] || node[legacy_remote_port_key])) {
        EndpointConfig remote_defaults = defaults.remote.value_or(EndpointConfig{});
        defaults.remote = decode_endpoint(
            node,
            remote_defaults,
            legacy_remote_host_key,
            legacy_remote_port_key);
    }

    return defaults;
}

PcfConnectionConfig decode_pcf_connection(const YAML::Node& node, PcfConnectionConfig defaults) {
    if (!node) {
        return defaults;
    }

    const YAML::Node pcf_node = node["pcf"] ? node["pcf"] : node;

    if (pcf_node["base_url"]) {
        defaults.base_url = pcf_node["base_url"].as<std::string>();
    } else if (pcf_node["pcf_base_url"]) {
        defaults.base_url = pcf_node["pcf_base_url"].as<std::string>();
    }

    defaults.use_tls = read_or<bool>(pcf_node, "use_tls", defaults.use_tls);
    defaults.api_version = read_or<std::string>(pcf_node, "api_version", defaults.api_version);

    return defaults;
}

QodConfig decode_qod(const YAML::Node& node, QodConfig defaults) {
    if (!node) {
        return defaults;
    }

    auto seconds_or = [&](const char* key, std::chrono::seconds fallback) {
        return node[key] ? std::chrono::seconds(node[key].as<long long>()) : fallback;
    };

    defaults.max_session_duration = seconds_or("max_session_duration", defaults.max_session_duration);
    defaults.min_session_duration = seconds_or("min_session_duration", defaults.min_session_duration);
    defaults.session_cleanup_interval = seconds_or("session_cleanup_interval", defaults.session_cleanup_interval);
    defaults.unavailable_session_ttl = seconds_or("unavailable_session_ttl", defaults.unavailable_session_ttl);
    defaults.enable_notifications = read_or<bool>(node, "enable_notifications", defaults.enable_notifications);
    defaults.api_base_url = read_or<std::string>(node, "api_base_url", defaults.api_base_url);

    return defaults;
}

void validate_endpoint(const EndpointConfig& endpoint, const std::string& field_name) {
    if (endpoint.host.empty()) {
        throw std::runtime_error(field_name + " host must not be empty");
    }

    if (endpoint.port == 0) {
        throw std::runtime_error(field_name + " port must be greater than 0");
    }
}

} // namespace

std::string to_string(CommunicationKind kind) {
    switch (kind) {
        case CommunicationKind::Grpc:
            return "grpc";
        case CommunicationKind::Direct:
            return "direct";
    }

    throw std::runtime_error("Unsupported communication kind");
}

CommunicationKind communication_kind_from_string(const std::string& value) {
    const auto normalized = to_lower(value);
    if (normalized == "grpc") {
        return CommunicationKind::Grpc;
    }

    if (normalized == "direct") {
        return CommunicationKind::Direct;
    }

    throw std::runtime_error("Unsupported communication kind: " + value);
}

spdlog::level::level_enum log_level_from_string(const std::string& value) {
    const auto normalized = to_lower(value);
    if (normalized == "trace") {
        return spdlog::level::trace;
    }
    if (normalized == "debug") {
        return spdlog::level::debug;
    }
    if (normalized == "info") {
        return spdlog::level::info;
    }
    if (normalized == "warn" || normalized == "warning") {
        return spdlog::level::warn;
    }
    if (normalized == "error") {
        return spdlog::level::err;
    }
    if (normalized == "critical") {
        return spdlog::level::critical;
    }

    throw std::runtime_error("Unsupported log level: " + value);
}

std::string endpoint_to_string(const EndpointConfig& endpoint) {
    return endpoint.host + ":" + std::to_string(endpoint.port);
}

std::string resolve_destination(
    CommunicationKind kind,
    const EndpointConfig& endpoint,
    const std::string& direct_service_name) {

    if (kind == CommunicationKind::Direct) {
        return direct_service_name;
    }

    std::string host = endpoint.host;
    if (host.empty() || host == "0.0.0.0") {
        host = direct_service_name;
    }

    return host + ":" + std::to_string(endpoint.port);
}

AppConfig load_app_config(const std::string& path) {
    AppConfig config;
    const YAML::Node root = YAML::LoadFile(path);

    if (root["af_core"]) {
        const auto af_core = root["af_core"];
        config.af_core.logging = decode_logging(af_core["logging"], config.af_core.logging);
        config.af_core.communication = decode_communication(af_core["communication"], config.af_core.communication);
        config.af_core.qod = decode_qod(af_core["qod"], config.af_core.qod);
    }

    if (root["pcf_handler"]) {
        const auto pcf_handler = root["pcf_handler"];
        config.pcf_handler.pcf = decode_pcf_connection(pcf_handler, config.pcf_handler.pcf);
        config.pcf_handler.logging = decode_logging(pcf_handler["logging"], config.pcf_handler.logging);
        config.pcf_handler.communication = decode_communication(
            pcf_handler["communication"],
            config.pcf_handler.communication,
            "core_address",
            "core_port");
    }

    return config;
}

void validate(const AppConfig& config) {
    validate_endpoint(config.af_core.communication.listen, "af_core.communication.listen");
    validate_endpoint(config.pcf_handler.communication.listen, "pcf_handler.communication.listen");

    if (config.pcf_handler.communication.kind != CommunicationKind::Direct &&
        !config.pcf_handler.communication.remote.has_value()) {
        throw std::runtime_error("pcf_handler.communication.remote must be configured for non-direct communication");
    }

    if (config.pcf_handler.communication.remote.has_value()) {
        validate_endpoint(*config.pcf_handler.communication.remote, "pcf_handler.communication.remote");
    }

    if (config.af_core.qod.min_session_duration <= std::chrono::seconds::zero()) {
        throw std::runtime_error("af_core.qod.min_session_duration must be greater than 0");
    }

    if (config.af_core.qod.max_session_duration < config.af_core.qod.min_session_duration) {
        throw std::runtime_error("af_core.qod.max_session_duration must be >= min_session_duration");
    }

    if (config.af_core.qod.session_cleanup_interval <= std::chrono::seconds::zero()) {
        throw std::runtime_error("af_core.qod.session_cleanup_interval must be greater than 0");
    }

    if (config.pcf_handler.pcf.base_url.empty()) {
        throw std::runtime_error("pcf_handler.pcf.base_url must not be empty");
    }

    if (config.pcf_handler.pcf.api_version.empty()) {
        throw std::runtime_error("pcf_handler.pcf.api_version must not be empty");
    }
}

} // namespace config
} // namespace af