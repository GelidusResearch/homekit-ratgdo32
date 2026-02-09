# Merge Notes for v3.4.3 Integration

## Overview
Merged upstream changes from `ratgdo/homekit-ratgdo32` tag `v3.4.3` into `GelidusResearch/homekit-ratgdo32` (main branch).

## Conflict Resolutions

### 1. `platformio.ini`
- **Conflict**: Upstream updated `lib_deps` to use `VL53L4CX` sensor library and `HomeSpan` version 2.1.6.
- **Resolution**: Kept local `GRGDO1` configuration:
  - Preserved `VL53L1X` sensor library (required for GRGDO1 hardware).
  - Preserved `HomeSpan` version 2.1.7 (newer/custom).
  - Kept `GRGDO1` specific build flags (`-D GRGDO1_V2`, pin definitions).

### 2. `src/vehicle.cpp`
- **Conflict**: Upstream updated implementation for `VL53L4CX` sensor.
- **Resolution**: Kept local `VL53L1X` implementation.
  - **Action Required**: Verify if any logic improvements from upstream `VL53L4CX` implementation (e.g. presence detection algorithms) are applicable/portable to `VL53L1X` code. The current merge preserves the old logic.

### 3. `src/homekit.h`
- **Conflict**: Upstream added/modified `notify_homekit_light` and motion sensor signatures.
- **Resolution**: Merged changes while preserving local `USE_DHT22` and `GRGDO1` specific definitions.

### 4. `src/json.h`
- **Conflict**: Macro definitions for JSON generation.
- **Resolution**: Adopted upstream improvements where compatible, ensured `JSON_ADD_FLOAT` and `JSON_ADD_RAW` availability.

### 5. `src/www/functions.js`
- **Conflict**: UI logic updates.
- **Resolution**: Merged upstream changes.

### 6. `docs/manifest.json`
- **Conflict**: Firmware file paths and versioning.
- **Resolution**: Updated to reflect `homekit-grgdo1` naming convention while adopting upstream version number `v1.2.0` (or `v3.4.3` where appropriate). *Note: `manifest.json` currently points to `v1.2.0` firmware filenames based on local branch state.*

## Verification
- **Build**: Successfully compiled both `rev1` and `rev2` environments using PlatformIO on 2026-02-09.
- **Build Script**: Updated `x.sh` to automatically build and package both hardware revisions during a release.
- **Logic**: Verified that `calculatePresence` algorithm in `src/vehicle.cpp` is identical to upstream v3.4.3.
- **Fixes**: Restored `MAX_DISTANCE` constant in `src/vehicle.cpp` which was lost during initial merge.

## Outstanding Items
- **Hardware Testing**: `src/vehicle.cpp` logic should be tested on actual hardware to ensure `VL53L1X` driver remains functional with system updates.
- **Dependencies**: `HomeSpan` 2.1.7 confirmed compatible via successful build and review of upstream changes (which used 2.1.6).
