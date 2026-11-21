/**
 * @file SmartGreenhouseDevice.ino
 * @brief Automated greenhouse climate controller with irrigation and ventilation.
 *
 * Monitors soil moisture and ambient temperature to automatically control a water pump
 * (via relay) and a ventilation fan (via servo motor). When soil is dry, the pump activates.
 * When temperature exceeds 30°C, the vent opens. Demonstrates command payload usage with
 * `DRIVE_TO_ANGLE_COMMAND` for servo angle control.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `Device` base class with async engine for loop-less execution
 * - `SoilMoistureSensor` (analog) for soil moisture monitoring
 * - `DhtSensor` (digital) for temperature/humidity monitoring
 * - `RelayActuator` with `RELAY_ON_COMMAND` and `RELAY_OFF_COMMAND` for water pump
 * - `ServoMotor` with `DRIVE_TO_ANGLE_COMMAND` (payload-based angle) for vent control
 * - `event.sourceId` disambiguation for soil vs. temperature sensors
 * - Threshold-based automation logic in `on(Event)` handler
 * - Direct actuator command dispatch via `handle(Command)`
 * @date 2026-11-09
 * @version 1.0
 */

#include <Arduino.h>
#include <ModestIoT.h>

// Pin definitions

static const int SOIL_MOISTURE_PIN    = 34;
static const int DHT_DATA_PIN         = 27;
static const int RELAY_PUMP_PIN       = 26;
static const int SERVO_VENT_PIN       = 13;

// Threshold constants

static constexpr int   MOISTURE_DRY_THRESHOLD    = 300;  ///< ADC value below which soil is considered dry
static constexpr float TEMPERATURE_HIGH_THRESHOLD = 30.0f; ///< Celsius above which ventilation is needed

// Servo angles

static constexpr int VENT_CLOSED_ANGLE = 0;
static constexpr int VENT_OPEN_ANGLE   = 90;
static constexpr bool WATER_PUMP_INITIAL_STATE = false; ///< Pump relay starts off.

// Timing constants

static const unsigned long SERIAL_BAUD_RATE = 115200;

// Asynchronous engine configuration

static const int            ASYNC_ENGINE_QUEUE_DEPTH    = 16;
static const unsigned long  SAMPLING_INTERVAL_IN_MILLISECONDS        = 1000;
static const unsigned long  MAIN_LOOP_DELAY_IN_MILLISECONDS          = 60000;
static const int            TIMER_GROUP_INDEX           = 0;

static const char*          SYSTEM_BOOT_MESSAGE         = "[System Boot] Smart Greenhouse active.";
static const char*          PUMP_ON_MESSAGE             = "[Irrigation] Pump ON — soil dry.";
static const char*          PUMP_OFF_MESSAGE            = "[Irrigation] Pump OFF — soil moist.";
static const char*          VENT_ON_MESSAGE             = "[Ventilation] Vent OPEN — temperature high.";
static const char*          VENT_OFF_MESSAGE            = "[Ventilation] Vent CLOSED — temperature normal.";
/** @brief Consecutive failed DHT22 readings before the sensor reports a fault. */
static const unsigned int SENSOR_FAULT_THRESHOLD = 3;
/** @brief Message printed when the DHT22 reports a fault. */
static const char* SENSOR_FAULT_MESSAGE = "[Climate Sensor] DHT22 not responding: check wiring and pull-up.";
/** @brief Message printed when the DHT22 answers again after a fault. */
static const char* SENSOR_RECOVERED_MESSAGE = "[Climate Sensor] DHT22 recovered.";

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "SGD-707 FW 1.0.0";

/**
 * @brief Mediator for the Smart Greenhouse Device.
 * Coordinates soil moisture and temperature sensors to control
 * a water pump (relay) and ventilation (servo motor).
 */
class SmartGreenhouseDevice : public Device {
private:
    SoilMoistureSensor soilSensor;
    DhtSensor          temperatureSensor;
    RelayActuator      waterPump;
    ServoMotor         ventMotor;

    /**
     * @brief Logs a message to Serial.
     * @param message Message string.
     */
    void logMessage(const char* message) {
        Serial.println(message);
    }

public:
    SmartGreenhouseDevice()
        : Device(SAMPLING_INTERVAL_IN_MILLISECONDS, TIMER_GROUP_INDEX),
          soilSensor(SOIL_MOISTURE_PIN, this),
          temperatureSensor(DHT_DATA_PIN, DhtSensor::SENSOR_MODEL_DHT22, this),
          waterPump(RELAY_PUMP_PIN, WATER_PUMP_INITIAL_STATE, this),
          ventMotor(SERVO_VENT_PIN, VENT_CLOSED_ANGLE, this)
    {
        // IMPORTANT: Registration must occur before initializeAsynchronousEngine(), because
        // the engine start guard rejects any registration after the engine is running.
        appendSensorToScheduler(&soilSensor,        Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        // Report a fault after consecutive failed readings instead of failing silently
        temperatureSensor.setFaultThreshold(SENSOR_FAULT_THRESHOLD);
        appendSensorToScheduler(&temperatureSensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);

        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH);
    }

    /**
     * @brief Handles sensor data-ready events to control irrigation and ventilation.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        // DHT22 health: report faults instead of failing silently
        if (event.identifier == Sensor::SENSOR_FAULT_EVENT_IDENTIFIER && event.sourceId == temperatureSensor.getPin()) {
            logMessage(SENSOR_FAULT_MESSAGE);
            return;
        }
        if (event.identifier == Sensor::SENSOR_RECOVERED_EVENT_IDENTIFIER && event.sourceId == temperatureSensor.getPin()) {
            logMessage(SENSOR_RECOVERED_MESSAGE);
            event.identifier = Sensor::DATA_READ_EVENT_IDENTIFIER; // The recovery carries a fresh reading
        }

        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER) {
            if (event.sourceId == SOIL_MOISTURE_PIN) {
                // Control water pump based on soil moisture
                int moisture = soilSensor.getMoistureLevel();
                if (moisture < MOISTURE_DRY_THRESHOLD) {
                    waterPump.handle(RelayActuator::RELAY_ON_COMMAND);
                    logMessage(PUMP_ON_MESSAGE);
                } else {
                    waterPump.handle(RelayActuator::RELAY_OFF_COMMAND);
                    logMessage(PUMP_OFF_MESSAGE);
                }
            } else if (event.sourceId == DHT_DATA_PIN) {
                // Control ventilation based on temperature
                float temperature = temperatureSensor.getTemperatureInCelsius();
                if (temperature > TEMPERATURE_HIGH_THRESHOLD) {
                    ventMotor.handle(Command{ServoMotor::DRIVE_TO_ANGLE_COMMAND_IDENTIFIER, VENT_OPEN_ANGLE});
                    logMessage(VENT_ON_MESSAGE);
                } else {
                    ventMotor.handle(Command{ServoMotor::DRIVE_TO_ANGLE_COMMAND_IDENTIFIER, VENT_CLOSED_ANGLE});
                    logMessage(VENT_OFF_MESSAGE);
                }
            }
        }
    }
};

static SmartGreenhouseDevice* smartGreenhouseApplication = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Device::setFirmwareVersion(FIRMWARE_VERSION);
    smartGreenhouseApplication = new SmartGreenhouseDevice();
    Serial.println(SYSTEM_BOOT_MESSAGE);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
