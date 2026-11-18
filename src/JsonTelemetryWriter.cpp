#include "JsonTelemetryWriter.h"
#include "ModestLog.h"

#ifndef MODEST_LOG_ERROR
#define MODEST_LOG_ERROR(fmt, ...) ((void)0)
#endif

JsonTelemetryWriter::JsonTelemetryWriter() {
    buffer[0] = '{';
    position = 1;
}

void JsonTelemetryWriter::writeCommaIfNeeded() {
    if (!firstField && !overflow && position < JSON_TELEMETRY_BUFFER_SIZE - 1) {
        buffer[position++] = ',';
    }
    firstField = false;
}

bool JsonTelemetryWriter::writeFieldName(const char* name) {
    if (overflow) return false;

    writeCommaIfNeeded();

    // "\"key\":"
    const int written = snprintf(buffer + position, JSON_TELEMETRY_BUFFER_SIZE - position, "\"%s\":", name);
    if (written < 0 || static_cast<size_t>(written) >= JSON_TELEMETRY_BUFFER_SIZE - position) {
        overflow = true;
        MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow writing field name: %s", name);
        buffer[position] = '\0';
        return false;
    }
    position += written;
    return true;
}

void JsonTelemetryWriter::field(const char* name, const int32_t value) {
    if (!writeFieldName(name)) return;
    if (overflow) return;

    const int written = snprintf(buffer + position, JSON_TELEMETRY_BUFFER_SIZE - position, "%d", value);
    if (written < 0 || static_cast<size_t>(written) >= JSON_TELEMETRY_BUFFER_SIZE - position) {
        overflow = true;
        MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow writing int field: %s", name);
        buffer[position] = '\0';
        return;
    }
    position += written;
}

void JsonTelemetryWriter::field(const char* name, const float value) {
    if (!writeFieldName(name)) return;
    if (overflow) return;

    const int written = snprintf(buffer + position, JSON_TELEMETRY_BUFFER_SIZE - position, "%.2f", value);
    if (written < 0 || static_cast<size_t>(written) >= JSON_TELEMETRY_BUFFER_SIZE - position) {
        overflow = true;
        MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow writing float field: %s", name);
        buffer[position] = '\0';
        return;
    }
    position += written;
}

void JsonTelemetryWriter::field(const char* name, const bool value) {
    if (!writeFieldName(name)) return;
    if (overflow) return;

    const char* boolStr = value ? "true" : "false";
    const int written = snprintf(buffer + position, JSON_TELEMETRY_BUFFER_SIZE - position, "%s", boolStr);
    if (written < 0 || static_cast<size_t>(written) >= JSON_TELEMETRY_BUFFER_SIZE - position) {
        overflow = true;
        MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow writing bool field: %s", name);
        buffer[position] = '\0';
        return;
    }
    position += written;
}

void JsonTelemetryWriter::field(const char* name, const char* value) {
    if (!writeFieldName(name)) return;
    if (overflow) return;

    if (value == nullptr) {
        const int written = snprintf(buffer + position, JSON_TELEMETRY_BUFFER_SIZE - position, "null");
        if (written < 0 || static_cast<size_t>(written) >= JSON_TELEMETRY_BUFFER_SIZE - position) {
            overflow = true;
            MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow writing null string: %s", name);
            buffer[position] = '\0';
            return;
        }
        position += written;
        return;
    }

    // Escape special characters in the string value
    const int written = snprintf(buffer + position, JSON_TELEMETRY_BUFFER_SIZE - position, "\"%s\"", value);
    if (written < 0 || static_cast<size_t>(written) >= JSON_TELEMETRY_BUFFER_SIZE - position) {
        overflow = true;
        MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow writing string field: %s", name);
        buffer[position] = '\0';
        return;
    }
    position += written;
}

void JsonTelemetryWriter::finalize() {
    if (overflow) return;
    if (position < JSON_TELEMETRY_BUFFER_SIZE - 1) {
        buffer[position++] = '}';
        buffer[position] = '\0';
    } else {
        overflow = true;
        MODEST_LOG_ERROR("[TelemetryWriter] Buffer overflow on finalize");
        buffer[JSON_TELEMETRY_BUFFER_SIZE - 1] = '\0';
    }
    finalized = true;
}

const char* JsonTelemetryWriter::getString() const {
    if (overflow) {
        return "";
    }
    return buffer;
}

bool JsonTelemetryWriter::hasOverflow() const {
    return overflow;
}
