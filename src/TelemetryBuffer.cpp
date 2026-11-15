/**
 * @file TelemetryBuffer.cpp
 * @brief Implements persistent telemetry buffering using LittleFS for offline replay.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 */

#include "TelemetryBuffer.h"

#ifdef ARDUINO_ARCH_ESP32
#include <LittleFS.h>
#endif

#include "ModestLog.h"

static constexpr auto TELEMETRY_BUFFER_COMPONENT = "TelemetryBuffer";

bool TelemetryBuffer::initialized = false;
int TelemetryBuffer::maxPacketsCount = DEFAULT_MAX_BUFFERED_PACKETS;
int TelemetryBuffer::packetCount = 0;
unsigned int TelemetryBuffer::nextWriteIndex = 0;
TelemetryBuffer::EvictionCallback TelemetryBuffer::onEvictionCallback = nullptr;

String TelemetryBuffer::getFilePath(unsigned int index) {
    String path(TELEMETRY_BUFFER_DIRECTORY);
    path += '/';
    // Zero-padded 5-digit monotonic index
    char num[6];
    snprintf(num, sizeof(num), "%05u", index + 1);
    path += num;
    path += ".json";
    return path;
}

int TelemetryBuffer::countPackets() {
    int count = 0;
#ifdef ARDUINO_ARCH_ESP32
    unsigned int maxIndex = 0;
    if (File dir = LittleFS.open(TELEMETRY_BUFFER_DIRECTORY, "r"); dir && dir.isDirectory()) {
        while (File file = dir.openNextFile()) {
            if (!file.isDirectory()) {
                count++;
                // Extract the monotonic index from the filename (e.g., "00003.json" -> 2)
                const char* name = file.name();
                unsigned int fileIndex = 0;
                sscanf(name, "%05u", &fileIndex);
                fileIndex--; // Convert back to 0-based index
                if (fileIndex >= maxIndex) {
                    maxIndex = fileIndex + 1;
                }
            }
        }
    }
    // Set nextWriteIndex to one past the highest existing index
    nextWriteIndex = maxIndex;
#endif
    return count;
}

String TelemetryBuffer::findOldestFilePath() {
    String oldestPath;
#ifdef ARDUINO_ARCH_ESP32
    unsigned int minIndex = 0xFFFFFFFF;
    if (File dir = LittleFS.open(TELEMETRY_BUFFER_DIRECTORY, "r"); dir && dir.isDirectory()) {
        while (File file = dir.openNextFile()) {
            if (!file.isDirectory()) {
                const char* name = file.name();
                unsigned int fileIndex = 0;
                sscanf(name, "%05u", &fileIndex);
                fileIndex--; // Convert back to 0-based index
                if (fileIndex < minIndex) {
                    minIndex = fileIndex;
                    oldestPath = getFilePath(minIndex);
                }
            }
        }
    }
#endif
    return oldestPath;
}

bool TelemetryBuffer::removeOldestPacket() {
#ifdef ARDUINO_ARCH_ESP32
    const String path = findOldestFilePath();
    if (path.isEmpty()) {
        return false;
    }
    // Simply delete the oldest file — no shifting needed with monotonic indices
    if (LittleFS.remove(path)) {
        packetCount--;
        return true;
    }
    return false;
#else
    // Host testing: simulate removal
    if (packetCount > 0) {
        packetCount--;
        return true;
    }
    return false;
#endif
}

bool TelemetryBuffer::begin(const int maxBufferedPacketsCount) {
    if (maxBufferedPacketsCount <= 0) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Invalid max buffered packets: %d", maxBufferedPacketsCount);
        return false;
    }

#ifdef ARDUINO_ARCH_ESP32
    if (!LittleFS.begin(true)) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Failed to mount LittleFS");
        initialized = false;
        return false;
    }

    // Create buffer directory if it doesn't exist
    if (!LittleFS.exists(TELEMETRY_BUFFER_DIRECTORY)) {
        if (!LittleFS.mkdir(TELEMETRY_BUFFER_DIRECTORY)) {
            MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Failed to create buffer directory");
            initialized = false;
            return false;
        }
    }

    TelemetryBuffer::maxPacketsCount = maxBufferedPacketsCount;
    packetCount = countPackets();
    initialized = true;

    if (packetCount > 0) {
        MODEST_LOG_W(TELEMETRY_BUFFER_COMPONENT, "Found %d buffered packets from previous session", packetCount);
    }

    MODEST_LOG_I(TELEMETRY_BUFFER_COMPONENT, "Telemetry buffer initialized (max %d packets)", maxBufferedPacketsCount);
    return true;
#else
    // Host testing: simulate initialization
    TelemetryBuffer::maxPacketsCount = maxBufferedPacketsCount;
    packetCount = 0;
    nextWriteIndex = 0;
    initialized = true;
    return true;
#endif
}

bool TelemetryBuffer::store(const String& jsonStringTelemetryPacket) {
    if (!initialized || jsonStringTelemetryPacket.length() == 0) {
        return false;
    }

    // If at capacity, remove oldest packet first
    if (packetCount >= maxPacketsCount) {
        if (!removeOldestPacket()) {
            MODEST_LOG_W(TELEMETRY_BUFFER_COMPONENT, "Failed to evict oldest packet; cannot store new packet");
            return false;
        }
        // Notify eviction callback after successful removal
        if (onEvictionCallback) {
            onEvictionCallback();
        }
    }

#ifdef ARDUINO_ARCH_ESP32
    // Append at the next monotonic index — no shifting needed
    const String path = getFilePath(nextWriteIndex);
    File file = LittleFS.open(path, "w");
    if (!file) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Failed to create buffer file: %s", path.c_str());
        return false;
    }

    const size_t written = file.print(jsonStringTelemetryPacket);
    file.close();

    if (written == jsonStringTelemetryPacket.length()) {
        nextWriteIndex++;
        packetCount++;
        return true;
    }

    // Write failed — clean up partial file
    LittleFS.remove(path);
    MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Incomplete write to buffer file");
    return false;
#else
    // Host testing: simulate storage
    packetCount++;
    return true;
#endif
}

bool TelemetryBuffer::retrieveNext(String& outJsonBufferedPacket) {
    if (!initialized || packetCount <= 0) {
        return false;
    }

#ifdef ARDUINO_ARCH_ESP32
    // Find and read the oldest file by monotonic index — no shifting needed
    const String path = findOldestFilePath();
    if (path.isEmpty()) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "No oldest packet found");
        return false;
    }
    File file = LittleFS.open(path, "r");
    if (!file || !file.isDirectory()) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Failed to open buffered packet: %s", path.c_str());
        return false;
    }

    outJsonBufferedPacket = file.readString();
    file.close();

    if (outJsonBufferedPacket.isEmpty()) {
        LittleFS.remove(path);
        MODEST_LOG_W(TELEMETRY_BUFFER_COMPONENT, "Empty buffered packet removed");
        packetCount--;
        return false;
    }

    // Remove only the retrieved file — no shifting needed with monotonic indices
    LittleFS.remove(path);
    packetCount--;
    return true;
#else
    // Host testing: simulate retrieval
    if (packetCount > 0) {
        outJsonBufferedPacket.assign("{\"sequence\":1,\"timestamp\":0,\"test\":true}");
        packetCount--;
        return true;
    }
    return false;
#endif
}

int TelemetryBuffer::getBufferedPacketCount() {
    if (!initialized) {
        return 0;
    }
#ifdef ARDUINO_ARCH_ESP32
    // Refresh count from filesystem for accuracy
    packetCount = countPackets();
#endif
    return packetCount;
}

int TelemetryBuffer::getMaxPacketsCount() {
    if (!initialized) {
        return DEFAULT_MAX_BUFFERED_PACKETS;
    }
    return maxPacketsCount;
}

bool TelemetryBuffer::hasBufferedPackets() {
    return initialized && packetCount > 0;
}

bool TelemetryBuffer::clear() {
    if (!initialized) {
        return false;
    }

#ifdef ARDUINO_ARCH_ESP32
    // Iterate over all files in the directory and remove them
    // (monotonic indices mean files are not at sequential positions)
    if (File dir = LittleFS.open(TELEMETRY_BUFFER_DIRECTORY, "r"); dir && dir.isDirectory()) {
        while (File file = dir.openNextFile()) {
            if (!file.isDirectory()) {
                String path = TELEMETRY_BUFFER_DIRECTORY;
                path += '/';
                path += file.name();
                LittleFS.remove(path);
            }
        }
    }
    packetCount = 0;
    nextWriteIndex = 0;
    MODEST_LOG_I(TELEMETRY_BUFFER_COMPONENT, "Buffer cleared");
    return true;
#else
    // Host testing: simulate clear
    packetCount = 0;
    nextWriteIndex = 0;
    return true;
#endif
}

bool TelemetryBuffer::isInitialized() {
    return initialized;
}

void TelemetryBuffer::onEviction(EvictionCallback callback) {
    onEvictionCallback = callback;
}
