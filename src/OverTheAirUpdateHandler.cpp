/**
 * @file OverTheAirUpdateHandler.cpp
 * @brief Implements the OverTheAirUpdateHandler class for Over-The-Air firmware updates via WiFi.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 */

#include "OverTheAirUpdateHandler.h"

#ifdef ARDUINO_ARCH_ESP32
#include <ArduinoOTA.h>
#endif

// Static member definitions
bool OverTheAirUpdateHandler::initialized = false;
OverTheAirUpdateStatus OverTheAirUpdateHandler::status = OverTheAirUpdateStatus::Idle;
int OverTheAirUpdateHandler::lastError = 0;
uint32_t OverTheAirUpdateHandler::progressInNumberOfBytes = 0;
uint32_t OverTheAirUpdateHandler::totalNumberOfBytes = 0;
std::function<void(uint32_t, uint32_t)> OverTheAirUpdateHandler::progressCallback;
std::function<void(int)> OverTheAirUpdateHandler::errorCallback;

namespace {
    std::function<void()> startCallback;
    std::function<void()> endCallback;
}

bool OverTheAirUpdateHandler::begin(const char* hostname) {
#ifdef ARDUINO_ARCH_ESP32
    if (hostname != nullptr) {
        ArduinoOTA.setHostname(hostname);
    }

    ArduinoOTA.onStart([&] {
        status = OverTheAirUpdateStatus::Starting;
        progressInNumberOfBytes = 0;
        totalNumberOfBytes = 0;
        if (startCallback) {
            startCallback();
        }
    });

    ArduinoOTA.onEnd([&] {
        if (status == OverTheAirUpdateStatus::Running) {
            status = OverTheAirUpdateStatus::Finished;
        }
        if (endCallback) {
            endCallback();
        }
    });

    ArduinoOTA.onProgress([&](const uint32_t progress, const uint32_t total) {
        status = OverTheAirUpdateStatus::Running;
        progressInNumberOfBytes = progress;
        totalNumberOfBytes = total;
        if (progressCallback) {
            progressCallback(progress, total);
        }
    });

    ArduinoOTA.onError([&](const int errorCode) {
        status = OverTheAirUpdateStatus::Error;
        lastError = errorCode;
        if (errorCallback) {
            errorCallback(errorCode);
        }
    });

    ArduinoOTA.begin();
    initialized = true;
    status = OverTheAirUpdateStatus::Idle;
    return true;
#else
    (void)hostname;
    initialized = true;
    status = OverTheAirUpdateStatus::Idle;
    return true;
#endif
}

void OverTheAirUpdateHandler::setPassword(const char* password) {
#ifdef ARDUINO_ARCH_ESP32
    if (password != nullptr) {
        ArduinoOTA.setPassword(password);
    }
#else
    (void)password;
#endif
}

void OverTheAirUpdateHandler::onProgress(std::function<void(uint32_t, uint32_t)> callback) {
    progressCallback = std::move(callback);
}

void OverTheAirUpdateHandler::onError(std::function<void(int)> callback) {
    errorCallback = std::move(callback);
}

void OverTheAirUpdateHandler::onStart(std::function<void()> callback) {
    startCallback = std::move(callback);
}

void OverTheAirUpdateHandler::onEnd(std::function<void()> callback) {
    endCallback = std::move(callback);
}

void OverTheAirUpdateHandler::service() {
#ifdef ARDUINO_ARCH_ESP32
    if (initialized) {
        ArduinoOTA.handle();
    }
#endif
}

OverTheAirUpdateStatus OverTheAirUpdateHandler::getStatus() {
    return status;
}

int OverTheAirUpdateHandler::getLastError() {
    return lastError;
}

float OverTheAirUpdateHandler::getProgressInPercentage() {
    if (totalNumberOfBytes == 0) {
        return 0.0f;
    }
    return static_cast<float>(progressInNumberOfBytes) / static_cast<float>(totalNumberOfBytes) * 100.0f;
}

bool OverTheAirUpdateHandler::isInitialized() {
    return initialized;
}
