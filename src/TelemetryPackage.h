#ifndef TELEMETRY_PACKAGE_H
#define TELEMETRY_PACKAGE_H

/**
 * @file TelemetryPackage.h
 * @brief Declares the abstract telemetry payload contract used by gateway clients.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-06-06
 * @version 1.0
 */

#include <Arduino.h>
// ReSharper disable once CppUnusedIncludeDirective
#include <cstring>
#include "TelemetryWriter.h"
#include "JsonTelemetryWriter.h"

/**
 * @brief Polymorphic payload contract for telemetry serialization.
 *
 * Subclasses implement write(TelemetryWriter&) to declare data fields without
 * knowing the serialization format. The base class provides convenience methods
 * for JSON string serialization using JsonTelemetryWriter.
 *
 * @ingroup core_module
 */
class TelemetryPackage {
protected:
    uint32_t captureTimestamp = 0; ///< Capture time in milliseconds from device boot.
    uint32_t sequenceNumber = 0; ///< Monotonically increasing sequence assigned before enqueue.
    const char* schemaVersionValue = nullptr; ///< Optional schema version string for telemetry format tracking.
    const char* firmwareVersionValue = nullptr; ///< Optional firmware version string for fleet management.
    bool highPriority = false; ///< True when this package is high-priority (set by Device::enqueueHighPriorityPayload).

public:
    /**
     * @brief Sets the envelope metadata values.
     * @param timestamp Capture time in milliseconds from device boot.
     * @param sequence Monotonically increasing sequence number.
     * @note Called by Device before enqueueing to assign metadata.
     */
    void setEnvelopeMetadata(const uint32_t timestamp, const uint32_t sequence) {
        captureTimestamp = timestamp;
        sequenceNumber = sequence;
    }

    /**
     * @brief Sets the optional schema and firmware version strings.
     * @param schemaVersion Optional schema version identifier (e.g., "1.0").
     * @param firmwareVersion Optional firmware version identifier (e.g., "1.0.0").
     * @note Pass nullptr to omit a field from the output. Called by Device
     *   before enqueueing when firmware version tracking is enabled.
     */
    void setVersionMetadata(const char* schemaVersion, const char* firmwareVersion) {
        schemaVersionValue = schemaVersion;
        firmwareVersionValue = firmwareVersion;
    }

    /**
     * @brief Marks this package as high-priority.
     *
     * When set, the telemetry envelope includes `"priority": "high"` to enable
     * edge-side routing for high-priority alerts. Called internally by
     * Device::enqueueHighPriorityPayload(); applications should not call this method directly.
     */
    void markAsHighPriority() { highPriority = true; }

    /**
     * @brief Returns whether this package is marked as high-priority.
     * @return `true` when the high-priority flag is set.
     */
    [[nodiscard]] bool isHighPriority() const { return highPriority; }

    /**
     * @brief Adds envelope metadata fields to the writer.
     * @param writer The telemetry writer to populate.
     * @note Optionally includes schemaVersion and firmwareVersion when set.
     */
    void addEnvelopeMetadata(TelemetryWriter& writer) const {
        writer.field("timestamp", static_cast<int32_t>(captureTimestamp));
        writer.field("sequence", static_cast<int32_t>(sequenceNumber));
        if (highPriority) {
            writer.field("priority", "high");
        }
        if (schemaVersionValue != nullptr) {
            writer.field("schemaVersion", schemaVersionValue);
        }
        if (firmwareVersionValue != nullptr) {
            writer.field("firmwareVersion", firmwareVersionValue);
        }
    }

    /** @brief Virtual destructor for safe polymorphic deletion. */
    virtual ~TelemetryPackage() = default;

    /**
     * @brief Writes telemetry data fields to the given writer.
     * @param writer The telemetry writer to populate with data fields.
     * @note Envelope metadata (timestamp, sequence) is added automatically by
     *   the `toJsonString()` convenience method before calling this method.
     */
    virtual void write(TelemetryWriter& writer) const = 0;

    /**
     * @brief Serializes telemetry to a JSON string.
     * @return A String containing the serialized JSON representation.
     * @note Uses JsonTelemetryWriter internally with envelope metadata.
     */
    [[nodiscard]] String toJsonString() const {
        JsonTelemetryWriter writer;
        this->addEnvelopeMetadata(writer);
        this->write(writer);
        writer.finalize();
        return String(writer.getString());
    }

    /**
     * @brief Serializes telemetry directly into a caller-provided buffer.
     * @param buffer The pre-allocated target buffer.
     * @param bufferSize The capacity of the target buffer in bytes.
     * @return The number of bytes written (excluding null terminator), or 0 on overflow.
     * @note The caller provides the buffer to avoid heap allocation on the network task.
     *   The result is null-terminated. This method encapsulates the full serialization
     *   pipeline: envelope metadata, data fields, finalization, and overflow check.
     */
    [[nodiscard]] size_t serializeToBuffer(char* buffer, const size_t bufferSize) const {
        JsonTelemetryWriter writer;
        this->addEnvelopeMetadata(writer);
        this->write(writer);
        writer.finalize();
        if (writer.hasOverflow()) {
            return 0;
        }
        const char* jsonString = writer.getString();
        const size_t length = strlen(jsonString);
        if (length >= bufferSize) {
            return 0;
        }
        memcpy(buffer, jsonString, length);
        buffer[length] = '\0';
        return length;
    }
};

#endif // TELEMETRY_PACKAGE_H
