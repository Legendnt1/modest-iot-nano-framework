#ifndef JSON_TELEMETRY_WRITER_H
#define JSON_TELEMETRY_WRITER_H

#include <cstddef>
#include "TelemetryWriter.h"

/**
 * @file JsonTelemetryWriter.h
 * @brief Declares a snprintf-based JSON telemetry writer with zero heap allocation.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-18
 * @version 1.0
 */

/**
 * @brief Maximum size of the JSON serialization buffer.
 *
 * Matches MAX_TELEMETRY_PAYLOAD_SIZE in MqttGatewayClient (ADR-011).
 * Ample for 3-5 scalar fields (~200 bytes typical).
 */
static constexpr size_t JSON_TELEMETRY_BUFFER_SIZE = 512;

/**
 * @brief snprintf-based JSON implementation of TelemetryWriter.
 *
 * Serializes telemetry fields to a fixed-size stack buffer using snprintf,
 * eliminating all heap allocation. The buffer is 512 bytes, sufficient for
 * the framework's typical telemetry payloads (3-5 scalar fields).
 *
 * Usage:
 * @code
 * JsonTelemetryWriter writer;
 * writer.field("timestamp", 12345);
 * writer.field("sequence", 1);
 * writer.field("temperature", 23.5f);
 * writer.finalize();
 * const char* json = writer.getString();
 * @endcode
 *
 * @ingroup core_module
 */
class JsonTelemetryWriter : public TelemetryWriter {
public:
    /**
     * @brief Constructs a new JsonTelemetryWriter and initializes the JSON object.
     */
    JsonTelemetryWriter();

    /** @brief Inherit field overloads from TelemetryWriter. */
    using TelemetryWriter::field;

    /**
     * @brief Writes an integer field.
     * @param name The field name.
     * @param value The field value.
     */
    void field(const char* name, int32_t value) override;

    /**
     * @brief Writes a floating-point field.
     * @param name The field name.
     * @param value The field value.
     */
    void field(const char* name, float value) override;

    /**
     * @brief Writes a boolean field.
     * @param name The field name.
     * @param value The field value.
     */
    void field(const char* name, bool value) override;

    /**
     * @brief Writes a string field.
     * @param name The field name.
     * @param value The field value (null-terminated).
     */
    void field(const char* name, const char* value) override;

    /**
     * @brief Finalizes the JSON object by closing the brace.
     *
     * Must be called after all field() calls to produce valid JSON.
     * No further fields may be added after finalize().
     */
    void finalize();

    /**
     * @brief Returns the serialized JSON string.
     * @return Null-terminated JSON string, or empty string if buffer overflow occurred.
     * @note The string is valid until the JsonTelemetryWriter is destroyed or reset.
     */
    [[nodiscard]] const char* getString() const;

    /**
     * @brief Checks whether a buffer overflow occurred during serialization.
     * @return True if the buffer was full and serialization stopped.
     */
    [[nodiscard]] bool hasOverflow() const;

private:
    char buffer[JSON_TELEMETRY_BUFFER_SIZE]; ///< Fixed-size serialization buffer.
    size_t position = 0; ///< Current write position in the buffer.
    bool firstField = true; ///< True if no fields have been written yet.
    bool overflow = false; ///< True if the buffer was full during serialization.
    bool finalized = false; ///< True if finalize() has been called.

    /**
     * @brief Writes a comma separator before the next field if needed.
     */
    void writeCommaIfNeeded();

    /**
     * @brief Writes the field name with quotes and colon.
     * @param name The field name.
     * @return False if buffer overflow occurred.
     */
    bool writeFieldName(const char* name);
};

#endif // JSON_TELEMETRY_WRITER_H
