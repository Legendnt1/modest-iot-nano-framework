# Dependencies

This document lists all external dependencies of the Modest IoT Nano Framework and how they are monitored for updates.

## Dependency Summary

| Dependency | Type | Version | Monitored By |
|------------|------|---------|--------------|
| **MQTT** | Arduino Library | ≥2.5.3 | Manual |
| **ESP32 Arduino Core** | Platform Core | Latest | Manual (CI installs latest) |
| **GitHub Actions** | CI/CD | Various | Dependabot (automated) |

## External Arduino Dependencies

### MQTT Library (≥2.5.3)

The only external Arduino library dependency after W46 (ArduinoJson removal).

- **Repository**: [256dpi/arduino-mqtt](https://github.com/256dpi/arduino-mqtt)
- **Used in**: `library.properties` (depends field), `library.json` (dependencies), `compile-examples.yml` (CI install)
- **Minimum version**: 2.5.3
- **Update process**: Manual — check releases page periodically, update all 3 files simultaneously

To update the MQTT library version:

1. Check [MQTT releases](https://github.com/256dpi/arduino-mqtt/releases) for the latest version.
2. Update `library.properties`:
   ```properties
   depends=MQTT (>=X.Y.Z)
   ```
3. Update `library.json`:
   ```json
   "dependencies": {
     "MQTT": ">=X.Y.Z"
   }
   ```
4. Update `.github/workflows/compile-examples.yml`:
   ```yaml
   arduino-cli lib install "MQTT@X.Y.Z"
   ```
5. Create a feature branch, run CI, and merge.

### ESP32 Arduino Core

The ESP32 platform core is installed dynamically in CI. No manifest pins a specific version.

- **Repository**: [espressif/arduino-esp32](https://github.com/espressif/arduino-esp32)
- **Used in**: `compile-examples.yml` (CI installs latest via `arduino-cli core install esp32:esp32`)
- **Update process**: CI always uses the latest core. To pin a specific version, modify `compile-examples.yml`.

## GitHub Actions Dependencies

GitHub Actions are monitored automatically by Dependabot (`.github/dependabot.yml`). When a new version is available, Dependabot opens a pull request with the version bump.

| Action | Current Version | Workflows |
|--------|-----------------|-----------|
| `actions/checkout` | v4 | All workflows |
| `actions/upload-artifact` | v4 | host-tests.yml (coverage) |
| `arduino/arduino-lint-action` | v1 | lint.yml |
| `arduino/setup-arduino-cli` | v1 | compile-examples.yml |

Dependabot checks for updates monthly. Review the Dependabot PR changelog for breaking changes before merging. CI validates all changes automatically.

## Build Tool Dependencies

These tools are required for local development and CI. They are not framework dependencies but are needed to build and test the project.

| Tool | Purpose | Where |
|------|---------|-------|
| **g++** | Host test compilation | Local, CI (Ubuntu) |
| **lcov** | Code coverage reporting | Local (optional), CI (Ubuntu) |
| **bc** | Coverage math | CI (Ubuntu) |
| **arduino-cli** | Example compilation | CI |
| **CMake** | Alternative test build | Local (optional) |

## Dependency Philosophy

After W46 (ArduinoJson removal), the framework has only **one external Arduino dependency** (MQTT). This minimal dependency footprint reduces:

- **Flash usage**: ~30-40 KB saved by removing ArduinoJson
- **Update surface**: Only 1 library to monitor instead of 2
- **Breakage risk**: Fewer upstream changes can break the framework
- **Build time**: Faster compilation with fewer dependencies

The ESP32 Arduino core is a platform requirement, not a framework choice — all ESP32 Arduino projects require it.
