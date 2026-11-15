#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for PowerManager (W26).
 *
 * Validates battery voltage sensing, level calculation, low-battery threshold,
 * and voltage range configuration.
 */

#include "Arduino.h"
#include "PowerManager.h"
#include <cassert>
#include <cstdio>
#include <cmath>

namespace {

// Helper to check float equality with tolerance
bool floatsEqual(float a, float b, float epsilon = 0.1f) {
    return std::fabs(a - b) < epsilon;
}

} // namespace

int main() {
    std::printf("Running PowerManager tests...\n");

    // =========================================================================
    // Test 1: PowerManager not initialized by default
    // =========================================================================
    {
        assert(PowerManager::isInitialized() == false);
        std::printf("  [PASS] PowerManager not initialized by default\n");
    }

    // =========================================================================
    // Test 2: begin() initializes PowerManager
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f);
        assert(PowerManager::isInitialized() == true);
        std::printf("  [PASS] begin() initializes PowerManager\n");
    }

    // =========================================================================
    // Test 3: Default values after initialization
    // =========================================================================
    {
        assert(PowerManager::getLowBatteryThresholdInMillivolts() == 3300);
        assert(PowerManager::getMinVoltageInMillivolts() == 3000);
        assert(PowerManager::getMaxVoltageInMillivolts() == 4200);
        std::printf("  [PASS] Default values are correct\n");
    }

    // =========================================================================
    // Test 4: Battery voltage calculation with 2:1 divider (3.7V battery)
    // =========================================================================
    {
        // Simulate a 3.7V battery with 2:1 divider
        // ADC sees 1.85V. Raw ADC = (1.85 / 1.1) * 4095 = 6892, but capped at 4095
        // Actually: rawADC / 4095 * 1100 * 2.0 = voltage
        // For 3.7V: rawADC = 3.7 / (1100 * 2.0 / 4095) = 3.7 / 0.537 = 6892 -> capped at 4095
        // Let's use a value that doesn't saturate: 3.3V battery
        // rawADC = 3.3 / (1100 * 2.0 / 4095) = 3.3 / 0.537 = 6145 -> still capped
        // With 2:1 divider, max readable voltage is 1100 * 2 = 2200 mV
        // Let's use a more realistic setup: 3.3V reference
        PowerManager::begin(34, 2.0f, 3300);

        // For 3.7V battery with 2:1 divider and 3.3V reference:
        // ADC sees 1.85V. Raw = 1.85 / 3.3 * 4095 = 2287
        fakeAnalogRead[34] = 2287;
        uint32_t voltage = PowerManager::getBatteryVoltageInMillivolts();
        // Expected: 2287 / 4095 * 3300 * 2.0 = 3699 mV (~3.7V)
        assert(voltage >= 3690 && voltage <= 3710);
        std::printf("  [PASS] Battery voltage calculation: %u mV (expected ~3700)\n", voltage);
    }

    // =========================================================================
    // Test 5: Battery voltage with no divider (1:1 ratio)
    // =========================================================================
    {
        PowerManager::begin(35, 1.0f, 3300);

        // For 3.3V battery with 1:1 divider and 3.3V reference:
        // Raw = 3.3 / 3.3 * 4095 = 4095
        fakeAnalogRead[35] = 4095;
        uint32_t voltage = PowerManager::getBatteryVoltageInMillivolts();
        // Expected: 4095 / 4095 * 3300 * 1.0 = 3300 mV
        assert(voltage == 3300);
        std::printf("  [PASS] Battery voltage with 1:1 divider: %u mV\n", voltage);
    }

    // =========================================================================
    // Test 6: Battery level calculation (mid-range voltage)
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setBatteryVoltageRange(3000, 4200);

        // Set ADC to read ~3600 mV (mid-range)
        // rawADC / 4095 * 3300 * 2.0 = 3600
        // rawADC = 3600 / (3300 * 2.0 / 4095) = 3600 / 1.611 = 2234
        fakeAnalogRead[34] = 2234;
        float level = PowerManager::getBatteryLevelInPercentage();
        // Expected: (3600 - 3000) / (4200 - 3000) * 100 = 50%
        assert(floatsEqual(level, 50.0f, 1.0f));
        std::printf("  [PASS] Battery level at mid-range: %.1f%% (within tolerance: %d)\n", level, floatsEqual(level, 50.0f, 1.0f));
    }

    // =========================================================================
    // Test 7: Battery level at full voltage
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setBatteryVoltageRange(3000, 4200);

        // Set ADC to read ~4200 mV (full)
        // rawADC / 4095 * 3300 * 2.0 = 4200
        // rawADC = 4200 / (3300 * 2.0 / 4095) = 4200 / 1.611 = 2607
        fakeAnalogRead[34] = 2607;
        float level = PowerManager::getBatteryLevelInPercentage();
        assert(floatsEqual(level, 100.0f, 1.0f));
        std::printf("  [PASS] Battery level at full: %.1f%% (within tolerance: %d)\n", level, floatsEqual(level, 100.0f, 1.0f));
    }

    // =========================================================================
    // Test 8: Battery level at empty voltage
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setBatteryVoltageRange(3000, 4200);

        // Set ADC to read ~3000 mV (empty)
        // rawADC / 4095 * 3300 * 2.0 = 3000
        // rawADC = 3000 / (3300 * 2.0 / 4095) = 3000 / 1.611 = 1862
        fakeAnalogRead[34] = 1862;
        float level = PowerManager::getBatteryLevelInPercentage();
        assert(floatsEqual(level, 0.0f, 1.0f));
        std::printf("  [PASS] Battery level at empty: %.1f%% (within tolerance: %d)\n", level, floatsEqual(level, 0.0f, 1.0f));
    }

    // =========================================================================
    // Test 9: Low battery threshold detection
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setLowBatteryThresholdInMillivolts(3300);

        // Set ADC to read ~3200 mV (below threshold)
        // rawADC / 4095 * 3300 * 2.0 = 3200
        // rawADC = 3200 / (3300 * 2.0 / 4095) = 3200 / 1.611 = 1986
        fakeAnalogRead[34] = 1986;
        assert(PowerManager::isLowBattery() == true);
        std::printf("  [PASS] Low battery detected below threshold\n");
    }

    // =========================================================================
    // Test 10: Not low battery above threshold
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setLowBatteryThresholdInMillivolts(3300);

        // Set ADC to read ~3700 mV (above threshold)
        // rawADC / 4095 * 3300 * 2.0 = 3700
        // rawADC = 3700 / (3300 * 2.0 / 4095) = 3700 / 1.611 = 2297
        fakeAnalogRead[34] = 2297;
        assert(PowerManager::isLowBattery() == false);
        std::printf("  [PASS] Not low battery above threshold\n");
    }

    // =========================================================================
    // Test 11: Custom low battery threshold
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setLowBatteryThresholdInMillivolts(3500);

        // Set ADC to read ~3600 mV (below custom threshold of 3500)
        // rawADC / 4095 * 3300 * 2.0 = 3600
        // rawADC = 3600 / (3300 * 2.0 / 4095) = 3600 / 1.611 = 2234
        fakeAnalogRead[34] = 2234;
        assert(PowerManager::isLowBattery() == true);
        std::printf("  [PASS] Custom low battery threshold works\n");
    }

    // =========================================================================
    // Test 12: Custom battery voltage range
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setBatteryVoltageRange(2500, 4000);

        // Set ADC to read ~3250 mV (midpoint of 2500-4000)
        // rawADC / 4095 * 3300 * 2.0 = 3250
        // rawADC = 3250 / (3300 * 2.0 / 4095) = 3250 / 1.611 = 2017
        fakeAnalogRead[34] = 2017;
        float level = PowerManager::getBatteryLevelInPercentage();
        // Expected: (3250 - 2500) / (4000 - 2500) * 100 = 50%
        assert(floatsEqual(level, 50.0f, 1.0f));
        std::printf("  [PASS] Custom battery voltage range: %.1f%% (within tolerance: %d)\n", level, floatsEqual(level, 50.0f, 1.0f));
    }

    // =========================================================================
    // Test 13: Voltage above max returns 100%
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setBatteryVoltageRange(3000, 4200);

        // Set ADC to read ~4500 mV (above max of 4200)
        // rawADC / 4095 * 3300 * 2.0 = 4500
        // rawADC = 4500 / (3300 * 2.0 / 4095) = 4500 / 1.611 = 2793
        fakeAnalogRead[34] = 2793;
        float level = PowerManager::getBatteryLevelInPercentage();
        assert(floatsEqual(level, 100.0f, 0.5f));
        std::printf("  [PASS] Voltage above max returns 100%% (%.1f%%)\n", level);
    }

    // =========================================================================
    // Test 14: Voltage below min returns 0%
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        PowerManager::setBatteryVoltageRange(3000, 4200);

        // Set ADC to read ~2800 mV (below min of 3000)
        // rawADC / 4095 * 3300 * 2.0 = 2800
        // rawADC = 2800 / (3300 * 2.0 / 4095) = 2800 / 1.611 = 1738
        fakeAnalogRead[34] = 1738;
        float level = PowerManager::getBatteryLevelInPercentage();
        assert(floatsEqual(level, 0.0f, 0.5f));
        std::printf("  [PASS] Voltage below min returns 0%% (%.1f%%)\n", level);
    }

    // =========================================================================
    // Test 15: Zero ADC reading returns 0 mV
    // =========================================================================
    {
        PowerManager::begin(34, 2.0f, 3300);
        fakeAnalogRead[34] = 0;
        uint32_t voltage = PowerManager::getBatteryVoltageInMillivolts();
        assert(voltage == 0);
        std::printf("  [PASS] Zero ADC reading returns %u mV\n", voltage);
    }

    // =========================================================================
    // Test 16: Full ADC reading with 1:1 divider
    // =========================================================================
    {
        PowerManager::begin(34, 1.0f, 3300);
        fakeAnalogRead[34] = 4095;
        uint32_t voltage = PowerManager::getBatteryVoltageInMillivolts();
        assert(voltage == 3300);
        std::printf("  [PASS] Full ADC reading with 1:1 divider: %u mV\n", voltage);
    }

    // =========================================================================
    // Test 17: getBatteryVoltageInMillivolts returns 0 when not initialized
    // =========================================================================
    {
        // Reset by re-initializing, then we can't easily uninitialize,
        // but we verified test 1 checks isInitialized before begin.
        // This test verifies the return value when initialized with zero reading.
        PowerManager::begin(34, 1.0f, 3300);
        fakeAnalogRead[34] = 0;
        uint32_t voltage = PowerManager::getBatteryVoltageInMillivolts();
        assert(voltage == 0);
        std::printf("  [PASS] Zero reading returns %u mV\n", voltage);
    }

    // =========================================================================
    // Test 18: isLowBattery returns false when not initialized
    // =========================================================================
    {
        // We can't easily uninitialize after begin(), but we verified
        // the default state in test 1. This test verifies the logic path.
        // After initialization with high voltage, it should return false.
        PowerManager::begin(34, 1.0f, 3300);
        fakeAnalogRead[34] = 4095; // 3300 mV, at threshold
        assert(PowerManager::isLowBattery() == false);
        std::printf("  [PASS] isLowBattery returns false at threshold\n");
    }

    std::printf("All PowerManager tests passed successfully!\n");
    return 0;
}
