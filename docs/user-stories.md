# User Stories for Modest IoT Nano Framework

This document contains user stories for the Modest IoT Nano Framework, designed for IoT developers creating applications on resource-constrained devices like the ESP32. Each story includes acceptance criteria, using the **Given-When-Then** format where applicable, covering the main functional and non-functional requirements that drove the design and implementation of the framework.

## IoT Developer Stories

### TS01: Handle Events for Sensors
- **As an** IoT Developer, **I want** a framework that defines and propagates sensor-originated events, **so that** I can build reactive IoT behavior without wiring ad-hoc polling logic.
- **Acceptance Criteria**:
    - **Given** a sensor input changes, **when** the framework captures that change, **then** an event is emitted and routed to a configured consumer.
    - **Given** no consumer is configured, **when** an event is emitted, **then** the framework remains stable and does not crash.
    - Event metadata is lightweight and deterministic enough for constrained devices.
    - **Given** a sensor faults after N consecutive read failures, **when** the threshold is reached, **then** a fault event (ID 22) is emitted exactly once.
    - **Given** a faulty sensor recovers with a successful read, **when** the next valid reading occurs, **then** a recovery event (ID 23) is emitted exactly once.
- **Status**: ✓ Implemented. See `Sensor.h` (fault identifiers 22/23), `Sensor.cpp` (debounced threshold state machine), and `tests/SensorFault/test.cpp`.
  Related: [ADR-009](adrs.md#adr-009-sensor-fault-events-with-debounced-threshold-state-machine), [ADR-003](adrs.md#adr-003-hybrid-approach-for-sensor-sampling-intervals)

### TS02: Issue Commands to Actuators
- **As an** IoT Developer, **I want** a framework that defines and executes commands for outputs, **so that** I can control actuators in a structured, reusable way.
- **Acceptance Criteria**:
    - **Given** a valid command is issued, **when** the framework dispatches it, **then** the target output state changes consistently.
    - **Given** a command has an upstream propagation target, **when** it is processed, **then** it can be forwarded after local execution.
    - Command handling is deterministic and safe under repeated invocations.
    - **Given** an actuator receives a command, **when** it executes, **then** it runs exclusively on the sampler task (documented via `@runs_on sampler`).
- **Status**: ✓ Implemented. See `Actuator.h`, `Buzzer.h`, `Led.h`, `RgbLed.h`, `ServoMotor.h`, `CharacterLcdDisplay.h`, and `Command.h`.
  Related: [ADR-015](adrs.md#adr-015-identifier-registry-with-reserved-ranges), [ADR-029](adrs.md#adr-029-native-servo-and-lcd-drivers-w19)

### TS03: Combine Inputs and Outputs in Devices
- **As an** IoT Developer, **I want** a unified device abstraction that combines event and command flows, **so that** I can build cohesive IoT units with minimal boilerplate.
- **Acceptance Criteria**:
    - **Given** a custom device implementation, **when** input events are received, **then** the device can map them into output commands.
    - **Given** a single device instance owns multiple inputs and outputs, **when** it runs, **then** it acts as the composition root for orchestration.
    - The abstraction supports scaling from simple one-input/one-output cases to multi-component devices.
    - **Given** a device with registered sensors, **when** the engine starts, **then** sensors are scheduled via `SamplingScheduler` with per-slot intervals.
    - **Given** a device needs to shut down, **when** `stopAsynchronousEngine()` is called, **then** tasks terminate cooperatively and resources are cleaned up.
- **Status**: ✓ Implemented. See `Device.h`, `Device.cpp`, `SamplingScheduler.h`, and `EngineConfiguration`.
  Related: [ADR-010](adrs.md#adr-010-engineconfig-for-tunable-freertos-task-parameters), [ADR-011](adrs.md#adr-011-per-slot-samplingscheduler-with-countdown-based-dispatch), [ADR-014](adrs.md#adr-014-threading-model--single-context-sampler-with-isolated-network-task), [ADR-020](adrs.md#adr-020-stopasynchronousengine--cooperative-shutdown-via-notification-bits)

### TS04: Modular and Reusable Components
- **As an** IoT Developer, **I want** modular and reusable component contracts, **so that** I can reuse capabilities across projects with different hardware mappings.
- **Acceptance Criteria**:
    - **Given** components are configured with different physical mappings, **when** the application starts, **then** behavior remains correct without framework source changes.
    - **Given** a new project, **when** the same component contracts are reused, **then** a different device behavior can be assembled quickly.
    - Developers can add new component types by extending documented contracts without modifying existing implementations.
    - **Given** a sensor subclass, **when** it declares default and minimum sampling intervals, **then** the scheduler resolves them polymorphically via virtual getters.
    - **Given** a sensor instance needs a custom period, **when** `setSamplingIntervalInMilliseconds()` is called, **then** the per-instance override takes precedence over the class default.
- **Status**: ✓ Implemented. See `Sensor.h` (virtual getters, runtime overrides), 10 sensor subclasses, and `tests/SamplingScheduler/test.cpp`.
  Related: [ADR-003](adrs.md#adr-003-hybrid-approach-for-sensor-sampling-intervals), [ADR-011](adrs.md#adr-011-per-slot-samplingscheduler-with-countdown-based-dispatch)

### TS05: Lightweight and Efficient Design
- **As an** IoT Developer, **I want** a lightweight and efficient framework, **so that** it performs well on resource-constrained microcontrollers.
- **Acceptance Criteria**:
    - **Given** a minimal application that uses core capabilities, **when** it is built, **then** framework overhead remains bounded and documented.
    - **Given** typical event/command workloads, **when** execution latency is measured, **then** timing remains within documented real-time targets.
    - Memory behavior is predictable under sustained operation and does not degrade over time.
    - **Given** logging is compiled out, **when** the application runs, **then** `MODEST_LOG_*` macros produce zero Flash and zero runtime overhead.
    - **Given** a telemetry payload exceeds the bounded serialization buffer, **when** serialization is attempted, **then** a warning is logged and the sending is skipped without heap allocation.
- **Status**: ✓ Implemented. See `ModestLog.h` (compile-time filtering), `MqttGatewayClient.cpp` (512-byte bounded buffer), and `DiagnosticsSnapshot`.
  Related: [ADR-001](adrs.md#adr-001-framework-naming-convention--modest_log_-macros-with-miot_log_-aliases), [ADR-006](adrs.md#adr-006-bounded-serialization-buffer-in-mqttgatewayclient), [ADR-018](adrs.md#adr-018-diagnosticssnapshot--on-demand-stack-and-heap-metrics)

### TS06: Manage Connectivity Lifecycle
- **As an** IoT Developer, **I want** connectivity to be abstracted behind a transport contract, **so that** link setup and health checks are portable across networks.
- **Acceptance Criteria**:
    - **Given** no active network link, **when** a connection is requested, **then** the framework initiates a non-blocking or bounded connection workflow.
    - **Given** a degraded or lost link, **when** connectivity is checked, **then** the framework reports not-ready state accurately.
    - **Given** a healthy link, **when** transmission readiness is queried, **then** the framework reports ready state consistently.
    - **Given** a Wi-Fi link drops, **when** reconnection is attempted, **then** the driver retries with configurable intervals and re-arms on failure.
    - **Given** an MQTT broker disconnects, **when** reconnection is attempted, **then** the client uses rate-limited backoff and continuous keep-alive via `service()`.
    - **Given** a BLE peer connects, **when** data is transmitted, **then** GATT notifications are sent via the Nordic UART Service (NUS) TX characteristic.
- **Status**: ✓ Implemented. See `WiFiConnectivityDriver`, `MqttGatewayClient` (reconnect backoff, keep-alive), `BleConnectivityDriver` (NUS GATT), and `tests/MqttGatewayClient/test.cpp`.
  Related: [ADR-002](adrs.md#adr-002-ble-connectivity-via-nordic-uart-service-nus), [ADR-019](adrs.md#adr-019-mqtt-reconnect-policy--rate-limited-backoff-with-continuous-keep-alive)

### TS07: Publish Telemetry to Edge Services
- **As an** IoT Developer, **I want** telemetry payload publication to be protocol-agnostic at the application level, **so that** I can switch transport strategies without changing device business logic.
- **Acceptance Criteria**:
    - **Given** a telemetry payload object, **when** publication is requested, **then** it is serialized to a transportable representation and sent through the selected gateway.
    - **Given** transient publication failure, **when** a publication attempt occurs, **then** the framework returns a clear success/failure result without crashing.
    - **Given** both request/response and broker-style integrations, **when** the same payload contract is used, **then** both publication paths are supported.
    - **Given** a telemetry package is enqueued, **when** it enters the pipeline, **then** it receives a monotonically increasing sequence number and a capture timestamp.
    - **Given** the telemetry queue is full, **when** a new packet is enqueued, **then** the oldest packet is dropped and the new one is inserted (drop-oldest policy).
    - **Given** a telemetry package is created with `std::make_unique`, **when** enqueue fails, **then** the `unique_ptr` automatically deletes the packet without manual cleanup.
- **Status**: ✓ Implemented. See `TelemetryPackage.h` (envelope metadata), `Device.cpp` (drop-oldest, `unique_ptr` overload), `MqttGatewayClient.cpp` (bounded buffer), and `tests/TelemetryPackage/test.cpp`.
  Related: [ADR-004](adrs.md#adr-004-unique_ptr-ownership-for-telemetry-enqueue), [ADR-005](adrs.md#adr-005-drop-oldest-queue-policy), [ADR-006](adrs.md#adr-006-bounded-serialization-buffer-in-mqttgatewayclient), [ADR-007](adrs.md#adr-007-envelope-metadata-in-telemetry-packages), [ADR-021](adrs.md#adr-021-golden-host-tests-for-telemetry-json-serialization)

### TS08: Orchestrate Asynchronous Sampling and Dispatch
- **As an** IoT Developer, **I want** asynchronous sampling and dispatch orchestration, **so that** hardware polling and network publication do not block each other.
- **Acceptance Criteria**:
    - **Given** multiple registered input tasks, **when** the scheduler runs, **then** work is processed in a bounded round-robin sequence.
    - **Given** telemetry items are produced faster than they are transmitted, **when** buffering occurs, **then** queue behavior follows documented capacity rules.
    - **Given** long-running operation, **when** background workers execute, **then** input capture and dispatch remain isolated and responsive.
    - **Given** sensors with different sampling periods, **when** the scheduler advances, **then** each sensor polls at its own interval via per-slot countdown timers.
    - **Given** a sensor's requested period is less than its minimum, **when** it is registered, **then** the period is clamped to the minimum and a warning is logged.
    - **Given** the network task runs, **when** the telemetry queue is empty, **then** it services MQTT keep-alive and Wi-Fi reconnection in the background.
- **Status**: ✓ Implemented. See `SamplingScheduler.h/.cpp`, `Device.cpp` (per-slot scheduling, bounded-timeout network worker), and `tests/SamplingScheduler/test.cpp`.
  Related: [ADR-011](adrs.md#adr-011-per-slot-samplingscheduler-with-countdown-based-dispatch), [ADR-014](adrs.md#adr-014-threading-model--single-context-sampler-with-isolated-network-task), [ADR-019](adrs.md#adr-019-mqtt-reconnect-policy--rate-limited-backoff-with-continuous-keep-alive)

### TS09: Handle Data Quality and Failure Conditions
- **As an** IoT Developer, **I want** explicit handling for invalid or duplicate measurements, **so that** downstream logic receives meaningful signals instead of noisy data.
- **Acceptance Criteria**:
    - **Given** an invalid sensor reading, **when** processing occurs, **then** propagation is suppressed or marked as invalid per policy.
    - **Given** unchanged readings where change-only propagation is enabled, **when** sampling repeats, **then** duplicate events are suppressed.
    - **Given** operational errors, **when** they occur, **then** the framework fails safely and preserves runtime stability.
    - **Given** a sensor faults, **when** the mediator receives the fault event, **then** it can react (e.g., turn LED red, publish health telemetry).
    - **Given** a packet is dropped from the queue, **when** the first drop occurs, **then** a warning is logged exactly once.
    - **Given** an MQTT send fails, **when** the first failure occurs, **then** a warning is logged exactly once with the topic name.
- **Status**: ✓ Implemented. See `Sensor.cpp` (fault state machine), `Device.cpp` (first-drop logging), `MqttGatewayClient.cpp` (first-failure logging), and `tests/SensorFault/test.cpp`.
  Related: [ADR-009](adrs.md#adr-009-sensor-fault-events-with-debounced-threshold-state-machine), [ADR-005](adrs.md#adr-005-drop-oldest-queue-policy)

### TS10: Extend Framework Safely
- **As an** IoT Developer, **I want** stable extension points for inputs, outputs, telemetry payloads, and transports, **so that** I can customize behavior without forking the framework.
- **Acceptance Criteria**:
    - **Given** a new custom component implementation, **when** it adheres to framework contracts, **then** it integrates without changing existing modules.
    - **Given** a custom telemetry payload shape, **when** it is serialized through the payload contract, **then** gateways can publish it without special-case logic.
    - **Given** a new connectivity backend, **when** it implements the transport contract, **then** higher-level application code remains unchanged.
    - **Given** a custom event identifier, **when** it is defined in the user range (1000+), **then** it does not collide with framework-reserved identifiers.
    - **Given** a new sensor subclass, **when** it implements virtual sampling interval getters, **then** it integrates with the scheduler automatically.
- **Status**: ✓ Implemented. See `EventHandler.h`, `Sensor.h`, `Actuator.h`, `ConnectivityDriver.h`, `GatewayClient.h`, `TelemetryPackage.h`, and README (Identifier Registry).
  Related: [ADR-015](adrs.md#adr-015-identifier-registry-with-reserved-ranges), [ADR-023](adrs.md#adr-023-domain-driven-design-adoption-and-bounded-contexts)

### TS11: Observe System Health via Diagnostics
- **As an** IoT Developer, **I want** to query runtime diagnostics at any time, **so that** I can tune task stacks, detect memory pressure, and observe queue behavior.
- **Acceptance Criteria**:
    - **Given** the engine is running, **when** `Device::diagnostics()` is called, **then** a 12-field snapshot is returned with heap, stack, queue, and send statistics.
    - **Given** stack high-water marks are queried, **when** the snapshot is computed, **then** values are reported in bytes (not words) for direct interpretation.
    - **Given** the diagnostics query runs, **when** it completes, **then** no background CPU overhead is incurred during normal operation.
    - **Given** gateway send statistics are available, **when** the snapshot is computed, **then** send success and failure counts are included.
- **Status**: ✓ Implemented. See `Device.h` (`DiagnosticsSnapshot`), `Device.cpp` (`diagnostics()`), `GatewayClient.h` (`GatewayStatistics`), and `tests/DiagnosticsSnapshot/test.cpp`.
  Related: [ADR-018](adrs.md#adr-018-diagnosticssnapshot--on-demand-stack-and-heap-metrics), [ADR-010](adrs.md#adr-010-engineconfig-for-tunable-freertos-task-parameters)

### TS12: Log Framework Activity with Zero-Cost Macros
- **As an** IoT Developer, **I want** compile-time filtered logging macros, **so that** I can debug device behavior without runtime overhead when logging is disabled.
- **Acceptance Criteria**:
    - **Given** `MODEST_LOG_LEVEL` is set to 2 (WARN), **when** `MODEST_LOG_D` is called, **then** the macro compiles to `((void)0)` with zero Flash and zero runtime cost.
    - **Given** `MODEST_LOG_LEVEL` is set to 4 (DEBUG), **when** `MODEST_LOG_D` is called, **then** the message is formatted and sent to the configured sink.
    - **Given** a host unit test needs to capture logs, **when** `modest_iot::setLogSink()` is called, **then** log output is redirected to the test sink.
    - **Given** a developer prefers short macro names, **when** `MODEST_USE_SHORT_LOG_MACROS` is defined, **then** `MIOT_LOG_*` aliases are available.
- **Status**: ✓ Implemented. See `ModestLog.h` and `tests/ModestLog/test.cpp`.
  Related: [ADR-001](adrs.md#adr-001-framework-naming-convention--modest_log_-macros-with-miot_log_-aliases), [ADR-013](adrs.md#adr-013-screaming_snake_case-for-all-preprocessor-macros)

### TS13: Configure Engine Task Parameters
- **As an** IoT Developer, **I want** to configure FreeRTOS task stack sizes, priorities, and core pinning, **so that** I can tune the engine for my device's memory and performance requirements.
- **Acceptance Criteria**:
    - **Given** an `EngineConfiguration` struct, **when** `initializeAsynchronousEngine(config)` is called, **then** tasks are created with the specified stack sizes, priorities, and core affinity.
    - **Given** no config is provided, **when** `initializeAsynchronousEngine(tickInterval)` is called, **then** default values are used (4096/8192 bytes, priority 3/1, core 1/1, queue 10).
    - **Given** the engine is running, **when** `stopAsynchronousEngine()` is called, **then** tasks terminate cooperatively, the timer is cleaned up, and the queue is drained.
    - **Given** `stopAsynchronousEngine()` is called twice, **when** the second call executes, **then** it is a no-op (idempotent double-stop guard).
- **Status**: ✓ Implemented. See `Device.h` (`EngineConfiguration`), `Device.cpp` (`stopAsynchronousEngine`), and `tests/EngineConfig/test.cpp`.
  Related: [ADR-010](adrs.md#adr-010-engineconfig-for-tunable-freertos-task-parameters), [ADR-020](adrs.md#adr-020-stopasynchronousengine--cooperative-shutdown-via-notification-bits)

### TS14: Ensure Telemetry Ownership Safety
- **As an** IoT Developer, **I want** safe ownership transfer for telemetry packages, **so that** I don't leak memory when enqueue fails or forget to clean up on error paths.
- **Acceptance Criteria**:
    - **Given** a telemetry package created with `std::make_unique`, **when** `enqueueTelemetryPayload(std::move(packet))` succeeds, **then** ownership transfers to the queue and the caller no longer owns the packet.
    - **Given** a telemetry package created with `std::make_unique`, **when** `enqueueTelemetryPayload(std::move(packet))` fails, **then** the `unique_ptr` automatically deletes the packet.
    - **Given** the telemetry queue is full, **when** a new packet is enqueued, **then** the oldest packet is dropped and the new one is inserted.
    - **Given** a telemetry package is serialized, **when** the JSON exceeds the 512-byte buffer, **then** a `PayloadTooLarge` warning is logged and the sending is skipped.
- **Status**: ✓ Implemented. See `Device.h` (`unique_ptr` overload), `Device.cpp` (drop-oldest), `MqttGatewayClient.cpp` (bounded buffer), and `tests/DiagnosticsSnapshot/test.cpp`.
  Related: [ADR-004](adrs.md#adr-004-unique_ptr-ownership-for-telemetry-enqueue), [ADR-005](adrs.md#adr-005-drop-oldest-queue-policy), [ADR-006](adrs.md#adr-006-bounded-serialization-buffer-in-mqttgatewayclient)

### TS15: Validate Framework with Automated Tests
- **As an** IoT Developer, **I want** the framework to have comprehensive host unit tests and CI workflows, **so that** I can trust the codebase compiles and behaves correctly without deploying to hardware.
- **Acceptance Criteria**:
    - **Given** a new pull request, **when** CI runs, **then** all 47 host test suites pass with Address and Undefined sanitizers enabled.
    - **Given** a new pull request, **when** CI runs, **then** all 23 example sketches compile cleanly on ESP32 and ESP32-S3 targets.
    - **Given** library metadata is changed, **when** CI runs, **then** `arduino-lint` validates Arduino Library Manager compliance.
    - **Given** a telemetry package is serialized in a host test, **when** the golden JSON is compared, **then** the exact output matches the expected string.
- **Status**: ✓ Implemented. See `tests/run_all.sh`, `.github/workflows/host-tests.yml`, `.github/workflows/compile-examples.yml`, `.github/workflows/lint.yml`, and `tests/TelemetryPackage/test.cpp`.
  Related: [ADR-017](adrs.md#adr-017-shared-mock-harness-for-host-unit-tests), [ADR-021](adrs.md#adr-021-golden-host-tests-for-telemetry-json-serialization)

### TS16: Monitor Battery State

- **As an** IoT Developer, **I want** to monitor battery voltage and level on battery-powered devices, **so that** I can take action when the battery is low (reduce sampling, enter deep sleep, or alert the user).
- **Acceptance Criteria**:
    - **Given** a battery is connected to an ADC pin, **when** `PowerManager::begin()` is called, **then** the voltage divider ratio is configured and readings begin.
    - **Given** the battery voltage is read, **when** `getBatteryVoltageInMillivolts()` is called, **then** the value is returned in millivolts with proper ADC-to-voltage conversion.
    - **Given** the battery voltage is known, **when** `getBatteryLevelInPercentage()` is called, **then** a 0.0–100.0 percentage is returned based on the configured voltage range.
    - **Given** the battery voltage drops below the threshold, **when** `isLowBattery()` is called, **then** it returns `true` and the low-battery callback fires if registered.
    - **Given** a custom battery chemistry, **when** `setBatteryVoltageRange()` is called, **then** the min/max voltage bounds are updated for level calculation.
- **Status**: ✓ Implemented (v1.1.0). See `PowerManager.h`, `PowerManager.cpp`, and `tests/PowerManager/test.cpp` (18 tests).

### TS17: Buffer Telemetry Offline

- **As an** IoT Developer, **I want** telemetry packets to be persisted when the network is unavailable, **so that** no data is lost during connectivity outages and packets are replayed when connectivity is restored.
- **Acceptance Criteria**:
    - **Given** the network is unavailable, **when** a telemetry send fails, **then** the packet JSON is stored in LittleFS via `TelemetryBuffer::store()`.
    - **Given** the buffer is at capacity, **when** a new packet is stored, **then** the oldest packet is evicted (FIFO policy) and the eviction callback fires if registered.
    - **Given** connectivity is restored, **when** `TelemetryBuffer::retrieveNext()` is called, **then** packets are returned in FIFO order until the buffer is empty.
    - **Given** the device reboots unexpectedly, **when** `TelemetryBuffer::begin()` is called, **then** previously buffered packets are recovered from LittleFS.
    - **Given** the buffer is cleared, **when** `TelemetryBuffer::clear()` is called, **then** all buffered packets are removed from LittleFS.
- **Status**: ✓ Implemented (v1.1.0). See `TelemetryBuffer.h`, `TelemetryBuffer.cpp`, and `tests/TelemetryBuffer/test.cpp` (22 tests).

### TS18: Receive OTA Updates

- **As an** IoT Developer, **I want** deployed devices to receive Over-The-Air firmware updates, **so that** I can fix bugs and add features without physical access to the device.
- **Acceptance Criteria**:
    - **Given** OTA is configured, **when** `OtaUpdateHandler::begin()` is called, **then** the device listens for incoming firmware updates with the specified hostname.
    - **Given** an update is in progress, **when** the progress callback fires, **then** the number of bytes written and total bytes are reported.
    - **Given** an update fails, **when** the error callback fires, **then** the error code is reported and the status transitions to `Error`.
    - **Given** the `service()` method is called, **when** no update is pending, **then** the call is a no-op (safe to call repeatedly from `maintainNetworkServices()`).
    - **Given** a password is set, **when** an update is requested, **then** the password must match for the update to proceed.
- **Status**: ✓ Implemented (v1.1.0). See `OtaUpdateHandler.h`, `OtaUpdateHandler.cpp`, and `tests/OtaUpdateHandler/test.cpp` (21 tests).

### TS19: Report Firmware Version in Telemetry

- **As an** IoT Developer, **I want** every telemetry packet to include the firmware version, **so that** I can track which version each device is running and correlate telemetry format changes.
- **Acceptance Criteria**:
    - **Given** a firmware version is set, **when** `Device::setFirmwareVersion()` is called, **then** the version string appears in the JSON envelope of every telemetry record.
    - **Given** a schema version is set, **when** `Device::setTelemetrySchemaVersion()` is called, **then** the schema version appears in the JSON envelope of every telemetry record.
    - **Given** neither version is set, **when** telemetry is serialized, **then** both fields are omitted from the JSON output with zero overhead.
    - **Given** only the firmware version is set, **when** telemetry is serialized, **then** only `firmwareVersion` appears, `schemaVersion` is omitted.
- **Status**: ✓ Implemented (v1.1.0). See `TelemetryPackage.h`, `Device.h`, `Device.cpp`, and `tests/TelemetryPackage/test.cpp` (5 new tests).

### TS20: Install via Main IoT Registry Providers

- **As an** IoT Developer, **I want** to install the Modest IoT Nano Framework from a registry provider without manual steps, **so that** I can use the framework in my project with a single configuration line.
- **Acceptance Criteria**:
    - **Given** the framework is published to the Arduino Library Manager, **when** the Arduino IDE Library Manager lists available libraries, **then** the framework appears with the correct name, version, and description.
    - **Given** the framework is published to the Arduino Library Manager, **when** installation is triggered, **then** the framework and its dependencies (ArduinoJson, MQTT) are downloaded and available in the sketch.
    - **Given** a `library.json` manifest exists, **when** `Modest-IoT-Nano-Framework` is added to `lib_deps` in `platformio.ini`, **then** PlatformIO resolves and downloads the framework and its dependencies.
    - **Given** the framework is installed via either registry, **when** a sketch that includes `ModestIoT.h` is built, **then** the build succeeds without missing header errors.
    - **Given** both `library.json` and `library.properties` exist, **when** the metadata is compared, **then** the name, version, and dependencies are consistent between the two files.
- **Status**: Planned for v1.1.2. See `library.properties` (Arduino Library Manager, published) and `library.json` (PlatformIO Registry, to be created).

### TS21: Follow Quick Start Guide

- **As an** IoT Developer, **I want** a step-by-step Quick Start Guide, **so that** I can go from zero to a working device in under 10 minutes.
- **Acceptance Criteria**:
    - **Given** an ESP32 board and USB cable, **when** the Quick Start Guide is followed, **then** the framework is installed, the `StatusIndicator` example is opened, and the sketch is uploaded to the board.
    - **Given** the Quick Start Guide covers installation, **when** the Installation section is read, **then** both Arduino Library Manager and PlatformIO `lib_deps` paths are documented.
    - **Given** the `StatusIndicator` example is uploaded, **when** the Serial Monitor is connected, **then** LED toggle status messages are output confirming the framework is running.
    - **Given** the Quick Start Guide is complete, **when** the Next Steps section is reached, **then** links to the full README, examples, and documentation are provided.
- **Status**: Planned for v1.1.2. See `docs/quick-start.md` (to be created) and `examples/StatusIndicator`.

---

*Created: March 23, 2025.*
*Last Updated: November 17, 2026.*
