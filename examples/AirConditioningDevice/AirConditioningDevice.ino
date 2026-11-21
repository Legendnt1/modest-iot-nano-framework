/**
 * @file AirConditioningDevice.ino
 * @brief Room air conditioning controller with temperature display and vent control.
 *
 * Monitors room temperature and humidity via DHT22, displays readings on a 16x2 LCD,
 * and controls a servo-driven air vent based on user-set target temperature. Two buttons
 * adjust the target temperature up or down. The vent opens when the room exceeds the
 * target and closes when it drops below.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `Device` base class with async engine for loop-less execution
 * - `DhtSensor` for temperature/humidity monitoring
 * - `Button` sensors with interrupt-driven operation via `appendSensorToTriggeringScheduler()`
 * - `CharacterLcdDisplay` actuator with `UPDATE_TEXT_COMMAND` for real-time display
 * - `ServoMotor` actuator with `LOCK_TO_MAXIMUM_LIMIT_COMMAND` and `LOCK_TO_MINIMUM_LIMIT_COMMAND`
 * - Event routing via `on(Event)` with `DATA_READ_EVENT_IDENTIFIER` and `BUTTON_PRESSED_EVENT_IDENTIFIER`
 * - `event.sourceId` disambiguation for multiple buttons
 * - Direct actuator command dispatch via `handle(Command)`
 * @date 2026-06-12
 * @version 1.0
 */

#include <Arduino.h>
#include <ModestIoT.h>

/** @brief Serial communication baud rate. */
static const unsigned long SERIAL_BAUD_RATE = 115200;

/** @brief Delay in the main loop when no work is pending. */
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 60000;

/** @brief Sampling interval for the DHT sensor (ms). */
static const unsigned long SAMPLING_INTERVAL_IN_MILLISECONDS = 2000;

/** @brief Debounce time for buttons (ms). */
static const unsigned long BUTTON_DEBOUNCE_IN_MILLISECONDS = 150;

/** @brief Sampler event queue length. */
static const int ASYNC_ENGINE_QUEUE_DEPTH = 8;

/** @brief Default target temperature (°C). */
static const int DEFAULT_TARGET_TEMPERATURE = 22;

/** @brief Hardware timer group index used by the device. */
static const uint8_t TIMER_GROUP_INDEX = 0;

/** @brief Initial servo angle of the air handler vent (degrees). */
static const int AIR_HANDLER_INITIAL_ANGLE_IN_DEGREES = 0;

/** @brief Initial temperature and humidity before the first reading. */
static const float INITIAL_MEASUREMENT_VALUE = 0.0f;

/** @name Character LCD Layout */
//@{
/** @brief Number of character columns of the LCD. */
static const int LCD_COLUMNS = 16;
/** @brief Number of character rows of the LCD. */
static const int LCD_ROWS = 2;
/** @brief Characters reserved for one LCD line plus the null terminator. */
static const size_t LCD_LINE_BUFFER_SIZE = LCD_COLUMNS + 1;
/** @brief LCD row showing the current temperature and humidity. */
static const int LCD_CURRENT_MEASURE_ROW = 0;
/** @brief LCD row showing the target temperature. */
static const int LCD_TARGET_MEASURE_ROW = 1;
/** @brief Format of the current measurement line (temperature, humidity). */
static const char* CURRENT_MEASURE_FORMAT = "Cur: %.1fC  %.0f%%";
/** @brief Format of the target temperature line. */
static const char* TARGET_MEASURE_FORMAT = "Target: %dC";
//@} // Character LCD Layout

/** @name Serial Output Configuration */
//@{
/** @brief Button context label for the increase button. */
static const char* INCREASE_BUTTON_CONTEXT = "Increase";
/** @brief Button context label for the decrease button. */
static const char* DECREASE_BUTTON_CONTEXT = "Decrease";
/** @brief Button context label used when the event source is not a known button. */
static const char* UNKNOWN_BUTTON_CONTEXT = "Unknown";
/** @brief Format of the button press line (button context). */
static const char* TARGET_BUTTON_PRESSED_FORMAT = "[AC Device] Target temperature %s button pressed.\n";
/** @brief Message printed once the application has booted. */
static const char* APPLICATION_ACTIVE_MESSAGE = "[System Boot] Loop-less Air Conditioning Orchestrator active.";
//@} // Serial Output Configuration

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "AC-500 FW 1.0.0";

/** @name Pin Definitions */
//@{

static const int HUMIDITY_AND_TEMPERATURE_SENSOR_PIN    = 4;
static const int TARGET_TEMPERATURE_UP_BUTTON_PIN       = 12;
static const int TARGET_TEMPERATURE_DOWN_BUTTON_PIN     = 14;
static const int STATUS_DISPLAY_ADDRESS                 = 0x27;
static const int AIR_HANDLER_PIN                        = 13;
//@} // Pin Definitions

/** @name Application Mediator Implementation */
//@{

class AirConditioningDevice : public Device {
    private:
    // Inputs (Sensors)
    DhtSensor humidityAndTemperatureSensor;
    Button targetTemperatureUpButton;
    Button targetTemperatureDownButton;

    // Outputs (Actuators)
    CharacterLcdDisplay statusDisplay;
    ServoMotor airHandler;

    // Internal Application State Context
    float currentTemperature;
    float currentHumidity;
    int targetTemperature;

    /**
     * @brief Formats and updates the first line of the LCD buffer (Line 0).
     * Focuses strictly on real-time environmental metrics.
     */
    void updateCurrentMeasureDisplay() {
        char currentMeasureBuffer[LCD_LINE_BUFFER_SIZE];
        snprintf(currentMeasureBuffer, sizeof(currentMeasureBuffer), CURRENT_MEASURE_FORMAT,
            currentTemperature, currentHumidity);

        statusDisplay.setLineBuffer(LCD_CURRENT_MEASURE_ROW, currentMeasureBuffer);
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);
    }

    /**
     * @brief Formats and updates the second line of the LCD buffer (Line 1).
     * Focuses strictly on user-targeted baseline configurations.
     */
    void updateTargetMeasureDisplay() {
        char targetMeasureBuffer[LCD_LINE_BUFFER_SIZE];
        snprintf(targetMeasureBuffer, sizeof(targetMeasureBuffer), TARGET_MEASURE_FORMAT, targetTemperature);

        statusDisplay.setLineBuffer(LCD_TARGET_MEASURE_ROW, targetMeasureBuffer);
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);
    }

    /**
     * @brief Adjusts the air vent angle based on current vs target error offsets.
     */
    void evaluateAirHandlerOperationState() {
        // Open the vent fully if room temperature exceeds target; otherwise close it safely
        if (currentTemperature > targetTemperature) {
            airHandler.handle(ServoMotor::LOCK_TO_MAXIMUM_LIMIT_COMMAND);
        } else {
            airHandler.handle(ServoMotor::LOCK_TO_MINIMUM_LIMIT_COMMAND);
        }
    }

    public:

    /**
     * @param dhtPin GPIO pin connected to the DHT climate module.
     * @param upPin GPIO pin linked to the Increase temperature button.
     * @param downPin GPIO pin linked to the Decrease temperature button.
     * @param lcdAddr I2C device address for the character display.
     * @param servoPin GPIO PWM output pin driving the vent servo.
     */
    AirConditioningDevice(int dhtPin, int upPin, int downPin, uint8_t lcdAddr, int servoPin)
        : Device(SAMPLING_INTERVAL_IN_MILLISECONDS, TIMER_GROUP_INDEX),
        humidityAndTemperatureSensor(dhtPin, DhtSensor::SENSOR_MODEL_DHT22),
        targetTemperatureUpButton(upPin, BUTTON_DEBOUNCE_IN_MILLISECONDS, this),
        targetTemperatureDownButton(downPin, BUTTON_DEBOUNCE_IN_MILLISECONDS, this),
        statusDisplay(lcdAddr, LCD_COLUMNS, LCD_ROWS, this),
        airHandler(servoPin, AIR_HANDLER_INITIAL_ANGLE_IN_DEGREES, this),
        currentTemperature(INITIAL_MEASUREMENT_VALUE),
        currentHumidity(INITIAL_MEASUREMENT_VALUE),
        targetTemperature(DEFAULT_TARGET_TEMPERATURE)
    {
        // 1. Register the DHT sensor using the baseline scheduler ticker token
        appendSensorToScheduler(&humidityAndTemperatureSensor,  Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);

        // 2. Register buttons for interrupt-driven operation (FALLING edge)
        appendSensorToTriggeringScheduler(&targetTemperatureUpButton,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          FALLING);
        appendSensorToTriggeringScheduler(&targetTemperatureDownButton,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          FALLING);

        // 3. Launch FreeRTOS execution background engines
        // Start the engine only after all registrations: Device rejects registrations once it is running.
        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH);

        // 4. Initial rendering pass to populate both display lines
        updateCurrentMeasureDisplay();
        updateTargetMeasureDisplay();
    }

    /**
     * @brief Inversion of Control interface contract handler.
     * Intercepts framework events dispatched from background tasks.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {

        // Context A: DHT Sensor successfully finalized data acquisition
        if (event.identifier == DhtSensor::DATA_READ_EVENT_IDENTIFIER) {
            currentTemperature = humidityAndTemperatureSensor.getTemperatureInCelsius();
            currentHumidity = humidityAndTemperatureSensor.getRelativeHumidityInPercentage();
            updateCurrentMeasureDisplay();
        }

        // Context B & C: User pressed one ot the buttons
        else if (event.identifier == Button::BUTTON_PRESSED_EVENT_IDENTIFIER) {
            const char* buttonContext = UNKNOWN_BUTTON_CONTEXT;
            if (event.sourceId == TARGET_TEMPERATURE_UP_BUTTON_PIN) {
                targetTemperature++;
                buttonContext = INCREASE_BUTTON_CONTEXT;
            } else if (event.sourceId == TARGET_TEMPERATURE_DOWN_BUTTON_PIN) {
                targetTemperature--;
                buttonContext = DECREASE_BUTTON_CONTEXT;
            }
            updateTargetMeasureDisplay();
            Serial.printf(TARGET_BUTTON_PRESSED_FORMAT, buttonContext);
        }
        // Optimized Sequence: Evaluate control loops at the very end.
        // Ensures physical updates operate exclusively on fully settled states.
        evaluateAirHandlerOperationState();
    }
};
//@} // Application Mediator Implementation

/** @name Top-Level Declarative Canvas */
//@{

static AirConditioningDevice* airConditioningController = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Device::setFirmwareVersion(FIRMWARE_VERSION);

    // Boot complete application context framework dynamically
    airConditioningController = new AirConditioningDevice(
        HUMIDITY_AND_TEMPERATURE_SENSOR_PIN,
        TARGET_TEMPERATURE_UP_BUTTON_PIN,
        TARGET_TEMPERATURE_DOWN_BUTTON_PIN,
        STATUS_DISPLAY_ADDRESS,
        AIR_HANDLER_PIN
    );

    Serial.println(APPLICATION_ACTIVE_MESSAGE);
}

void loop() {
    // 100% of execution resources are permanently yielded to background FreeRTOS workers
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
//@} // Top-Level Declarative Canvas