/**
 * @file DhtSensor.cpp
 * @brief Implements DHT sampling with native single-wire protocol.
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-06-06
 * @version 1.0
 */

#include "DhtSensor.h"

DhtSensor::DhtSensor(const int signalGpioPin, const uint8_t dhtModelType, EventHandler* parentHandler)
    : Sensor(signalGpioPin, parentHandler),
      dhtModelType(dhtModelType),
      cachedTemperatureInCelsius(-999.0f),
      cachedRelativeHumidityInPercentage(-999.0f)
{
    pinMode(pin, INPUT_PULLUP);
}

void DhtSensor::sendStartSignal() const {
    // MCU pulls the data line LOW to wake the sensor up
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    delayMicroseconds(dhtModelType == SENSOR_MODEL_DHT11
            ? DHT11_START_SIGNAL_LOW_TIME_IN_MICROSECONDS
            : DHT22_START_SIGNAL_LOW_TIME_IN_MICROSECONDS);
}

uint32_t DhtSensor::measurePulse(const int level) const {
    const unsigned long pulseStart = micros();
    while (digitalRead(pin) == level) {
        if (micros() - pulseStart > DHT_PULSE_TIMEOUT_IN_MICROSECONDS) {
            return DHT_PULSE_TIMEOUT_SENTINEL;
        }
    }
    return static_cast<uint32_t>(micros() - pulseStart);
}

bool DhtSensor::readData(uint8_t* data) const {
    // Release the line (never drive it HIGH: the sensor answers by pulling it LOW)
    pinMode(pin, INPUT_PULLUP);
    delayMicroseconds(DHT_LINE_RELEASE_TIME_IN_MICROSECONDS);

    // Sensor response: ~80 µs LOW followed by ~80 µs HIGH
    if (measurePulse(LOW) == DHT_PULSE_TIMEOUT_SENTINEL ||
        measurePulse(HIGH) == DHT_PULSE_TIMEOUT_SENTINEL) {
        return false;
    }

    // Capture every pulse first and decode afterwards to keep the timing loop tight
    uint32_t lowPulseDurations[DHT_DATA_BIT_COUNT];
    uint32_t highPulseDurations[DHT_DATA_BIT_COUNT];
    for (uint8_t bitIndex = 0; bitIndex < DHT_DATA_BIT_COUNT; ++bitIndex) {
        lowPulseDurations[bitIndex] = measurePulse(LOW);
        highPulseDurations[bitIndex] = measurePulse(HIGH);
    }

    for (uint8_t byteIndex = 0; byteIndex < DHT_DATA_BYTE_COUNT; ++byteIndex) {
        data[byteIndex] = 0;
    }
    for (uint8_t bitIndex = 0; bitIndex < DHT_DATA_BIT_COUNT; ++bitIndex) {
        const uint32_t lowPulse = lowPulseDurations[bitIndex];
        const uint32_t highPulse = highPulseDurations[bitIndex];
        if (lowPulse == DHT_PULSE_TIMEOUT_SENTINEL || highPulse == DHT_PULSE_TIMEOUT_SENTINEL) {
            return false;
        }
        const uint8_t byteIndex = bitIndex / DHT_BITS_PER_BYTE;
        data[byteIndex] = static_cast<uint8_t>(data[byteIndex] << 1);
        if (highPulse > lowPulse) {
            data[byteIndex] |= 1;
        }
    }
    return true;
}

bool DhtSensor::validateChecksum(const uint8_t* data) {
    const uint8_t checksum = static_cast<uint8_t>(
            data[DHT_HUMIDITY_HIGH_BYTE_INDEX] +
            data[DHT_HUMIDITY_LOW_BYTE_INDEX] +
            data[DHT_TEMPERATURE_HIGH_BYTE_INDEX] +
            data[DHT_TEMPERATURE_LOW_BYTE_INDEX]);
    return checksum == data[DHT_CHECKSUM_BYTE_INDEX];
}

void DhtSensor::processEvent(Event& event) {
    if (event.identifier == MEASURE_DATA_REQUESTED_EVENT_IDENTIFIER) {
        uint8_t rawData[DHT_DATA_BYTE_COUNT] = {};

        sendStartSignal();
        // The frame is timing-critical (~4 ms): keep interrupts and task switches out of it
        noInterrupts();
        const bool frameReceived = readData(rawData);
        interrupts();

        if (!frameReceived || !validateChecksum(rawData)) {
            reportReadFailure(event);
            if (event.identifier != Sensor::SENSOR_FAULT_EVENT_IDENTIFIER) {
                event.identifier = SENSOR_ERROR_SUPPRESSION_IDENTIFIER;
            }
            return;
        }

        float humidity;
        float temperature;
        if (dhtModelType == SENSOR_MODEL_DHT11) {
            // DHT11 transmits integer values in the high bytes
            humidity = static_cast<float>(rawData[DHT_HUMIDITY_HIGH_BYTE_INDEX]);
            temperature = static_cast<float>(rawData[DHT_TEMPERATURE_HIGH_BYTE_INDEX]);
        } else {
            // DHT22/DHT21 transmit values x 10; bit 7 of the temperature high byte is the sign
            const uint16_t rawHumidity = static_cast<uint16_t>(
                    (rawData[DHT_HUMIDITY_HIGH_BYTE_INDEX] << DHT_BITS_PER_BYTE) |
                    rawData[DHT_HUMIDITY_LOW_BYTE_INDEX]);
            const uint16_t rawTemperatureMagnitude = static_cast<uint16_t>(
                    ((rawData[DHT_TEMPERATURE_HIGH_BYTE_INDEX] & DHT22_TEMPERATURE_MAGNITUDE_MASK) << DHT_BITS_PER_BYTE) |
                    rawData[DHT_TEMPERATURE_LOW_BYTE_INDEX]);
            humidity = static_cast<float>(rawHumidity) / DHT22_DECIMAL_SCALE;
            temperature = static_cast<float>(rawTemperatureMagnitude) / DHT22_DECIMAL_SCALE;
            if ((rawData[DHT_TEMPERATURE_HIGH_BYTE_INDEX] & DHT22_TEMPERATURE_SIGN_BIT) != 0) {
                temperature = -temperature;
            }
        }

        reportReadSuccess(event);
        if (event.identifier == SENSOR_RECOVERED_EVENT_IDENTIFIER) {
            // First valid reading after a fault: refresh the cache and emit the recovery event
            // (with sourceId = pin) so the application can log it and refresh its outputs.
            cachedTemperatureInCelsius = temperature;
            cachedRelativeHumidityInPercentage = humidity;
            return;
        }

        if (temperature == cachedTemperatureInCelsius && humidity == cachedRelativeHumidityInPercentage) {
            event.identifier = SENSOR_ERROR_SUPPRESSION_IDENTIFIER;
            return;
        }

        cachedTemperatureInCelsius = temperature;
        cachedRelativeHumidityInPercentage = humidity;
        event = Event(DATA_READ_EVENT_IDENTIFIER, pin);
    }
}

float DhtSensor::getTemperatureInCelsius() const { return cachedTemperatureInCelsius; }
float DhtSensor::getRelativeHumidityInPercentage() const { return cachedRelativeHumidityInPercentage; }

uint32_t DhtSensor::minimumSamplingIntervalFor(const uint8_t dhtModelType) {
    if (dhtModelType == SENSOR_MODEL_DHT11) {
        return DHT11_MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS;
    }
    // DHT22, DHT21, AM2301 all have 2000 ms minimum
    return DHT22_MINIMUM_SAMPLING_INTERVAL_IN_MILLISECONDS;
}

uint32_t DhtSensor::getDefaultSamplingIntervalInMilliseconds() const {
    return DEFAULT_SAMPLING_INTERVAL_IN_MILLISECONDS;
}

uint32_t DhtSensor::getMinimalSamplingIntervalInMilliseconds() const {
    return minimumSamplingIntervalFor(dhtModelType);
}
