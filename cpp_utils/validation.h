/**
 * @file validation.h
 * @brief Common validation result types and utilities
 *
 * Provides a ValidationResult type for collecting validation errors and
 * helper functions for formatting error messages.
 */

#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace af {
namespace utils {

/**
 * @brief Result of validation operations
 *
 * Accumulates validation errors and provides utilities for error formatting.
 */
struct ValidationResult {
    bool is_valid = true;
    std::vector<std::string> errors;

    /**
     * @brief Default constructor
     */
    ValidationResult() = default;

    /**
     * @brief Construct with initial validity state
     */
    explicit ValidationResult(bool valid) : is_valid(valid) {}

    /**
     * @brief Add an error message
     */
    void add_error(const std::string& error) {
        is_valid = false;
        errors.push_back(error);
    }

    /**
     * @brief Merge another validation result
     * @param other The result to merge
     */
    void merge(const ValidationResult& other) {
        if (!other.is_valid) {
            is_valid = false;
            errors.insert(errors.end(), other.errors.begin(), other.errors.end());
        }
    }

    /**
     * @brief Get error message as a single string
     * @param separator String to separate multiple errors (default: "; ")
     * @return Combined error message
     */
    std::string get_error_message(const std::string& separator = "; ") const {
        if (errors.empty()) {
            return "";
        }

        std::ostringstream oss;
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i > 0) {
                oss << separator;
            }
            oss << errors[i];
        }
        return oss.str();
    }

    /**
     * @brief Get formatted error message with prefix
     * @param prefix Prefix to add before errors
     * @param separator String to separate multiple errors (default: "; ")
     * @return Formatted error message
     */
    std::string format_errors(const std::string& prefix, const std::string& separator = "; ") const {
        if (errors.empty()) {
            return "";
        }

        std::ostringstream oss;
        oss << prefix;
        if (!prefix.empty() && !errors.empty()) {
            oss << ": ";
        }
        oss << get_error_message(separator);
        return oss.str();
    }

    /**
     * @brief Get number of errors
     */
    size_t error_count() const {
        return errors.size();
    }

    /**
     * @brief Check if there are any errors
     */
    bool has_errors() const {
        return !is_valid;
    }

    /**
     * @brief Clear all errors and reset to valid state
     */
    void clear() {
        is_valid = true;
        errors.clear();
    }

    /**
     * @brief Allow implicit conversion to bool
     */
    operator bool() const {
        return is_valid;
    }
};

/**
 * @brief Helper function to combine multiple validation error messages
 * @param prefix Prefix for the combined message
 * @param results Vector of validation results to combine
 * @param separator String to separate errors (default: "; ")
 * @return Combined error message
 */
inline std::string combine_validation_errors(
    const std::string& prefix,
    const std::vector<ValidationResult>& results,
    const std::string& separator = "; ") {

    std::ostringstream oss;
    oss << prefix;

    bool has_errors = false;
    for (const auto& result : results) {
        if (!result.is_valid) {
            has_errors = true;
            break;
        }
    }

    if (!has_errors) {
        return "";
    }

    if (!prefix.empty()) {
        oss << ": ";
    }

    bool first = true;
    for (const auto& result : results) {
        for (const auto& error : result.errors) {
            if (!first) {
                oss << separator;
            }
            oss << error;
            first = false;
        }
    }

    return oss.str();
}

/**
 * @brief Helper function to append errors from one result to a stream
 * @param stream Output stream
 * @param result Validation result
 * @param separator String to separate errors (default: "; ")
 */
inline void append_validation_errors(
    std::ostringstream& stream,
    const ValidationResult& result,
    const std::string& separator = "; ") {

    for (const auto& error : result.errors) {
        stream << separator << error;
    }
}

} // namespace utils
} // namespace af
