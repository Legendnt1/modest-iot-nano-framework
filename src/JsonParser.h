#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <cstddef>
#include <cstdint>

/**
 * @file JsonParser.h
 * @brief Declares a native JSON parser for flat key-value payloads with zero heap allocation.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-18
 * @version 1.0
 */

/**
 * @brief Native JSON parser for flat key-value payloads.
 *
 * Parses simple JSON objects with scalar values (int, int32_t, float, bool,
 * string). Designed for cloud-to-device command parsing and HTTP response
 * extraction. No heap allocation — all operations use stack buffers and C
 * string functions.
 *
 * @ingroup core_module
 */
class JsonParser {
public:
    /**
     * @brief Extracts an integer value for the given key from a JSON string.
     * @param jsonString The null-terminated JSON string.
     * @param key The field name to search for.
     * @param outValue The pointer to store the extracted value.
     * @return True if the field was found and parsed successfully.
     */
    static bool parseInt(const char* jsonString, const char* key, int* outValue);

    /**
     * @brief Extracts an int32_t value for the given key from a JSON string.
     * @param jsonString The null-terminated JSON string.
     * @param key The field name to search for.
     * @param outValue The pointer to store the extracted value.
     * @return True if the field was found and parsed successfully.
     */
    static bool parseInt32(const char* jsonString, const char* key, int32_t* outValue);

    /**
     * @brief Extracts a float value for the given key from a JSON string.
     * @param jsonString The null-terminated JSON string.
     * @param key The field name to search for.
     * @param outValue The pointer to store the extracted value.
     * @return True if the field was found and parsed successfully.
     */
    static bool parseFloat(const char* jsonString, const char* key, float* outValue);

    /**
     * @brief Extracts a boolean value for the given key from a JSON string.
     *
     * Supports "true"/"false" strings and 1/0 integer values.
     * @param jsonString The null-terminated JSON string.
     * @param key The field name to search for.
     * @param outValue The pointer to store the extracted value.
     * @return True if the field was found and parsed successfully.
     */
    static bool parseBool(const char* jsonString, const char* key, bool* outValue);

    /**
     * @brief Extracts a string value for the given key from a JSON string.
     * @param jsonString The null-terminated JSON string.
     * @param key The field name to search for.
     * @param outBuffer The buffer to store the extracted string.
     * @param bufferSize The maximum size of the output buffer.
     * @return True if the field was found and copied successfully.
     */
    static bool parseString(const char* jsonString, const char* key, char* outBuffer, size_t bufferSize);
};

#endif // JSON_PARSER_H
