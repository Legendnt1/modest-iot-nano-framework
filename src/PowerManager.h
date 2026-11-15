/**
 * @file PowerManager.h
 * @brief Manages battery voltage sensing and power monitoring for ESP32 IoT devices.
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 * @ingroup core_module
 */

#ifndef MODEST_IOT_NANO_FRAMEWORK_POWER_MANAGER_H
#define MODEST_IOT_NANO_FRAMEWORK_POWER_MANAGER_H

#include <cstdint>

/**
 * @brief Default ADC reading for the ESP32 (12-bit resolution).
 *
 * The ESP32 ADC returns values from 0 to 4095 for a 12-bit reading.
 */
constexpr uint16_t ADC_MAX_VALUE = 4095;

/**
 * @brief Default VCC reference voltage in millivolts for ESP32 ADC.
 *
 * The ESP32 ADC reference voltage is typically 1100 mV (1.1V internal reference).
 */
constexpr uint32_t DEFAULT_VCC_REFERENCE_IN_MILLIVOLTS = 1100;

/**
 * @brief Manages battery voltage sensing and power monitoring.
 *
 * Reads battery voltage via an ADC pin with configurable voltage divider ratio,
 * calculates battery level percentage, and provides low-battery threshold alerts.
 *
 * All methods are static — the component is a singleton infrastructure service.
 * Falls back to safe defaults when not initialized, preserving backward compatibility.
 *
 * @par Voltage Divider
 * Most battery circuits use a voltage divider to scale the battery voltage to
 * the ADC input range. For example, a 2:1 divider (two equal resistors) halves
 * the battery voltage before it reaches the ADC. The divider ratio is configured
 * via setDividerRatio().
 *
 * @par Example
 * @code
 * // Configure battery monitoring on ADC pin 34 with a 2:1 voltage divider
 * PowerManager::begin(34, 2.0f);
 *
 * // Read battery voltage
 * uint32_t voltage = PowerManager::getBatteryVoltageInMillivolts(); // in millivolts
 * float level = PowerManager::getBatteryLevelInPercentage();        // 0.0 to 100.0
 *
 * // Check low battery status
 * if (PowerManager::isLowBattery()) {
 *     // Take action: reduce sampling, enter deep sleep, etc.
 * }
 * @endcode
 *
 * @par Threading
 * @runs_on{any} — all methods are thread-safe; safe to call from any task.
 */
class PowerManager {
public:
    /**
     * @brief Initializes battery monitoring on the specified ADC pin.
     * @param batteryPin ADC pin connected to the battery voltage divider.
     * @param dividerRatio Voltage divider ratio (e.g., 2.0f for a 2:1 divider).
     * @param vccReferenceInMillivolts ADC reference voltage in millivolts (default 1100 mV).
     * @note Call this once in setup() before reading battery values.
     *       The divider ratio scales the ADC reading back to the actual battery voltage.
     *       For example, with a 2:1 divider and a 3.7V battery, the ADC sees ~1.85V.
     */
    static void begin(uint8_t batteryPin, float dividerRatio, uint32_t vccReferenceInMillivolts = DEFAULT_VCC_REFERENCE_IN_MILLIVOLTS);

    /**
     * @brief Reads the current battery voltage from the ADC.
     * @return Battery voltage in millivolts, or 0 if not initialized.
     * @note Applies the configured voltage divider ratio and ADC reference voltage
     *       to convert the raw ADC reading to actual battery voltage.
     */
    [[nodiscard]] static uint32_t getBatteryVoltageInMillivolts();

    /**
     * @brief Returns the estimated battery level as a percentage.
     * @return Battery level from 0.0 (empty) to 100.0 (full).
     * @note Uses a linear scale between minVoltage and maxVoltage.
     *       Values below minVoltage return 0.0; values above maxVoltage return 100.0.
     *       Default range is 3000 mV (empty) to 4200 mV (full) for Li-ion batteries.
     */
    [[nodiscard]] static float getBatteryLevelInPercentage();

    /**
     * @brief Checks whether the battery voltage is below the low-battery threshold.
     * @return true if battery voltage is below the configured threshold.
     * @note Default threshold is 3300 mV. Applications can adjust via setLowBatteryThresholdInMillivolts().
     */
    [[nodiscard]] static bool isLowBattery();

    /**
     * @brief Sets the low-battery voltage threshold.
     * @param thresholdInMillivolts Low-battery threshold in millivolts (default 3300 mV).
     * @note When battery voltage drops below this threshold, isLowBattery() returns true.
     */
    static void setLowBatteryThresholdInMillivolts(uint32_t thresholdInMillivolts);

    /**
     * @brief Returns the current low-battery threshold.
     * @return Low-battery threshold in millivolts.
     */
    [[nodiscard]] static uint32_t getLowBatteryThresholdInMillivolts();

    /**
     * @brief Sets the voltage range for battery level percentage calculation.
     * @param minimumVoltageInMillivolts Empty battery voltage in millivolts (default 3000 mV).
     * @param maximumVoltageInMillivolts Full battery voltage in millivolts (default 4200 mV).
     * @note Used by getBatteryLevelInPercentage() to calculate the percentage.
     *       Adjust these values to match your specific battery chemistry.
     */
    static void setBatteryVoltageRange(uint32_t minimumVoltageInMillivolts, uint32_t maximumVoltageInMillivolts);

    /**
     * @brief Returns the minimum voltage for battery level calculation.
     * @return Minimum voltage in millivolts.
     */
    [[nodiscard]] static uint32_t getMinVoltageInMillivolts();

    /**
     * @brief Returns the maximum voltage for battery level calculation.
     * @return Maximum voltage in millivolts.
     */
    [[nodiscard]] static uint32_t getMaxVoltageInMillivolts();

    /**
     * @brief Checks whether battery monitoring has been initialized.
     * @return true if begin() has been called with a valid pin.
     */
    [[nodiscard]] static bool isInitialized();

private:
    static uint8_t batteryPin;          ///< ADC pin connected to the battery voltage divider.
    static float dividerRatio;          ///< Voltage divider ratio for scaling ADC readings.
    static uint32_t vccReferenceInMillivolts;     ///< ADC reference voltage in millivolts.
    static uint32_t lowBatteryThresholdInMillivolts; ///< Low-battery threshold in millivolts.
    static uint32_t minVoltageInMillivolts;       ///< Minimum voltage for battery level calculation (empty).
    static uint32_t maxVoltageInMillivolts;       ///< Maximum voltage for battery level calculation (full).
    static bool initialized;            ///< Tracks whether begin() was called.
};

#endif // MODEST_IOT_NANO_FRAMEWORK_POWER_MANAGER_H
