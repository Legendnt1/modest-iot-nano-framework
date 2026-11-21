/**
 * @file EquipmentHealthMonitor.ino
 * @brief Industrial equipment health dashboard with structured logging and diagnostics.
 *
 * This example demonstrates a grounded real-world device: a monitoring unit attached to
 * industrial equipment (motor, pump, compressor) that tracks vibration (via LoadCellAmplifier
 * as a force proxy) and ambient temperature, reporting diagnostics to help predict
 * maintenance needs.
 *
 * The primary purpose of this example is to demonstrate the ModestLog zero-overhead
 * compile-time logging system and the Device::collectDiagnostics() runtime health monitoring
 * API, which are essential for production deployments.
 *
 * @par Modest IoT Nano Framework aspects illustrated:
 * - `ModestLog` zero-overhead compile-time logging with `MIOT_LOG_*` macros
 * - Log level filtering: `MIOT_LOG_I` (Info), `MIOT_LOG_W` (Warn), `MIOT_LOG_E` (Error)
 * - `Device::collectDiagnostics()` for runtime health snapshot (`DiagnosticsSnapshot`)
 * - `EngineConfiguration` for custom task stack sizes, priorities, and core pinning
 * - `LoadCellAmplifier` for force/vibration measurement (proxy for accelerometer)
 * - `DhtSensor` for ambient temperature monitoring
 * - Periodic diagnostics reporting to Serial
 * - Loop-less, event-driven asynchronous execution via `initializeAsynchronousEngine()`
 *
 * @par Diagnostics metrics reported:
 * - Missed ticks: sampler handler overrun count
 * - Packets enqueued/dropped: telemetry queue health
 * - Queue high water mark: maximum queue depth observed
 * - Free heap / min free heap: memory pressure
 * - Stack high water marks: task stack usage
 * - Uptime seconds: device runtime
 *
 * @par Configuring log level:
 * - Define `MODEST_LOG_LEVEL` before including ModestIoT.h:
 *   `#define MODEST_LOG_LEVEL MODEST_LOG_LEVEL_INFO` (for Info and above)
 *   `#define MODEST_LOG_LEVEL MODEST_LOG_LEVEL_DEBUG` (for Debug and above)
 * - Default level is `MODEST_LOG_LEVEL_WARN` (Warn and above)
 *
 * @date 2026-11-12
 * @version 1.0
 */

#include <Arduino.h>
#include <cstdint>
#include "ModestIoT.h"

/** @brief Log tag for this device component. */
static const char* EQUIP_HEALTH_LOG_TAG = "EquipHealth";

/** @brief Consecutive failed DHT22 readings before the sensor reports a fault. */
static const unsigned int SENSOR_FAULT_THRESHOLD = 3;

/**
 * @name Structured Log Formats
 *
 * Defined as macros on purpose: on ESP32, `MIOT_LOG_*` expands to
 * `log_x("[%s] " fmt, ...)`, which concatenates string literals at compile time.
 * A `const char*` variable would not compile there.
 */
//@{
#define DIAGNOSTICS_HEADER_LOG_FORMAT "=== Diagnostics Report (sample #%lu) ==="
#define UPTIME_LOG_FORMAT "Uptime: %lu seconds"
#define MISSED_TICKS_LOG_FORMAT "Missed ticks: %lu"
#define PACKETS_LOG_FORMAT "Packets enqueued: %lu, dropped: %lu"
#define QUEUE_HIGH_WATER_MARK_LOG_FORMAT "Queue high water mark: %u"
#define FREE_HEAP_LOG_FORMAT "Free heap: %lu bytes (min: %lu)"
#define SAMPLER_STACK_LOG_FORMAT "Sampler stack free: %u words"
#define NETWORK_STACK_LOG_FORMAT "Network stack free: %u words"
#define DIAGNOSTICS_FOOTER_LOG_FORMAT "=== End Diagnostics ==="
#define ENGINE_CONFIGURED_LOG_FORMAT "Engine configured: sampler stack %lu bytes, queue length %u"
#define SAMPLE_LOG_FORMAT "Sample #%lu -> Vibration: %.2f g, Temp: %.1f°C"
#define HIGH_VIBRATION_LOG_FORMAT "High vibration detected: %.2f g (sample #%lu)"
#define STARTING_LOG_FORMAT "Equipment health monitor starting..."
#define RECALIBRATION_LOG_FORMAT "Load cell amplifier may need recalibration on pin %d"
#define DHT_FAULT_LOG_FORMAT "DHT22 not responding on pin %d: check wiring and pull-up"
#define DHT_RECOVERED_LOG_FORMAT "DHT22 recovered on pin %d"
#define MONITORING_ACTIVE_LOG_FORMAT "Monitoring active. Diagnostics every %u samples."
//@} // Structured Log Formats

/** @name Engine Configuration */
//@{
/** @brief Sampler task stack size (larger for sensor processing). */
static const uint32_t SAMPLER_STACK_SIZE_IN_BYTES = 8192;
/** @brief Network task stack size (smaller: this device has no network). */
static const uint32_t NETWORK_STACK_SIZE_IN_BYTES = 4096;
/** @brief FreeRTOS priority of the sampler task. */
static const uint8_t SAMPLER_TASK_PRIORITY = 3;
//@} // Engine Configuration

/** @brief Weight below which the load cell is considered miscalibrated after tare (grams). */
static const float MINIMUM_VALID_WEIGHT_IN_GRAMS = 0.0f;

/** @brief Firmware version reported in the telemetry envelope. */
static const char* FIRMWARE_VERSION = "EHM-600 FW 1.0.0";

/** @brief Number of samples between diagnostics reports. */
static const uint32_t DIAGNOSTICS_REPORT_INTERVAL_SAMPLES = 10;

/** @brief Sampler event queue length for equipment health monitoring. */
static const int SAMPLER_QUEUE_LENGTH = 16;

/** @brief Serial communication baud rate. */
static const unsigned long SERIAL_BAUD_RATE = 115200;

/** @brief Delay in the main loop when no work is pending. */
static const unsigned long MAIN_LOOP_DELAY_IN_MILLISECONDS = 60000;

/** @name Serial Output Configuration */
//@{
/** @brief Log prefix for all EquipmentHealthMonitor serial output. */
static const char* EQUIPMENT_HEALTH_LOG_PREFIX = "[EquipmentHealthMonitor]";
/** @brief Message printed once the monitor has started. */
static const char* APPLICATION_ONLINE_MESSAGE = " Equipment health monitor online.";
/** @brief Message printed when the device cannot start. */
static const char* STARTUP_FAILED_MESSAGE = " Device startup failed.";
//@} // Serial Output Configuration

/** @name Application Mediator Implementation */
//@{

/** @brief Minimum vibration weight for load cell calibration (grams). */
static const float VIBRATION_MIN_WEIGHT_IN_GRAMS = 0.0f;

/** @brief Maximum vibration weight for load cell calibration (grams). */
static const float VIBRATION_MAX_WEIGHT_IN_GRAMS = 1000.0f;

/** @brief Minimum raw ADC value for load cell calibration. */
static const long VIBRATION_MIN_RAW_VALUE = 0;

/** @brief Maximum raw ADC value for load cell calibration. */
static const long VIBRATION_MAX_RAW_VALUE = 1000000;

/** @brief Moving average filter depth for the load cell. */
static const int VIBRATION_FILTER_DEPTH = 5;

/** @brief Vibration warning threshold in grams. */
static const float VIBRATION_WARNING_THRESHOLD_IN_GRAMS = 5.0f;

/**
 * @brief Industrial equipment health monitoring device.
 *
 * This class extends the `Device` base class to manage vibration and temperature sensors,
 * using ModestLog for structured logging and Device::collectDiagnostics() for runtime health
 * monitoring. Custom EngineConfiguration is used to tune task parameters for industrial
 * monitoring workloads.
 */
class EquipmentHealthMonitor : public Device {
private:
    LoadCellAmplifier vibrationSensor;
    DhtSensor tempSensor;

    uint32_t sampleCount; ///< Running count of samples for periodic diagnostics.

    /**
     * @brief Reports the current diagnostics snapshot to Serial.
     */
    void reportDiagnostics() {
        DiagnosticsSnapshot diag = collectDiagnostics();

        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, DIAGNOSTICS_HEADER_LOG_FORMAT, sampleCount);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, UPTIME_LOG_FORMAT, diag.uptimeSeconds);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, MISSED_TICKS_LOG_FORMAT, diag.missedTicks);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, PACKETS_LOG_FORMAT,
                   diag.packetsEnqueued, diag.packetsDropped);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, QUEUE_HIGH_WATER_MARK_LOG_FORMAT, diag.queueHighWaterMark);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, FREE_HEAP_LOG_FORMAT,
                   diag.freeHeapBytes, diag.minFreeHeapBytes);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, SAMPLER_STACK_LOG_FORMAT,
                   diag.samplerStackFreeWords);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, NETWORK_STACK_LOG_FORMAT,
                   diag.networkStackFreeWords);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, DIAGNOSTICS_FOOTER_LOG_FORMAT);
    }

protected:
    void processQueuedTelemetryData(const TelemetryPackage* rawQueueItemPayload) const override {
        // This device does not send telemetry; it only monitors locally.
        (void)rawQueueItemPayload;
    }

    void maintainNetworkServices() const override {
        // No network services for this standalone monitoring device.
    }

public:
    /**
     * @brief Constructs a new EquipmentHealthMonitor application instance.
     *
     * Uses custom EngineConfiguration with increased stack sizes for industrial monitoring.
     *
     * @param vibrationDataPin GPIO pin connected to the HX711 Data (DOUT) line.
     * @param vibrationClockPin GPIO pin connected to the HX711 Clock (PD_SCK) line.
     * @param tempPin GPIO pin connected to the DHT22 data line.
     * @param samplingIntervalInMilliseconds How often to sample sensors (ms).
     * @param timerGroupIndex Hardware timer group index (0-3).
     */
    EquipmentHealthMonitor(int vibrationDataPin, int vibrationClockPin, int tempPin,
                           unsigned long samplingIntervalInMilliseconds,
                           uint8_t timerGroupIndex = 0)
        : Device(samplingIntervalInMilliseconds, timerGroupIndex),
          vibrationSensor(vibrationDataPin, vibrationClockPin, VIBRATION_MIN_WEIGHT_IN_GRAMS,
                          VIBRATION_MAX_WEIGHT_IN_GRAMS, VIBRATION_MIN_RAW_VALUE,
                          VIBRATION_MAX_RAW_VALUE, VIBRATION_FILTER_DEPTH, this),
          tempSensor(tempPin, DhtSensor::SENSOR_MODEL_DHT22, this),
          sampleCount(0)
    {
        // Custom engine configuration for industrial monitoring workload
        EngineConfiguration config;
        config.samplerStackBytes = SAMPLER_STACK_SIZE_IN_BYTES;
        config.networkStackBytes = NETWORK_STACK_SIZE_IN_BYTES;
        config.samplerPriority = SAMPLER_TASK_PRIORITY;
        config.queueLength = SAMPLER_QUEUE_LENGTH;

        // Hybrid scheduling: interrupt-driven data-ready detection eliminates the 100 ms busy-wait.
        // IMPORTANT: Registration must occur before initializeAsynchronousEngine(), because
        // the engine start guard rejects any registration after the engine is running.
        appendToBothSchedulers(&vibrationSensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER, FALLING);
        // Report a fault after consecutive failed readings instead of failing silently
        tempSensor.setFaultThreshold(SENSOR_FAULT_THRESHOLD);
        appendSensorToScheduler(&tempSensor, Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);

        initializeAsynchronousEngine(config);

        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, ENGINE_CONFIGURED_LOG_FORMAT,
                   config.samplerStackBytes, config.queueLength);
    }

    ~EquipmentHealthMonitor() override {
        stopAsynchronousEngine();
    }

    /**
     * @brief Handles sensor data events and reports readings with structured logging.
     *
     * @param event The triggered event context payload.
     * @runs_on{sampler}
     */
    void on(Event event) override {
        // DHT22 health: report faults instead of failing silently
        if (event.identifier == Sensor::SENSOR_FAULT_EVENT_IDENTIFIER && event.sourceId == tempSensor.getPin()) {
            MIOT_LOG_E(EQUIP_HEALTH_LOG_TAG, DHT_FAULT_LOG_FORMAT, event.sourceId);
            return;
        }
        if (event.identifier == Sensor::SENSOR_RECOVERED_EVENT_IDENTIFIER && event.sourceId == tempSensor.getPin()) {
            MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, DHT_RECOVERED_LOG_FORMAT, event.sourceId);
            event.identifier = Sensor::DATA_READ_EVENT_IDENTIFIER; // The recovery carries a fresh reading
        }

        if (event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER) {
            sampleCount++;

            float vibration = vibrationSensor.getWeightInGrams();
            float temperature = tempSensor.getTemperatureInCelsius();

            MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, SAMPLE_LOG_FORMAT,
                       sampleCount, vibration, temperature);

            // Check for abnormal vibration levels
            if (vibration > VIBRATION_WARNING_THRESHOLD_IN_GRAMS) {
                MIOT_LOG_W(EQUIP_HEALTH_LOG_TAG, HIGH_VIBRATION_LOG_FORMAT,
                           vibration, sampleCount);
            }

            // Periodically report full diagnostics
            if (sampleCount % DIAGNOSTICS_REPORT_INTERVAL_SAMPLES == 0) {
                reportDiagnostics();
            }
        }
    }

    void handle(Command command) override {
        // This device does not process external commands.
        (void)command;
    }

    /**
     * @brief Initializes the device and logs startup information.
     * @return True if sensors are responding.
     */
    bool start() {
        Device::setFirmwareVersion(FIRMWARE_VERSION);
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, STARTING_LOG_FORMAT);

        vibrationSensor.tare();
        if (vibrationSensor.getWeightInGrams() < MINIMUM_VALID_WEIGHT_IN_GRAMS) {
            MIOT_LOG_W(EQUIP_HEALTH_LOG_TAG, RECALIBRATION_LOG_FORMAT,
                       vibrationSensor.getSignalGpioPin());
        }

        Serial.println((String(EQUIPMENT_HEALTH_LOG_PREFIX) + APPLICATION_ONLINE_MESSAGE).c_str());
        MIOT_LOG_I(EQUIP_HEALTH_LOG_TAG, MONITORING_ACTIVE_LOG_FORMAT,
                   DIAGNOSTICS_REPORT_INTERVAL_SAMPLES);
        return true;
    }
};
//@} // Application Mediator Implementation

/** @name Top-Level Declarative Canvas */
//@{

static const int VIBRATION_SENSOR_DATA_PIN   = 34; ///< GPIO pin for HX711 Data (DOUT).
static const int VIBRATION_SENSOR_CLOCK_PIN  = 32; ///< GPIO pin for HX711 Clock (PD_SCK).
static const int TEMPERATURE_SENSOR_PIN      = 4;  ///< GPIO pin for DHT22 data line.

static const unsigned long SAMPLING_INTERVAL_IN_MILLISECONDS = 2000;
static const uint8_t HARDWARE_TIMER_CHANNEL_INDEX = 0;

static EquipmentHealthMonitor* equipmentHealthApplication = nullptr;

void setup() {
    Serial.begin(SERIAL_BAUD_RATE);

    equipmentHealthApplication = new EquipmentHealthMonitor(
        VIBRATION_SENSOR_DATA_PIN,
        VIBRATION_SENSOR_CLOCK_PIN,
        TEMPERATURE_SENSOR_PIN,
        SAMPLING_INTERVAL_IN_MILLISECONDS,
        HARDWARE_TIMER_CHANNEL_INDEX
    );

    if (!equipmentHealthApplication->start()) {
        Serial.println((String(EQUIPMENT_HEALTH_LOG_PREFIX) + STARTUP_FAILED_MESSAGE).c_str());
        return;
    }
}

void loop() {
    // The master loop is completely clean and empty.
    // The framework handles all timing, sensor sampling, and diagnostics reporting.
    vTaskDelay(pdMS_TO_TICKS(MAIN_LOOP_DELAY_IN_MILLISECONDS));
}
//@} // Top-Level Declarative Canvas
