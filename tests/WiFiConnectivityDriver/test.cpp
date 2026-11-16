#include <time.h>
/**
 * @file test.cpp
 * @brief Host unit tests for the WiFiConnectivityDriver component.
 *
 * Validates connection management, reconnection intervals, and network state.
 */

#include "Arduino.h"
#include "WiFi.h"
#include "WiFiConnectivityDriver.h"
#include <cassert>
#include <cstdio>

int main() {
    std::printf("Running WiFiConnectivityDriver tests...\n");

    // Reset mock state
    WiFiClass::resetMockState();

    // Test 1: Constructor initializes with correct credentials
    {
        WiFiConnectivityDriver driver("TestSSID", "TestPassword");
        assert(WiFiClass::beginCallCount == 0); // connect() not called yet
        assert(WiFiClass::lastBeginSsid == "");
        std::printf("  [PASS] Constructor initializes without connecting\n");
    }

    // Test 2: connect() initiates WiFi connection
    {
        WiFiConnectivityDriver driver("MySSID", "MyPassword");
        driver.connect();
        assert(WiFiClass::beginCallCount == 1);
        assert(WiFiClass::lastBeginSsid == "MySSID");
        assert(WiFiClass::lastBeginPassword == "MyPassword");
        std::printf("  [PASS] connect() initiates WiFi connection\n");
    }

    // Test 3: isConnected() returns current status
    {
        WiFi.setStatus(WL_DISCONNECTED);
        WiFiConnectivityDriver driver("SSID", "PASS");
        assert(driver.isConnected() == false);

        WiFi.setStatus(WL_CONNECTED);
        assert(driver.isConnected() == true);
        std::printf("  [PASS] isConnected() returns current status\n");
    }

    // Test 4: canTransmit() delegates to isConnected()
    {
        WiFi.setStatus(WL_DISCONNECTED);
        WiFiConnectivityDriver driver("SSID", "PASS");
        assert(driver.canTransmit() == false);

        WiFi.setStatus(WL_CONNECTED);
        assert(driver.canTransmit() == true);
        std::printf("  [PASS] canTransmit() delegates to isConnected()\n");
    }

    // Test 5: transmit() always returns false
    {
        WiFiConnectivityDriver driver("SSID", "PASS");
        assert(driver.transmit("topic", "data") == false);
        std::printf("  [PASS] transmit() always returns false\n");
    }

    // Test 6: Reconnect interval management
    {
        WiFiConnectivityDriver driver("SSID", "PASS");
        assert(driver.getReconnectInterval() == 10000); // Default
        driver.setReconnectInterval(5000);
        assert(driver.getReconnectInterval() == 5000);
        driver.setReconnectInterval(30000);
        assert(driver.getReconnectInterval() == 30000);
        std::printf("  [PASS] Reconnect interval management\n");
    }

    // Test 7: getWiFiStatus() returns raw status
    {
        WiFiConnectivityDriver driver("SSID", "PASS");
        WiFi.setStatus(WL_NO_SSID_AVAIL);
        assert(driver.getWiFiStatus() == WL_NO_SSID_AVAIL);

        WiFi.setStatus(WL_CONNECTED);
        assert(driver.getWiFiStatus() == WL_CONNECTED);
        std::printf("  [PASS] getWiFiStatus() returns raw status\n");
    }

    // Test 8: disconnect() clears connection
    {
        WiFi.setStatus(WL_CONNECTED);
        WiFiConnectivityDriver driver("SSID", "PASS");
        driver.disconnect();
        assert(WiFi.status() == WL_DISCONNECTED);
        std::printf("  [PASS] disconnect() clears connection\n");
    }

    // Test 9: getLocalIpAddress() returns IP
    {
        WiFiConnectivityDriver driver("SSID", "PASS");
        WiFi.setLocalIP(IPAddress(192, 168, 1, 100));
        IPAddress ip = driver.getLocalIpAddress();
        assert(ip.isSet() == true);
        std::printf("  [PASS] getLocalIpAddress() returns IP\n");
    }

    // Test 10: getSignalStrengthInDbm() returns RSSI
    {
        WiFiConnectivityDriver driver("SSID", "PASS");
        WiFi.setSignalStrength(-65);
        assert(driver.getSignalStrengthInDbm() == -65);
        std::printf("  [PASS] getSignalStrengthInDbm() returns RSSI\n");
    }

    // Test 11: connect() respects reconnect interval
    {
        fakeMillis = 10000;
        WiFi.setStatus(WL_DISCONNECTED);
        WiFiClass::resetMockState();
        WiFiConnectivityDriver driver("SSID", "PASS");
        driver.setReconnectInterval(5000);

        driver.connect();
        int afterFirstConnect = WiFiClass::beginCallCount;
        assert(afterFirstConnect >= 1);

        fakeMillis = 12000; // Within interval (10000 + 2000)
        driver.connect();
        assert(WiFiClass::beginCallCount == afterFirstConnect); // Not retried

        fakeMillis = 16000; // Past interval (10000 + 6000)
        driver.connect();
        assert(WiFiClass::beginCallCount == afterFirstConnect + 1); // Retried
        std::printf("  [PASS] connect() respects reconnect interval\n");
    }

    std::printf("All WiFiConnectivityDriver tests passed successfully!\n");
    return 0;
}
