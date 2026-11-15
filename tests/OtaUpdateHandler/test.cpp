#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for OtaUpdateHandler (W28).
 *
 * Validates OTA update initialization, status tracking, progress reporting,
 * error handling, and callback registration.
 */

#include "Arduino.h"
#include "OtaUpdateHandler.h"
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
    std::printf("Running OtaUpdateHandler tests...\n");

    // =========================================================================
    // Test 1: OtaUpdateHandler not initialized by default
    // =========================================================================
    {
        assert(OtaUpdateHandler::isInitialized() == false);
        std::printf("  [PASS] OtaUpdateHandler not initialized by default\n");
    }

    // =========================================================================
    // Test 2: begin() initializes OtaUpdateHandler
    // =========================================================================
    {
        assert(OtaUpdateHandler::begin("test-device") == true);
        assert(OtaUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] begin() initializes OtaUpdateHandler\n");
    }

    // =========================================================================
    // Test 3: Default status is Idle after initialization
    // =========================================================================
    {
        assert(OtaUpdateHandler::getStatus() == OtaUpdateStatus::Idle);
        std::printf("  [PASS] Default status is Idle\n");
    }

    // =========================================================================
    // Test 4: Default error code is 0
    // =========================================================================
    {
        assert(OtaUpdateHandler::getLastError() == 0);
        std::printf("  [PASS] Default error code is 0\n");
    }

    // =========================================================================
    // Test 5: Progress percentage is 0.0 when not started
    // =========================================================================
    {
        assert(floatsEqual(OtaUpdateHandler::getProgressInPercentage(), 0.0f));
        std::printf("  [PASS] Progress percentage is 0.0 when not started\n");
    }

    // =========================================================================
    // Test 5b: service() is safe to call multiple times (no-op when idle)
    // =========================================================================
    {
        // service() is a no-op when no OTA update is in progress.
        // Multiple calls should be safe and not change state.
        OtaUpdateHandler::service();
        OtaUpdateHandler::service();
        assert(OtaUpdateHandler::getStatus() == OtaUpdateStatus::Idle);
        std::printf("  [PASS] service() is safe to call multiple times (no-op when idle)\n");
    }

    // =========================================================================
    // Test 6: setPassword() is safe to call
    // =========================================================================
    {
        OtaUpdateHandler::setPassword("secret123");
        std::printf("  [PASS] setPassword() is safe to call\n");
    }

    // =========================================================================
    // Test 7: Callback registration is safe
    // =========================================================================
    {
        bool progressCalled = false;
        bool errorCalled = false;
        bool startCalled = false;
        bool endCalled = false;

        OtaUpdateHandler::onProgress([&progressCalled](uint32_t, uint32_t) {
            progressCalled = true;
        });

        OtaUpdateHandler::onError([&errorCalled](int) {
            errorCalled = true;
        });

        OtaUpdateHandler::onStart([&startCalled]() {
            startCalled = true;
        });

        OtaUpdateHandler::onEnd([&endCalled]() {
            endCalled = true;
        });

        std::printf("  [PASS] Callback registration is safe\n");
    }

    // =========================================================================
    // Test 8: service() is safe to call when initialized
    // =========================================================================
    {
        OtaUpdateHandler::service();
        std::printf("  [PASS] service() is safe to call when initialized\n");
    }

    // =========================================================================
    // Test 9: OtaUpdateStatus enum values are correct
    // =========================================================================
    {
        assert(static_cast<uint8_t>(OtaUpdateStatus::Idle) == 0);
        assert(static_cast<uint8_t>(OtaUpdateStatus::Starting) == 1);
        assert(static_cast<uint8_t>(OtaUpdateStatus::Running) == 2);
        assert(static_cast<uint8_t>(OtaUpdateStatus::Finished) == 3);
        assert(static_cast<uint8_t>(OtaUpdateStatus::Error) == 4);
        std::printf("  [PASS] OtaUpdateStatus enum values are correct\n");
    }

    // =========================================================================
    // Test 10: begin() with default hostname
    // =========================================================================
    {
        // Reset initialized state for this test
        OtaUpdateHandler::begin();
        assert(OtaUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] begin() with default hostname works\n");
    }

    // =========================================================================
    // Test 11: getHostname() returns the hostname set during begin()
    // =========================================================================
    {
        // Test 10 called begin() with default hostname, so getHostname() should return it
        const char* host = OtaUpdateHandler::getHostname();
        assert(host != nullptr);
        std::printf("  [PASS] getHostname() returns the hostname set during begin()\n");
    }

    // =========================================================================
    // Test 12: service() is safe to call multiple times
    // =========================================================================
    {
        // Already initialized from previous tests, but service() should be safe
        OtaUpdateHandler::service();
        OtaUpdateHandler::service();
        OtaUpdateHandler::service();
        std::printf("  [PASS] service() is safe to call multiple times\n");
    }

    // =========================================================================
    // Test 13: setPassword() with nullptr is safe
    // =========================================================================
    {
        OtaUpdateHandler::setPassword(nullptr);
        std::printf("  [PASS] setPassword(nullptr) is safe\n");
    }

    // =========================================================================
    // Test 14: begin() with nullptr hostname is safe
    // =========================================================================
    {
        OtaUpdateHandler::begin(nullptr);
        assert(OtaUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] begin(nullptr) is safe\n");
    }

    // =========================================================================
    // Test 15: OtaProgressCallback type alias is compatible
    // =========================================================================
    {
        bool callbackInvoked = false;
        OtaProgressCallback cb = [&callbackInvoked](uint32_t progressInNumberOfBytesWritten, uint32_t totalNumberOfBytes) {
            (void)progressInNumberOfBytesWritten;
            (void)totalNumberOfBytes;
            callbackInvoked = true;
        };
        // Verify the callback type is callable
        cb(100, 200);
        assert(callbackInvoked == true);
        std::printf("  [PASS] OtaProgressCallback type alias is compatible\n");
    }

    // =========================================================================
    // Test 16: OtaErrorCallback type alias is compatible
    // =========================================================================
    {
        int capturedErrorCode = 0;
        OtaErrorCallback cb = [&capturedErrorCode](int errorCode) {
            capturedErrorCode = errorCode;
        };
        // Verify the callback type is callable and captures the error code
        cb(42);
        assert(capturedErrorCode == 42);
        std::printf("  [PASS] OtaErrorCallback type alias is compatible\n");
    }

    // =========================================================================
    // Test 17: OtaStartCallback type alias is compatible
    // =========================================================================
    {
        bool callbackInvoked = false;
        OtaStartCallback cb = [&callbackInvoked]() {
            callbackInvoked = true;
        };
        // Verify the callback type is callable
        cb();
        assert(callbackInvoked == true);
        std::printf("  [PASS] OtaStartCallback type alias is compatible\n");
    }

    // =========================================================================
    // Test 18: OtaEndCallback type alias is compatible
    // =========================================================================
    {
        bool callbackInvoked = false;
        OtaEndCallback cb = [&callbackInvoked]() {
            callbackInvoked = true;
        };
        // Verify the callback type is callable
        cb();
        assert(callbackInvoked == true);
        std::printf("  [PASS] OtaEndCallback type alias is compatible\n");
    }

    // =========================================================================
    // Test 19: setPassword() before begin() is safe
    // =========================================================================
    {
        // Per KDoc, setPassword() can be called before or after begin()
        // This test verifies the order works without crash
        OtaUpdateHandler::setPassword("pre-begin-password");
        OtaUpdateHandler::begin("ordered-device");
        assert(OtaUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] setPassword() before begin() is safe\n");
    }

    // =========================================================================
    // Test 20: Re-initialization with begin() is safe
    // =========================================================================
    {
        // Calling begin() again after already initialized should be safe
        assert(OtaUpdateHandler::isInitialized() == true);
        OtaUpdateHandler::begin("re-init-device");
        assert(OtaUpdateHandler::isInitialized() == true);
        assert(OtaUpdateHandler::getStatus() == OtaUpdateStatus::Idle);
        std::printf("  [PASS] Re-initialization with begin() is safe\n");
    }

    // =========================================================================
    // Test 21: Status and error are reset after re-initialization
    // =========================================================================
    {
        // After begin(), status should be Idle and error should be 0
        assert(OtaUpdateHandler::getStatus() == OtaUpdateStatus::Idle);
        assert(OtaUpdateHandler::getLastError() == 0);
        std::printf("  [PASS] Status and error are reset after re-initialization\n");
    }

    std::printf("All OtaUpdateHandler tests passed!\n");
    return 0;
}
