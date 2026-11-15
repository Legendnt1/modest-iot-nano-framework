#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for OverTheAirUpdateHandler (W28).
 *
 * Validates OTA update initialization, status tracking, progress reporting,
 * error handling, and callback registration.
 */

#include "Arduino.h"
#include "OverTheAirUpdateHandler.h"
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
    std::printf("Running OverTheAirUpdateHandler tests...\n");

    // =========================================================================
    // Test 1: OverTheAirUpdateHandler not initialized by default
    // =========================================================================
    {
        assert(OverTheAirUpdateHandler::isInitialized() == false);
        std::printf("  [PASS] OverTheAirUpdateHandler not initialized by default\n");
    }

    // =========================================================================
    // Test 2: begin() initializes OverTheAirUpdateHandler
    // =========================================================================
    {
        assert(OverTheAirUpdateHandler::begin("test-device") == true);
        assert(OverTheAirUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] begin() initializes OverTheAirUpdateHandler\n");
    }

    // =========================================================================
    // Test 3: Default status is Idle after initialization
    // =========================================================================
    {
        assert(OverTheAirUpdateHandler::getStatus() == OverTheAirUpdateStatus::Idle);
        std::printf("  [PASS] Default status is Idle\n");
    }

    // =========================================================================
    // Test 4: Default error code is 0
    // =========================================================================
    {
        assert(OverTheAirUpdateHandler::getLastError() == 0);
        std::printf("  [PASS] Default error code is 0\n");
    }

    // =========================================================================
    // Test 5: Progress percentage is 0.0 when not started
    // =========================================================================
    {
        assert(floatsEqual(OverTheAirUpdateHandler::getProgressInPercentage(), 0.0f));
        std::printf("  [PASS] Progress percentage is 0.0 when not started\n");
    }

    // =========================================================================
    // Test 5b: getProgressInPercentage() returns 0.0 before any update
    // =========================================================================
    {
        // After initialization, progress bytes are 0, so percentage should be 0.0
        // The formula is: (progressInNumberOfBytes / totalNumberOfBytes) * 100
        // When totalNumberOfBytes is 0, it returns 0.0 — verified by Test 5
        // This test confirms repeated calls are stable
        assert(floatsEqual(OverTheAirUpdateHandler::getProgressInPercentage(), 0.0f));
        assert(floatsEqual(OverTheAirUpdateHandler::getProgressInPercentage(), 0.0f));
        std::printf("  [PASS] getProgressInPercentage() is stable at 0.0\n");
    }

    // =========================================================================
    // Test 6: setPassword() is safe to call
    // =========================================================================
    {
        OverTheAirUpdateHandler::setPassword("secret123");
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

        OverTheAirUpdateHandler::onProgress([&progressCalled](uint32_t, uint32_t) {
            progressCalled = true;
        });

        OverTheAirUpdateHandler::onError([&errorCalled](int) {
            errorCalled = true;
        });

        OverTheAirUpdateHandler::onStart([&startCalled]() {
            startCalled = true;
        });

        OverTheAirUpdateHandler::onEnd([&endCalled]() {
            endCalled = true;
        });

        std::printf("  [PASS] Callback registration is safe\n");
    }

    // =========================================================================
    // Test 8: service() is safe to call when initialized
    // =========================================================================
    {
        OverTheAirUpdateHandler::service();
        std::printf("  [PASS] service() is safe to call when initialized\n");
    }

    // =========================================================================
    // Test 9: OverTheAirUpdateStatus enum values are correct
    // =========================================================================
    {
        assert(static_cast<uint8_t>(OverTheAirUpdateStatus::Idle) == 0);
        assert(static_cast<uint8_t>(OverTheAirUpdateStatus::Starting) == 1);
        assert(static_cast<uint8_t>(OverTheAirUpdateStatus::Running) == 2);
        assert(static_cast<uint8_t>(OverTheAirUpdateStatus::Finished) == 3);
        assert(static_cast<uint8_t>(OverTheAirUpdateStatus::Error) == 4);
        std::printf("  [PASS] OverTheAirUpdateStatus enum values are correct\n");
    }

    // =========================================================================
    // Test 10: begin() with default hostname
    // =========================================================================
    {
        // Reset initialized state for this test
        OverTheAirUpdateHandler::begin();
        assert(OverTheAirUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] begin() with default hostname works\n");
    }

    // =========================================================================
    // Test 11: service() is safe to call multiple times
    // =========================================================================
    {
        // Already initialized from previous tests, but service() should be safe
        OverTheAirUpdateHandler::service();
        OverTheAirUpdateHandler::service();
        OverTheAirUpdateHandler::service();
        std::printf("  [PASS] service() is safe to call multiple times\n");
    }

    // =========================================================================
    // Test 12: setPassword() with nullptr is safe
    // =========================================================================
    {
        OverTheAirUpdateHandler::setPassword(nullptr);
        std::printf("  [PASS] setPassword(nullptr) is safe\n");
    }

    // =========================================================================
    // Test 13: begin() with nullptr hostname is safe
    // =========================================================================
    {
        OverTheAirUpdateHandler::begin(nullptr);
        assert(OverTheAirUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] begin(nullptr) is safe\n");
    }

    // =========================================================================
    // Test 14: OtaProgressCallback type alias is compatible
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
    // Test 15: OtaErrorCallback type alias is compatible
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
    // Test 16: setPassword() before begin() is safe
    // =========================================================================
    {
        // Per KDoc, setPassword() should be called before begin()
        // This test verifies the recommended order works without crash
        OverTheAirUpdateHandler::setPassword("pre-begin-password");
        OverTheAirUpdateHandler::begin("ordered-device");
        assert(OverTheAirUpdateHandler::isInitialized() == true);
        std::printf("  [PASS] setPassword() before begin() is safe\n");
    }

    // =========================================================================
    // Test 17: Re-initialization with begin() is safe
    // =========================================================================
    {
        // Calling begin() again after already initialized should be safe
        assert(OverTheAirUpdateHandler::isInitialized() == true);
        OverTheAirUpdateHandler::begin("re-init-device");
        assert(OverTheAirUpdateHandler::isInitialized() == true);
        assert(OverTheAirUpdateHandler::getStatus() == OverTheAirUpdateStatus::Idle);
        std::printf("  [PASS] Re-initialization with begin() is safe\n");
    }

    // =========================================================================
    // Test 18: Status and error are reset after re-initialization
    // =========================================================================
    {
        // After begin(), status should be Idle and error should be 0
        assert(OverTheAirUpdateHandler::getStatus() == OverTheAirUpdateStatus::Idle);
        assert(OverTheAirUpdateHandler::getLastError() == 0);
        std::printf("  [PASS] Status and error are reset after re-initialization\n");
    }

    std::printf("All OverTheAirUpdateHandler tests passed!\n");
    return 0;
}
