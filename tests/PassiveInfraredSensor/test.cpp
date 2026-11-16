#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the PassiveInfraredSensor component.
 *
 * Validates motion detection, motion stopped detection, and propagation suppression.
 */

#include "Arduino.h"
#include "Event.h"
#include "Sensor.h"
#include "PassiveInfraredSensor.h"
#include <cassert>
#include <cstdio>

namespace {
class TestablePassiveInfraredSensor : public PassiveInfraredSensor {
public:
    using PassiveInfraredSensor::PassiveInfraredSensor;
    using PassiveInfraredSensor::processEvent;
    using PassiveInfraredSensor::PROPAGATION_SUPPRESSION_IDENTIFIER;
};
} // namespace

int main() {
    std::printf("Running PassiveInfraredSensor tests...\n");

    // Test 1: Constructor initializes state correctly
    {
        TestablePassiveInfraredSensor sensor(4);
        assert(sensor.getSamplingIntervalInMilliseconds() == 100);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 20);
        std::printf("  [PASS] Constructor initializes state correctly\n");
    }

    // Test 2: Motion detected when pin goes HIGH
    {
        TestablePassiveInfraredSensor sensor(5);
        fakeDigitalReadValue = HIGH; // Simulate motion

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        assert(event.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER);
        assert(event.sourceId == 5);
        std::printf("  [PASS] Motion detected when pin goes HIGH\n");
    }

    // Test 3: Motion stopped when pin goes LOW
    {
        TestablePassiveInfraredSensor sensor(6);
        fakeDigitalReadValue = HIGH;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER);

        fakeDigitalReadValue = LOW; // Simulate motion stopped
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        assert(event.identifier == PassiveInfraredSensor::MOTION_STOPPED_EVENT_IDENTIFIER);
        assert(event.sourceId == 6);
        std::printf("  [PASS] Motion stopped when pin goes LOW\n");
    }

    // Test 4: Suppresses propagation when state hasn't changed
    {
        TestablePassiveInfraredSensor sensor(7);
        fakeDigitalReadValue = HIGH;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER);

        // Same state again - should suppress
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == sensor.PROPAGATION_SUPPRESSION_IDENTIFIER);
        std::printf("  [PASS] Suppresses propagation when state hasn't changed\n");
    }

    // Test 5: Multiple motion cycles
    {
        TestablePassiveInfraredSensor sensor(8);

        // Cycle 1: Motion detected
        fakeDigitalReadValue = HIGH;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER);

        // Cycle 1: Motion stopped
        fakeDigitalReadValue = LOW;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == PassiveInfraredSensor::MOTION_STOPPED_EVENT_IDENTIFIER);

        // Cycle 2: Motion detected again
        fakeDigitalReadValue = HIGH;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER);

        // Cycle 2: Motion stopped again
        fakeDigitalReadValue = LOW;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == PassiveInfraredSensor::MOTION_STOPPED_EVENT_IDENTIFIER);
        std::printf("  [PASS] Multiple motion cycles\n");
    }

    // Test 6: Static event instances
    {
        assert(PassiveInfraredSensor::MOTION_DETECTED_EVENT.identifier == PassiveInfraredSensor::MOTION_DETECTED_EVENT_IDENTIFIER);
        assert(PassiveInfraredSensor::MOTION_STOPPED_EVENT.identifier == PassiveInfraredSensor::MOTION_STOPPED_EVENT_IDENTIFIER);
        std::printf("  [PASS] Static event instances\n");
    }

    std::printf("All PassiveInfraredSensor tests passed successfully!\n");
    return 0;
}
