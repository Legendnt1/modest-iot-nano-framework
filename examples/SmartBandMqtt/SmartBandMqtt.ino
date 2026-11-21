/**
 * @file SmartBandMqtt.ino
 * @brief Wearable fitness band that monitors heart rate and ambient climate, publishing via MQTT.
 *
 * Combines a PulseSensor for heart rate, a DHT22 for temperature/humidity, and a Button
 * for user interaction. All readings are packaged into a TelemetryPackage and published
 * over MQTT to a cloud broker. Demonstrates the full telemetry pipeline from sensor to cloud.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `Device` base class with async engine and MQTT gateway integration
 * - `PulseSensor` (analog), `DhtSensor` (digital), `Button` (interrupt-driven) sensors
 * - `TelemetryPackage` subclass with custom `serialize()` for JSON schema
 * - `MqttGatewayClient` for cloud telemetry publishing
 * - `WiFiConnectivityDriver` for WiFi link management
 * - Cross-task event routing via `on(Event)` handler
 * - `@runs_on{sampler}` threading context documentation
 * @date 2026-06-12
 * @version 1.0
 */

#include <Arduino.h>
#include <memory>
#include "ModestIoT.h"

/** @name Telemetry JSON Keys */
//@{
/** @brief JSON key for heart rate in beats per minute. */
#define JSON_KEY_HEART_BEATS_PER_MINUTE "heartBeatsPerMinute"
/** @brief JSON key for skin temperature in Celsius. */
#define JSON_KEY_SKIN_TEMPERATURE_IN_CELSIUS "skinTemperatureInCelsius"
/** @brief JSON key for relative humidity in percentage. */
#define JSON_KEY_RELATIVE_HUMIDITY_PERCENTAGE "relativeHumidityPercentage"
//@} // Telemetry JSON Keys

/** @name Device Engine Configuration */
//@{
/** @brief Sampler event queue length for the smart band. */
static const int ASYNC_ENGINE_QUEUE_DEPTH = 10;

/** @brief Debounce window applied to the user action button. */
static const unsigned long BUTTON_DEBOUNCE_DELAY_IN_MILLISECONDS = 50;
//@} // Device Engine Configuration

/** @name Serial Output Configuration */
//@{
/** @brief Message printed when the action button press is handled on the sampler task. */
static const char* BUTTON_PRESSED_MESSAGE = "[SmartBand UI] Action button click registered safely out of ISR!";

/** @brief Message printed once the application has booted. */
static const char* APPLICATION_ONLINE_MESSAGE = "[SmartBand Core] Thread execution loop online. Architecture finalized.";
//@} // Serial Output Configuration

/** @name Concrete Application Data Schema Specification */
//@{

class SmartBandTelemetryPackage : public TelemetryPackage {
    private:
    int heartBeatsPerMinute;
    float skinTemperatureInCelsius;
    float relativeHumidityInPercentage;

    public:
    SmartBandTelemetryPackage(int heartRate, float temperature, float humidity)
        : heartBeatsPerMinute(heartRate),
        skinTemperatureInCelsius(temperature),
        relativeHumidityInPercentage(humidity) {}

    ~SmartBandTelemetryPackage() override = default;

    void serialize(JsonDocument& serializationDestination) const override {
        serializationDestination[JSON_KEY_HEART_BEATS_PER_MINUTE]       = this->heartBeatsPerMinute;
        serializationDestination[JSON_KEY_SKIN_TEMPERATURE_IN_CELSIUS]   = this->skinTemperatureInCelsius;
        serializationDestination[JSON_KEY_RELATIVE_HUMIDITY_PERCENTAGE] = this->relativeHumidityInPercentage;
    }
};
//@} // Concrete Application Data Schema Specification

/** @name Application Mediator Implementation */
//@{

class SmartBandDevice : public Device {
    private:
    PulseSensor heartRateSensor;
    DhtSensor ambientClimateSensor;
    Button userActionButton; // Clean polled button integration
    MqttGatewayClient& gatewayClient;

    void packageAndQueueTelemetrySnapshot() {
        auto packet = std::make_unique<SmartBandTelemetryPackage>(
            heartRateSensor.getBeatsPerMinute(),
            ambientClimateSensor.getTemperatureInCelsius(),
            ambientClimateSensor.getRelativeHumidityInPercentage()
        );
        enqueueTelemetryPayload(std::move(packet));
    }

    protected:
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) const override {
        if (rawQueueItemPayload != nullptr) {
            gatewayClient.sendTelemetryRecord(*rawQueueItemPayload);
        }
    }

    void maintainNetworkServices() const override {
        gatewayClient.service();
    }

    public:
    SmartBandDevice(int pulsePin, int climatePin, int buttonPin, MqttGatewayClient& client,
    unsigned long samplingIntervalInMilliseconds, uint8_t timerGroupIndex = 0)
    : Device(samplingIntervalInMilliseconds, timerGroupIndex),
    heartRateSensor(pulsePin, nullptr),
    ambientClimateSensor(climatePin, DhtSensor::SENSOR_MODEL_DHT22, nullptr),
    userActionButton(buttonPin, BUTTON_DEBOUNCE_DELAY_IN_MILLISECONDS, nullptr),
    gatewayClient(client)
    {
        heartRateSensor.setHandler(this);
        ambientClimateSensor.setHandler(this);
        userActionButton.setHandler(this);

        // Sensors are registered for periodic polling; the button uses interrupt-driven mode
        appendSensorToScheduler(&heartRateSensor,       Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        appendSensorToScheduler(&ambientClimateSensor,  Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        appendSensorToTriggeringScheduler(&userActionButton,
                                          Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER,
                                          FALLING);

        // Start the engine only after all registrations: Device rejects registrations once it is running.
        initializeAsynchronousEngine(ASYNC_ENGINE_QUEUE_DEPTH);
    }

    ~SmartBandDevice() override = default;

    /**
     * @brief Handles sensor data-ready and button press events.
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER) {
            packageAndQueueTelemetrySnapshot();
        }
        else if (event.identifier == Button::BUTTON_PRESSED_EVENT_IDENTIFIER) {
            Serial.println(BUTTON_PRESSED_MESSAGE);
        }
    }

    /**
     * @brief Handles commands (unused in this example).
     * @param command The incoming command.
     * @runs_on{sampler}
     */
    void handle(Command command) override {}
};
//@} // Application Mediator Implementation

/** @name Top-Level Declarative Canvas */
//@{

static const int BIOLOGICAL_PULSE_ANALOG_PIN = 34;
static const int CLIMATE_ONE_WIRE_BUS_PIN    = 4;
static const int USER_INTERFACE_BUTTON_PIN   = 14;

static const char* WIRELESS_SSID             = "SmartBand_Secure_Mesh";
static const char* WIRELESS_PASSWORD         = "TelemetryNodePass2026";
static const char* MQTT_BROKER_IP            = "192.168.1.200";
static const uint16_t MQTT_BROKER_PORT       = 1883;
static const char* MQTT_PUBLISH_TOPIC        = "nodes/smartband/vitals";
static const char* MQTT_CLIENT_ID            = "SmartBand_Wrist_01";

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "SBM-606 FW 1.0.0";

static const unsigned long TELEMETRY_POLLING_INTERVAL_IN_MILLISECONDS = 1000;
static const uint8_t HARDWARE_TIMER_CHANNEL_INDEX = 0;

/** @brief Serial communication baud rate. */
static const unsigned long SERIAL_BAUD_RATE = 115200;

/** @brief Delay in the main loop when no work is pending. */
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 60000;

static WiFiConnectivityDriver* transportDriver = nullptr;
static MqttGatewayClient* gatewayClient        = nullptr;
static SmartBandDevice* smartBandApplication   = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);
    Device::setFirmwareVersion(FIRMWARE_VERSION);

    transportDriver = new WiFiConnectivityDriver(WIRELESS_SSID, WIRELESS_PASSWORD);

    gatewayClient = new MqttGatewayClient(
    *transportDriver,
    transportDriver->getNetworkSocket(),
        MQTT_BROKER_IP,
        MQTT_BROKER_PORT,
        MQTT_PUBLISH_TOPIC,
        MQTT_CLIENT_ID
    );

    smartBandApplication = new SmartBandDevice(
        BIOLOGICAL_PULSE_ANALOG_PIN,
        CLIMATE_ONE_WIRE_BUS_PIN,
        USER_INTERFACE_BUTTON_PIN,
        *gatewayClient,
        TELEMETRY_POLLING_INTERVAL_IN_MILLISECONDS,
        HARDWARE_TIMER_CHANNEL_INDEX
    );

    Serial.println(APPLICATION_ONLINE_MESSAGE);
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}