#ifndef TELEMETRY_BUFFER_H
#define TELEMETRY_BUFFER_H

/**
 * @file TelemetryBuffer.h
 * @brief Declares a persistent telemetry buffer using LittleFS for offline replay.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 */

#include <Arduino.h>

/**
 * @brief Maximum number of buffered packets before dropping oldest.
 *
 * Each buffered packet is stored as a separate JSON file in LittleFS.
 * Default of 50 packets provides ~25-50 KB of buffer depending on payload size.
 */
constexpr int DEFAULT_MAX_BUFFERED_PACKETS = 50;

/**
 * @brief LittleFS directory path for buffered telemetry packets.
 */
constexpr auto TELEMETRY_BUFFER_DIRECTORY = "/telemetry_buffer";

/**
 * @brief Persistent telemetry buffer backed by LittleFS for offline replay.
 * @ingroup core_module
 *
 * When the network is unavailable, telemetry packets that fail to send can be
 * persisted to LittleFS instead of being dropped. When connectivity is restored,
 * buffered packets are replayed in FIFO order.
 *
 * The buffer uses a file-per-packet strategy with sequential naming
 * (e.g., `/telemetry_buffer/00001.json`) for simple FIFO ordering.
 *
 * @par Thread Safety
 * All public methods are designed to be called from the network dispatcher task.
 * The buffer is not thread-safe across multiple tasks; use from a single context.
 *
 * @par Memory Usage
 * Each buffered packet consumes approximately 100-300 bytes in LittleFS depending
 * on payload size. With the default limit of 50 packets, expect ~5-15 KB usage.
 *
 * @par Example
 * @code
 * // Initialize the buffer in setup()
 * TelemetryBuffer::begin();
 *
 * // In processQueuedTelemetryData(), if send fails:
 * if (!gatewayClient.sendTelemetryRecord(*packet)) {
 *     TelemetryBuffer::store(packet->toJsonString());
 * }
 *
 * // When connectivity is restored, replay buffered packets:
 * if (gatewayClient.isConnected() && TelemetryBuffer::hasBufferedPackets()) {
 *     String buffered;
 *     while (TelemetryBuffer::retrieveNext(buffered)) {
 *         // Parse and resend the buffered JSON
 *     }
 * }
 * @endcode
 */
class TelemetryBuffer {
private:
    static bool initialized; ///< Tracks whether begin() was called successfully.
    static int maxPackets; ///< Maximum number of buffered packets before dropping oldest.
    static int packetCount; ///< Current number of buffered packets in LittleFS.

    /**
     * @brief Generates the file path for a given packet index.
     * @param index Zero-based packet index.
     * @return String path like "/telemetry_buffer/00001.json".
     */
    [[nodiscard]] static String getFilePath(int index);

    /**
     * @brief Counts the existing buffered packets in LittleFS.
     * @return Number of .json files in the buffer directory.
     */
    [[nodiscard]] static int countPackets();

    /**
     * @brief Removes the oldest packet file from LittleFS.
     * @return `true` if a packet was removed, `false` if buffer was empty.
     */
    static bool removeOldestPacket();

public:
    /**
     * @brief Initializes the telemetry buffer in LittleFS.
     * @param maxBufferedPackets Maximum number of packets to buffer (default: 50).
     * @return `true` if LittleFS was mounted and the buffer directory exists.
     *
     * Creates the buffer directory if it doesn't exist and counts any
     * remaining packets from a previous session (e.g., unexpected reboot).
     *
     * @note Must be called before any other buffer operations.
     * Call once during device setup, after LittleFS is available.
     */
    static bool begin(int maxBufferedPackets = DEFAULT_MAX_BUFFERED_PACKETS);

    /**
     * @brief Stores a serialized telemetry packet in the buffer.
     * @param jsonStringTelemetryPacket The JSON string representation of the telemetry packet.
     * @return `true` if the packet was stored, `false` if buffer is full and oldest couldn't be removed.
     *
     * If the buffer is at capacity, the oldest packet is automatically removed
     * to make room for the new one (FIFO eviction).
     *
     * @note Should be called when `sendTelemetryRecord()` returns false.
     */
    static bool store(const String& jsonStringTelemetryPacket);

    /**
     * @brief Retrieves the next buffered packet in FIFO order.
     * @param outJsonBufferedPacket Reference to receive the JSON string content.
     * @return `true` if a packet was retrieved, `false` if buffer is empty.
     *
     * Returns the oldest buffered packet and removes it from storage.
     * Call repeatedly until it returns false to drain the buffer.
     */
    static bool retrieveNext(String& outJsonBufferedPacket);

    /**
     * @brief Returns the number of buffered packets.
     * @return Current count of buffered packets in LittleFS.
     */
    [[nodiscard]] static int getBufferedPacketCount();

    /**
     * @brief Checks whether there are buffered packets available.
     * @return `true` if at least one packet is buffered.
     */
    [[nodiscard]] static bool hasBufferedPackets();

    /**
     * @brief Clears all buffered packets from LittleFS.
     * @return `true` if the directory was cleared successfully.
     *
     * Use this to discard all buffered packets (e.g., after a firmware update
     * where old schema packets should not be replayed).
     */
    static bool clear();

    /**
     * @brief Checks whether the buffer was initialized.
     * @return `true` if begin() was called successfully.
     */
    [[nodiscard]] static bool isInitialized();
};

#endif // TELEMETRY_BUFFER_H
