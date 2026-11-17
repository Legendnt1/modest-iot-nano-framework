# Modest IoT Improvements

Active work items, comparison tables, and future work for the Modest IoT Nano Framework.

## Document Map

| Information | Document |
|-------------|----------|
| Quick start & components | [README.md](../README.md) |
| Version history | [CHANGELOG.md](../CHANGELOG.md) |
| Architecture decisions | [adrs.md](adrs.md) |
| User stories | [user-stories.md](user-stories.md) |
| Dual-framework vision | [vision.md](vision.md) |
| Class diagram | [class-diagram.puml](class-diagram.puml) |
| Completed work items W1–W28 | [evolution.md](evolution.md) |
| Doxygen API reference | [doxygen-mainpage.md](doxygen-mainpage.md) |

## Completed Work Items

Work items W1–W28 are complete and released. See [evolution.md](evolution.md) for full implementation details.

## Status Overview

| Tier                                    | Status       |
|-----------------------------------------|--------------|
| 0: Make it correct and shippable        | **Complete** |
| 1: Safety net and visibility            | **Complete** |
| 2: Reliability, diagnostics & footprint | **Complete** |
| 3: Core performance and DX              | **Complete** |
| 4: Capabilities                         | **Complete** |
| 5: Optimization and power               | **Complete** |
| Pre-Release Prerequisites (W20–W24)     | **Complete** |
| v1.1.0 Features (W25–W28)              | **Complete** |

## Active Work Items

### W29: Event aggregation & rules engine — **Deferred** (v1.2.0)

**Score**: 4 | **Cost**: L | **Target**: v1.2.0

`EventAggregator` for composite event patterns (e.g., "motion AND door opened"). Application-layer feature; can be implemented by users using the event handler pattern.

### W30: Multi-device coordination — **Deferred** (v1.3.0)

**Score**: 3 | **Cost**: L | **Target**: v1.3.0

ESP-NOW or MQTT-based device-to-device messaging for relay nodes and distributed sensing.

### W18: Telemetry pool and writer interface — **Deferred**

**Score**: 2 | **Cost**: L

Packet pool with placement-new and `TelemetryWriter` format abstraction. Requires heap measurements to justify the architectural investment. Full details in [evolution.md](evolution.md#tier-5-optimization-and-power-complete).

## Deferred to Future Versions

- **Event aggregation & rules engine** (W29, v1.2.0): Application-layer feature; can be implemented by users using the event handler pattern.
- **Multi-device coordination** (W30, v1.3.0): Niche use case; ESP-NOW or MQTT-based approach TBD based on community demand.
- **On-device ML inference** (Tier 5, v1.3.0+): `AiMicroModelController` is opt-in and requires ML runtime dependencies. Deferred until core stability is proven.

## Deliberately out of scope

- Replacing ArduinoJson (no viable embedded alternative).
- TLS/SSL implementation in the framework (use ESP32's built-in `WiFiClientSecure`; W22 examples are complete).
- Targeted (per-instance) commands until a use case appears.
- Switchable sensor power rails and ADC/DMA sampling.
- Non-ESP32 targets. The ESP32 family (WROOM, S3, C3, C6, H2) covers the framework's target use cases: Wi-Fi + BLE + FreeRTOS + deep sleep + ample RAM/Flash. A hardware abstraction layer would require ~2,900 lines of new code for uncertain community demand. Re-evaluate if community demand emerges; STM32WB series is the most viable non-ESP32 target.

### Microcontroller Coverage Expansion

**Status**: Deferred — ESP32-only remains the strategic choice.

**Rationale**: The ESP32 family provides sufficient variety across price points and capabilities. Edge Framework dual-framework vision provides more value than multi-platform Nano support.

**Re-evaluation trigger**: If community demand for non-ESP32 support emerges (e.g., GitHub issues, forum requests), re-evaluate via a phased approach. STM32WB series (Wi-Fi + BLE + FreeRTOS) is the most viable non-ESP32 target. RP2040 requires external Wi-Fi/BLE modules. ESP8266 lacks BLE and has tight RAM/Flash constraints.

---

## Comparison with traditional development

Compares three points: a traditional Arduino `loop()` app, Modest IoT v1.1.1 (current release), and Modest IoT v1.2.0 (planned).

> **Note**: Comparison tables are replaced with each release — past comparisons are not archived. When v1.2.0 ships, this table is updated to compare v1.2.0 vs v1.3.0.

### Developer experience

| Aspect          | Traditional `loop()`                         | Modest IoT v1.1.1 (current)                                                 | Modest IoT v1.2.0 (planned)                                 |
|-----------------|----------------------------------------------|-------------------------------------------------------------------------------|----------------------------------------------------------------------|
| First build     | Works with the board package                 | All 23 examples compile in CI; 2 dependencies                                 | Same                                                                |
| Structure       | One growing `loop()` with `millis()` flags   | Sensors emit events, a `Device` mediates, actuators take commands             | Same, with event aggregation for composite patterns (W29)    |
| Adding a sensor | New globals, new timing variable, new branch | Subclass or reuse, register in the scheduler; per-sensor default periods; `PowerManager` for battery-aware sensors | Same, with event aggregation for composite patterns (W29)                  |
| Sampling rates  | Hand-written per-sensor timers               | Per-slot periods, minimums enforced, staggering handled                       | Same, with event aggregation for composite patterns (W29)                |
| Timing mistakes | Easy: a blocking call stalls everything      | Handler budgets documented; blocking HX711/ultrasonic removed; interrupt-driven sensors; `PowerManager` for configurable sleep modes | Same, with event aggregation for composite patterns (W29)          |
| Debugging       | `Serial.println` scattered                   | `MODEST_LOG_*`, stack high-water marks, sensor fault events, diagnostics, `firmwareVersion` in telemetry for fleet tracking | Same, with event aggregation for composite patterns (W29)         |
| Configuration   | Hardcoded credentials and intervals          | `ConfigurationStore` with ESP32 NVS, namespace isolation, type-safe access, read-only mode    | Same                                                               |
| Time handling   | `millis()` from boot                         | `TimeSynchronizationDriver` with NTP over Wi-Fi and RTC fallback for absolute time             | Same, with synchronized timestamps across devices (W30)                   |
| Networking      | Blocking connect/publish in `loop()`         | Separate network task, reconnect policy, keep-alive, TLS examples, `createSecureSocket()`, TLS/SSL as documented best practice     | Same, with event aggregation for composite patterns (W29)                      |
| Cloud-to-device | Hand-rolled callback                         | `Device::enqueueCommand` with `int32_t` payload for parameterized commands                   | Same, with event aggregation for composite patterns (W29)   |
| Offline data    | Lost when network unavailable                | `TelemetryBuffer` with LittleFS persistence for offline replay                                      | Same, with event aggregation for composite patterns (W29)       |
| Updates         | Manual reflashing required                   | OTA updates via `OtaUpdateHandler` component                                                    | Same                                                     |
| Testing         | Rarely possible off-device                   | 47 host test suites with CI, sanitizers enabled                               | Same                                                |
| Documentation   | README or inline comments                    | 30 ADRs with rationale, user stories with ADR cross-references, PlantUML class diagram, Doxygen API reference, versioned CHANGELOG, and cross-linked doc ecosystem | Same                                                |

### Performance

| Aspect                     | Traditional `loop()`              | Modest IoT v1.1.1 (current)                             | Modest IoT v1.2.0 (planned)                              |
|----------------------------|-----------------------------------|-----------------------------------------------------------|--------------------------------------------------------------------|
| Timing model               | Cooperative polling; jitter grows | Timer ISR plus sampler task; per-slot periods; interrupt-driven sensors | Same, with event aggregation for composite patterns (W29) |
| Sample period accuracy     | Depends on `loop()` duration      | Per-slot periods independent of slot count                | Same |
| Worst-case stall           | Any blocking call                 | Interrupt-driven HX711/ultrasonic; documented budgets; `PowerManager` handles sleep/wake transitions     | Same, with event aggregation for composite patterns (W29)          |
| Edge latency (button, PIR) | Up to one `loop()` pass           | Interrupt mode: task wake-up latency                      | Same, with event aggregation for composite event patterns (W29)    |
| Network isolation          | A slow publish blocks sensing     | Isolated in the network task; bounded queue policy, `TelemetryBuffer` for offline replay        | Same, with event aggregation for composite patterns (W29)        |
| Heap behaviour             | Usually static                    | `unique_ptr` ownership, drop-oldest policy; LittleFS reduces heap pressure via `TelemetryBuffer` | Same; pool if measurement justifies it (W18)   |
| CPU when idle              | Spins in `loop()`                 | Sleeps between ticks; tickless and deep sleep supported; `PowerManager` handles sleep/wake transitions | Same, with event aggregation for composite patterns (W29) |
| Energy                     | High unless hand-tuned            | Deep sleep: ~10µA; light sleep via tickless mode; battery monitoring enables adaptive sleep strategies and optimized sleep modes via `PowerManager` | Same, with event aggregation for composite patterns (W29)               |
| Time synchronization      | None; boot-relative only          | NTP-synchronized absolute time via `TimeSynchronizationDriver`; RTC fallback for deep-sleep devices | Same, with synchronized timestamps across devices (W30) |
| Secure communication      | Manual TLS setup if needed        | TLS examples provided; `createSecureSocket()` builder; `WiFiClientSecure` documented; TLS/SSL as standard practice   | Same, with event aggregation for composite patterns (W29)       |

### Resource consumption

| Resource                 | Traditional `loop()` | Modest IoT v1.1.1 (current)                                                               | Modest IoT v1.2.0 (planned)                           |
|--------------------------|----------------------|---------------------------------------------------------------------------------------------|----------------------------------------------------------------|
| Task stacks              | `loop()` task only   | 4096 B sampler + 8192 B network, configurable via `EngineConfiguration`; `PowerManager` adds minimal stack for sleep handling | Same defaults                                             |
| Queue and packets        | None                 | Queue of pointers + one heap packet per reading; bounded by queue policy; `TelemetryBuffer` adds LittleFS storage for offline replay | Same, with event aggregation for composite patterns (W29) |
| Flash                    | Smallest             | Framework, Wi-Fi, BLE driver; only 2 dependencies (ArduinoJson, MQTT); plus `OtaUpdateHandler`, `TelemetryBuffer` | Same, no new Flash increase for W29       |
| Static RAM               | Small                | Slot table, per-object state; trigger table (4 slots); ConfigurationStore NVS cache; `TelemetryBuffer` adds LittleFS buffer | Same                                             |
| Heap fragmentation       | Low                  | Lower with explicit ownership; `unique_ptr` transfer; LittleFS reduces heap pressure        | Same; pool in Tier 5 if measured (W18)                              |
| Third-party dependencies | As needed            | Only those used (ArduinoJson, MQTT) — ESP32Servo, LiquidCrystal_I2C, PubSubClient, DHT, Adafruit, and HX711 removed | Same; no new external dependencies for W29               |
| Logging overhead         | User-controlled      | Zero when compiled out; UART cost when on                                                  | Same                                                           |
| Persistent storage       | Manual SPIFFS/LittleFS | ConfigurationStore uses ESP32 NVS with namespace isolation; `TelemetryBuffer` uses LittleFS for offline replay | Same, with event aggregation for composite patterns (W29)  |
| Security overhead        | None by default      | TLS examples provided; `NetworkClientSecure` usage guided; no framework-level TLS enforcement                                  | Same, with event aggregation for composite patterns (W29) |

---

## How to validate the comparison

Run the same three reference applications on the same ESP32 board, once per variant (plain `loop()`, Modest IoT v1.1.0, Modest IoT v1.2.0):

1. **Multi-sensor monitor:** DHT, BH1750 and a button, with a display and Wi-Fi/MQTT telemetry.
2. **Blocking-sensor stress:** the monitor plus an HX711 and an ultrasonic sensor.
3. **Network fault:** the monitor with the broker unreachable for 60 seconds, then restored.

| Metric                                  | Method                                                                     |
|-----------------------------------------|----------------------------------------------------------------------------|
| Sample period jitter                    | GPIO toggle per sample on a logic analyzer, or `esp_timer_get_time` deltas |
| Button and PIR edge-to-action latency   | Logic analyzer: input edge to actuator output edge                         |
| Sensing continuity during network fault | Count missed samples while the broker is down                              |
| Reconnect time                          | Time from broker restore to first successful publish                       |
| Stack use                               | `uxTaskGetStackHighWaterMark` for each task                                |
| Heap                                    | `ESP.getFreeHeap()` and `getMinFreeHeap()` after 24 hours                  |
| Flash and static RAM                    | Size report from the build for each variant                                |
| Current draw                            | Inline current meter (Wokwi cannot measure it)                             |
| Lines of application code               | Count in the reference sketches                                            |

---

## Future Work

### Benchmark Table Publication

**Goal**: Publish a benchmark comparison table in the README with measurable evidence of the framework's performance claims.

**Requirements**:
- Include the ESP32 board model, Arduino core version, and framework commit hash for reproducibility.
- Capture measurements after each tier (Tier 0–5), not only at the end, to show the incremental impact of each tier.
- Use the same three reference applications defined in "How to validate the comparison" above.
- Record all metrics from the validation table (jitter, latency, stack use, heap, Flash, current draw, etc.).

**Target**: Complete after v1.1.1 release when hardware testing infrastructure is available.

**Status**: Not started.

### PlatformIO Registry Submission

**Goal**: Publish the Modest IoT Nano Framework to the PlatformIO Registry for professional IoT developer discoverability and instant dependency management.

**Rationale**: PlatformIO's user base skews toward professional IoT developers who use CI/CD pipelines, version-pinned dependencies, and professional IDEs (VS Code, CLion). Manual registration provides enhanced discoverability, instant updates, and professional credibility compared to relying on PlatformIO's auto-crawler of the Arduino Library Registry.

**Requirements**:
- Create `library.json` in the project root with rich metadata (name, version, description, keywords, repository, authors, license, frameworks, platforms, dependencies).
- Install PlatformIO CLI (`pip install platformio`).
- Validate package with `pio pkg pack`.
- Register PlatformIO account and publish via `pio pkg publish`.
- Integrate `pio pkg publish` into the release workflow for future versions.

**Benefits**:
- Instant update control (no 24-hour auto-crawler delay).
- Enhanced discoverability via custom library ID and rich metadata.
- Professional credibility with version history, download stats, and example code.
- CI/CD integration for automated testing and publishing.

**Target**: Complete after v1.1.1 release, alongside Benchmark Table Publication.

**Status**: Not started.

### Documentation Improvements

**Goal**: Reduce onboarding friction and support burden through targeted documentation additions.

**Items**:
- **Quick Start Guide**: Step-by-step guide from installing Arduino IDE + ESP32 board support to compiling StatusIndicator and progressing to SmartBandMqtt.
- **Troubleshooting Guide**: Common issues and solutions (Wi-Fi connection failures, MQTT disconnects, sensor read failures, stack overflow, heap fragmentation).
- **Migration Guide**: Before/after code comparison showing traditional `loop()` vs Modest IoT Device subclass, converting `millis()` timing to SamplingScheduler, blocking to interrupt-driven patterns.

**Target**: v1.1.2 or v1.2.0.

**Status**: Not started.

### Process Improvements

**Goal**: Improve release consistency, test quality, and dependency hygiene through automated workflows.

**Items**:
- **Automated Release Workflow**: GitHub Actions workflow that runs full test suite, compiles all 23 examples, generates Doxygen docs, creates GitHub release, and publishes to PlatformIO Registry on tag push.
- **Code Coverage Reporting**: Add gcov/lcov to test workflow to measure code coverage, report percentage in CI, and set minimum threshold (e.g., 80%).
- **Dependency Update Monitoring**: Add dependabot or renovate to monitor ArduinoJson, MQTT library, ESP32 Arduino core, and CI workflow action updates.

**Target**: v1.2.0.

**Status**: Not started.

### Sensor/Actuator Expansion

**Goal**: Expand the component library (currently 21 sensors, 9 actuators) to cover additional IoT use cases.

**Priority Sensors** (following ADR-016 dependency hygiene):
- **BMP280** (Barometric Altimeter): Extends existing BmeEnvironmentalSensor, low complexity.
- **INA219** (Current Sensor): I²C power monitoring, low complexity.
- **SGP30/SGP40** (Air Quality Index): VOC/CO2 detection, medium complexity.
- **BMA4000/LIS2DH12** (Accelerometer): Motion detection, tilt, medium complexity.
- **VL53L0X** (Proximity Sensor): Time-of-flight distance, medium complexity.
- **CAP1188** (Touch Sensor): Capacitive touch buttons, low complexity.

**Priority Actuators**:
- **DC Motor Controller** (L298N/L293D): Motor speed/direction, low complexity.
- **Stepper Motor** (A4988/TMC2209): Precise positioning, medium complexity.
- **Multi-channel Relay Board**: Extends RelayActuator, low complexity.

**Target**: v1.2.0+ (prioritized by dependency hygiene: zero-dependency first, common I²C next, niche last).

**Status**: Not started.

### Component Enhancements

**Goal**: Enhance existing core components with fleet management and advanced power features.

**ConfigurationStore**:
- Schema migration: Versioned schemas with automatic migration hooks when firmware version changes.
- Configuration profiles: Multiple profiles (development, production, testing) switchable at runtime.
- Configuration backup/restore: Export/import as JSON for fleet management.

**PowerManager**:
- Fuel gauge integration: Support for MAX17048, BQ27441 ICs via I²C for state-of-charge estimation.
- Charging status monitoring: Detect charging state (charging, full, discharging).
- Power budget tracking: Track average current consumption to estimate remaining battery life.

**Target**: v1.3.0.

**Status**: Not started.

### W31: Event Aggregator with Time Windows

**Target**: v1.2.0 | **Cost**: Medium | **Impact**: High

Refinement of W29: Extend the `Event` class with an optional `uint32_t timestamp` field (populated from `TimeSynchronizationDriver`), and introduce `EventAggregator` as an `EventHandler` maintaining a sliding window of recent events. Support AND/OR/NOT operators with configurable time windows (e.g., "motion AND door opened within 5 seconds"), emitting composite events with a new identifier range (800-899).

**Status**: Not started.

### W32: Alarm Lane Priority Queue

**Target**: v1.3.0 | **Cost**: Medium | **Impact**: High

Add a parallel high-priority lane for alarm telemetry. Critical alerts (water leak, fire, security breach) bypass the standard queue via `Device::enqueueAlarm()`. Network task checks alarm lane first. Alarm packets get a distinct envelope field (`"priority": "alarm"`) for edge-side routing. Bounded alarm queue (e.g., 4 slots) prevents flooding.

**Status**: Not started.

### W33: Telemetry Schema Registry

**Target**: v1.3.0 | **Cost**: Low-Medium | **Impact**: Medium

Lightweight schema versioning with validation. `TelemetrySchema` struct defines required fields, types, and version. `Device::registerSchema()` validates outgoing packets. Envelope metadata already supports `schemaVersion` — adds validation hooks. Prevents silent data corruption during fleet upgrades.

**Status**: Not started.

### W34: MQTT QoS and Retained Message Support

**Target**: v1.2.0 | **Cost**: Low | **Impact**: Medium

`MqttGatewayClient` currently uses hardcoded QoS 0. Expose QoS and retained message controls: `setQoS(uint8_t)` for default QoS (0/1/2), per-message QoS overload on `sendTelemetryRecord()`, and `setRetained(bool)` for last-known-state topics. Alarm lane (W32) could default to QoS 1.

**Status**: Not started.

### W35: CoAP Gateway Client

**Target**: v1.4.0 | **Cost**: Large | **Impact**: Medium

Add `CoapGatewayClient` extending `GatewayClient`. UDP-based lightweight alternative to MQTT for constrained networks (RFC 7252). Observe pattern for subscriptions. DTLS support for secure CoAP. Useful for Thread-based networks and LwM2M device management.

**Status**: Not started.

### W36: ESP-NOW Gateway Client

**Target**: v1.3.0 | **Cost**: Medium | **Impact**: Medium

Add `EspNowGatewayClient` for mesh relay scenarios. Direct ESP32-to-ESP32 telemetry without Wi-Fi AP or MQTT broker. Broadcast or unicast modes. Lower latency than MQTT for local device coordination. Complements W30 by providing the transport layer.

**Status**: Not started.

### W37: Adaptive Sampling Based on Battery Level

**Target**: v1.3.0 | **Cost**: Low-Medium | **Impact**: High

Built-in adaptive sampling strategy. `Device::setAdaptiveSampling(bool)` — opt-in. When battery drops below threshold, automatically increase sampling intervals. Configurable tiers: normal (100-50%), reduced (50-20%), critical (20-0%). Emit `BATTERY_LOW_EVENT` (910) and `BATTERY_CRITICAL_EVENT` (911).

**Status**: Not started.

### W38: Sleep Schedule with Wake Windows

**Target**: v1.3.0 | **Cost**: Medium | **Impact**: Medium

Time-aware sleep scheduling. `SleepSchedule` struct with time windows and intervals. `Device::setSleepSchedule()` replaces fixed deep-sleep interval. Uses `TimeSynchronizationDriver` for absolute time. Example: `{06:00-22:00 → 5min, 22:00-06:00 → 1hour}`.

**Status**: Not started.

### W39: Secure Element Integration

**Target**: v1.4.0 | **Cost**: Large | **Impact**: High

Abstract secure credential storage. `SecureElement` interface for hardware-backed key access. `EspSecureElement` using ESP32 eFuse and secure boot. `Atecc608aSecureElement` for Microchip ATECC608A via I²C. `ConfigurationStore` integration reads secrets from secure element instead of NVS.

**Status**: Not started.

### W40: Device Provisioning via BLE

**Target**: v1.3.0 | **Cost**: Medium | **Impact**: High

BLE-based provisioning flow. `ProvisioningMode` — device advertises as configurable via BLE. Smartphone app or Edge Framework sends credentials via BLE NUS. Credentials stored in `ConfigurationStore`. Auto-exit provisioning mode after successful Wi-Fi connection.

**Status**: Not started.

### W41: Remote Diagnostics Endpoint

**Target**: v1.2.0 | **Cost**: Low | **Impact**: High

Built-in diagnostics telemetry on demand. `Device::publishDiagnostics()` serializes `DiagnosticsSnapshot` as telemetry. Subscribe to diagnostics command topic; respond with diagnostics payload. Edge Framework can poll diagnostics from the fleet. Optional periodic publishing with configurable interval.

**Status**: Not started.

### W42: On-Device Statistical Aggregation

**Target**: v1.2.0 | **Cost**: Medium | **Impact**: High

Built-in statistical aggregation before transmission. `AggregationPolicy` — min, max, average, count over a time window. `Device::setAggregationPolicy(Sensor*, AggregationPolicy)` — per-sensor opt-in. Aggregated telemetry replaces individual readings. Reduces network traffic by 10-100x for high-frequency sensors.

**Status**: Not started.

### W43: Change-Only Telemetry Mode

**Target**: v1.2.0 | **Cost**: Low | **Impact**: Medium

Framework-level change detection. `Device::setChangeOnlyMode(bool, float epsilon)` — global or per-sensor. Compare current reading with last sent value; suppress if within epsilon. Reduces telemetry volume for stable sensors. Complements W42 as a simpler alternative.

**Status**: Not started.

### W44: Component Dependency Graph Tool

**Target**: v1.2.0 | **Cost**: Low | **Impact**: Medium

Host tool that generates a dependency graph. Parse `#include` directives across all headers. Generate PlantUML or Mermaid diagram of component dependencies. Identify circular dependencies or unnecessary coupling. CI integration fails if new circular dependency is introduced.

**Status**: Not started.

### W45: Example Template Generator

**Target**: v1.2.0 | **Cost**: Low-Medium | **Impact**: High

CLI tool or script that generates a skeleton device. `generate_device.py --name MyDevice --sensors DHT,BME280 --actuator Led --gateway MQTT`. Generates: `MyDevice.h`, `MyDevice.cpp`, `MyTelemetryPackage.h`, example sketch. Pre-configured with selected sensors, actuators, and gateway.

**Status**: Not started.

### Prioritized Roadmap

| Priority | Improvement | Target | Cost | Impact |
|----------|-------------|--------|------|--------|
| 1 | README Future Directions update | Immediate | Low | High |
| 2 | `library.json` for PlatformIO | v1.1.2 | Low | High |
| 3 | Benchmark measurements | Post-v1.1.1 | Medium | High |
| 4 | Quick Start Guide | v1.1.2 | Low | High |
| 5 | Troubleshooting Guide | v1.1.2 | Low | High |
| 6 | W31: Event Aggregator with Time Windows | v1.2.0 | Medium | High |
| 7 | W41: Remote Diagnostics Endpoint | v1.2.0 | Low | High |
| 8 | W34: MQTT QoS & Retained Messages | v1.2.0 | Low | Medium |
| 9 | W42: On-Device Statistical Aggregation | v1.2.0 | Medium | High |
| 10 | W43: Change-Only Telemetry Mode | v1.2.0 | Low | Medium |
| 11 | Migration Guide | v1.2.0 | Low | High |
| 12 | BMP280 sensor | v1.2.0 | Low | Medium |
| 13 | INA219 current sensor | v1.2.0 | Low | Medium |
| 14 | Code coverage reporting | v1.2.0 | Low | Medium |
| 15 | Automated release workflow | v1.2.0 | Medium | Medium |
| 16 | Dependency update monitoring | v1.2.0 | Low | Medium |
| 17 | W44: Component Dependency Graph Tool | v1.2.0 | Low | Medium |
| 18 | W45: Example Template Generator | v1.2.0 | Low-Medium | High |
| 19 | W29: Event Aggregation & Rules Engine | v1.2.0 | Large | High |
| 20 | W32: Alarm Lane Priority Queue | v1.3.0 | Medium | High |
| 21 | W33: Telemetry Schema Registry | v1.3.0 | Low-Medium | Medium |
| 22 | W37: Adaptive Sampling by Battery | v1.3.0 | Low-Medium | High |
| 23 | W38: Sleep Schedule with Wake Windows | v1.3.0 | Medium | Medium |
| 24 | W40: BLE Device Provisioning | v1.3.0 | Medium | High |
| 25 | SGP30 air quality sensor | v1.3.0 | Medium | Medium |
| 26 | PowerManager fuel gauge | v1.3.0 | Medium | Medium |
| 27 | ConfigurationStore migration | v1.3.0 | Medium | Medium |
| 28 | W30: Multi-device coordination | v1.3.0 | Large | Medium |
| 29 | W36: ESP-NOW Gateway Client | v1.3.0 | Medium | Medium |
| 30 | W35: CoAP Gateway Client | v1.4.0 | Large | Medium |
| 31 | W39: Secure Element Integration | v1.4.0 | Large | High |
| 32 | MicroPython Edition proof of concept | v1.4.0 | Very Large | High |
