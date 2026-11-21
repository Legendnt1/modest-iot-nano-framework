/**
 * @file DriverAssistantDevice.ino
 * @brief Vehicle driver assistant with GPS navigation and parking distance alerts.
 *
 * Combines a GPS module for speed tracking and two ultrasonic sensors (front and rear)
 * for parking assistance. Displays speed and distance readings on a 16x2 LCD. The GPS
 * uses a dedicated HardwareSerial port, while ultrasonic sensors use GPIO pins for
 * trigger/echo timing.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `Device` base class with async engine for loop-less execution
 * - `GeolocationSensor` with `HardwareSerial` for GPS NMEA parsing
 * - `UltrasonicSensor` (two instances) for front/rear distance measurement
 * - `CharacterLcdDisplay` actuator with `UPDATE_TEXT_COMMAND` for real-time display
 * - `event.sourceId` disambiguation for multiple sensors of the same type
 * - `LocationData` value object for GPS coordinate storage
 * - Cross-task event routing via `on(Event)` handler
 * @date 2026-06-15
 * @version 1.0
 */

#include <Arduino.h>
#include <ModestIoT.h>

// Pin definitions

static const int GPS_RX_PIN         = 16;
static const int GPS_TX_PIN         = 17;
static const int FRONT_TRIGGER_PIN  = 4;
static const int FRONT_ECHO_PIN     = 5;
static const int REAR_TRIGGER_PIN   = 18;
static const int REAR_ECHO_PIN      = 19;
static const int LCD_I2C_ADDRESS    = 0x27;

// Configuration constants

static const int GPS_BAUD_RATE = 9600;

// LCD dimensions

static const int LCD_COLUMNS    = 16;
static const int LCD_ROWS       = 2;

// Conversion factors

static constexpr float KNOTS_TO_KPH_CONVERSION_FACTOR = 1.852f;
static constexpr float CENTIMETER_TO_METER_CONVERSION_FACTOR = 0.01f;

// Timing constants

static const unsigned long SERIAL_BAUD_RATE = 115200;

// Asynchronous engine configuration

static const int            ASYNC_ENGINE_QUEUE_DEPTH    = 8;
static const unsigned long  SAMPLING_INTERVAL_IN_MILLISECONDS        = 1000;
static const unsigned long  MAIN_LOOP_DELAY_IN_MILLISECONDS          = 60000;
static const int            TIMER_GROUP_INDEX           = 0;

/** @brief GPS fix quality value meaning "no fix" (see LocationData::fixQuality). */
static const int GPS_NO_FIX_QUALITY = 0;

/** @brief Decimal places printed for latitude and longitude. */
static const int COORDINATE_DECIMAL_PLACES = 6;

/** @brief Initial distance before the first ultrasonic reading (meters). */
static const float INITIAL_DISTANCE_IN_METERS = 0.0f;

/** @name Character LCD Layout */
//@{
/** @brief Characters reserved for one LCD line plus the null terminator. */
static const size_t LCD_LINE_BUFFER_SIZE = LCD_COLUMNS + 1;
/** @brief LCD row showing GPS status or speed. */
static const int LCD_LOCATION_ROW = 0;
/** @brief LCD row showing front and rear distances. */
static const int LCD_DISTANCE_ROW = 1;
/** @brief Text shown while the GPS has no fix. */
static const char* NO_GPS_FIX_TEXT = "No GPS Fix";
/** @brief Format of the speed line (km/h). */
static const char* SPEED_FORMAT = "S:%.1fKmph";
/** @brief Format of the distance line (front and rear meters). */
static const char* DISTANCE_FORMAT = "F:%.1fm R:%.1fm";
//@} // Character LCD Layout

/** @name Serial Output Configuration */
//@{
/** @brief Label printed before the latitude. */
static const char* LATITUDE_LABEL = "Latitude: ";
/** @brief Label printed before the longitude. */
static const char* LONGITUDE_LABEL = "Longitude: ";
/** @brief Label printed before the GPS milliseconds field. */
static const char* MILLISECONDS_LABEL = "Milliseconds: ";
/** @brief Message printed once the application has booted. */
static const char* APPLICATION_ACTIVE_MESSAGE = "[System Boot] Driver Assistant active.";
//@} // Serial Output Configuration

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "DAD-400 FW 1.0.0";

/**
 * @brief Mediator for the Driver Assistant Device.
 * Coordinates Geolocation and Ultrasonic sensors to provide navigation
 * and parking assistance on an LCD display.
 */
class DriverAssistantDevice : public Device {
private:
    GeolocationSensor gpsSensor;
    UltrasonicSensor frontSensor;
    UltrasonicSensor rearSensor;
    CharacterLcdDisplay statusDisplay;

    // Internal state
    LocationData lastLocation;
    float frontDistanceInMeters;
    float rearDistanceInMeters;

    static constexpr float convertCentimetersToMeters(float measureInCentimeters) {
        return measureInCentimeters * CENTIMETER_TO_METER_CONVERSION_FACTOR;
    }

    /**
     * @brief Internal state maintenance and LCD interface update.
     * Updates the status display with the last known position and sensor metrics.
     */
    void updateStatusDisplay() {
        char locationStatusBuffer[LCD_LINE_BUFFER_SIZE];
        char measureStatusBuffer[LCD_LINE_BUFFER_SIZE];

        // Speed in km/h: knots * factor
        float speedInKph = lastLocation.speedInKnots * KNOTS_TO_KPH_CONVERSION_FACTOR;

        if (lastLocation.fixQuality == GPS_NO_FIX_QUALITY) {
        snprintf(locationStatusBuffer, sizeof(locationStatusBuffer), "%s", NO_GPS_FIX_TEXT);
    } else {
        // snprintf(locationStatusBuffer, sizeof(locationStatusBuffer), "Pos:%.1f,%.1f",
        //    lastLocation.latitudeInDegrees, lastLocation.longitudeInDegrees);
        snprintf(locationStatusBuffer, sizeof(locationStatusBuffer), SPEED_FORMAT, speedInKph);
        // Show additional information by console
        Serial.print(LATITUDE_LABEL); Serial.println(lastLocation.latitudeInDegrees, COORDINATE_DECIMAL_PLACES);
        Serial.print(LONGITUDE_LABEL); Serial.println(lastLocation.longitudeInDegrees, COORDINATE_DECIMAL_PLACES);
        Serial.print(MILLISECONDS_LABEL); Serial.println(lastLocation.milliseconds); // int: printed in base 10
    }

        snprintf(measureStatusBuffer, sizeof(measureStatusBuffer), DISTANCE_FORMAT,
        frontDistanceInMeters, rearDistanceInMeters);

        statusDisplay.setLineBuffer(LCD_LOCATION_ROW, locationStatusBuffer);
        statusDisplay.setLineBuffer(LCD_DISTANCE_ROW, measureStatusBuffer);
        statusDisplay.handle(CharacterLcdDisplay::UPDATE_TEXT_COMMAND);
    }

public:
    /**
     * @param gpsReceivePin GPIO pin for GPS RX.
     * @param gpsTransmitPin GPIO pin for GPS TX.
     * @param frontTriggerPin GPIO pin for Front Ultrasonic Trigger.
     * @param frontEchoPin GPIO pin for Front Ultrasonic Echo.
     * @param rearTriggerPin GPIO pin for Rear Ultrasonic Trigger.
     * @param rearEchoPin GPIO pin for Rear Ultrasonic Echo.
     * @param lcdI2cAddress I2C device address for the character display.
     */
    DriverAssistantDevice(HardwareSerial& gpsSerialPort, int gpsReceivePin, int gpsTransmitPin, int frontTriggerPin,
        int frontEchoPin, int rearTriggerPin, int rearEchoPin, uint8_t lcdI2cAddress)
        : Device(SAMPLING_INTERVAL_IN_MILLISECONDS, TIMER_GROUP_INDEX),
          gpsSensor(gpsSerialPort, gpsReceivePin, gpsTransmitPin, GPS_BAUD_RATE, this),
          frontSensor(frontTriggerPin, frontEchoPin, this),
          rearSensor(rearTriggerPin, rearEchoPin, this),
          statusDisplay(lcdI2cAddress, LCD_COLUMNS, LCD_ROWS, this),
          lastLocation(),
          frontDistanceInMeters(INITIAL_DISTANCE_IN_METERS),
          rearDistanceInMeters(INITIAL_DISTANCE_IN_METERS)
    {
        // IMPORTANT: Registration must occur before initializeAsynchronousEngine(), because
        // the engine start guard rejects any registration after the engine is running.
        appendSensorToScheduler(&gpsSensor,     Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        appendSensorToScheduler(&frontSensor,   Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        appendSensorToScheduler(&rearSensor,    Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);

        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH);
    }

    /**
     * @brief Inversion of Control interface contract handler.
     * Intercepts framework events dispatched from background tasks.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER) {
            if (event.sourceId == FRONT_TRIGGER_PIN) {
                frontDistanceInMeters = convertCentimetersToMeters(frontSensor.getDistanceInCentimeters());
            } else if (event.sourceId == REAR_TRIGGER_PIN) {
                rearDistanceInMeters = convertCentimetersToMeters(rearSensor.getDistanceInCentimeters());
            } else if (event.sourceId == GPS_RX_PIN) {
                lastLocation = gpsSensor.getLocationData();
            }
            updateStatusDisplay();
        }
    }
};

static DriverAssistantDevice* driverAssistantApplication = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Device::setFirmwareVersion(FIRMWARE_VERSION);
    driverAssistantApplication = new DriverAssistantDevice(
        Serial2,
        GPS_RX_PIN, GPS_TX_PIN,
        FRONT_TRIGGER_PIN, FRONT_ECHO_PIN,
        REAR_TRIGGER_PIN, REAR_ECHO_PIN,
        LCD_I2C_ADDRESS
    );
    Serial.println(APPLICATION_ACTIVE_MESSAGE);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
