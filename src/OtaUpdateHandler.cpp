/**
 * @file OtaUpdateHandler.cpp
 * @brief Implements the OtaUpdateHandler class for Over-The-Air firmware updates via WiFi.
 * @ingroup core_module
 * @copyright (c) 2025-2026 Angel Velasquez
 * @date 2026-11-14
 * @version 1.0
 */

#include "OtaUpdateHandler.h"

#ifdef ARDUINO_ARCH_ESP32
#include <ArduinoOTA.h>
#endif

// Static member definitions
bool OtaUpdateHandler::initialized = false;
const char* OtaUpdateHandler::hostname = nullptr;
OtaUpdateStatus OtaUpdateHandler::status = OtaUpdateStatus::Idle;
int OtaUpdateHandler::lastError = 0;
uint32_t OtaUpdateHandler::progressInNumberOfBytes = 0;
uint32_t OtaUpdateHandler::totalNumberOfBytes = 0;
std::function<void(uint32_t, uint32_t)> OtaUpdateHandler::progressCallback;
std::function<void(int)> OtaUpdateHandler::errorCallback;

namespace {
    std::function<void()> startCallback;
    std::function<void()> endCallback;
}

bool OtaUpdateHandler::begin(const char* hostnameParam) {
#ifdef ARDUINO_ARCH_ESP32
    if (hostnameParam != nullptr) {
        ArduinoOTA.setHostname(hostnameParam);
        hostname = hostnameParam;
    }

    ArduinoOTA.onStart([&] {
        status = OtaUpdateStatus::Starting;
        progressInNumberOfBytes = 0;
        totalNumberOfBytes = 0;
        if (startCallback) {
            startCallback();
        }
    });

    ArduinoOTA.onEnd([&] {
        if (status == OtaUpdateStatus::Running) {
            status = OtaUpdateStatus::Finished;
        }
        if (endCallback) {
            endCallback();
        }
    });

    ArduinoOTA.onProgress([&](const uint32_t progress, const uint32_t total) {
        status = OtaUpdateStatus::Running;
        progressInNumberOfBytes = progress;
        totalNumberOfBytes = total;
        if (progressCallback) {
            progressCallback(progress, total);
        }
    });

    ArduinoOTA.onError([&](const int errorCode) {
        status = OtaUpdateStatus::Error;
        lastError = errorCode;
        if (errorCallback) {
            errorCallback(errorCode);
        }
    });

    ArduinoOTA.begin();
    initialized = true;
    status = OtaUpdateStatus::Idle;
    return true;
#else
    if (hostnameParam != nullptr) {
        hostname = hostnameParam;
    }
    initialized = true;
    status = OtaUpdateStatus::Idle;
    return true;
#endif
}

void OtaUpdateHandler::setPassword(const char* password) {
#ifdef ARDUINO_ARCH_ESP32
    if (password != nullptr) {
        ArduinoOTA.setPassword(password);
    }
#else
    (void)password;
#endif
}

void OtaUpdateHandler::onProgress(OtaProgressCallback callback) {
    progressCallback = std::move(callback);
}

void OtaUpdateHandler::onError(OtaErrorCallback callback) {
    errorCallback = std::move(callback);
}

void OtaUpdateHandler::onStart(OtaStartCallback callback) {
    startCallback = std::move(callback);
}

void OtaUpdateHandler::onEnd(OtaEndCallback callback) {
    endCallback = std::move(callback);
}

void OtaUpdateHandler::service() {
#ifdef ARDUINO_ARCH_ESP32
    if (initialized) {
        ArduinoOTA.handle();
    }
#endif
}

const char* OtaUpdateHandler::getHostname() {
    return hostname;
}

OtaUpdateStatus OtaUpdateHandler::getStatus() {
    return status;
}

int OtaUpdateHandler::getLastError() {
    return lastError;
}

float OtaUpdateHandler::getProgressInPercentage() {
    if (totalNumberOfBytes == 0) {
        return 0.0f;
    }
    return static_cast<float>(progressInNumberOfBytes) / static_cast<float>(totalNumberOfBytes) * 100.0f;
}

bool OtaUpdateHandler::isInitialized() {
    return initialized;
}
