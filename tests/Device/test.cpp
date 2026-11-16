#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the Device component.
 *
 * Tests static methods and constants that don't require FreeRTOS.
 * Device requires FreeRTOS tasks/queues, so we test only the static API.
 */

#include "Arduino.h"
#include "Device.h"
#include <cassert>
#include <cstdio>

int main() {
    std::printf("Running Device tests...\n");

    // Test 1: Default telemetry schema version is nullptr
    {
        const char* version = Device::getTelemetrySchemaVersion();
        assert(version == nullptr);
        std::printf("  [PASS] Default telemetry schema version is nullptr\n");
    }

    // Test 2: Set and get telemetry schema version
    {
        Device::setTelemetrySchemaVersion("2.0");
        assert(Device::getTelemetrySchemaVersion() != nullptr);
        std::printf("  [PASS] Set and get telemetry schema version\n");
    }

    // Test 3: Reset telemetry schema version to nullptr
    {
        Device::setTelemetrySchemaVersion(nullptr);
        assert(Device::getTelemetrySchemaVersion() == nullptr);
        std::printf("  [PASS] Reset telemetry schema version to nullptr\n");
    }

    // Test 4: Default firmware version is nullptr
    {
        const char* version = Device::getFirmwareVersion();
        assert(version == nullptr);
        std::printf("  [PASS] Default firmware version is nullptr\n");
    }

    // Test 5: Set and get firmware version
    {
        Device::setFirmwareVersion("1.2.3");
        assert(Device::getFirmwareVersion() != nullptr);
        std::printf("  [PASS] Set and get firmware version\n");
    }

    // Test 6: Reset firmware version to nullptr
    {
        Device::setFirmwareVersion(nullptr);
        assert(Device::getFirmwareVersion() == nullptr);
        std::printf("  [PASS] Reset firmware version to nullptr\n");
    }

    // Test 7: MAX_TRIGGERED_SENSORS constant
    {
        assert(Device::MAX_TRIGGERED_SENSORS == 4);
        std::printf("  [PASS] MAX_TRIGGERED_SENSORS constant\n");
    }

    // Test 8: DeviceConstants::MAX_GPIO_INTERRUPTS
    {
        assert(DeviceConstants::MAX_GPIO_INTERRUPTS == 64);
        std::printf("  [PASS] DeviceConstants::MAX_GPIO_INTERRUPTS\n");
    }

    // Test 9: NotificationBitFlag values
    {
        assert(static_cast<uint32_t>(Device::NotificationBitFlag::TimerTickArrived) == 0x1);
        assert(static_cast<uint32_t>(Device::NotificationBitFlag::TerminationSignal) == 0x2);
        assert(static_cast<uint32_t>(Device::NotificationBitFlag::SensorTrigger) == 0x4);
        assert(static_cast<uint32_t>(Device::NotificationBitFlag::CommandAvailable) == 0x8);
        std::printf("  [PASS] NotificationBitFlag values\n");
    }

    // Test 10: PowerMode enum values
    {
        assert(static_cast<uint8_t>(Device::PowerMode::Periodic) == 0);
        assert(static_cast<uint8_t>(Device::PowerMode::Tickless) == 1);
        assert(static_cast<uint8_t>(Device::PowerMode::DeepSleep) == 2);
        std::printf("  [PASS] PowerMode enum values\n");
    }

    // Test 11: CommandEntry default constructor
    {
        Device::CommandEntry entry;
        assert(entry.targetHandler == nullptr);
        assert(entry.command.identifier == -1);
        std::printf("  [PASS] CommandEntry default constructor\n");
    }

    // Test 12: CommandEntry parameterized constructor
    {
        struct FakeHandler : CommandHandler {
            void handle(Command) override {}
        };
        FakeHandler handler;
        Device::CommandEntry entry(&handler, Command{42});
        assert(entry.targetHandler == &handler);
        assert(entry.command.identifier == 42);
        std::printf("  [PASS] CommandEntry parameterized constructor\n");
    }

    std::printf("All Device tests passed successfully!\n");
    return 0;
}
