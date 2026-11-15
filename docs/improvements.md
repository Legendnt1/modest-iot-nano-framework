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
- Non-ESP32 targets.

---

## Comparison with traditional development

Compares three points: a traditional Arduino `loop()` app, Modest IoT v1.1.0 (current release), and Modest IoT v1.2.0 (planned).

> **Note**: Comparison tables are replaced with each release — past comparisons are not archived. When v1.2.0 ships, this table is updated to compare v1.2.0 vs v1.3.0.

### Developer experience

| Aspect          | Traditional `loop()`                         | Modest IoT v1.1.0 (current)                                                 | Modest IoT v1.2.0 (planned)                                 |
|-----------------|----------------------------------------------|-------------------------------------------------------------------------------|----------------------------------------------------------------------|
| First build     | Works with the board package                 | All 22 examples compile in CI; 2 dependencies                                 | Same                                                                |
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
| Testing         | Rarely possible off-device                   | 36 host test suites with CI, sanitizers enabled                               | Same                                                |
| Documentation   | README or inline comments                    | 30 ADRs with rationale, user stories with ADR cross-references, PlantUML class diagram, Doxygen API reference, versioned CHANGELOG, and cross-linked doc ecosystem | Same                                                |

### Performance

| Aspect                     | Traditional `loop()`              | Modest IoT v1.1.0 (current)                             | Modest IoT v1.2.0 (planned)                              |
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

| Resource                 | Traditional `loop()` | Modest IoT v1.1.0 (current)                                                               | Modest IoT v1.2.0 (planned)                           |
|--------------------------|----------------------|---------------------------------------------------------------------------------------------|----------------------------------------------------------------|
| Task stacks              | `loop()` task only   | 4096 B sampler + 8192 B network, configurable via `EngineConfig`; `PowerManager` adds minimal stack for sleep handling | Same defaults                                             |
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

**Target**: Complete after v1.1.0 release when hardware testing infrastructure is available.

**Status**: Not started.
