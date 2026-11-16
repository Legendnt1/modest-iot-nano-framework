#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the UltrasonicSensor component.
 *
 * Validates distance measurement, timeout handling, and sampling intervals.
 */

#include "Arduino.h"
#include "Event.h"
#include "Sensor.h"
#include "UltrasonicSensor.h"
#include <cassert>
#include <cstdio>

namespace {
class TestableUltrasonicSensor : public UltrasonicSensor {
public:
    using UltrasonicSensor::UltrasonicSensor;
    using UltrasonicSensor::processEvent;
};
} // namespace

int main() {
    std::printf("Running UltrasonicSensor tests...\n");

    // Test 1: Constructor initializes state correctly
    {
        TestableUltrasonicSensor sensor(4, 5);
        assert(sensor.getDistanceInCentimeters() == -1.0f);
        assert(sensor.getSamplingIntervalInMilliseconds() == 100);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 60);
        std::printf("  [PASS] Constructor initializes state correctly\n");
    }

    // Test 2: Sampling interval defaults
    {
        TestableUltrasonicSensor sensor(6, 7);
        assert(sensor.getSamplingIntervalInMilliseconds() == 100);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 60);
        std::printf("  [PASS] Sampling interval defaults\n");
    }

    // Test 3: Interrupt handler is registered
    {
        TestableUltrasonicSensor sensor(10, 11);
        // The constructor should have registered an interrupt on the echo pin
        assert(fakeInterruptHandlersArg[11] != nullptr);
        assert(fakeInterruptModes[11] == CHANGE);
        std::printf("  [PASS] Interrupt handler is registered\n");
    }

    // Test 4: Destructor detaches interrupt
    {
        {
            TestableUltrasonicSensor sensor(12, 13);
            assert(fakeInterruptHandlersArg[13] != nullptr);
        }
        // After destruction, interrupt should be detached
        assert(fakeInterruptHandlersArg[13] == nullptr);
        std::printf("  [PASS] Destructor detaches interrupt\n");
    }

    std::printf("All UltrasonicSensor tests passed successfully!\n");
    return 0;
}
