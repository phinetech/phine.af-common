/**
 * @file af_typed_config.hpp
 * @brief Typed AF configuration models and helpers.
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <spdlog/spdlog.h>
#include <string>

namespace af {
namespace config {

enum class CommunicationKind {
    Grpc,
    Direct
};

struct EndpointConfig {
    std::string host{"0.0.0.0"};
    std::uint16_t port{0};
};

struct LoggingConfig {
    spdlog::level::level_enum level{spdlog::level::info};
    bool console_output{true};
    bool file_output{false};
    std::string file_path;
};

struct CommunicationConfig {
    CommunicationKind kind{CommunicationKind::Grpc};
    EndpointConfig listen{"0.0.0.0", 50051};
    std::optional<EndpointConfig> remote;
    bool client_only{false};
};

struct PcfConnectionConfig {
    std::string base_url{"http://pcf:80/npcf-policyauthorization/v1"};
    bool use_tls{false};
    std::string api_version{"v1"};
};

struct QodConfig {
    std::chrono::seconds max_session_duration{86400};
    std::chrono::seconds min_session_duration{60};
    std::chrono::seconds session_cleanup_interval{60};
    std::chrono::seconds unavailable_session_ttl{360};
    bool enable_notifications{true};
    std::string api_base_url{"https://api.example.com/quality-on-demand/v1"};
};

struct AfCoreConfig {
    LoggingConfig logging{spdlog::level::info, true, false, "/app/logs/af_core.log"};
    CommunicationConfig communication{CommunicationKind::Grpc, {"0.0.0.0", 50051}, std::nullopt, false};
    QodConfig qod{};
};

struct PcfHandlerConfig {
    bool enabled{true};
    PcfConnectionConfig pcf{};
    LoggingConfig logging{spdlog::level::info, true, false, "/app/logs/pcf_handler.log"};
    CommunicationConfig communication{CommunicationKind::Grpc, {"0.0.0.0", 50055}, EndpointConfig{"af_core", 50051}, false};
};

struct AppConfig {
    AfCoreConfig af_core{};
    PcfHandlerConfig pcf_handler{};
};

inline std::string port_to_string(std::uint16_t port) {
    return std::to_string(static_cast<unsigned int>(port));
}

std::string to_string(CommunicationKind kind);

CommunicationKind communication_kind_from_string(const std::string& value);
spdlog::level::level_enum log_level_from_string(const std::string& value);

std::string endpoint_to_string(const EndpointConfig& endpoint);
std::string resolve_destination(
    CommunicationKind kind,
    const EndpointConfig& endpoint,
    const std::string& direct_service_name);

AppConfig load_app_config(const std::string& path);
void validate(const AppConfig& config);

} // namespace config
} // namespace af