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
int TelemetryBuffer::maxPackets = DEFAULT_MAX_BUFFERED_PACKETS;
int TelemetryBuffer::packetCount = 0;

String TelemetryBuffer::getFilePath(int index) {
    String path(TELEMETRY_BUFFER_DIRECTORY);
    path += '/';
    // Zero-padded 5-digit index for correct alphabetical sorting
    char num[6];
    snprintf(num, sizeof(num), "%05d", index + 1);
    path += num;
    path += ".json";
    return path;
}

int TelemetryBuffer::countPackets() {
    int count = 0;
#ifdef ARDUINO_ARCH_ESP32
    if (File dir = LittleFS.open(TELEMETRY_BUFFER_DIRECTORY, "r"); dir && dir.isDirectory()) {
        while (File file = dir.openNextFile()) {
            if (!file.isDirectory()) {
                count++;
            }
        }
    }
#endif
    return count;
}

bool TelemetryBuffer::removeOldestPacket() {
    const String path = getFilePath(0);
#ifdef ARDUINO_ARCH_ESP32
    if (LittleFS.remove(path)) {
        // Shift all remaining files down by one index
        for (int i = 1; i < packetCount; i++) {
            String oldPath = getFilePath(i);
            String newPath = getFilePath(i - 1);
            LittleFS.rename(oldPath.c_str(), newPath.c_str());
        }
        packetCount--;
        return true;
    }
#endif
    return false;
}

bool TelemetryBuffer::begin(const int maxBufferedPackets) {
    if (maxBufferedPackets <= 0) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Invalid max buffered packets: %d", maxBufferedPackets);
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

    TelemetryBuffer::maxPackets = maxBufferedPackets;
    packetCount = countPackets();
    initialized = true;

    if (packetCount > 0) {
        MODEST_LOG_W(TELEMETRY_BUFFER_COMPONENT, "Found %d buffered packets from previous session", packetCount);
    }

    MODEST_LOG_I(TELEMETRY_BUFFER_COMPONENT, "Telemetry buffer initialized (max %d packets)", maxBufferedPackets);
    return true;
#else
    // Host testing: simulate initialization
    TelemetryBuffer::maxPackets = maxBufferedPackets;
    packetCount = 0;
    initialized = true;
    return true;
#endif
}

bool TelemetryBuffer::store(const String& jsonStringTelemetryPacket) {
    if (!initialized || jsonStringTelemetryPacket.length() == 0) {
        return false;
    }

    // If at capacity, remove oldest packet first
    if (packetCount >= maxPackets) {
        if (!removeOldestPacket()) {
            MODEST_LOG_W(TELEMETRY_BUFFER_COMPONENT, "Failed to evict oldest packet; cannot store new packet");
            return false;
        }
    }

#ifdef ARDUINO_ARCH_ESP32
    const String path = getFilePath(packetCount);
    File file = LittleFS.open(path, "w");
    if (!file) {
        MODEST_LOG_E(TELEMETRY_BUFFER_COMPONENT, "Failed to create buffer file: %s", path.c_str());
        return false;
    }

    const size_t written = file.print(jsonStringTelemetryPacket);
    file.close();

    if (written == jsonStringTelemetryPacket.length()) {
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
    const String path = getFilePath(0);
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

    // Remove the retrieved file and shift remaining files
    LittleFS.remove(path);
    for (int i = 1; i < packetCount; i++) {
        String oldPath = getFilePath(i);
        String newPath = getFilePath(i - 1);
        LittleFS.rename(oldPath.c_str(), newPath.c_str());
    }
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

bool TelemetryBuffer::hasBufferedPackets() {
    return initialized && packetCount > 0;
}

bool TelemetryBuffer::clear() {
    if (!initialized) {
        return false;
    }

#ifdef ARDUINO_ARCH_ESP32
    for (int i = packetCount - 1; i >= 0; i--) {
        LittleFS.remove(getFilePath(i));
    }
    packetCount = 0;
    MODEST_LOG_I(TELEMETRY_BUFFER_COMPONENT, "Buffer cleared");
    return true;
#else
    // Host testing: simulate clear
    packetCount = 0;
    return true;
#endif
}

bool TelemetryBuffer::isInitialized() {
    return initialized;
}
