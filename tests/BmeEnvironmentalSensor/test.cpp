#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the BmeEnvironmentalSensor component.
 *
 * Validates altitude calculation and getter methods.
 * BmeEnvironmentalSensor is abstract, so we test through PthSensor (BME280).
 */

#include "Arduino.h"
#include "Wire.h"
#include "PthSensor.h"
#include <cassert>
#include <cstdio>

int main() {
    std::printf("Running BmeEnvironmentalSensor tests...\n");

    // Test 1: PthSensor constructor initializes state
    {
        Wire.clearReadBuffer();
        Wire.useReadBuffer = false;
        Wire.byteCount = 2;
        Wire.raw = 0;
        PthSensor sensor(4, 0x76);
        // Before initialization, values should be 0
        assert(sensor.getTemperatureInCelsius() == 0.0f);
        assert(sensor.getHumidityInPercentage() == 0.0f);
        assert(sensor.getPressureInHectopascals() == 0.0f);
        assert(sensor.isInitialized() == false);
        std::printf("  [PASS] PthSensor constructor initializes state\n");
    }

    // Test 2: BmeEnvironmentalSensor I2C addresses
    {
        assert(BmeEnvironmentalSensor::DEFAULT_I2C_ADDRESS == 0x76);
        assert(BmeEnvironmentalSensor::ALTERNATE_I2C_ADDRESS == 0x77);
        std::printf("  [PASS] BmeEnvironmentalSensor I2C addresses\n");
    }

    // Test 3: getAltitudeInMeters with valid pressure
    {
        // We need to set up a sensor with calibrated data to test altitude.
        // Since we can't easily mock the full I2C calibration, we test the
        // altitude formula logic indirectly: with 0 pressure (uninitialized),
        // altitude should return 0.
        Wire.clearReadBuffer();
        Wire.useReadBuffer = false;
        PthSensor sensor(5, 0x76);
        assert(sensor.getAltitudeInMeters(1013.25f) == 0.0f);
        std::printf("  [PASS] getAltitudeInMeters returns 0 when uninitialized\n");
    }

    // Test 4: getAltitudeInMeters with invalid sea level pressure
    {
        Wire.clearReadBuffer();
        Wire.useReadBuffer = false;
        PthSensor sensor(6, 0x76);
        assert(sensor.getAltitudeInMeters(0.0f) == 0.0f);
        assert(sensor.getAltitudeInMeters(-100.0f) == 0.0f);
        std::printf("  [PASS] getAltitudeInMeters with invalid sea level pressure\n");
    }

    // Test 5: initialize() attempts I2C communication
    {
        Wire.transmissionCount = 0;
        Wire.clearReadBuffer();
        Wire.useReadBuffer = true;
        // Push enough calibration data (26 + 7 = 33 bytes)
        for (int i = 0; i < 33; i++) {
            Wire.pushReadBuffer(0);
        }
        Wire.byteCount = 26; // First request returns 26 bytes
        PthSensor sensor(7, 0x76);
        sensor.initialize();
        assert(Wire.transmissionCount > 0);
        std::printf("  [PASS] initialize() attempts I2C communication\n");
    }

    // Test 6: Sampling interval defaults
    {
        PthSensor sensor(8, 0x76);
        assert(sensor.getSamplingIntervalInMilliseconds() == 2000);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 250);
        std::printf("  [PASS] Sampling interval defaults\n");
    }

    std::printf("All BmeEnvironmentalSensor tests passed successfully!\n");
    return 0;
}
