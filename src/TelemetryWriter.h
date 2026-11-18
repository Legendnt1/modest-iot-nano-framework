#ifndef TELEMETRY_WRITER_H
#define TELEMETRY_WRITER_H

// ReSharper disable once CppUnusedIncludeDirective
#include <cstdint>

/**
 * @file TelemetryWriter.h
 * @brief Declares the format-neutral telemetry serialization interface.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-18
 * @version 1.0
 */

/**
 * @brief Abstract interface for telemetry field serialization.
 *
 * Decouples TelemetryPackage subclasses from any specific serialization format
 * (JSON, CBOR, MessagePack, binary). Implementations handle the encoding;
 * telemetry packages only declare what data they carry via field() calls.
 *
 * @ingroup core_module
 */
class TelemetryWriter {
public:
    /** @brief Virtual destructor for safe polymorphic deletion. */
    virtual ~TelemetryWriter() = default;

    /**
     * @brief Writes an integer field.
     * @param name The field name.
     * @param value The field value.
     */
    virtual void field(const char* name, int32_t value) = 0;

    /**
     * @brief Writes a floating-point field.
     * @param name The field name.
     * @param value The field value.
     */
    virtual void field(const char* name, float value) = 0;

    /**
     * @brief Writes a boolean field.
     * @param name The field name.
     * @param value The field value.
     */
    virtual void field(const char* name, bool value) = 0;

    /**
     * @brief Writes a string field.
     * @param name The field name.
     * @param value The field value (null-terminated).
     */
    virtual void field(const char* name, const char* value) = 0;
};

#endif // TELEMETRY_WRITER_H
