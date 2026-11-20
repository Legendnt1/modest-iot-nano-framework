#!/usr/bin/env bash
set -euo pipefail

# Modest IoT Nano Framework - Code Coverage Runner
# Compiles and runs all host tests with gcov coverage instrumentation,
# then generates an HTML coverage report using lcov/genhtml.
#
# Usage:
#   ./tests/run_coverage.sh              # Generate report in build/coverage/
#   MIN_COVERAGE=70 ./tests/run_coverage.sh  # Set minimum threshold (default: 80)
#
# Requirements:
#   - g++ (GCC, not Clang, for gcov support)
#   - lcov package (provides genhtml)
#   - Run on Linux (gcov behavior differs on macOS)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

CXX="${CXX:-g++}"
MIN_COVERAGE="${MIN_COVERAGE:-80}"

# Check for GCC (gcov does not work with Clang)
if ! ${CXX} --version | grep -qi "gcc\|GNU"; then
    echo "ERROR: gcov requires GCC. Detected: ${CXX}"
    echo "Set CXX=g++ to use GCC."
    exit 1
fi

# Check for genhtml (from lcov package)
if ! command -v genhtml &>/dev/null; then
    echo "ERROR: genhtml not found. Install lcov package:"
    echo "  Ubuntu: sudo apt-get install -y lcov"
    echo "  macOS:  brew install lcov"
    exit 1
fi

# Coverage-specific flags: instrument code, suppress line numbers in paths for cleaner reports
COVERAGE_FLAGS="-fprofile-arcs -ftest-coverage --coverage"

# Base flags without sanitizers (sanitizers can interfere with gcov)
BASE_FLAGS="-std=c++17 -Wall -Wextra -Werror -Itests/fakes -Isrc ${COVERAGE_FLAGS}"

# Build directory for coverage artifacts
BUILD_DIR="$(mktemp -d /tmp/modest_iot_coverage.XXXXXX)"
COVERAGE_DIR="${PROJECT_ROOT}/build/coverage"
rm -rf "${COVERAGE_DIR}"
mkdir -p "${COVERAGE_DIR}"

cleanup() {
    rm -rf "${BUILD_DIR}"
}
trap cleanup EXIT

echo "=========================================================="
echo " Modest IoT Code Coverage Report"
echo " Compiler: ${CXX}"
echo " Min coverage threshold: ${MIN_COVERAGE}%"
echo " Report directory: ${COVERAGE_DIR}"
echo "=========================================================="

PASSED=0
FAILED=0

run_test() {
    local test_name="$1"
    local binary="${BUILD_DIR}/${test_name}"
    shift
    local sources=("$@")

    echo -n "==> [BUILD] ${test_name} ... "
    if ${CXX} ${BASE_FLAGS} "${sources[@]}" -o "${binary}" --coverage; then
        echo "OK"
    else
        echo "FAILED (compilation)"
        FAILED=$((FAILED + 1))
        return 1
    fi

    echo -n "==> [RUN]   ${test_name} ... "
    if "${binary}"; then
        echo "PASS"
        PASSED=$((PASSED + 1))
    else
        echo "FAIL"
        FAILED=$((FAILED + 1))
        return 1
    fi
}

# ============================================================================
# All test suites (same order as run_all.sh)
# ============================================================================

# 1. Digital Light Sensor Suite
run_test "test_DigitalLightIntensitySensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/DigitalLightIntensitySensor.cpp \
    tests/DigitalLightIntensitySensor/test.cpp

# 2. RGB LED Actuator Suite
run_test "test_RgbLed" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/RgbLed.cpp \
    tests/RgbLed/test.cpp

# 3. RFID Sensor Suite
run_test "test_RfidSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/RfidSensor.cpp \
    tests/RfidSensor/test.cpp

# 4. BLE Connectivity Driver Suite
run_test "test_BleConnectivityDriver" \
    -DTEST_MOCK_BLE \
    tests/fakes/ArduinoFake.cpp \
    src/BleConnectivityDriver.cpp \
    tests/BleConnectivityDriver/test.cpp

# 5. Logging and MIOT_LOG Aliases Suite
run_test "test_ModestLog" \
    tests/ModestLog/test.cpp

# 6. MQTT Gateway and WiFi Reconnection Suite
run_test "test_MqttGatewayClient" \
    tests/fakes/ArduinoFake.cpp \
    src/MqttGatewayClient.cpp \
    src/WiFiConnectivityDriver.cpp \
    src/Device.cpp \
    src/SamplingScheduler.cpp \
    src/Sensor.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    src/DiagnosticsTelemetryPackage.cpp \
    tests/MqttGatewayClient/test.cpp

# 7. EngineConfiguration and Device Configuration Suite
run_test "test_EngineConfiguration" \
    tests/fakes/ArduinoFake.cpp \
    tests/EngineConfiguration/test.cpp

# 8. Diagnostics Snapshot and Shared Counters Suite
run_test "test_DiagnosticsSnapshot" \
    -DTEST_MOCK_DEVICE \
    tests/fakes/ArduinoFake.cpp \
    tests/DiagnosticsSnapshot/test.cpp

# 9. Sensor Fault and Recovery Events Suite
run_test "test_SensorFault" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    tests/SensorFault/test.cpp

# 10. SamplingScheduler Suite
run_test "test_SamplingScheduler" \
    tests/fakes/ArduinoFake.cpp \
    src/SamplingScheduler.cpp \
    tests/SamplingScheduler/test.cpp

# 11. Telemetry Serialization Golden Tests Suite
run_test "test_TelemetryPackage" \
    tests/fakes/ArduinoFake.cpp \
    src/Device.cpp \
    src/SamplingScheduler.cpp \
    src/Sensor.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    src/DiagnosticsTelemetryPackage.cpp \
    tests/TelemetryPackage/test.cpp

# 11b. JsonParser Suite
run_test "test_JsonParser" \
    src/JsonParser.cpp \
    tests/JsonParser/test.cpp

# 12. Triggering Scheduler Suite
run_test "test_TriggeringScheduler" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/Device.cpp \
    src/SamplingScheduler.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    src/DiagnosticsTelemetryPackage.cpp \
    tests/TriggeringScheduler/test.cpp

# 13. Command Marshaling Suite
run_test "test_CommandMarshaling" \
    tests/fakes/ArduinoFake.cpp \
    src/Device.cpp \
    src/SamplingScheduler.cpp \
    src/Sensor.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    src/DiagnosticsTelemetryPackage.cpp \
    tests/CommandMarshaling/test.cpp

# 14. WaterFlowSensor Suite
run_test "test_WaterFlowSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/WaterFlowSensor.cpp \
    tests/WaterFlowSensor/test.cpp

# 15. SoilMoistureSensor Suite
run_test "test_SoilMoistureSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/SoilMoistureSensor.cpp \
    tests/SoilMoistureSensor/test.cpp

# 16. RelayActuator Suite
run_test "test_RelayActuator" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/RelayActuator.cpp \
    tests/RelayActuator/test.cpp

# 17. DeepSleep Suite
run_test "test_DeepSleep" \
    tests/fakes/ArduinoFake.cpp \
    src/Device.cpp \
    src/SamplingScheduler.cpp \
    src/Sensor.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    src/DiagnosticsTelemetryPackage.cpp \
    tests/DeepSleep/test.cpp

# 18. ServoMotor Suite
run_test "test_ServoMotor" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/ServoMotor.cpp \
    tests/ServoMotor/test.cpp

# 19. CharacterLcdDisplay Suite
run_test "test_CharacterLcdDisplay" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/CharacterLcdDisplay.cpp \
    src/Hd44780Driver.cpp \
    tests/CharacterLcdDisplay/test.cpp

# 20. OledDisplay Suite
run_test "test_OledDisplay" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/OledDisplay.cpp \
    src/Ssd1306Driver.cpp \
    tests/OledDisplay/test.cpp

# 21. PthSensor Suite
run_test "test_PthSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/BmeEnvironmentalSensor.cpp \
    src/PthSensor.cpp \
    tests/PthSensor/test.cpp

# 22. WaterLeakSensor Suite
run_test "test_WaterLeakSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/WaterLeakSensor.cpp \
    tests/WaterLeakSensor/test.cpp

# 23. DoorWindowSensor Suite
run_test "test_DoorWindowSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/DoorWindowSensor.cpp \
    tests/DoorWindowSensor/test.cpp

# 24. OneWireTemperatureSensor Suite
run_test "test_OneWireTemperatureSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/OneWireTemperatureSensor.cpp \
    tests/OneWireTemperatureSensor/test.cpp

# 25. DhtSensor Suite
run_test "test_DhtSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/DhtSensor.cpp \
    tests/DhtSensor/test.cpp

# 26. EnvironmentalGasSensor Suite
run_test "test_EnvironmentalGasSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/BmeEnvironmentalSensor.cpp \
    src/EnvironmentalGasSensor.cpp \
    tests/EnvironmentalGasSensor/test.cpp

# 27. BleGatewayClient Suite
run_test "test_BleGatewayClient" \
    -DTEST_MOCK_BLE \
    tests/fakes/ArduinoFake.cpp \
    src/BleGatewayClient.cpp \
    src/BleConnectivityDriver.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    tests/BleGatewayClient/test.cpp

# 28. HttpGatewayClient Suite
run_test "test_HttpGatewayClient" \
    tests/fakes/ArduinoFake.cpp \
    src/HttpGatewayClient.cpp \
    src/WiFiConnectivityDriver.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    tests/HttpGatewayClient/test.cpp

# 29. Buzzer Suite
run_test "test_Buzzer" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/Buzzer.cpp \
    tests/Buzzer/test.cpp

# 30. ConfigurationStore Suite
run_test "test_ConfigurationStore" \
    tests/fakes/ArduinoFake.cpp \
    src/ConfigurationStore.cpp \
    tests/ConfigurationStore/test.cpp

# 31. TimeSynchronizationDriver Suite
run_test "test_TimeSynchronizationDriver" \
    tests/fakes/ArduinoFake.cpp \
    src/TimeSynchronizationDriver.cpp \
    tests/TimeSynchronizationDriver/test.cpp

# 32. GasSensor Suite
run_test "test_GasSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/GasSensor.cpp \
    tests/GasSensor/test.cpp

# 33. Button Suite
run_test "test_Button" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/Button.cpp \
    tests/Button/test.cpp

# 34. PowerManager Suite
run_test "test_PowerManager" \
    tests/fakes/ArduinoFake.cpp \
    src/PowerManager.cpp \
    tests/PowerManager/test.cpp

# 35. TelemetryBuffer Suite
run_test "test_TelemetryBuffer" \
    tests/fakes/ArduinoFake.cpp \
    src/TelemetryBuffer.cpp \
    tests/TelemetryBuffer/test.cpp

# 36. OtaUpdateHandler Suite
run_test "test_OtaUpdateHandler" \
    tests/fakes/ArduinoFake.cpp \
    src/OtaUpdateHandler.cpp \
    tests/OtaUpdateHandler/test.cpp

# 37. Led Suite
run_test "test_Led" \
    tests/fakes/ArduinoFake.cpp \
    src/Actuator.cpp \
    src/Led.cpp \
    tests/Led/test.cpp

# 38. PassiveInfraredSensor Suite
run_test "test_PassiveInfraredSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/PassiveInfraredSensor.cpp \
    tests/PassiveInfraredSensor/test.cpp

# 39. PulseSensor Suite
run_test "test_PulseSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/PulseSensor.cpp \
    tests/PulseSensor/test.cpp

# 40. Hd44780Driver Suite
run_test "test_Hd44780Driver" \
    tests/fakes/ArduinoFake.cpp \
    src/Hd44780Driver.cpp \
    tests/Hd44780Driver/test.cpp

# 41. Ssd1306Driver Suite
run_test "test_Ssd1306Driver" \
    tests/fakes/ArduinoFake.cpp \
    src/Ssd1306Driver.cpp \
    tests/Ssd1306Driver/test.cpp

# 42. WiFiConnectivityDriver Suite
run_test "test_WiFiConnectivityDriver" \
    tests/fakes/ArduinoFake.cpp \
    src/WiFiConnectivityDriver.cpp \
    tests/WiFiConnectivityDriver/test.cpp

# 43. BmeEnvironmentalSensor Suite
run_test "test_BmeEnvironmentalSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/BmeEnvironmentalSensor.cpp \
    src/PthSensor.cpp \
    tests/BmeEnvironmentalSensor/test.cpp

# 44. UltrasonicSensor Suite
run_test "test_UltrasonicSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/UltrasonicSensor.cpp \
    tests/UltrasonicSensor/test.cpp

# 45. LoadCellAmplifier Suite
run_test "test_LoadCellAmplifier" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/LoadCellAmplifier.cpp \
    tests/LoadCellAmplifier/test.cpp

# 46. GeolocationSensor Suite
run_test "test_GeolocationSensor" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/GeolocationSensor.cpp \
    tests/GeolocationSensor/test.cpp

# 47. Device Suite
run_test "test_Device" \
    tests/fakes/ArduinoFake.cpp \
    src/Sensor.cpp \
    src/Actuator.cpp \
    src/Device.cpp \
    src/SamplingScheduler.cpp \
    src/JsonTelemetryWriter.cpp \
    src/JsonParser.cpp \
    src/DiagnosticsTelemetryPackage.cpp \
    tests/Device/test.cpp

# 48. EventAggregator Suite
run_test "test_EventAggregator" \
    tests/fakes/ArduinoFake.cpp \
    src/EventAggregator.cpp \
    src/TimeSynchronizationDriver.cpp \
    tests/EventAggregator/test.cpp

echo "=========================================================="
echo " Test Summary: ${PASSED} passed, ${FAILED} failed"
echo "=========================================================="

if [[ ${FAILED} -ne 0 ]]; then
    echo "ERROR: Some tests failed. Cannot generate meaningful coverage report."
    exit 1
fi

# ============================================================================
# Collect coverage data and generate report
# ============================================================================

echo ""
echo "=========================================================="
echo " Generating Coverage Report"
echo "=========================================================="

# Collect all .gcda files from the build directory and current directory
# (gcda files are created next to the corresponding .o files during test execution)
find "${BUILD_DIR}" -name "*.gcda" -type f -print0 | xargs -0 -r gcov -b 2>/dev/null || true

# Find all .gcda files (they may be in the build dir or project root)
GCDA_FILES=$(find "${BUILD_DIR}" "${PROJECT_ROOT}/src" -name "*.gcda" -type f 2>/dev/null || true)

if [[ -z "${GCDA_FILES}" ]]; then
    echo "WARNING: No .gcda files found. Coverage data may not be available."
    echo "This can happen if tests were run in a different environment."
    exit 0
fi

# Generate HTML report using genhtml
# --prefix ensures clean paths in the report
# --highlight enables line-by-line highlighting
# --sort-name sorts by filename
# --no-function-coverage skips function coverage (we care about lines)
echo "Generating HTML report in ${COVERAGE_DIR}..."
genhtml \
    --output-directory "${COVERAGE_DIR}" \
    --highlight \
    --sort-name \
    --prefix "${PROJECT_ROOT}/" \
    --title "Modest IoT Nano Framework - Code Coverage" \
    --no-function-coverage \
    --demangle-cpp \
    ${GCDA_FILES}

echo ""
echo "=========================================================="
echo " Coverage Report Generated"
echo "=========================================================="
echo " HTML report: ${COVERAGE_DIR}/index.html"
echo ""

# Extract overall coverage percentage from genhtml summary
if [[ -f "${COVERAGE_DIR}/coverage.summary" ]]; then
    echo "Coverage Summary:"
    grep -E "lines?" "${COVERAGE_DIR}/coverage.summary" | head -5 || true
    echo ""

    # Extract the overall line coverage percentage
    overall_coverage=$(grep "^lines" "${COVERAGE_DIR}/coverage.summary" | head -1 | awk '{print $2}' | tr -d '%')

    if [[ -n "${overall_coverage}" ]]; then
        echo "Overall line coverage: ${overall_coverage}%"
        echo "Minimum threshold:     ${MIN_COVERAGE}%"

        # Check against threshold
        if (( $(echo "${overall_coverage} < ${MIN_COVERAGE}" | bc -l 2>/dev/null || echo "0") )); then
            echo ""
            echo "WARNING: Coverage (${overall_coverage}%) is below the minimum threshold (${MIN_COVERAGE}%)."
            echo "The build will fail if running in CI with ENFORCE_COVERAGE=1."

            if [[ "${ENFORCE_COVERAGE:-0}" == "1" ]]; then
                echo "ERROR: Coverage threshold not met. Failing build."
                exit 1
            fi
        else
            echo "✓ Coverage threshold met."
        fi
    fi
else
    echo "Note: coverage.summary not found. Check the HTML report for details."
fi

echo ""
echo "To view the report: open ${COVERAGE_DIR}/index.html in a browser."
echo "=========================================================="

# Clean up .gcda and .gcno files from project directory
find "${PROJECT_ROOT}/src" -name "*.gcda" -delete 2>/dev/null || true
find "${PROJECT_ROOT}/src" -name "*.gcno" -delete 2>/dev/null || true

exit 0
