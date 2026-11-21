#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H

/**
 * @file DhtSensor.h
 * @brief Declares a DHT temperature and humidity sensor with native single-wire protocol.
 * @ingroup sensors_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-06-06
 * @version 1.0
 */

#include "Sensor.h"

/**
 * @brief Sensor adapter for DHT-family devices (temperature and humidity).
 *
 * Implements the DHT single-wire protocol natively using digitalRead/digitalWrite
 * with microsecond-level timing. Supports DHT11, DHT22, DHT21, and AM2301 models.
 *
 * @ingroup sensors_module
 */
class DhtSensor : public Sensor {
public:
    /// DHT11 model type identifier.
    static constexpr uint8_t SENSOR_MODEL_DHT11 = 11;
    /// DHT22 model type identifier.
    static constexpr uint8_t SENSOR_MODEL_DHT22 = 22;
    /// DHT21 model type identifier.
    static constexpr uint8_t SENSOR_MODEL_DHT21 = 21;
    /// AM2301 model type identifier, equivalent to DHT21.
    static constexpr uint8_t SENSOR_MODEL_AM2301 = 21;

private:
    uint8_t dhtModelType; ///< DHT model constant (DHT11, DHT22, DHT21, AM2301).
    float cachedTemperatureInCelsius; ///< Last successful temperature reading.
    float cachedRelativeHumidityInPercentage; ///< Last successful humidity reading.

    static constexpr int SENSOR_ERROR_SUPPRESSION_IDENTIFIER = -1; ///< Identifier used to suppress error events.

    /// Minimum sampling interval for DHT11 model in milliseconds.
    /// @note DHT11 specification limits readings to 1 Hz (1000 ms).
    static constexpr uint32_t DHT11_MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS = 1000;
    /// Minimum sampling interval for DHT22/DHT21 model in milliseconds.
    /// @note DHT22 specification limits readings to 0.5 Hz (2000 ms).
    static constexpr uint32_t DHT22_MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS = 2000;
    /// Default sampling interval in milliseconds (conservative, covers all models).
    static constexpr uint32_t DEFAULT_SAMPLING_INTERVAL_IN_MILLISECONDS = 2000;

    // DHT single-wire protocol timing constants (microseconds)
    static constexpr uint32_t DHT22_START_SIGNAL_LOW_TIME_IN_MICROSECONDS = 1100; ///< DHT22 start LOW (spec: at least 1 ms).
    static constexpr uint32_t DHT11_START_SIGNAL_LOW_TIME_IN_MICROSECONDS = 18000; ///< DHT11 start signal LOW duration.
    static constexpr uint32_t DHT_LINE_RELEASE_TIME_IN_MICROSECONDS = 55; ///< Wait after releasing the line (sensor answers in 20-40 µs).
    static constexpr uint32_t DHT_PULSE_TIMEOUT_IN_MICROSECONDS = 1000; ///< Maximum duration of any protocol pulse.
    static constexpr uint32_t DHT_PULSE_TIMEOUT_SENTINEL = UINT32_MAX; ///< measurePulse() result when the pulse timed out.

    // DHT22 value encoding constants
    static constexpr uint8_t DHT_BITS_PER_BYTE = 8; ///< Bits per data byte.
    static constexpr uint8_t DHT22_TEMPERATURE_SIGN_BIT = 0x80; ///< Sign flag in the DHT22 temperature high byte.
    static constexpr uint8_t DHT22_TEMPERATURE_MAGNITUDE_MASK = 0x7F; ///< Magnitude bits of the DHT22 temperature high byte.
    static constexpr float DHT22_DECIMAL_SCALE = 10.0f; ///< DHT22/DHT21 values are transmitted × 10.

    // DHT data format constants
    static constexpr uint8_t DHT_DATA_BYTE_COUNT = 5; ///< Total bytes in DHT response (humidity×2 + temp×2 + checksum).
    static constexpr uint8_t DHT_DATA_BIT_COUNT = 40; ///< Total bits in DHT response (5 bytes × 8 bits).
    static constexpr uint8_t DHT_HUMIDITY_HIGH_BYTE_INDEX = 0; ///< Index of humidity high byte.
    static constexpr uint8_t DHT_HUMIDITY_LOW_BYTE_INDEX = 1; ///< Index of humidity low byte.
    static constexpr uint8_t DHT_TEMPERATURE_HIGH_BYTE_INDEX = 2; ///< Index of temperature high byte.
    static constexpr uint8_t DHT_TEMPERATURE_LOW_BYTE_INDEX = 3; ///< Index of temperature low byte.
    static constexpr uint8_t DHT_CHECKSUM_BYTE_INDEX = 4; ///< Index of checksum byte.

    /**
     * @brief Pulls the data line LOW for the model-specific start time.
     * @runs_on{sampler}
     */
    void sendStartSignal() const;

    /**
     * @brief Measures how long the data line stays at a given level.
     * @param level Logic level to measure (LOW or HIGH).
     * @return Pulse duration in microseconds, or DHT_PULSE_TIMEOUT_SENTINEL on timeout.
     * @runs_on{sampler}
     */
    uint32_t measurePulse(int level) const;

    /**
     * @brief Releases the line, waits for the sensor response and reads the 40-bit frame.
     *
     * Each bit is decoded by comparing its HIGH pulse against its preceding LOW pulse
     * (~50 µs): a longer HIGH (~70 µs) is a 1 and a shorter one (~26 µs) is a 0. Comparing
     * the two pulses keeps the decoding independent of the absolute timing overhead.
     * Must run with interrupts disabled so the pulse measurements are not stretched.
     *
     * @param data Buffer of 5 bytes to store the raw DHT data.
     * @return true if the response and all 40 bits were received, false on timeout.
     * @runs_on{sampler}
     */
    bool readData(uint8_t* data) const;

protected:
    /**
     * @brief Performs a DHT read cycle and updates cached measurements.
     * @param event Mutable event request/response payload.
     */
    void processEvent(Event& event) override;

    [[nodiscard]] uint32_t getDefaultSamplingIntervalInMilliseconds() const override;
    [[nodiscard]] uint32_t getMinimalSamplingIntervalInMilliseconds() const override;

public:
    /**
     * @brief Constructs a DHT sensor wrapper.
     * @param signalGpioPin Data pin connected to the DHT module.
     * @param dhtModelType DHT model constant (for example SENSOR_MODEL_DHT11, SENSOR_MODEL_DHT22).
     * @param parentHandler Optional upstream handler for propagated events.
     */
    DhtSensor(int signalGpioPin, uint8_t dhtModelType, EventHandler* parentHandler = nullptr);

    /**
     * @brief Returns the most recent successful temperature reading.
     * @return Temperature in degrees Celsius.
     */
    [[nodiscard]] float getTemperatureInCelsius() const;

    /**
     * @brief Returns the most recent successful relative humidity reading.
     * @return Relative humidity percentage (0-100).
     */
    [[nodiscard]] float getRelativeHumidityInPercentage() const;

    /**
     * @brief Validates the DHT checksum (sum of first 4 bytes equals the 5th byte).
     * @param data 5-byte buffer containing DHT response data.
     * @return true if checksum is valid, false otherwise.
     */
    static bool validateChecksum(const uint8_t* data);

    /**
     * @brief Returns the minimum sampling interval for the given DHT model.
     * @param dhtModelType DHT model constant (e.g., SENSOR_MODEL_DHT11, SENSOR_MODEL_DHT22).
     * @return Minimum sampling interval in milliseconds.
     * @note DHT22 and DHT21 return 2000 ms; DHT11 returns 1000 ms; unknown models return 2000 ms.
     */
    [[nodiscard]] static uint32_t minimumSamplingIntervalFor(uint8_t dhtModelType);
};

#endif // DHT_SENSOR_H
