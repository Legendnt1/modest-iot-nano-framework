/**
 * @file OtaUpdateDevice.ino
 * @brief Demonstrates Over-The-Air firmware updates with progress tracking.
 *
 * This example shows how to enable OTA firmware updates on an ESP32 device
 * using the OtaUpdateHandler component. The device connects to Wi-Fi, starts
 * an OTA listener with hostname and password protection, and reports update
 * progress via Serial. A DHT22 sensor provides basic telemetry to demonstrate
 * a realistic device that can be updated remotely.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `OtaUpdateHandler` for OTA firmware updates via Wi-Fi
 * - `OtaUpdateHandler::begin()` with hostname configuration
 * - `OtaUpdateHandler::setPassword()` for password protection
 * - `OtaUpdateHandler::onProgress()` for progress tracking
 * - `OtaUpdateHandler::onError()` for error reporting
 * - `OtaUpdateHandler::onStart()` and `onEnd()` for lifecycle callbacks
 * - `OtaUpdateHandler::service()` called from `maintainNetworkServices()`
 * - `Device::setFirmwareVersion()` for fleet management in telemetry
 * - `DhtSensor` for temperature and humidity measurement
 * - `WiFiConnectivityDriver` with `waitForConnection()` timeout
 * - Loop-less, event-driven asynchronous execution via `initializeAsynchronousEngine()`
 *
 * @par Flashing via OTA
 * 1. First, flash the device normally via USB.
 * 2. Note the IP address printed to Serial.
 * 3. In Arduino IDE, select the device from Tools > Port (it appears as
 *    "modest-iot-ota.local" or by IP address).
 * 4. Upload new firmware as usual — the IDE uses OTA automatically.
 *
 * @date 2026-11-15
 * @version 1.0
 */

#include <Arduino.h>
#include <ModestIoT.h>

/** @name Network Configuration */
//@{
static const char* WIRELESS_SSID = "ModestIoT_Network";
static const char* WIRELESS_PASSWORD = "SecureMesh2026";
//@} // Network Configuration

/** @name OTA Configuration */
//@{
/** @brief Device hostname for OTA discovery on the network. */
static const char* OTA_HOSTNAME = "modest-iot-ota";

/** @brief Password required for OTA update authentication. */
static const char* OTA_PASSWORD = "ota-secret-2026";
//@} // OTA Configuration

/** @name Sensor Configuration */
//@{
static const int DHT_SENSOR_PIN = 4;
static const unsigned long TELEMETRY_POLLING_INTERVAL_IN_MILLISECONDS = 60000;
static const uint8_t HARDWARE_TIMER_CHANNEL_INDEX = 0;
static const int SAMPLER_QUEUE_LENGTH = 5;
//@} // Sensor Configuration

/** @name General Configuration */
//@{
static const unsigned long SERIAL_BAUD_RATE = 115200;
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 60000;
static const unsigned long CONNECTION_TIMEOUT_IN_MILLISECONDS = 15000;
//@} // General Configuration

/**
 * @brief OTA Update Device.
 * Demonstrates OTA firmware updates with a simple DHT22 sensor.
 * The device reports temperature and humidity, and can receive
 * firmware updates over Wi-Fi.
 */
class OtaUpdateDevice : public Device {
private:
    DhtSensor dhtSensor;

protected:
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) const override {
        // No gateway client in this example — telemetry is logged to Serial only.
        // In a real device, you would send the payload via MQTT or HTTP.
        if (rawQueueItemPayload != nullptr) {
            Serial.println("[OtaUpdateDevice] Telemetry ready (logged locally only).");
        }
    }

    /**
     * @brief Services network components including OTA update handler.
     * This is where OtaUpdateHandler::service() is called to process
     * incoming OTA update requests.
     */
    void maintainNetworkServices() const override {
        OtaUpdateHandler::service();
    }

public:
    OtaUpdateDevice(int dhtPin,
                    unsigned long samplingIntervalInMilliseconds,
                    uint8_t timerGroupIndex = 0)
        : Device(samplingIntervalInMilliseconds, timerGroupIndex),
          dhtSensor(dhtPin, DhtSensor::SENSOR_MODEL_DHT22, this)
    {
        initializeAsynchronousEngine(SAMPLER_QUEUE_LENGTH);
        appendSensorToScheduler(&dhtSensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
    }

    ~OtaUpdateDevice() override = default;

    /**
     * @brief Handles sensor data-ready events to log readings.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER) {
            char buffer[128];
            snprintf(buffer, sizeof(buffer),
                     "[OtaUpdateDevice] Temp: %.1f°C, Humidity: %.1f%%",
                     dhtSensor.getTemperatureInCelsius(),
                     dhtSensor.getRelativeHumidityInPercentage());
            Serial.println(buffer);
        }
    }

    void handle(Command command) override {}
};

/** @name Top-Level Declarative Canvas */
//@{

static WiFiConnectivityDriver* transportDriver = nullptr;
static OtaUpdateDevice* otaDeviceApplication = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);

    // Set firmware version for fleet management in telemetry
    Device::setFirmwareVersion("MOD-OTA-01 FW 1.0.0");

    // Connect to Wi-Fi
    Serial.println("[OtaUpdateDevice] Connecting to Wi-Fi...");
    transportDriver = new WiFiConnectivityDriver(WIRELESS_SSID, WIRELESS_PASSWORD);

    if (!transportDriver->waitForConnection(CONNECTION_TIMEOUT_IN_MILLISECONDS)) {
        Serial.println("[OtaUpdateDevice] Wi-Fi connection timed out");
        return;
    }

    Serial.printf("[OtaUpdateDevice] Connected! IP: %s\n",
                  transportDriver->getLocalIpAddress().toString().c_str());

    // Configure OTA updates
    OtaUpdateHandler::begin(OTA_HOSTNAME);
    OtaUpdateHandler::setPassword(OTA_PASSWORD);

    // Track OTA update progress
    OtaUpdateHandler::onProgress([](uint32_t progressInNumberOfBytesWritten, uint32_t totalNumberOfBytes) {
        float percent = static_cast<float>(progressInNumberOfBytesWritten)
                       / static_cast<float>(totalNumberOfBytes) * 100.0f;
        Serial.printf("[OtaUpdateDevice] OTA Progress: %.1f%%\r", percent);
    });

    // Handle OTA update errors
    OtaUpdateHandler::onError([](int errorCode) {
        Serial.printf("[OtaUpdateDevice] OTA Error: %d\n", errorCode);
    });

    // Log OTA update start
    OtaUpdateHandler::onStart([]() {
        Serial.println("\n[OtaUpdateDevice] OTA Update started...");
    });

    // Log OTA update end
    OtaUpdateHandler::onEnd([]() {
        Serial.println("\n[OtaUpdateDevice] OTA Update ended.");
    });

    Serial.printf("[OtaUpdateDevice] OTA updates enabled on hostname: %s\n", OTA_HOSTNAME);
    Serial.println("[OtaUpdateDevice] To update firmware, select this device from Arduino IDE > Tools > Port");

    // Create the device
    otaDeviceApplication = new OtaUpdateDevice(
        DHT_SENSOR_PIN,
        TELEMETRY_POLLING_INTERVAL_IN_MILLISECONDS,
        HARDWARE_TIMER_CHANNEL_INDEX
    );
}

void loop() {
    // The master loop is completely clean and empty.
    // The framework handles all timing, sensor sampling, and OTA update servicing.
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
