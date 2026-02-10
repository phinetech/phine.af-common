#pragma once

#include "qod_session.h"
#include "cpp_utils/validation.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace af {
namespace common {
namespace qod {

// Import ValidationResult from common utils
using ValidationResult = af::utils::ValidationResult;

void to_json(nlohmann::json& json, const QosProfileMapping& mapping);
void from_json(const nlohmann::json& json, QosProfileMapping& mapping);

/**
 * @brief Helper class for QoS profile mapping serialization/deserialization
 */
class QosProfileMapper {
public:
    /**
     * @brief Serialize QosProfileMapping to JSON
     * @param mapping The mapping to serialize
     * @return JSON object representing the mapping
     */
    static nlohmann::json serialize_qos_mapping(const QosProfileMapping& mapping) {
        nlohmann::json j;
        to_json(j, mapping);
        return j;
    }

    /**
     * @brief Deserialize QosProfileMapping from JSON
     * @param json The JSON object to deserialize
     * @return Deserialized QosProfileMapping
     */
    /**
     * @brief Deserialize QosProfileMapping from JSON
     * @param json The JSON object to deserialize
     * @return Deserialized QosProfileMapping
     */
    static QosProfileMapping deserialize_qos_mapping(const nlohmann::json& json) {
        QosProfileMapping mapping;
        ValidationResult result = try_deserialize_qos_mapping(json, mapping);
        if (!result.is_valid) {
            throw std::invalid_argument(result.format_errors("Invalid qos_profile_mapping"));
        }
        return mapping;
    }

    /**
     * @brief Deserialize QosProfileMapping from JSON without throwing
     * @param json The JSON object to deserialize
     * @param mapping Output mapping
     * @return Result of validation
     */
    static ValidationResult try_deserialize_qos_mapping(const nlohmann::json& json, QosProfileMapping& mapping) {
        ValidationResult result;

        if (!json.is_object()) {
            result.add_error("qos_profile_mapping must be an object");
            return result;
        }

        if (!json.contains("fiveqi") || !json["fiveqi"].is_number_integer()) {
            result.add_error("fiveqi must be an integer");
        } else {
            mapping.fiveqi = json["fiveqi"].get<int>();
        }

        if (!json.contains("is_gbr") || !json["is_gbr"].is_boolean()) {
            result.add_error("is_gbr must be a boolean");
        } else {
            mapping.is_gbr = json["is_gbr"].get<bool>();
        }

        if (json.contains("priority_level") && !json["priority_level"].is_null()) {
            if (!json["priority_level"].is_number_integer()) {
                result.add_error("priority_level must be an integer");
            } else {
                mapping.priority_level = json["priority_level"].get<int>();
            }
        }

        if (json.contains("packet_delay_budget") && !json["packet_delay_budget"].is_null()) {
            if (!json["packet_delay_budget"].is_number_integer()) {
                result.add_error("packet_delay_budget must be an integer");
            } else {
                mapping.packet_delay_budget = json["packet_delay_budget"].get<int>();
            }
        }

        if (json.contains("packet_error_rate") && !json["packet_error_rate"].is_null()) {
            if (!json["packet_error_rate"].is_number()) {
                result.add_error("packet_error_rate must be a number");
            } else {
                mapping.packet_error_rate = json["packet_error_rate"].get<double>();
            }
        }

        if (json.contains("max_data_burst_volume") && !json["max_data_burst_volume"].is_null()) {
            if (!json["max_data_burst_volume"].is_number_integer()) {
                result.add_error("max_data_burst_volume must be an integer");
            } else {
                mapping.max_data_burst_volume = json["max_data_burst_volume"].get<int>();
            }
        }

        if (json.contains("guaranteed_uplink_rate") && !json["guaranteed_uplink_rate"].is_null()) {
            if (!json["guaranteed_uplink_rate"].is_string()) {
                result.add_error("guaranteed_uplink_rate must be a string");
            } else {
                mapping.guaranteed_uplink_rate = json["guaranteed_uplink_rate"].get<std::string>();
            }
        }

        if (json.contains("guaranteed_downlink_rate") && !json["guaranteed_downlink_rate"].is_null()) {
            if (!json["guaranteed_downlink_rate"].is_string()) {
                result.add_error("guaranteed_downlink_rate must be a string");
            } else {
                mapping.guaranteed_downlink_rate = json["guaranteed_downlink_rate"].get<std::string>();
            }
        }

        if (json.contains("max_uplink_rate") && !json["max_uplink_rate"].is_null()) {
            if (!json["max_uplink_rate"].is_string()) {
                result.add_error("max_uplink_rate must be a string");
            } else {
                mapping.max_uplink_rate = json["max_uplink_rate"].get<std::string>();
            }
        }

        if (json.contains("max_downlink_rate") && !json["max_downlink_rate"].is_null()) {
            if (!json["max_downlink_rate"].is_string()) {
                result.add_error("max_downlink_rate must be a string");
            } else {
                mapping.max_downlink_rate = json["max_downlink_rate"].get<std::string>();
            }
        }

        return result;
    }

    /**
     * @brief Validate QosProfileMapping
     * @param mapping The mapping to validate
     * @return Result of validation
     */
    static ValidationResult validate_qos_mapping(const QosProfileMapping& mapping) {
        ValidationResult result;

        // Basic validation
        if (mapping.fiveqi < 0 || mapping.fiveqi > 255) { // 5QI range
            result.add_error("Invalid 5QI value: " + std::to_string(mapping.fiveqi));
        }

        if (mapping.priority_level) {
            // TS 29.514 ReservPriority
            if (*mapping.priority_level < 1 || *mapping.priority_level > 16) {
                result.add_error("Invalid priority level: " + std::to_string(*mapping.priority_level));
            }
        }

        if (mapping.packet_error_rate && *mapping.packet_error_rate < 0.0) {
            result.add_error("packet_error_rate must be non-negative");
        }

        if (mapping.is_gbr) {
            if (!mapping.guaranteed_uplink_rate || !mapping.guaranteed_downlink_rate) {
                result.add_error("GBR mapping requires guaranteed_uplink_rate and guaranteed_downlink_rate");
            }
        }

        auto validate_bitrate = [&result](const std::optional<std::string>& value, const std::string& field) {
            if (!value) {
                return;
            }
            static const std::regex pattern(R"(^\d+(\.\d+)? (bps|Kbps|Mbps|Gbps|Tbps)$)");
            if (!std::regex_match(*value, pattern)) {
                result.add_error(field + " must match BitRate format");
            }
        };

        validate_bitrate(mapping.guaranteed_uplink_rate, "guaranteed_uplink_rate");
        validate_bitrate(mapping.guaranteed_downlink_rate, "guaranteed_downlink_rate");
        validate_bitrate(mapping.max_uplink_rate, "max_uplink_rate");
        validate_bitrate(mapping.max_downlink_rate, "max_downlink_rate");

        return result;
    }
};

inline void to_json(nlohmann::json& json, const QosProfileMapping& mapping) {
    json = nlohmann::json::object();
    json["fiveqi"] = mapping.fiveqi;
    json["is_gbr"] = mapping.is_gbr;

    if (mapping.priority_level) json["priority_level"] = *mapping.priority_level;
    if (mapping.packet_delay_budget) json["packet_delay_budget"] = *mapping.packet_delay_budget;
    if (mapping.packet_error_rate) json["packet_error_rate"] = *mapping.packet_error_rate;
    if (mapping.max_data_burst_volume) json["max_data_burst_volume"] = *mapping.max_data_burst_volume;

    if (mapping.guaranteed_uplink_rate) json["guaranteed_uplink_rate"] = *mapping.guaranteed_uplink_rate;
    if (mapping.guaranteed_downlink_rate) json["guaranteed_downlink_rate"] = *mapping.guaranteed_downlink_rate;
    if (mapping.max_uplink_rate) json["max_uplink_rate"] = *mapping.max_uplink_rate;
    if (mapping.max_downlink_rate) json["max_downlink_rate"] = *mapping.max_downlink_rate;
}

inline void from_json(const nlohmann::json& json, QosProfileMapping& mapping) {
    mapping.fiveqi = json.at("fiveqi").get<int>();
    mapping.is_gbr = json.at("is_gbr").get<bool>();

    if (json.contains("priority_level") && !json["priority_level"].is_null()) {
        mapping.priority_level = json["priority_level"].get<int>();
    }
    if (json.contains("packet_delay_budget") && !json["packet_delay_budget"].is_null()) {
        mapping.packet_delay_budget = json["packet_delay_budget"].get<int>();
    }
    if (json.contains("packet_error_rate") && !json["packet_error_rate"].is_null()) {
        mapping.packet_error_rate = json["packet_error_rate"].get<double>();
    }
    if (json.contains("max_data_burst_volume") && !json["max_data_burst_volume"].is_null()) {
        mapping.max_data_burst_volume = json["max_data_burst_volume"].get<int>();
    }

    if (json.contains("guaranteed_uplink_rate") && !json["guaranteed_uplink_rate"].is_null()) {
        mapping.guaranteed_uplink_rate = json["guaranteed_uplink_rate"].get<std::string>();
    }
    if (json.contains("guaranteed_downlink_rate") && !json["guaranteed_downlink_rate"].is_null()) {
        mapping.guaranteed_downlink_rate = json["guaranteed_downlink_rate"].get<std::string>();
    }
    if (json.contains("max_uplink_rate") && !json["max_uplink_rate"].is_null()) {
        mapping.max_uplink_rate = json["max_uplink_rate"].get<std::string>();
    }
    if (json.contains("max_downlink_rate") && !json["max_downlink_rate"].is_null()) {
        mapping.max_downlink_rate = json["max_downlink_rate"].get<std::string>();
    }
}

} // namespace qod
} // namespace common
} // namespace af
