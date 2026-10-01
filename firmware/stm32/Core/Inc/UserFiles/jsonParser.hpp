#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>

// Finds "key": <number> in a flat JSON object and returns the number, e.g.
//   jsonNumber("{\"hum\":45,\"rpm\":1200}", "rpm")  ->  1200
// Returns std::nullopt when the key is missing or its value is not a number.
inline std::optional<float> jsonNumber(const char *json, const char *key) {

    // Build the text to look for: "key" (with the quotation marks)
    char pattern[24];
    int patternLen = std::snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    if (patternLen <= 0 || patternLen >= static_cast<int>(sizeof(pattern))) {
        return std::nullopt;
    }

    const char *p = std::strstr(json, pattern);
    if (p == nullptr) {
        return std::nullopt;
    }

    // Skip the key, optional spaces, and expect a colon
    p += patternLen;
    while (*p == ' ') {
        p++;
    }
    if (*p != ':') {
        return std::nullopt;
    }
    p++;

    // Read the number that follows (strtof skips the leading spaces itself)
    char *end;
    float value = std::strtof(p, &end);
    if (end == p) {
        return std::nullopt;   // no number here, e.g. true or a string
    }
    return value;
}
