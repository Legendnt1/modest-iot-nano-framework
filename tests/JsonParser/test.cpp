#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "JsonParser.h"

static void assert_true(bool condition, const char* message) {
    if (!condition) {
        std::printf("[FAIL] %s\n", message);
        exit(1);
    }
}

static void assert_false(bool condition, const char* message) {
    if (condition) {
        std::printf("[FAIL] %s\n", message);
        exit(1);
    }
}

static void assert_equal_int(int expected, int actual, const char* message) {
    if (expected != actual) {
        std::printf("[FAIL] %s: expected %d, got %d\n", message, expected, actual);
        exit(1);
    }
}

static void assert_equal_float(float expected, float actual, const char* message) {
    const float diff = (expected > actual) ? (expected - actual) : (actual - expected);
    if (diff > 0.01f) {
        std::printf("[FAIL] %s: expected %.2f, got %.2f\n", message, expected, actual);
        exit(1);
    }
}

int main() {
    // Test 1: parseInt - basic
    {
        const char* json = "{\"commandId\": 42, \"payload\": 100}";
        int value;
        assert_true(JsonParser::parseInt(json, "commandId", &value), "parseInt should find commandId");
        assert_equal_int(42, value, "parseInt should return 42");
    }

    // Test 2: parseInt - second field
    {
        const char* json = "{\"commandId\": 42, \"payload\": 100}";
        int value;
        assert_true(JsonParser::parseInt(json, "payload", &value), "parseInt should find payload");
        assert_equal_int(100, value, "parseInt should return 100");
    }

    // Test 3: parseInt - missing key
    {
        const char* json = "{\"commandId\": 42}";
        int value;
        assert_false(JsonParser::parseInt(json, "missing", &value), "parseInt should fail for missing key");
    }

    // Test 4: parseInt - null inputs
    {
        int value;
        assert_false(JsonParser::parseInt(nullptr, "key", &value), "parseInt should fail with null json");
        assert_false(JsonParser::parseInt("{\"a\":1}", nullptr, &value), "parseInt should fail with null key");
        assert_false(JsonParser::parseInt("{\"a\":1}", "a", nullptr), "parseInt should fail with null outValue");
    }

    // Test 5: parseInt32
    {
        const char* json = "{\"value\": 2147483647}";
        int32_t value;
        assert_true(JsonParser::parseInt32(json, "value", &value), "parseInt32 should find value");
        assert_equal_int(2147483647, static_cast<int>(value), "parseInt32 should return max int32");
    }

    // Test 6: parseInt32 - null outValue
    {
        assert_false(JsonParser::parseInt32("{\"a\":1}", "a", nullptr), "parseInt32 should fail with null outValue");
    }

    // Test 7: parseFloat
    {
        const char* json = "{\"temperature\": 23.5, \"humidity\": 60.2}";
        float value;
        assert_true(JsonParser::parseFloat(json, "temperature", &value), "parseFloat should find temperature");
        assert_equal_float(23.5f, value, "parseFloat should return 23.5");
    }

    // Test 8: parseFloat - missing key
    {
        const char* json = "{\"temperature\": 23.5}";
        float value;
        assert_false(JsonParser::parseFloat(json, "missing", &value), "parseFloat should fail for missing key");
    }

    // Test 9: parseFloat - null inputs
    {
        float value;
        assert_false(JsonParser::parseFloat(nullptr, "key", &value), "parseFloat should fail with null json");
    }

    // Test 10: parseBool - true
    {
        const char* json = "{\"enabled\": true}";
        bool value;
        assert_true(JsonParser::parseBool(json, "enabled", &value), "parseBool should find enabled");
        assert_true(value, "parseBool should return true");
    }

    // Test 11: parseBool - false
    {
        const char* json = "{\"enabled\": false}";
        bool value;
        assert_true(JsonParser::parseBool(json, "enabled", &value), "parseBool should find enabled");
        assert_false(value, "parseBool should return false");
    }

    // Test 12: parseBool - integer 1
    {
        const char* json = "{\"enabled\": 1}";
        bool value;
        assert_true(JsonParser::parseBool(json, "enabled", &value), "parseBool should parse integer 1");
        assert_true(value, "parseBool should return true for 1");
    }

    // Test 13: parseBool - integer 0
    {
        const char* json = "{\"enabled\": 0}";
        bool value;
        assert_true(JsonParser::parseBool(json, "enabled", &value), "parseBool should parse integer 0");
        assert_false(value, "parseBool should return false for 0");
    }

    // Test 14: parseBool - null inputs
    {
        bool value;
        assert_false(JsonParser::parseBool(nullptr, "key", &value), "parseBool should fail with null json");
    }

    // Test 15: parseString
    {
        const char* json = "{\"name\": \"sensor_01\", \"status\": \"active\"}";
        char buffer[32];
        assert_true(JsonParser::parseString(json, "name", buffer, sizeof(buffer)), "parseString should find name");
        assert_true(strcmp(buffer, "sensor_01") == 0, "parseString should return sensor_01");
    }

    // Test 16: parseString - second field
    {
        const char* json = "{\"name\": \"sensor_01\", \"status\": \"active\"}";
        char buffer[32];
        assert_true(JsonParser::parseString(json, "status", buffer, sizeof(buffer)), "parseString should find status");
        assert_true(strcmp(buffer, "active") == 0, "parseString should return active");
    }

    // Test 17: parseString - missing key
    {
        const char* json = "{\"name\": \"sensor_01\"}";
        char buffer[32];
        assert_false(JsonParser::parseString(json, "missing", buffer, sizeof(buffer)), "parseString should fail for missing key");
    }

    // Test 18: parseString - null inputs
    {
        char buffer[32];
        assert_false(JsonParser::parseString(nullptr, "key", buffer, sizeof(buffer)), "parseString should fail with null json");
        assert_false(JsonParser::parseString("{\"a\":\"b\"}", "a", nullptr, 32), "parseString should fail with null buffer");
        assert_false(JsonParser::parseString("{\"a\":\"b\"}", "a", buffer, 0), "parseString should fail with zero buffer size");
    }

    // Test 19: parseString - buffer overflow protection
    {
        const char* json = "{\"name\": \"very_long_string_that_exceeds_buffer\"}";
        char buffer[8];
        assert_true(JsonParser::parseString(json, "name", buffer, sizeof(buffer)), "parseString should handle small buffer");
        assert_true(buffer[7] == '\0', "parseString should null-terminate within buffer");
    }

    // Test 20: Complex JSON (MQTT command format)
    {
        const char* json = "{\"commandId\": 1, \"commandPayload\": 42}";
        int cmdId;
        int32_t payload;
        assert_true(JsonParser::parseInt(json, "commandId", &cmdId), "parseInt should find commandId");
        assert_equal_int(1, cmdId, "commandId should be 1");
        assert_true(JsonParser::parseInt32(json, "commandPayload", &payload), "parseInt32 should find commandPayload");
        assert_equal_int(42, static_cast<int>(payload), "commandPayload should be 42");
    }

    std::printf("All JsonParser tests passed successfully!\n");
    return 0;
}
