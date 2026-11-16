#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the GeolocationSensor component.
 *
 * Validates NMEA parsing, coordinate conversion, and sampling intervals.
 */

#include "Arduino.h"
#include "Event.h"
#include "Sensor.h"
#include "GeolocationSensor.h"
#include <cassert>
#include <cstdio>
#include <cstring>

namespace {
class TestableGeolocationSensor : public GeolocationSensor {
public:
    using GeolocationSensor::GeolocationSensor;
    using GeolocationSensor::processEvent;
};
} // namespace

int main() {
    std::printf("Running GeolocationSensor tests...\n");

    // Test 1: Constructor initializes state correctly
    {
        HardwareSerial serial;
        TestableGeolocationSensor sensor(serial, 4, 5, 9600);
        const auto& loc = sensor.getLocationData();
        assert(loc.latitudeInDegrees == 0.0f);
        assert(loc.longitudeInDegrees == 0.0f);
        assert(loc.altitudeInMeters == 0.0f);
        assert(sensor.getSamplingIntervalInMilliseconds() == 1000);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 100);
        std::printf("  [PASS] Constructor initializes state correctly\n");
    }

    // Test 2: Sampling interval defaults
    {
        HardwareSerial serial;
        TestableGeolocationSensor sensor(serial, 6, 7, 9600);
        assert(sensor.getSamplingIntervalInMilliseconds() == 1000);
        assert(sensor.getEffectiveMinimalSamplingIntervalInMilliseconds() == 100);
        std::printf("  [PASS] Sampling interval defaults\n");
    }

    // Test 3: processEvent with no serial data
    {
        HardwareSerial serial;
        TestableGeolocationSensor sensor(serial, 8, 9, 9600);
        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        // No data available; event identifier unchanged
        assert(event.identifier == Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        std::printf("  [PASS] processEvent with no serial data\n");
    }

    // Test 4: Coordinate conversion via GGA parsing
    {
        HardwareSerial serial;
        // 4042.6570,N,07400.4170,W -> lat ~40.71, lon ~-74.01
        serial.feed("$GPGGA,123519,4042.6570,N,07400.4170,W,1,08,0.9,50.0,M,0.0,M,,*46\n");
        TestableGeolocationSensor sensor(serial, 10, 11, 9600);

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        const auto& loc = sensor.getLocationData();
        // Latitude: 40 + 42.6570/60 = 40.71095
        assert(loc.latitudeInDegrees > 40.7f && loc.latitudeInDegrees < 40.8f);
        // Longitude: -(74 + 0.4170/60) = -74.00695
        assert(loc.longitudeInDegrees < -74.0f && loc.longitudeInDegrees > -74.1f);
        std::printf("  [PASS] Coordinate conversion via GGA parsing\n");
    }

    // Test 5: NMEA GGA sentence parsing
    {
        HardwareSerial serial;
        // $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47
        serial.feed("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47\n");
        TestableGeolocationSensor sensor(serial, 12, 13, 9600);

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        const auto& loc = sensor.getLocationData();
        assert(loc.fixQuality == 1);
        assert(loc.satellitesTracked == 8);
        assert(loc.altitudeInMeters == 545.4f);
        // Latitude: 48 + 7.038/60 = 48.1173
        assert(loc.latitudeInDegrees > 48.1f && loc.latitudeInDegrees < 48.2f);
        // Longitude: 11 + 31.000/60 = 11.5167
        assert(loc.longitudeInDegrees > 11.5f && loc.longitudeInDegrees < 11.6f);
        std::printf("  [PASS] NMEA GGA sentence parsing\n");
    }

    // Test 6: NMEA RMC sentence parsing
    {
        HardwareSerial serial;
        // $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.0,230394,,,*10
        serial.feed("$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.0,230394,,,*10\n");
        TestableGeolocationSensor sensor(serial, 12, 13, 9600);

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);

        const auto& loc = sensor.getLocationData();
        assert(loc.isDataValid == true);
        assert(loc.hour == 12);
        assert(loc.minute == 35);
        assert(loc.second == 19);
        assert(loc.day == 23);
        assert(loc.month == 3);
        assert(loc.year == 94);
        assert(loc.speedInKnots == 22.4f);
        assert(loc.courseInDegrees == 84.0f);
        std::printf("  [PASS] NMEA RMC sentence parsing\n");
    }

    // Test 7: Suppression when location hasn't changed
    {
        HardwareSerial serial;
        serial.feed("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47\n");
        TestableGeolocationSensor sensor(serial, 14, 15, 9600);

        Event event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        // First read should emit event
        assert(event.identifier == Sensor::DATA_READ_EVENT_IDENTIFIER);

        // Feed same data again
        serial.feed("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,47.0,M,,*47\n");
        event = Event(Sensor::MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER);
        sensor.processEvent(event);
        // Same location should suppress
        assert(event.identifier == -1);
        std::printf("  [PASS] Suppression when location hasn't changed\n");
    }

    std::printf("All GeolocationSensor tests passed successfully!\n");
    return 0;
}
