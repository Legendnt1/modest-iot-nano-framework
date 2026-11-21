/**
 * @file PerimeterMonitor.ino
 * @brief Property perimeter intrusion monitor with four-zone motion detection.
 *
 * Places PIR motion sensors at four corners (N, E, S, W) of a property perimeter,
 * with an LED indicator at each zone and a 16x2 LCD showing the overall status.
 * All PIR sensors use interrupt-driven operation for zero-latency motion detection.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `Device` base class with async engine for loop-less execution
 * - `PassiveInfraredSensor` (four instances) with interrupt-driven operation
 * - `Led` actuator (four instances) with `TURN_ON_COMMAND` and `TURN_OFF_COMMAND`
 * - `CharacterLcdDisplay` actuator with `UPDATE_TEXT_COMMAND` for status display
 * - `event.sourceId` disambiguation for four sensors of the same type
 * - `appendSensorToTriggeringScheduler()` with `CHANGE` edge for all PIR sensors
 * - Cross-task event routing via `on(Event)` handler
 * @date 2026-06-20
 * @version 1.1
 */

#include <Arduino.h>
#include <ModestIoT.h>

// Pin definitions
static const int NORTH_CORNER_PIR_PIN           = 4;
static const int EAST_CORNER_PIR_PIN            = 18;
static const int SOUTH_CORNER_PIR_PIN           = 26;
static const int WEST_CORNER_PIR_PIN            = 32;

static const int NORTH_CORNER_LED_PIN           = 5;
static const int EAST_CORNER_LED_PIN            = 19;
static const int SOUTH_CORNER_LED_PIN           = 27;
static const int WEST_CORNER_LED_PIN            = 33;

static const int LCD_I2C_ADDRESS                = 0x27;

// LCD dimensions
static const int LCD_COLUMNS                    = 16;
static const int LCD_BUFFER_SIZE                = LCD_COLUMNS + 1;
static const int LCD_ROWS                       = 2;

// Timing
static const unsigned long SAMPLING_INTERVAL_IN_MILLISECONDS = 100;
static const int ASYNC_ENGINE_QUEUE_DEPTH = 32;
static const uint8_t TIMER_GROUP_INDEX = 0;           ///< Hardware timer group index used by the device.
static const bool ZONE_LED_INITIAL_STATE = false;     ///< Zone LEDs start off (no motion).
static const bool LCD_BACKLIGHT_INITIAL_STATE = true; ///< LCD backlight starts on.
static const int LCD_TITLE_ROW = 0;                   ///< LCD row with the title.
static const int LCD_STATUS_ROW = 1;                  ///< LCD row with the zone status.
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 1000;

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "PMN-202 FW 1.0.0";

/**
 * @brief Perimeter Monitor Device.
 */
class PerimeterMonitor : public Device {
private:
    PassiveInfraredSensor northCornerPir;
    PassiveInfraredSensor eastCornerPir;
    PassiveInfraredSensor southCornerPir;
    PassiveInfraredSensor westCornerPir;
    
    Led northCornerLed;
    Led eastCornerLed;
    Led southCornerLed;
    Led westCornerLed;
    
    CharacterLcdDisplay statusDisplay;

    void updateStatusDisplay() {
        static const char* STATUS_FORMAT = "N:%c E:%c S:%c W:%c";
        static const char MOTION_CHAR = 'M';
        static const char NO_MOTION_CHAR = '-';
        char buffer[LCD_BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), STATUS_FORMAT, 
                 northCornerLed.isActive()  ? MOTION_CHAR : NO_MOTION_CHAR,
                 eastCornerLed.isActive()   ? MOTION_CHAR : NO_MOTION_CHAR,
                 southCornerLed.isActive()  ? MOTION_CHAR : NO_MOTION_CHAR,
                 westCornerLed.isActive()   ? MOTION_CHAR : NO_MOTION_CHAR);
        statusDisplay.setLineBuffer(LCD_STATUS_ROW, buffer);
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);
    }

public:
    PerimeterMonitor()
        : Device(SAMPLING_INTERVAL_IN_MILLISECONDS, TIMER_GROUP_INDEX),
          northCornerPir(NORTH_CORNER_PIR_PIN, this),
          eastCornerPir(EAST_CORNER_PIR_PIN, this),
          southCornerPir(SOUTH_CORNER_PIR_PIN, this),
          westCornerPir(WEST_CORNER_PIR_PIN, this),
          northCornerLed(NORTH_CORNER_LED_PIN, ZONE_LED_INITIAL_STATE, this),
          eastCornerLed(EAST_CORNER_LED_PIN, ZONE_LED_INITIAL_STATE, this),
          southCornerLed(SOUTH_CORNER_LED_PIN, ZONE_LED_INITIAL_STATE, this),
          westCornerLed(WEST_CORNER_LED_PIN, ZONE_LED_INITIAL_STATE, this),
          statusDisplay(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS, LCD_BACKLIGHT_INITIAL_STATE, this)
    {
        // Register PIR sensors for interrupt-driven operation (CHANGE edge)
        // Motion detected (RISING) and motion stopped (FALLING) are both captured
        // IMPORTANT: Registration must occur before initializeAsynchronousEngine(), because
        // the engine start guard rejects any registration after the engine is running.
        appendSensorToTriggeringScheduler(&northCornerPir,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          CHANGE);
        appendSensorToTriggeringScheduler(&eastCornerPir,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          CHANGE);
        appendSensorToTriggeringScheduler(&southCornerPir,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          CHANGE);
        appendSensorToTriggeringScheduler(&westCornerPir,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          CHANGE);

        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH);

        static const char* TITLE_MESSAGE = "Perimeter Status";
        static const char* INITIALIZING_MESSAGE = "Initializing...";
        statusDisplay.setLineBuffer(LCD_TITLE_ROW, TITLE_MESSAGE);
        statusDisplay.setLineBuffer(LCD_STATUS_ROW, INITIALIZING_MESSAGE);
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);
    }

    /**
     * @brief Handles motion detected/stopped events from four PIR zones.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        // Handle Motion Events
        if (event.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER) {
            if (event.sourceId == NORTH_CORNER_PIR_PIN) northCornerLed.handle(Led::TURN_ON_COMMAND);
            else if (event.sourceId == EAST_CORNER_PIR_PIN) eastCornerLed.handle(Led::TURN_ON_COMMAND);
            else if (event.sourceId == SOUTH_CORNER_PIR_PIN) southCornerLed.handle(Led::TURN_ON_COMMAND);
            else if (event.sourceId == WEST_CORNER_PIR_PIN) westCornerLed.handle(Led::TURN_ON_COMMAND);
        } 
        else if (event.identifier == PassiveInfraredSensor::MOTION_STOPPED_EVENT_IDENTIFIER) {
            if (event.sourceId == NORTH_CORNER_PIR_PIN) northCornerLed.handle(Led::TURN_OFF_COMMAND);
            else if (event.sourceId == EAST_CORNER_PIR_PIN) eastCornerLed.handle(Led::TURN_OFF_COMMAND);
            else if (event.sourceId == SOUTH_CORNER_PIR_PIN) southCornerLed.handle(Led::TURN_OFF_COMMAND);
            else if (event.sourceId == WEST_CORNER_PIR_PIN) westCornerLed.handle(Led::TURN_OFF_COMMAND);
        }
        updateStatusDisplay();
    }
};

static PerimeterMonitor* perimeterMonitorApplication = nullptr;

void setup() {
    Device::setFirmwareVersion(FIRMWARE_VERSION);
    perimeterMonitorApplication = new PerimeterMonitor();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
