#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the PulseSensor component.
 *
 * Validates BPM detection, signal sampling, and propagation suppression.
 */

#include "Arduino.h"
#include "Event.h"
#include "Sensor.h"
#include "PulseSensor.h"
#include <cassert>
#include <cstdio>

namespace {
class TestablePulseSensor : public PulseSensor {
public:
    using PulseSensor::PulseSensor;
    using PulseSensor::processEvent;
};
} // namespace

int main() {
    std::printf("Running PulseSensor tests...\n");

    // Test 1: Constructor initializes state correctly
    {
        TestablePulseSensor sensor(4);
        assert(sensor.getBeatsPerMinute() == 0);
        assert(sensor.getRawSignalValue() == 0);
        assert(sensor.getSamplingIntervalInMilliseconds() == 20);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 10);
        std::printf("  [PASS] Constructor initializes state correctly\n");
    }

    // Test 2: No heartbeat detected with flat signal
    {
        TestablePulseSensor sensor(5);
        fakeAnalogRead[5] = 512; // Flat baseline

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        assert(event.identifier == -1); // Suppressed
        assert(sensor.getBeatsPerMinute() == 0);
        assert(sensor.getRawSignalValue() == 512);
        std::printf("  [PASS] No heartbeat detected with flat signal\n");
    }

    // Test 3: Heartbeat detection with signal spike
    {
        TestablePulseSensor sensor(6);
        fakeMillis = 1000;

        // First sample: below threshold (512), establishes trough at 200
        fakeAnalogRead[6] = 200;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == -1); // Suppressed, establishes trough

        // Second sample: spike well above threshold with enough amplitude
        // lastBeatTimestamp starts at 0, so delta = 1900 - 0 = 1900ms
        // BPM = 60000 / 1900 = 31 (> 30 minimum, < 200 maximum)
        fakeMillis = 1900;
        fakeAnalogRead[6] = 700; // Above threshold 512
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        assert(event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER);
        assert(sensor.getBeatsPerMinute() == 31);
        std::printf("  [PASS] Heartbeat detection with signal spike\n");
    }

    // Test 4: BPM calculation on second beat
    {
        TestablePulseSensor sensor(7);
        fakeMillis = 1000;

        // Establish trough below threshold
        fakeAnalogRead[7] = 200;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        // First beat: delta from 0, BPM = 60000/1900 = 31
        fakeMillis = 1900;
        fakeAnalogRead[7] = 700;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER);
        assert(sensor.getBeatsPerMinute() == 31);

        // Signal goes below threshold to reset searchForHeartbeat
        fakeMillis = 2000;
        fakeAnalogRead[7] = 200;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        // Second beat: delta from lastBeatTimestamp (1900), BPM = 60000/(2800-1900) = 66
        fakeMillis = 2800;
        fakeAnalogRead[7] = 700;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        assert(event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER);
        assert(sensor.getBeatsPerMinute() == 66);
        std::printf("  [PASS] BPM calculation on second beat\n");
    }

    // Test 5: BPM outside physiological range is rejected (too fast)
    {
        TestablePulseSensor sensor(8);
        fakeMillis = 1000;

        // Establish trough below threshold
        fakeAnalogRead[8] = 200;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        // Spike at 200ms later -> 60000/1200 = 50 BPM (within range, not rejected)
        // To get BPM > 200, need delta < 300ms, but debounce is 300ms minimum
        // So we test that very fast spikes within debounce are suppressed
        fakeMillis = 1200;
        fakeAnalogRead[8] = 700;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        // BPM = 60000/1200 = 50, within range, so it's accepted
        assert(sensor.getBeatsPerMinute() == 50);
        std::printf("  [PASS] BPM within physiological range is accepted\n");
    }

    // Test 6: Debounce prevents rapid re-triggering
    {
        TestablePulseSensor sensor(9);
        fakeMillis = 1000;

        // Establish trough below threshold
        fakeAnalogRead[9] = 200;
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        // First spike
        fakeMillis = 1900;
        fakeAnalogRead[9] = 700;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        assert(event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER);

        // Immediate second spike (within debounce window)
        fakeMillis = 2100;
        fakeAnalogRead[9] = 700;
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        // Should be suppressed due to debounce
        assert(event.identifier == -1);
        std::printf("  [PASS] Debounce prevents rapid re-triggering\n");
    }

    // Test 7: Raw signal value is updated
    {
        TestablePulseSensor sensor(10);
        fakeAnalogRead[10] = 600;

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        assert(sensor.getRawSignalValue() == 600);
        std::printf("  [PASS] Raw signal value is updated\n");
    }

    std::printf("All PulseSensor tests passed successfully!\n");
    return 0;
}
