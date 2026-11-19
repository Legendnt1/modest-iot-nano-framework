/**
 * @file JsonParser.cpp
 * @brief Implements native JSON parser for flat key-value payloads.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-18
 * @version 1.0
 */

#include "JsonParser.h"
#include <cstdlib>
#include <cstring>

// --- Private Helpers ---

/**
 * @brief Finds the given key in a JSON string and returns a pointer to the
 *        value start position (after the colon and any whitespace).
 * @param jsonString The null-terminated JSON string.
 * @param key The field name to search for.
 * @return Pointer to the value start position, or nullptr if not found.
 */
static const char* findJsonValueByKey(const char* jsonString, const char* key) {
    if (!jsonString || !key) {
        return nullptr;
    }

    const char* pos = strstr(jsonString, "\"");
    while (pos != nullptr) {
        pos = pos + 1; // Skip opening quote of the key
        const char* keyStart = pos;
        const char* keyEnd = strchr(pos, '"');
        if (keyEnd == nullptr) break;

        if (const size_t keyLen = static_cast<size_t>(keyEnd - keyStart);
            strlen(key) == keyLen && strncmp(keyStart, key, keyLen) == 0) {
            // Found the key — advance past ": " to the value
            const char* valueStart = keyEnd + 1;
            while (*valueStart == ' ' || *valueStart == ':') {
                valueStart++;
            }
            return valueStart;
        }
        // Look for the next key
        pos = strchr(keyEnd, '"');
    }
    return nullptr;
}

// --- Public API ---

bool JsonParser::parseInt(const char* jsonString, const char* key, int* outValue) {
    if (!outValue) {
        return false;
    }
    const char* valueStart = findJsonValueByKey(jsonString, key);
    if (valueStart == nullptr) {
        return false;
    }
    char* endPtr;
    const long val = strtol(valueStart, &endPtr, 10);
    if (endPtr != valueStart) {
        *outValue = static_cast<int>(val);
        return true;
    }
    return false;
}

bool JsonParser::parseInt32(const char* jsonString, const char* key, int32_t* outValue) {
    if (!outValue) {
        return false;
    }
    int intValue;
    if (!parseInt(jsonString, key, &intValue)) {
        return false;
    }
    *outValue = static_cast<int32_t>(intValue);
    return true;
}

bool JsonParser::parseFloat(const char* jsonString, const char* key, float* outValue) {
    if (!outValue) {
        return false;
    }
    const char* valueStart = findJsonValueByKey(jsonString, key);
    if (valueStart == nullptr) {
        return false;
    }
    char* endPtr;
    const double val = strtod(valueStart, &endPtr);
    if (endPtr != valueStart) {
        *outValue = static_cast<float>(val);
        return true;
    }
    return false;
}

bool JsonParser::parseBool(const char* jsonString, const char* key, bool* outValue) {
    if (!outValue) {
        return false;
    }
    const char* valueStart = findJsonValueByKey(jsonString, key);
    if (valueStart == nullptr) {
        return false;
    }
    // Check for "true" or "false"
    if (strncmp(valueStart, "true", 4) == 0) {
        *outValue = true;
        return true;
    } else if (strncmp(valueStart, "false", 5) == 0) {
        *outValue = false;
        return true;
    }
    // Check for 1 or 0
    char* endPtr;
    const long val = strtol(valueStart, &endPtr, 10);
    if (endPtr != valueStart) {
        *outValue = (val != 0);
        return true;
    }
    return false;
}

bool JsonParser::parseString(const char* jsonString, const char* key, char* outBuffer, size_t bufferSize) {
    if (!outBuffer || bufferSize == 0) {
        return false;
    }
    const char* valueStart = findJsonValueByKey(jsonString, key);
    if (valueStart == nullptr) {
        return false;
    }
    // String value should start with a quote
    if (*valueStart != '"') {
        return false;
    }
    valueStart++; // Skip opening quote
    const char* valueEnd = strchr(valueStart, '"');
    if (!valueEnd) {
        return false;
    }
    const size_t valueLen = static_cast<size_t>(valueEnd - valueStart);
    const size_t copyLen = (valueLen < bufferSize - 1) ? valueLen : bufferSize - 1;
    memcpy(outBuffer, valueStart, copyLen);
    outBuffer[copyLen] = '\0';
    return true;
}
