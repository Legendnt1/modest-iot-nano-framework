/**
 * @file AirQualityMonitor.ino
 * @brief Desktop air quality monitor with gas detection, LCD display, and audible alarm.
 *
 * Uses an MQ-2 gas sensor to detect combustible gases (LPG, propane, methane), displays
 * real-time PPM readings and sensor mode on a 16x2 LCD, and sounds a buzzer when gas
 * is detected. The sensor auto-calibrates on startup, then switches to accurate mode
 * for continuous monitoring.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `Device` base class with async engine for loop-less execution
 * - `GasSensor` with calibration flow (`CalibratingMode` → `AccurateMode`)
 * - `Buzzer` actuator with `PLAY_TONE_COMMAND` for audible alerts
 * - `CharacterLcdDisplay` actuator with `UPDATE_TEXT_COMMAND` for real-time display
 * - Event routing via `on(Event)` with `DATA_READ_EVENT_IDENTIFIER` and `DIGITAL_GAS_DETECTED_EVENT`
 * - Sensor operating mode tracking via `getOperatingMode()`
 * - Direct actuator command dispatch via `handle(Command)`
 * @date 2026-06-12
 * @version 1.0
 */

#include <Arduino.h>
#include <memory>
#include "ModestIoT.h"

/** @brief Serial communication baud rate. */
static const unsigned long SERIAL_BAUD_RATE = 115200;

/** @brief Delay in the main loop when no work is pending. */
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 60000;

/** @brief Serial port connection timeout (ms). */
static const unsigned long SERIAL_TIMEOUT_IN_MILLISECONDS = 5000;

/** @brief Sampler event queue length. */
static const int ASYNC_ENGINE_QUEUE_DEPTH = 10;

/** @brief Buzzer tone frequency (Hz). */
static const unsigned long BUZZER_TONE_FREQ = 2000;

/** @brief Buzzer tone duration (ms). */
static const unsigned long BUZZER_TONE_DURATION_IN_MILLISECONDS = 100;

/** @brief Sampling interval of the gas sensor. */
static const unsigned long SAMPLING_INTERVAL_IN_MILLISECONDS = 1000;

/** @brief LCD backlight starts on. */
static const bool LCD_BACKLIGHT_INITIAL_STATE = true;

/** @name Character LCD Text Layout */
//@{
/** @brief Number of text lines written to the LCD. */
static const int LCD_TEXT_LINE_COUNT = 2;
/** @brief Characters reserved per LCD line (up to 20 visible characters plus the null terminator). */
static const size_t LCD_TEXT_LINE_BUFFER_SIZE = 21;
/** @brief Index of the operating mode line. */
static const int LCD_MODE_LINE_INDEX = 0;
/** @brief Index of the measurement line. */
static const int LCD_VALUE_LINE_INDEX = 1;
/** @brief Mode line shown in raw mode. */
static const char* RAW_MODE_TEXT = "Mode: RAW";
/** @brief Measurement line format in raw mode (raw ADC value). */
static const char* RAW_VALUE_FORMAT = "Raw Val: %d";
/** @brief Mode line shown while calibrating. */
static const char* CALIBRATING_MODE_TEXT = "Mode: CALIBRATING";
/** @brief Instruction shown while calibrating. */
static const char* CALIBRATING_INSTRUCTION_TEXT = "Keep in clean air!";
/** @brief Mode line shown in accurate mode. */
static const char* ACCURATE_MODE_TEXT = "Mode: ACCURATE";
/** @brief Measurement line format in accurate mode (gas name, PPM). */
static const char* GAS_CONCENTRATION_FORMAT = "%s: %.1f PPM";
/** @brief Measurement line format when no target gas is set (Rs/R0 ratio). */
static const char* RS_R0_RATIO_FORMAT = "Rs/R0: %.2f";
//@} // Character LCD Text Layout

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "AQM-200 FW 1.0.0";

/** @name Telemetry JSON Keys */
//@{
/** @brief JSON key for gas concentration in PPM. */
#define JSON_KEY_GAS_PPM "gasPpm"
/** @brief JSON key for gas detection status. */
#define JSON_KEY_GAS_DETECTED "gasDetected"
/** @brief JSON key for sensor operating mode. */
#define JSON_KEY_SENSOR_MODE "sensorMode"
//@} // Telemetry JSON Keys

/** @name Concrete Application Data Schema Specification */
//@{

/**
 * @brief Telemetry package for Air Quality Monitor data.
 * Contains gas concentration, detection status, and sensor mode.
 */
class AirQualityTelemetryPackage : public TelemetryPackage {
private:
    /// @brief Gas concentration in PPM.
    float gasConcentrationInPpm;
    /// @brief Flag indicating if gas is detected.
    bool gasDetected;
    /// @brief The operating mode of the sensor, cast from `GasSensor::OperatingMode`.
    int operatingMode;

public:
    AirQualityTelemetryPackage(float ppm, bool detected, GasSensor::OperatingMode mode)
        : gasConcentrationInPpm(ppm), gasDetected(detected), operatingMode(static_cast<int>(mode)) {}

    ~AirQualityTelemetryPackage() override = default;

    /**
     * @brief Serializes the telemetry data into a JSON object.
     * @param serializationDestination The `JsonDocument` where data will be stored.
     */
    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination[JSON_KEY_GAS_PPM] = this->gasConcentrationInPpm;
        serializationDestination[JSON_KEY_GAS_DETECTED] = this->gasDetected;
        serializationDestination[JSON_KEY_SENSOR_MODE] = this->operatingMode;
    }
};
//@} // Concrete Application Data Schema Specification

/** @name Application Mediator Implementation */
//@{

/**
 * @brief Orchestrates the GasSensor, Buzzer, and CharacterLcdDisplay for air quality monitoring.
 *
 * This device manages the gas sensor's calibration and operational modes, updates
 * an LCD with real-time data, and provides audible alerts via a buzzer.
 */
class AirQualityMonitorDevice : public Device {
private:
    /// @brief The gas sensor instance.
    GasSensor gasSensor;
    /// @brief The buzzer instance for alarms.
    Buzzer alarmBuzzer;
    /// @brief The LCD display instance for status.
    CharacterLcdDisplay statusDisplay;
    /// @brief The MQTT gateway client reference.
    MqttGatewayClient& gatewayClient;

    /**
     * @brief Updates the LCD display with current sensor status and backlight based on gas detection.
     *
     * Depending on the sensor's `OperatingMode` (Raw, Calibrating, or Accurate),
     * it formats the appropriate text and displays it on the LCD.
     * Also controls the backlight based on whether gas is detected.
     */
    void updateOperationStatusDisplay() {
        char textBufferInternal[LCD_TEXT_LINE_COUNT][LCD_TEXT_LINE_BUFFER_SIZE];
        const char* textBufferPtrs[LCD_TEXT_LINE_COUNT];
        char* modeLine = textBufferInternal[LCD_MODE_LINE_INDEX];
        char* valueLine = textBufferInternal[LCD_VALUE_LINE_INDEX];

        switch (gasSensor.getOperatingMode()) {
            case GasSensor::OperatingMode::RawMode:
                snprintf(modeLine, LCD_TEXT_LINE_BUFFER_SIZE, "%s", RAW_MODE_TEXT);
                snprintf(valueLine, LCD_TEXT_LINE_BUFFER_SIZE, RAW_VALUE_FORMAT, gasSensor.getRawAnalogValue());
                break;
            case GasSensor::OperatingMode::CalibratingMode:
                snprintf(modeLine, LCD_TEXT_LINE_BUFFER_SIZE, "%s", CALIBRATING_MODE_TEXT);
                snprintf(valueLine, LCD_TEXT_LINE_BUFFER_SIZE, "%s", CALIBRATING_INSTRUCTION_TEXT);
                break;
            case GasSensor::OperatingMode::AccurateMode:
                snprintf(modeLine, LCD_TEXT_LINE_BUFFER_SIZE, "%s", ACCURATE_MODE_TEXT);
                if (gasSensor.getTargetGasType() != GasSensor::GasType::None) {
                    snprintf(valueLine, LCD_TEXT_LINE_BUFFER_SIZE, GAS_CONCENTRATION_FORMAT,
                             getGasTypeName(gasSensor.getTargetGasType()),
                             gasSensor.getGasConcentrationInPpm());
                } else {
                    snprintf(valueLine, LCD_TEXT_LINE_BUFFER_SIZE, RS_R0_RATIO_FORMAT, gasSensor.getRsR0Ratio());
                }
                break;
        }

        textBufferPtrs[LCD_MODE_LINE_INDEX] = modeLine;
        textBufferPtrs[LCD_VALUE_LINE_INDEX] = valueLine;

        statusDisplay.setLineBuffer(textBufferPtrs); // Use the overloaded method
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);

        // Control backlight based on gas detection
        if (gasSensor.isGasDetected()) {
            statusDisplay.handle(CharacterLcdDisplay::TURN_BACKLIGHT_ON_COMMAND);
        } else {
            statusDisplay.handle(CharacterLcdDisplay::TURN_BACKLIGHT_OFF_COMMAND);
        }
    }

    /**
     * @brief Gets the string representation of a gas type.
     * @param type The `GasSensor::GasType`.
     * @return The gas type name as a string.
     */
    const char* getGasTypeName(GasSensor::GasType type) {
        switch (type) {
            case GasSensor::GasType::LPG: return "LPG";
            case GasSensor::GasType::Methane: return "CH4";
            case GasSensor::GasType::CarbonMonoxide: return "CO";
            case GasSensor::GasType::Alcohol: return "Alcohol";
            case GasSensor::GasType::Propane: return "Propane";
            case GasSensor::GasType::Hydrogen: return "H2";
            case GasSensor::GasType::Smoke: return "Smoke";
            case GasSensor::GasType::Ammonia: return "NH3";
            case GasSensor::GasType::Benzene: return "Benzene";
            case GasSensor::GasType::Toluene: return "Toluene";
            case GasSensor::GasType::Acetone: return "Acetone";
            default: return "Unknown";
        }
    }

protected:
    /**
     * @brief Processes telemetry data by sending it via MQTT.
     * @param rawQueueItemPayload Pointer to the queued telemetry payload.
     */
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) const override {
        if (rawQueueItemPayload != nullptr) {
            gatewayClient.sendTelemetryRecord(*rawQueueItemPayload);
        }
    }

    /**
     * @brief Executes periodic background network maintenance.
     */
    void maintainNetworkServices() const override {
        gatewayClient.service();
    }

public:
    /**
     * @brief Constructs the AirQualityMonitorDevice.
     * @param gasAnalogPin Analog pin for the gas sensor.
     * @param gasDigitalPin Digital pin for the gas sensor's threshold output.
     * @param buzzerPin GPIO pin for the buzzer.
     * @param lcdI2CAddress I2C address of the LCD.
     * @param lcdColumns Number of columns on the LCD.
     * @param lcdRows Number of rows on the LCD.
     * @param client MQTT Gateway Client reference.
     * @param samplingIntervalInMilliseconds Polling interval for sensors.
     * @param timerGroupIndex Hardware timer index.
     */
    AirQualityMonitorDevice(int gasAnalogPin, int gasDigitalPin, int buzzerPin,
                            uint8_t lcdI2CAddress, uint8_t lcdColumns, uint8_t lcdRows,
                            MqttGatewayClient& client,
                            unsigned long samplingIntervalInMilliseconds, uint8_t timerGroupIndex = 0)
        : Device(samplingIntervalInMilliseconds, timerGroupIndex),
          gasSensor(gasAnalogPin, gasDigitalPin, this, GasSensor::OperatingMode::RawMode), // Start in RawMode
          alarmBuzzer(buzzerPin, this),
          statusDisplay(lcdI2CAddress, lcdColumns, lcdRows, LCD_BACKLIGHT_INITIAL_STATE, this),
          gatewayClient(client)
    {
        // Set handlers for sensors (this device acts as the EventHandler)
        gasSensor.setHandler(this);

        // Append sensors to the scheduler
        appendSensorToScheduler(&gasSensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);

        // Initialize asynchronous engine
        // Start the engine only after all registrations: Device rejects registrations once it is running.
        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH);
    }

    /**
     * @brief Handles sensor data-ready and gas detection events.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER ||
            event.identifier == GasSensor::DIGITAL_GAS_DETECTED_EVENT_IDENTIFIER) {

            updateOperationStatusDisplay(); // Renamed from updateLcd

            // Control buzzer based on gas detection status
            if (gasSensor.isGasDetected()) {
                alarmBuzzer.setTone(BUZZER_TONE_FREQ, BUZZER_TONE_DURATION_IN_MILLISECONDS);
                alarmBuzzer.handle(Buzzer::PLAY_TONE_COMMAND);
            } else {
                alarmBuzzer.handle(Buzzer::TURN_OFF_COMMAND);
            }

            // Enqueue telemetry
            auto packet = std::make_unique<AirQualityTelemetryPackage>(
                gasSensor.getGasConcentrationInPpm(),
                gasSensor.isGasDetected(),
                gasSensor.getOperatingMode()
            );
            enqueueTelemetryPayload(std::move(packet));
        }

        if (gasSensor.getOperatingMode() == GasSensor::OperatingMode::CalibratingMode) {
            // During calibration, we want to update the LCD with instructions and status even if no new data is propagated
            updateOperationStatusDisplay();
        }
    }

    /**
     * @brief Handles incoming commands (not directly used by this device, but required by interface).
     * @param command The command to handle.
     */
    void handle(Command command) override {
        // Not directly handling commands for this example, but could be extended
    }

    /**
     * @brief Starts the gas sensor calibration process.
     */
    void startGasSensorCalibration() {
        gasSensor.startCalibration();
    }

    /**
     * @brief Sets the target gas type for PPM calculation.
     * @param gasType The GasType to target.
     */
    void setGasSensorTargetGas(GasSensor::GasType gasType) {
        gasSensor.setTargetGas(gasType);
    }
};
//@} // Application Mediator Implementation

/** @name Top-Level Declarative Canvas */
//@{

/** @brief Hardware Pin Definitions */
/// @brief Analog pin for the gas sensor (ADC input).
static const int GAS_ANALOG_PIN = 34; // Example: ESP32 ADC1_CH6
/// @brief Digital pin for the gas sensor's threshold output (digital I/O).
static const int GAS_DIGITAL_PIN = 2; // Example: ESP32 GPIO2 (D0 on some MQ boards)
/// @brief GPIO pin for the buzzer (PWM output).
static const int BUZZER_PIN = 13; // Example: ESP32 GPIO13

/** @brief LCD Display Configurations */
/// @brief I2C address of the LCD module (e.g., 0x27 or 0x3F).
static const uint8_t LCD_I2C_ADDRESS = 0x27; 
/// @brief Number of columns on the LCD (e.g., 16 or 20).
static const uint8_t LCD_COLUMNS = 16;
/// @brief Number of rows on the LCD (e.g., 2 or 4).
static const uint8_t LCD_ROWS = 2;

/** @brief Network and MQTT Configuration */
/// @brief WiFi SSID for network connection.
static const char* WIFI_SSID = "Wokwi-GUEST";
/// @brief WiFi password for network connection.
static const char* WIFI_PASSWORD = "";
/// @brief MQTT broker IP address.
static const char* MQTT_BROKER_IP = "192.168.1.100"; 
/// @brief MQTT broker port.
static const uint16_t MQTT_BROKER_PORT = 1883;
/// @brief MQTT topic for publishing telemetry data.
static const char* MQTT_PUBLISH_TOPIC = "airquality/monitor/data";
/// @brief MQTT client ID to identify this device.
static const char* MQTT_CLIENT_ID = "AirQualityMonitor01";

// Framework Instance Pointers
static WiFiConnectivityDriver* transportDriver = nullptr;
static MqttGatewayClient* gatewayClient = nullptr;
static AirQualityMonitorDevice* airQualityApplication = nullptr;

/** @name Serial Output Configuration */
//@{
/** @brief Log prefix for all AirQualityMonitor serial output. */
static const char* AIR_QUALITY_MONITOR_LOG_PREFIX = "[AirQualityMonitor]";
/** @brief Message printed when setup begins. */
static const char* STARTING_MESSAGE = " Starting up...";
/** @brief Message printed when setup finishes. */
static const char* SETUP_COMPLETE_MESSAGE = " Setup complete. Monitoring started.";
//@} // Serial Output Configuration

/**
 * @brief Initializes the device, sensors, connectivity, and MQTT gateway.
 */
void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Device::setFirmwareVersion(FIRMWARE_VERSION);
    while (!Serial && millis() < SERIAL_TIMEOUT_IN_MILLISECONDS) { /* wait for serial port to connect */ }
    Serial.println((String(AIR_QUALITY_MONITOR_LOG_PREFIX) + STARTING_MESSAGE).c_str());

    // 1. Initialize Connectivity
    transportDriver = new WiFiConnectivityDriver(WIFI_SSID, WIFI_PASSWORD);
    transportDriver->connect(); // Start WiFi connection in background

    // 2. Initialize MQTT Gateway
    gatewayClient = new MqttGatewayClient(
        *transportDriver,
        transportDriver->getNetworkSocket(), // Use the exposed WiFiClient
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT,
        MQTT_PUBLISH_TOPIC,
        MQTT_CLIENT_ID
    );

    // 3. Initialize Air Quality Monitor Device
    airQualityApplication = new AirQualityMonitorDevice(
        GAS_ANALOG_PIN,
        GAS_DIGITAL_PIN,
        BUZZER_PIN,
        LCD_I2C_ADDRESS,
        LCD_COLUMNS,
        LCD_ROWS,
        *gatewayClient,
        SAMPLING_INTERVAL_IN_MILLISECONDS
    );

    // 4. Configure Gas Sensor Calibration and Target Gas
    // IMPORTANT: Ensure the sensor is in clean air during calibration!
    airQualityApplication->startGasSensorCalibration();
    airQualityApplication->setGasSensorTargetGas(GasSensor::GasType::LPG); // Example: Target LPG for PPM calculation

    Serial.println((String(AIR_QUALITY_MONITOR_LOG_PREFIX) + SETUP_COMPLETE_MESSAGE).c_str());
}

/**
 * @brief The main application loop, kept empty as the device runs asynchronously.
 */
void loop() {
    // The master loop is completely clean and empty.
    // It yields 100% of its execution cycles natively back to the FreeRTOS kernel scheduler.
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
//@} // Top-Level Declarative Canvas