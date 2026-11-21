/**
 * @file IndoorAirQualityDisplay.ino
 * @brief Indoor air quality monitor with OLED display showing VOC, temperature, humidity, and pressure.
 *
 * This example demonstrates a grounded real-world device: a desktop air quality monitor
 * that uses a BME680 sensor to measure volatile organic compounds (VOC), temperature,
 * humidity, and atmospheric pressure. The readings are displayed in real-time on a
 * 128x64 OLED screen using the SSD1306 driver.
 *
 * The BME680 gas resistance reading serves as a VOC/IAQ indicator: lower resistance
 * indicates higher VOC concentration (poorer air quality). The display shows a simple
 * bar graph for the VOC level alongside numeric readings.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `EnvironmentalGasSensor` for BME680 VOC/IAQ detection (extends BmeEnvironmentalSensor)
 * - `OledDisplay` for SSD1306-based 128x64 monochrome display
 * - `setText()` for rendering text on the OLED display
 * - `fillRectangle()` for drawing a VOC level bar graph
 * - `drawHorizontalLine()` for display layout separators
 * - `UPDATE_DISPLAY_COMMAND` and `CLEAR_DISPLAY_COMMAND` for display control
 * - Multi-sensor data aggregation on a single display
 * - Loop-less, event-driven asynchronous execution via `initializeAsynchronousEngine()`
 *
 * @par Display layout (128x64 pixels):
 * - Row 0 (y=0):   "Air Quality Monitor"
 * - Row 1 (y=8):   Separator line
 * - Row 2 (y=16):  VOC level bar graph + label
 * - Row 3 (y=24):  Temperature reading
 * - Row 4 (y=32):  Humidity reading
 * - Row 5 (y=40):  Pressure reading
 * - Row 6 (y=48):  Gas resistance (kOhms)
 *
 * @date 2026-11-12
 * @version 1.0
 */

#include <Arduino.h>
#include <cstddef>
#include "ModestIoT.h"

/** @brief Sampler event queue length for air quality display. */
static const int SAMPLER_QUEUE_LENGTH = 10;

/** @brief Buffer size for formatted display strings. */
static const size_t DISPLAY_STRING_BUFFER_SIZE = 32;

/** @brief Serial communication baud rate. */
static const unsigned long SERIAL_BAUD_RATE = 115200;

/** @brief Delay in the main loop when no work is pending. */
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 60000;

/** @name Serial Output Configuration */
//@{
/** @brief Log prefix for all IndoorAirQualityDisplay serial output. */
static const char* INDOOR_AIR_QUALITY_LOG_PREFIX = "[IndoorAirQualityDisplay]";
/** @brief Format of the serial readings line (prefix, gas, temperature, humidity, pressure). */
static const char* SERIAL_READINGS_FORMAT = "%s VOC: %.0fkOhm, Temp: %.1fC, Hum: %.1f%%, Pres: %.1fhPa\n";
/** @brief Message printed when the BME680 does not answer on the I2C bus. */
static const char* SENSOR_NOT_FOUND_MESSAGE = " BME680 sensor not found on I2C bus";
/** @brief Message printed once the monitor has started. */
static const char* APPLICATION_ONLINE_MESSAGE = " Indoor air quality monitor online.";
/** @brief Message printed when the device cannot start. */
static const char* STARTUP_FAILED_MESSAGE = " Device startup failed.";
//@} // Serial Output Configuration

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "IAQ-800 FW 1.0.0";

/** @name VOC Level Estimation */
//@{
/** @brief Gas resistance treated as clean air (0% VOC level), in kOhm. */
static const float VOC_CLEAN_AIR_RESISTANCE_IN_KOHMS = 100.0f;
/** @brief Scale factor that converts a 0-1 ratio to a percentage. */
static const float PERCENTAGE_SCALE = 100.0f;
/** @brief Lowest VOC level percentage reported. */
static const float MINIMUM_VOC_LEVEL_PERCENTAGE = 0.0f;
/** @brief Highest VOC level percentage reported. */
static const float MAXIMUM_VOC_LEVEL_PERCENTAGE = 100.0f;
//@} // VOC Level Estimation

/** @name OLED Layout (128x64 SSD1306, pixel coordinates) */
//@{
/** @brief Display width used by the separator line. */
static const uint8_t OLED_WIDTH_IN_PIXELS = 128;
/** @brief Left column for all text lines. */
static const uint8_t TEXT_COLUMN_X = 0;
/** @brief Row of the title (and first startup line). */
static const uint8_t TITLE_ROW_Y = 0;
/** @brief Row of the separator line (and second startup line). */
static const uint8_t SEPARATOR_ROW_Y = 8;
/** @brief Row of the startup status line. */
static const uint8_t STARTUP_STATUS_ROW_Y = 24;
/** @brief Left edge of the VOC bar graph. */
static const uint8_t VOC_BAR_X = 2;
/** @brief Top edge of the VOC bar graph. */
static const uint8_t VOC_BAR_Y = 18;
/** @brief Width of the VOC bar at 100%. */
static const uint8_t VOC_BAR_MAXIMUM_WIDTH_IN_PIXELS = 80;
/** @brief Height of the VOC bar graph. */
static const uint8_t VOC_BAR_HEIGHT_IN_PIXELS = 4;
/** @brief Column of the VOC label next to the bar. */
static const uint8_t VOC_LABEL_X = 90;
/** @brief Row of the VOC label. */
static const uint8_t VOC_LABEL_Y = 16;
/** @brief Row of the temperature line. */
static const uint8_t TEMPERATURE_ROW_Y = 24;
/** @brief Row of the humidity line. */
static const uint8_t HUMIDITY_ROW_Y = 32;
/** @brief Row of the pressure line. */
static const uint8_t PRESSURE_ROW_Y = 40;
/** @brief Row of the gas resistance line. */
static const uint8_t GAS_RESISTANCE_ROW_Y = 48;
//@} // OLED Layout

/** @name OLED Display Text */
//@{
static const char* DISPLAY_TITLE_TEXT = "Air Quality Monitor";      ///< Title of the readings screen.
static const char* VOC_LABEL_TEXT = "VOC";                          ///< Label of the VOC bar graph.
static const char* TEMPERATURE_FORMAT = "Temp: %.1f C";             ///< Temperature line format.
static const char* HUMIDITY_FORMAT = "Hum: %.1f%%";                 ///< Humidity line format.
static const char* PRESSURE_FORMAT = "Pres: %.1f hPa";              ///< Pressure line format.
static const char* GAS_RESISTANCE_FORMAT = "Gas: %.0fkOhm";         ///< Gas resistance line format.
static const char* STARTUP_TITLE_LINE_1 = "Air Quality";            ///< Startup screen, first line.
static const char* STARTUP_TITLE_LINE_2 = "Monitor";                ///< Startup screen, second line.
static const char* STARTUP_STATUS_TEXT = "Starting...";             ///< Startup screen, status line.
//@} // OLED Display Text

/** @name Application Mediator Implementation */
//@{

/**
 * @brief Indoor air quality monitor with OLED display.
 *
 * This class extends the `Device` base class to manage a BME680 environmental gas sensor
 * and an SSD1306 OLED display. Sensor readings are rendered on the display with a VOC
 * level bar graph and numeric values for temperature, humidity, and pressure.
 */
class IndoorAirQualityDisplay : public Device {
private:
    EnvironmentalGasSensor airQualitySensor;
    OledDisplay statusDisplay;

    char displayBuffer[DISPLAY_STRING_BUFFER_SIZE]; ///< Reusable buffer for formatted strings.

    /**
     * @brief Computes a VOC level (0-100) from gas resistance.
     *
     * Lower gas resistance indicates higher VOC concentration.
     * The scale is approximate: >100kOhms = excellent (0%), <10kOhms = poor (100%).
     *
     * @param gasResistanceInKOhms Current gas resistance reading.
     * @return VOC level percentage (0-100).
     */
    uint8_t computeVocLevelPercentage(float gasResistanceInKOhms) const {
        if (gasResistanceInKOhms <= 0.0f) return 0;
        // Map resistance to VOC level: higher resistance = lower VOC = better air
        float vocLevel = PERCENTAGE_SCALE * (1.0f - (gasResistanceInKOhms / VOC_CLEAN_AIR_RESISTANCE_IN_KOHMS));
        if (vocLevel < MINIMUM_VOC_LEVEL_PERCENTAGE) vocLevel = MINIMUM_VOC_LEVEL_PERCENTAGE;
        if (vocLevel > MAXIMUM_VOC_LEVEL_PERCENTAGE) vocLevel = MAXIMUM_VOC_LEVEL_PERCENTAGE;
        return static_cast<uint8_t>(vocLevel);
    }

    /**
     * @brief Renders all sensor readings on the OLED display.
     */
    void renderDisplay() {
        // Clear the display
        statusDisplay.clearFrameBuffer();

        // Title
        statusDisplay.setText(DISPLAY_TITLE_TEXT, TEXT_COLUMN_X, TITLE_ROW_Y);

        // Separator line
        statusDisplay.drawHorizontalLine(TEXT_COLUMN_X, SEPARATOR_ROW_Y, OLED_WIDTH_IN_PIXELS);

        // VOC level bar graph
        uint8_t vocLevel = computeVocLevelPercentage(
            airQualitySensor.getGasResistanceInKOhms());
        uint8_t barWidth = static_cast<uint8_t>((vocLevel * VOC_BAR_MAXIMUM_WIDTH_IN_PIXELS) / MAXIMUM_VOC_LEVEL_PERCENTAGE);
        statusDisplay.fillRectangle(VOC_BAR_X, VOC_BAR_Y, barWidth, VOC_BAR_HEIGHT_IN_PIXELS);
        statusDisplay.setText(VOC_LABEL_TEXT, VOC_LABEL_X, VOC_LABEL_Y);

        // Temperature
        snprintf(displayBuffer, sizeof(displayBuffer), TEMPERATURE_FORMAT,
                 airQualitySensor.getTemperatureInCelsius());
        statusDisplay.setText(displayBuffer, TEXT_COLUMN_X, TEMPERATURE_ROW_Y);

        // Humidity
        snprintf(displayBuffer, sizeof(displayBuffer), HUMIDITY_FORMAT,
                 airQualitySensor.getHumidityInPercentage());
        statusDisplay.setText(displayBuffer, TEXT_COLUMN_X, HUMIDITY_ROW_Y);

        // Pressure
        snprintf(displayBuffer, sizeof(displayBuffer), PRESSURE_FORMAT,
                 airQualitySensor.getPressureInHectopascals());
        statusDisplay.setText(displayBuffer, TEXT_COLUMN_X, PRESSURE_ROW_Y);

        // Gas resistance
        snprintf(displayBuffer, sizeof(displayBuffer), GAS_RESISTANCE_FORMAT,
                 airQualitySensor.getGasResistanceInKOhms());
        statusDisplay.setText(displayBuffer, TEXT_COLUMN_X, GAS_RESISTANCE_ROW_Y);

        // Update the display
        statusDisplay.handle(OledDisplay::UPDATE_DISPLAY_COMMAND);
    }

protected:
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) const override {
        // This device does not send telemetry; it only displays readings locally.
        (void)rawQueueItemPayload;
    }

    void maintainNetworkServices() const override {
        // No network services for this standalone display device.
    }

public:
    /**
     * @brief Constructs a new IndoorAirQualityDisplay application instance.
     *
     * @param sensorPin Source identifier for the BME680 sensor (not a GPIO pin;
     *                   BME680 uses I2C).
     * @param displayI2cAddress I2C address of the SSD1306 OLED display.
     * @param samplingIntervalInMilliseconds How often to sample sensors (ms).
     * @param timerGroupIndex Hardware timer group index (0-3).
     */
    IndoorAirQualityDisplay(int sensorPin, uint8_t displayI2cAddress,
                            unsigned long samplingIntervalInMilliseconds,
                            uint8_t timerGroupIndex = 0)
        : Device(samplingIntervalInMilliseconds, timerGroupIndex),
          airQualitySensor(sensorPin, EnvironmentalGasSensor::DEFAULT_I2C_ADDRESS, this),
          statusDisplay(displayI2cAddress, this)
    {
        appendSensorToScheduler(&airQualitySensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);

        // Start the engine only after all registrations: Device rejects registrations once it is running.
        initializeAsynchronousEngine(SAMPLER_QUEUE_LENGTH);
    }

    ~IndoorAirQualityDisplay() override = default;

    /**
     * @brief Handles sensor data events and updates the display.
     *
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER) {
            renderDisplay();
            Serial.printf(SERIAL_READINGS_FORMAT,
                          INDOOR_AIR_QUALITY_LOG_PREFIX,
                          airQualitySensor.getGasResistanceInKOhms(),
                          airQualitySensor.getTemperatureInCelsius(),
                          airQualitySensor.getHumidityInPercentage(),
                          airQualitySensor.getPressureInHectopascals());
        }
    }

    void handle(Command command) override {
        // This device does not process external commands.
        (void)command;
    }

    /**
     * @brief Initializes the display and shows a startup message.
     * @return True if the sensor initialized successfully.
     */
    bool start() {
        Device::setFirmwareVersion(FIRMWARE_VERSION);
        if (!airQualitySensor.isInitialized()) {
            Serial.println((String(INDOOR_AIR_QUALITY_LOG_PREFIX) + SENSOR_NOT_FOUND_MESSAGE).c_str());
            return false;
        }

        // Show startup message
        statusDisplay.clearFrameBuffer();
        statusDisplay.setText(STARTUP_TITLE_LINE_1, TEXT_COLUMN_X, TITLE_ROW_Y);
        statusDisplay.setText(STARTUP_TITLE_LINE_2, TEXT_COLUMN_X, SEPARATOR_ROW_Y);
        statusDisplay.setText(STARTUP_STATUS_TEXT, TEXT_COLUMN_X, STARTUP_STATUS_ROW_Y);
        statusDisplay.handle(OledDisplay::UPDATE_DISPLAY_COMMAND);

        Serial.println((String(INDOOR_AIR_QUALITY_LOG_PREFIX) + APPLICATION_ONLINE_MESSAGE).c_str());
        return true;
    }
};
//@} // Application Mediator Implementation

/** @name Top-Level Declarative Canvas */
//@{

static const int BME680_SOURCE_IDENTIFIER = 1; ///< Source ID for BME680 (not a GPIO pin).
static const uint8_t OLED_DISPLAY_I2C_ADDRESS = 0x3C; ///< I2C address of SSD1306 display.

static const unsigned long SAMPLING_INTERVAL_IN_MILLISECONDS = 2000;
static const uint8_t HARDWARE_TIMER_CHANNEL_INDEX = 0;

static IndoorAirQualityDisplay* airQualityApplication = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);

    airQualityApplication = new IndoorAirQualityDisplay(
        BME680_SOURCE_IDENTIFIER,
        OLED_DISPLAY_I2C_ADDRESS,
        SAMPLING_INTERVAL_IN_MILLISECONDS,
        HARDWARE_TIMER_CHANNEL_INDEX
    );

    if (!airQualityApplication->start()) {
        Serial.println((String(INDOOR_AIR_QUALITY_LOG_PREFIX) + STARTUP_FAILED_MESSAGE).c_str());
        return;
    }
}

void loop() {
    // The master loop is completely clean and empty.
    // The framework handles all timing, sensor sampling, and display updates.
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
//@} // Top-Level Declarative Canvas
