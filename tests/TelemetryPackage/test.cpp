#include <time.h>
#include "Device.h"
#include "DiagnosticsTelemetryPackage.h"
#include "JsonTelemetryWriter.h"
#include <cassert>
#include <cstdio>
#include <string>

namespace {

// Concrete telemetry packages matching the examples
class SmartBandTelemetryPackage : public TelemetryPackage {
private:
    int32_t heartBeatsPerMinute;
    float skinTemperatureInCelsius;
    float relativeHumidityInPercentage;

public:
    SmartBandTelemetryPackage(int heartRate, float temperature, float humidity)
        : heartBeatsPerMinute(heartRate),
          skinTemperatureInCelsius(temperature),
          relativeHumidityInPercentage(humidity) {}

    ~SmartBandTelemetryPackage() override = default;

    void write(TelemetryWriter& writer) const override {
        writer.field("heartBeatsPerMinute", this->heartBeatsPerMinute);
        writer.field("skinTemperatureInCelsius", this->skinTemperatureInCelsius);
        writer.field("relativeHumidityPercentage", this->relativeHumidityInPercentage);
    }
};

class AirQualityTelemetryPackage : public TelemetryPackage {
private:
    float gasConcentrationInPpm;
    bool gasDetected;
    int32_t operatingMode;

public:
    AirQualityTelemetryPackage(float ppm, bool detected, int mode)
        : gasConcentrationInPpm(ppm), gasDetected(detected), operatingMode(mode) {}

    ~AirQualityTelemetryPackage() override = default;

    void write(TelemetryWriter& writer) const override {
        writer.field("gasPpm", this->gasConcentrationInPpm);
        writer.field("gasDetected", this->gasDetected);
        writer.field("sensorMode", this->operatingMode);
    }
};

// Telemetry packages from HomeSecurityDevice example (W32 high-priority lane)
class SecurityStatusTelemetryPackage : public TelemetryPackage {
private:
    bool doorOpen;
    bool motionDetected;
    bool waterLeak;

public:
    SecurityStatusTelemetryPackage(bool door, bool motion, bool leak)
        : doorOpen(door),
          motionDetected(motion),
          waterLeak(leak) {}

    ~SecurityStatusTelemetryPackage() override = default;

    void write(TelemetryWriter& writer) const override {
        writer.field("doorOpen", this->doorOpen);
        writer.field("motionDetected", this->motionDetected);
        writer.field("waterLeak", this->waterLeak);
    }
};

class SecurityAlertTelemetryPackage : public TelemetryPackage {
private:
    const char* alertType;
    bool alertState;

public:
    SecurityAlertTelemetryPackage(const char* type, bool state)
        : alertType(type),
          alertState(state) {}

    ~SecurityAlertTelemetryPackage() override = default;

    void write(TelemetryWriter& writer) const override {
        writer.field("alertType", this->alertType);
        writer.field("waterLeak", this->alertState);
    }
};

} // anonymous namespace

[[nodiscard]] int main() {
    std::printf("Running Telemetry Serialization Golden Tests...\n");

    // =========================================================================
    // Test 1: SmartBandTelemetryPackage default envelope metadata
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(0, 0);
        String json = packet.toJsonString();

        // Keys in insertion order: envelope metadata first, then subclass fields
        std::string expected = "{\"timestamp\":0,\"sequence\":0,\"heartBeatsPerMinute\":72,\"skinTemperatureInCelsius\":36.50,\"relativeHumidityPercentage\":45.20}";
        assert(json == expected);
        std::printf("  SmartBand default metadata: PASS\n");
    }

    // =========================================================================
    // Test 2: SmartBandTelemetryPackage with custom envelope metadata
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(1234567, 42);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":1234567,\"sequence\":42,\"heartBeatsPerMinute\":72,\"skinTemperatureInCelsius\":36.50,\"relativeHumidityPercentage\":45.20}";
        assert(json == expected);
        std::printf("  SmartBand custom metadata: PASS\n");
    }

    // =========================================================================
    // Test 3: AirQualityTelemetryPackage default envelope metadata
    // =========================================================================
    {
        AirQualityTelemetryPackage packet(250.5f, true, 0);
        packet.setEnvelopeMetadata(0, 0);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":0,\"sequence\":0,\"gasPpm\":250.50,\"gasDetected\":true,\"sensorMode\":0}";
        assert(json == expected);
        std::printf("  AirQuality default metadata: PASS\n");
    }

    // =========================================================================
    // Test 4: AirQualityTelemetryPackage with custom envelope metadata
    // =========================================================================
    {
        AirQualityTelemetryPackage packet(250.5f, true, 0);
        packet.setEnvelopeMetadata(9876543, 99);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":9876543,\"sequence\":99,\"gasPpm\":250.50,\"gasDetected\":true,\"sensorMode\":0}";
        assert(json == expected);
        std::printf("  AirQuality custom metadata: PASS\n");
    }

    // =========================================================================
    // Test 5: SmartBandTelemetryPackage with zero values
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(0, 0.0f, 0.0f);
        packet.setEnvelopeMetadata(0, 0);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":0,\"sequence\":0,\"heartBeatsPerMinute\":0,\"skinTemperatureInCelsius\":0.00,\"relativeHumidityPercentage\":0.00}";
        assert(json == expected);
        std::printf("  SmartBand zero values: PASS\n");
    }

    // =========================================================================
    // Test 6: AirQualityTelemetryPackage with gas not detected
    // =========================================================================
    {
        AirQualityTelemetryPackage packet(0.0f, false, 2);
        packet.setEnvelopeMetadata(100, 1);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":100,\"sequence\":1,\"gasPpm\":0.00,\"gasDetected\":false,\"sensorMode\":2}";
        assert(json == expected);
        std::printf("  AirQuality gas not detected: PASS\n");
    }

    // =========================================================================
    // Test 7: JsonTelemetryWriter overflow detection
    // =========================================================================
    {
        JsonTelemetryWriter writer;
        // Fill the buffer with many fields to trigger overflow
        for (int i = 0; i < 100; i++) {
            writer.field("field", static_cast<int32_t>(i));
        }
        writer.finalize();
        assert(writer.hasOverflow());
        std::printf("  JsonTelemetryWriter overflow: PASS\n");
    }

    // =========================================================================
    // Test 8: toJsonString() contains all fields
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(1234567, 42);
        String json = packet.toJsonString();

        assert(std::string(json).find("\"heartBeatsPerMinute\":72") != std::string::npos);
        assert(std::string(json).find("\"sequence\":42") != std::string::npos);
        assert(std::string(json).find("\"timestamp\":1234567") != std::string::npos);
        std::printf("  toJsonString contains all fields: PASS\n");
    }

    // =========================================================================
    // Test 9: SmartBandTelemetryPackage field count via string search
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(100, 1);
        String json = packet.toJsonString();

        // Count fields by checking each key is present
        assert(std::string(json).find("\"timestamp\"") != std::string::npos);
        assert(std::string(json).find("\"sequence\"") != std::string::npos);
        assert(std::string(json).find("\"heartBeatsPerMinute\"") != std::string::npos);
        assert(std::string(json).find("\"skinTemperatureInCelsius\"") != std::string::npos);
        assert(std::string(json).find("\"relativeHumidityPercentage\"") != std::string::npos);
        std::printf("  SmartBand field count: PASS\n");
    }

    // =========================================================================
    // Test 10: AirQualityTelemetryPackage field count via string search
    // =========================================================================
    {
        AirQualityTelemetryPackage packet(250.5f, true, 0);
        packet.setEnvelopeMetadata(200, 2);
        String json = packet.toJsonString();

        assert(std::string(json).find("\"timestamp\"") != std::string::npos);
        assert(std::string(json).find("\"sequence\"") != std::string::npos);
        assert(std::string(json).find("\"gasPpm\"") != std::string::npos);
        assert(std::string(json).find("\"gasDetected\"") != std::string::npos);
        assert(std::string(json).find("\"sensorMode\"") != std::string::npos);
        std::printf("  AirQuality field count: PASS\n");
    }

    // =========================================================================
    // Test 11: Version metadata omitted when not set (nullptr default)
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(100, 1);
        // setVersionMetadata not called — fields should be omitted
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":100,\"sequence\":1,\"heartBeatsPerMinute\":72,\"skinTemperatureInCelsius\":36.50,\"relativeHumidityPercentage\":45.20}";
        assert(json == expected);
        assert(std::string(json).find("schemaVersion") == std::string::npos);
        assert(std::string(json).find("firmwareVersion") == std::string::npos);
        std::printf("  Version metadata omitted: PASS\n");
    }

    // =========================================================================
    // Test 12: Version metadata included when set
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(100, 1);
        packet.setVersionMetadata("1.0", "1.0.0");
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":100,\"sequence\":1,\"schemaVersion\":\"1.0\",\"firmwareVersion\":\"1.0.0\",\"heartBeatsPerMinute\":72,\"skinTemperatureInCelsius\":36.50,\"relativeHumidityPercentage\":45.20}";
        assert(json == expected);
        std::printf("  Version metadata included: PASS\n");
    }

    // =========================================================================
    // Test 13: Only firmware version set (schema version nullptr)
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(100, 1);
        packet.setVersionMetadata(nullptr, "2.0.0");
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":100,\"sequence\":1,\"firmwareVersion\":\"2.0.0\",\"heartBeatsPerMinute\":72,\"skinTemperatureInCelsius\":36.50,\"relativeHumidityPercentage\":45.20}";
        assert(json == expected);
        assert(std::string(json).find("schemaVersion") == std::string::npos);
        std::printf("  Only firmware version: PASS\n");
    }

    // =========================================================================
    // Test 14: Only schema version set (firmware version nullptr)
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(100, 1);
        packet.setVersionMetadata("2.0", nullptr);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":100,\"sequence\":1,\"schemaVersion\":\"2.0\",\"heartBeatsPerMinute\":72,\"skinTemperatureInCelsius\":36.50,\"relativeHumidityPercentage\":45.20}";
        assert(json == expected);
        assert(std::string(json).find("firmwareVersion") == std::string::npos);
        std::printf("  Only schema version: PASS\n");
    }

    // =========================================================================
    // Test 15: Version metadata field count via string search
    // =========================================================================
    {
        SmartBandTelemetryPackage packet(72, 36.5f, 45.2f);
        packet.setEnvelopeMetadata(100, 1);
        packet.setVersionMetadata("1.0", "1.0.0");
        String json = packet.toJsonString();

        assert(std::string(json).find("\"timestamp\"") != std::string::npos);
        assert(std::string(json).find("\"sequence\"") != std::string::npos);
        assert(std::string(json).find("\"schemaVersion\"") != std::string::npos);
        assert(std::string(json).find("\"firmwareVersion\"") != std::string::npos);
        assert(std::string(json).find("\"heartBeatsPerMinute\"") != std::string::npos);
        assert(std::string(json).find("\"skinTemperatureInCelsius\"") != std::string::npos);
        assert(std::string(json).find("\"relativeHumidityPercentage\"") != std::string::npos);
        std::printf("  Version metadata field count: PASS\n");
    }

    // =========================================================================
    // Test 16: SecurityStatusTelemetryPackage (routine, standard queue)
    // =========================================================================
    {
        SecurityStatusTelemetryPackage packet(true, false, false);
        packet.setEnvelopeMetadata(100, 1);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":100,\"sequence\":1,\"doorOpen\":true,\"motionDetected\":false,\"waterLeak\":false}";
        assert(json == expected);
        assert(!packet.isHighPriority());
        std::printf("  SecurityStatus routine telemetry: PASS\n");
    }

    // =========================================================================
    // Test 17: SecurityAlertTelemetryPackage with high priority
    // =========================================================================
    {
        SecurityAlertTelemetryPackage packet("water_leak", true);
        packet.setEnvelopeMetadata(200, 2);
        packet.markAsHighPriority();
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":200,\"sequence\":2,\"priority\":\"high\",\"alertType\":\"water_leak\",\"waterLeak\":true}";
        assert(json == expected);
        assert(packet.isHighPriority());
        std::printf("  SecurityAlert high-priority telemetry: PASS\n");
    }

    // =========================================================================
    // Test 18: SecurityAlertTelemetryPackage without high priority
    // =========================================================================
    {
        SecurityAlertTelemetryPackage packet("door_opened", true);
        packet.setEnvelopeMetadata(300, 3);
        // markAsHighPriority() not called — no priority field
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":300,\"sequence\":3,\"alertType\":\"door_opened\",\"waterLeak\":true}";
        assert(json == expected);
        assert(!packet.isHighPriority());
        assert(std::string(json).find("priority") == std::string::npos);
        std::printf("  SecurityAlert without priority: PASS\n");
    }

    // =========================================================================
    // Test 19: DiagnosticsTelemetryPackage golden test (W41)
    // =========================================================================
    {
        DiagnosticsSnapshot snapshot;
        snapshot.missedTicks = 5;
        snapshot.worstCaseWakeMicros = 1500;
        snapshot.packetsEnqueued = 100;
        snapshot.packetsDropped = 2;
        snapshot.packetsDroppedOldest = 1;
        snapshot.queueHighWaterMark = 8;
        snapshot.highPriorityEnqueued = 10;
        snapshot.highPriorityDropped = 0;
        snapshot.highPriorityQueueHighWaterMark = 3;
        snapshot.sendsSucceeded = 95;
        snapshot.sendsFailed = 5;
        snapshot.freeHeapBytes = 131072;
        snapshot.minFreeHeapBytes = 98304;
        snapshot.samplerStackFreeWords = 512;
        snapshot.networkStackFreeWords = 1024;
        snapshot.uptimeSeconds = 3600;

        DiagnosticsTelemetryPackage packet(snapshot);
        packet.setEnvelopeMetadata(1000, 42);
        String json = packet.toJsonString();

        std::string expected = "{\"timestamp\":1000,\"sequence\":42,"
            "\"missedTicks\":5,\"worstCaseWakeMicros\":1500,"
            "\"packetsEnqueued\":100,\"packetsDropped\":2,\"packetsDroppedOldest\":1,"
            "\"queueHighWaterMark\":8,\"highPriorityEnqueued\":10,\"highPriorityDropped\":0,"
            "\"highPriorityQueueHighWaterMark\":3,\"sendsSucceeded\":95,\"sendsFailed\":5,"
            "\"freeHeapBytes\":131072,\"minFreeHeapBytes\":98304,"
            "\"samplerStackFreeWords\":512,\"networkStackFreeWords\":1024,\"uptimeSeconds\":3600}";
        assert(json == expected);
        std::printf("  DiagnosticsTelemetryPackage golden test: PASS\n");
    }

    std::printf("All telemetry serialization tests passed.\n");
    return 0;
}
