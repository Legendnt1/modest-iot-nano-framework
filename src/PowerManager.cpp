/**
 * @file PowerManager.cpp
 * @brief Implements battery voltage sensing and power monitoring for ESP32 IoT devices.
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 */

#include "PowerManager.h"
#include <Arduino.h>

// Static member definitions
uint8_t     PowerManager::batteryPin                       = 0;
float       PowerManager::dividerRatio                     = 1.0f;
uint32_t    PowerManager::vccReferenceInMillivolts         = DEFAULT_VCC_REFERENCE_IN_MILLIVOLTS;
uint32_t    PowerManager::lowBatteryThresholdInMillivolts  = 3300; ///< Default low-battery threshold: 3.3V
uint32_t    PowerManager::minVoltageInMillivolts           = 3000; ///< Default empty battery: 3.0V (Li-ion)
uint32_t    PowerManager::maxVoltageInMillivolts           = 4200; ///< Default full battery: 4.2V (Li-ion)
bool        PowerManager::initialized                      = false;

void PowerManager::begin(const uint8_t pin, const float dividerRatioValue, const uint32_t vccReference) {
    batteryPin = pin;
    dividerRatio = dividerRatioValue;
    vccReferenceInMillivolts = vccReference;
    initialized = true;
#ifdef ARDUINO_ARCH_ESP32
    pinMode(pin, INPUT);
#endif
}

uint32_t PowerManager::getBatteryVoltageInMillivolts() {
    if (!initialized) {
        return 0;
    }

    int rawADC = analogRead(batteryPin);
    if (rawADC < 0) {
        rawADC = 0;
    }

    // Convert raw ADC reading to voltage: (raw / ADC_MAX) * vccReference * dividerRatio
    const uint32_t voltageInMillivolts = static_cast<uint32_t>(
        static_cast<float>(rawADC) / static_cast<float>(ADC_MAX_VALUE) *
        static_cast<float>(vccReferenceInMillivolts) *
        dividerRatio
    );

    return voltageInMillivolts;
}

float PowerManager::getBatteryLevelInPercentage() {
    if (!initialized) {
        return 0.0f;
    }

    const uint32_t voltageInMillivolts = getBatteryVoltageInMillivolts();

    if (voltageInMillivolts <= minVoltageInMillivolts) {
        return 0.0f;
    }
    if (voltageInMillivolts >= maxVoltageInMillivolts) {
        return 100.0f;
    }

    // Linear interpolation between min and max voltage
    const float levelInPercentage = static_cast<float>(voltageInMillivolts - minVoltageInMillivolts) /
                  static_cast<float>(maxVoltageInMillivolts - minVoltageInMillivolts) * 100.0f;

    return levelInPercentage;
}

bool PowerManager::isLowBattery() {
    if (!initialized) {
        return false;
    }

    const uint32_t voltageInMillivolts = getBatteryVoltageInMillivolts();
    return voltageInMillivolts < lowBatteryThresholdInMillivolts;
}

void PowerManager::setLowBatteryThresholdInMillivolts(const uint32_t thresholdInMillivolts) {
    lowBatteryThresholdInMillivolts = thresholdInMillivolts;
}

uint32_t PowerManager::getLowBatteryThresholdInMillivolts() {
    return lowBatteryThresholdInMillivolts;
}

void PowerManager::setBatteryVoltageRange(const uint32_t minimumVoltageInMillivolts,
                                          const uint32_t maximumVoltageInMillivolts) {
    minVoltageInMillivolts = minimumVoltageInMillivolts;
    maxVoltageInMillivolts = maximumVoltageInMillivolts;
}

uint32_t PowerManager::getMinVoltageInMillivolts() {
    return minVoltageInMillivolts;
}

uint32_t PowerManager::getMaxVoltageInMillivolts() {
    return maxVoltageInMillivolts;
}

bool PowerManager::isInitialized() {
    return initialized;
}
