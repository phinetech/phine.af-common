/**
 * @file path_pattern.h
 * @brief Shared utility for converting URL path patterns to regex strings.
 *
 * Supports two wildcard styles:
 *   - {param}  named placeholder, becomes a capturing group ([^/]+)
 *   - *        anonymous segment, becomes a non-capturing group [^/]+
 *
 * All regex metacharacters in the literal parts of the pattern are escaped
 * before substitution so that routes like "/v1/sessions.active" are safe.
 *
 * Usage (transport layer — existence check only):
 *   std::regex re("^" + path_pattern::to_regex(pattern) + "$");
 *   bool matched = std::regex_match(path_only, re);
 *
 * Usage (rest_router — extract named captures):
 *   std::string re_str = path_pattern::to_regex(pattern);
 *   // Then extract capture groups and correlate with param names parsed
 *   // separately from the original pattern.
 */

#pragma once

#include <regex>
#include <string>

namespace af {
namespace communication {
namespace http {
namespace path_pattern {

/**
 * @brief Convert a path pattern to a regex string (no anchors).
 *
 * The caller is responsible for adding ^ / $ anchors if a full-path
 * match is required.
 *
 * @param pattern  Path pattern, e.g. "/sessions/{sessionId}/extend" or
 *                 "/sessions/*"
 * @return         Regex string with placeholders replaced by capture groups.
 */
inline std::string to_regex(const std::string& pattern) {
    // Pass 1: escape regex metacharacters in literal parts.
    // { } and * are intentionally left unescaped — they are replaced in
    // passes 2 and 3 below.
    std::string escaped;
    escaped.reserve(pattern.size() * 2);
    for (char c : pattern) {
        switch (c) {
            case '.': case '+': case '?': case '^': case '$':
            case '(': case ')': case '[': case ']': case '|': case '\\':
                escaped += '\\';
                break;
            default:
                break;
        }
        escaped += c;
    }

    // Pass 2: replace {param} placeholders with capturing groups.
    static const std::regex brace_re(R"(\{[^}]+\})");
    std::string after_braces =
        std::regex_replace(escaped, brace_re, "([^/]+)");

    // Pass 3: replace bare * wildcards with a non-capturing segment match.
    std::string result;
    result.reserve(after_braces.size() * 2);
    for (char c : after_braces) {
        if (c == '*') {
            result += "[^/]+";
        } else {
            result += c;
        }
    }

    return result;
}

/**
 * @brief Return true if @p path matches @p pattern.
 *
 * Convenience wrapper — anchors the regex and performs a full-string match.
 */
inline bool matches(const std::string& pattern, const std::string& path) {
    const std::regex re("^" + to_regex(pattern) + "$");
    return std::regex_match(path, re);
}

} // namespace path_pattern
} // namespace http
} // namespace communication
} // namespace af
