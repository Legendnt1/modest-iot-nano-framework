/**
 * @file OtaUpdateHandler.h
 * @brief Declares the OtaUpdateHandler class for Over-The-Air firmware updates via Wi-Fi
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 */

#ifndef MODEST_IOT_NANO_FRAMEWORK_OTA_HANDLER_H
#define MODEST_IOT_NANO_FRAMEWORK_OTA_HANDLER_H

#include <functional>
#include <cstdint>

/**
 * @brief OTA update status codes for tracking the current state of an update.
 */
enum class OtaUpdateStatus : uint8_t {
    Idle,         ///< No update in progress; ready to accept a new update.
    Starting,     ///< Update has been initiated; waiting for data.
    Running,      ///< Firmware data is being received and written.
    Finished,     ///< Update completed successfully; device should restart.
    Error         ///< Update failed; error code available via getLastError().
};

/**
 * @brief Callback type for reporting OTA update progress.
 *
 * @param progressInNumberOfBytesWritten Bytes written so far.
 * @param totalNumberOfBytes Total bytes expected.
 */
using OtaProgressCallback = std::function<void(uint32_t progressInNumberOfBytesWritten, uint32_t totalNumberOfBytes)>;

/**
 * @brief Callback type for reporting OTA update errors.
 *
 * @param errorCode Numeric error code from the underlying OTA library.
 */
using OtaErrorCallback = std::function<void(int errorCode)>;

/**
 * @brief Callback type for when an OTA update starts.
 */
using OtaStartCallback = std::function<void()>;

/**
 * @brief Callback type for when an OTA update ends.
 */
using OtaEndCallback = std::function<void()>;

/**
 * @brief Manages Over-The-Air (OTA) firmware updates via Wi-Fi.
 *
 * Wraps the ESP32 ArduinoOTA library to provide a simple, static API for
 * receiving firmware updates over the network. Supports hostname configuration,
 * password protection, and progress/error callbacks.
 *
 * All methods are static — the component is a singleton infrastructure service.
 * Falls back to safe defaults when not initialized, preserving backward compatibility.
 *
 * @par Example
 * @code
 * // Configure OTA updates in setup()
 * OtaUpdateHandler::begin("my-device-name");
 * OtaUpdateHandler::setPassword("secret123");
 *
 * // Optional: track progress
 * OtaUpdateHandler::onProgress([](uint32_t progressInNumberOfBytesWritten, uint32_t totalNumberOfBytes) {
 *     float percent = (float)progressInNumberOfBytesWritten / (float)totalNumberOfBytes * 100.0f;
 *     Serial.printf("OTA Progress: %.1f%%\r", percent);
 * });
 *
 * // In your Device subclass, call service() from maintainNetworkServices()
 * void maintainNetworkServices() const override {
 *     gatewayClient.service();
 *     OtaUpdateHandler::service();
 * }
 * @endcode
 *
 * @par Threading
 * @runs_on{network} — `service()` should be called from the network dispatcher task
 * via `maintainNetworkServices()` to process incoming OTA update requests.
 */
class OtaUpdateHandler {
public:
    /**
     * @brief Initializes the OTA update service.
     * @param hostname Device hostname for OTA discovery (default: "modest-iot").
     * @return `true` if the OTA service started successfully.
     *
     * Configures the device hostname and starts the OTA update listener.
     * The device will be discoverable on the network for OTA updates.
     *
     * @note Must be called after Wi-Fi is connected. Call once during device setup.
     * The hostname is used by the Arduino IDE to identify the device when
     * selecting the OTA port for uploading firmware.
     * @note Calling begin() multiple times resets the status to Idle and clears
     * any error code, but does not reset callbacks registered via onProgress(),
     * onError(), onStart(), or onEnd(). Callbacks persist across re-initialization.
     */
    static bool begin(const char* hostname = "modest-iot");

    /**
     * @brief Sets the password required for OTA updates.
     * @param password Password string for authenticating OTA update requests.
     * @note Can be called before or after begin() to require authentication for
     *       OTA updates. Without a password, any device on the network can
     *       trigger an update.
     */
    static void setPassword(const char* password);

    /**
     * @brief Registers a callback for reporting update progress.
     * @param callback Function called periodically during an update with progress data.
     * @note The callback receives the number of bytes written and the total bytes expected.
     *       Use this to display progress indicators on an LCD or LED.
     */
    static void onProgress(OtaProgressCallback callback);

    /**
     * @brief Registers a callback for reporting update errors.
     * @param callback Function called when an OTA error occurs.
     * @note The callback receives the numeric error code from the underlying OTA library.
     */
    static void onError(OtaErrorCallback callback);

    /**
     * @brief Registers a callback for when an update starts.
     * @param callback Function called when an OTA update begins.
     * @note Use this to pause non-essential tasks or log the start of an update.
     */
    static void onStart(OtaStartCallback callback);

    /**
     * @brief Registers a callback for when an update ends.
     * @param callback Function called when an OTA update completes (success or failure).
     * @note Use this to log completion status or trigger a restart.
     */
    static void onEnd(OtaEndCallback callback);

    /**
     * @brief Processes incoming OTA update requests.
     *
     * Must be called periodically from the network dispatcher task via
     * `Device::maintainNetworkServices()` to handle incoming OTA update
     * connections and data transfers.
     *
     * @note This is a non-blocking call that returns immediately.
     *       During an active update, it handles the data transfer internally.
     */
    static void service();

    /**
     * @brief Returns the OTA hostname.
     * @return The hostname set during begin(), or nullptr if not set.
     * @note Returns the hostname used for device discovery on the network.
     */
    [[nodiscard]] static const char* getHostname();

    /**
     * @brief Returns the current OTA update status.
     * @return OtaUpdateStatus enum value indicating the current state.
     */
    [[nodiscard]] static OtaUpdateStatus getStatus();

    /**
     * @brief Returns the last OTA error code.
     * @return Error code from the last failed OTA update, or 0 if no error.
     * @note Check this after getStatus() returns OtaUpdateStatus::Error.
     */
    [[nodiscard]] static int getLastError();

    /**
     * @brief Returns the current update progress percentage.
     * @return Progress from 0.0 (not started) to 100.0 (complete).
     * @note Returns 0.0 when no update is in progress.
     */
    [[nodiscard]] static float getProgressInPercentage();

    /**
     * @brief Checks whether the OTA service has been initialized.
     * @return `true` if begin() has been called successfully.
     */
    [[nodiscard]] static bool isInitialized();

private:
    static bool initialized; ///< Tracks whether begin() was called successfully.
    static const char* hostname; ///< OTA hostname for device discovery.
    static OtaUpdateStatus status; ///< Current OTA update status.
    static int lastError; ///< Last OTA error code.
    static uint32_t progressInNumberOfBytes; ///< Bytes written during current update.
    static uint32_t totalNumberOfBytes; ///< Total bytes expected for current update.
    static std::function<void(uint32_t, uint32_t)> progressCallback; ///< User-provided progress callback.
    static std::function<void(int)> errorCallback; ///< User-provided error callback.
};

#endif // MODEST_IOT_NANO_FRAMEWORK_OTA_HANDLER_H
