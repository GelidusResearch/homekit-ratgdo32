# GRGDO1 releases

This integration proposes source version `v1.4.0`. It includes
[`ratgdo/homekit-ratgdo32` commit `5ab633c166a367fd48303eca37bc9e086d2bfb6d`](https://github.com/ratgdo/homekit-ratgdo32/commit/5ab633c166a367fd48303eca37bc9e086d2bfb6d)
from October 18, 2026, plus the Gelidus `v1.3.1` release history. That upstream
commit includes the fixes following `v3.5.5`; upstream calls it the unreleased
`v3.5.6` development version.

`VERSION` sets the firmware version. The installer manifests describe the last
published binaries, so they stay at `v1.3.0` until the release workflow publishes
the new artifacts.

## Rev1 runtime correction

The `fc26ab2` test build became unresponsive on a physical GRGDO1 Rev1 using
Security+ 2.0. Boot logs show that the VL53L1X probe failed and entered the old
UART restoration path. That code copied `HardwareSerial` objects, including
their driver pointers, mutexes, and task ownership. Those copies are removed.
The exact cause of the observed freeze still needs confirmation on the device.

A failed restore produced a core dump from the exact `fc26ab2` binary. Its
backtrace resolves to `uxListRemove -> vTaskDelete -> handle_firmware_upload`,
before `Update.begin()` writes flash. The updater now pauses HomeSpan through
its own polling mutex, retains the polling task, and resumes it after upload
completion or abort. Repeated upload attempts cannot delete a stale task handle.
The MD5 query argument also keeps its owning String alive until the checksum is
copied. `tests/update_poll_guard.cpp` covers repeated starts, completion/abort,
and an idle/no-task case.

A subsequent clean OTA attempt restored the official Rev1 v1.3.0 image. Its
SHA256 matched the Gelidus release asset. Running version 1.3.0, retained pairing,
increasing uptime, and restoration of the original log level were checked on the
device. That recovery confirms the rollback; it does not validate the corrected
candidate on hardware.

Rev1 now stops Improv and HomeSpan serial clients, disables automatic serial
startup in the logger, and releases UART0 before I2C takes GPIO1/3. Logs remain
available over the network. If no sensor is detected, `Wire.end()` releases the
pins before the original UART0 is restarted and its clients restored. When the
sensor is present, those pins remain dedicated to I2C. No UART1 remapping or
serial object assignment occurs. Serial startup also has a re-entry guard and
returns on failure instead of spinning forever.

`tests/serial_console.cpp` uses a noncopyable serial-port model and exercises
logging during begin/end, missing-sensor restoration, attempted pin reclamation
while I2C is active, and serial-start failure. The original Rev1 test build is
withdrawn, and PR #21 remains draft pending corrected-build device validation.

## Security+ 1.0 regression and release status

[Gelidus issue #19](https://github.com/GelidusResearch/homekit-ratgdo32/issues/19)
reports uncommanded door movement with v1.3.1 and recovery after reverting to
v1.3.0. The published installer therefore stays on the v1.3.0 files from the
current Gelidus main branch. Merging the v1.3.1 history must not restore its
withdrawn installer assets.

The integration includes the fix for the linked
[upstream issue #192](https://github.com/ratgdo/homekit-ratgdo32/issues/192): after
starting the ESP32 Security+ 1.0 hardware UART, it explicitly disables RX
pull-down and enables RX pull-up. The initialization now checks driver/bias
failures, forces byte-at-a-time reception, and allows 20 ms for an echo (an 8E1
character at 1200 baud alone takes 9.17 ms). Failed initialization leaves TX
inactive and the communications loop stopped.

The old shutdown and panic paths called `gpio_reset_pin()` on TX. ESP-IDF
specifies that reset enables a pull-up; this project's inverted transmitter
uses LOW to release the door bus. The replacement preloads LOW before switching
the GPIO mux, disconnects the UART signal and its output-enable, disables the
pull-up, and drives the inactive level. It runs at startup, before/after normal
communications teardown, through the ESP restart callback, and first in the
panic handler. The panic path uses only inlined register operations and the
DRAM pin-offset table; CI checks the built machine code is in IRAM with no
function calls or flash references.

The host UART regression test models the broken framework initialization and
verifies that RX echoes recover. It also checks stale TX latches, late UART
output after shutdown, repeated cleanup, and UART/RX/FIFO initialization
failures. These tests establish the source-level behavior; they do not measure
the electrical waveform on a GRGDO1. Test the affected wall panel, cold boots,
reboots, wall-panel detection, repeated open/close/stop/TTC operations, and
observe for uncommanded movement before declaring issue #19 resolved.

Gelidus also describes separate GDOLIB migration and TTC work in issue #19,
with v1.4.0 planned for that work. This upstream integration retains the current
non-GDOLIB backend (`USE_GDOLIB` is off); it does not implement that migration.
Version 1.4.0 is a proposed integration version and needs maintainer agreement
with that release plan. Issue #19 remains open pending device validation.

## Build and review

Install PlatformIO Core 6.2.0 with Python 3.12, then run:

```sh
git submodule update --init --recursive
python -m unittest discover -s tests -v
node --test tests/firmware-selection.test.cjs
c++ -std=c++17 -Wall -Wextra -Isrc tests/json_float.cpp -o /tmp/json-test
/tmp/json-test
c++ -std=c++17 -Wall -Wextra -Isrc tests/gdo_uart.cpp -o /tmp/uart-test
/tmp/uart-test
c++ -std=c++17 -Wall -Wextra tests/serial_console.cpp -o /tmp/console-test
/tmp/console-test
pio run -e ratgdo_esp32dev
pio run -e ratgdo_esp32dev_rev1
```

The Firmware workflow runs the checks and builds both revisions for pull
requests. It uploads the firmware, bootloader, partition table, OTA initialization
image, ELF, MD5 checksum, SHA256 checksums, dependency list, and source commit.

Before publishing, test both physical board revisions: Security+ door commands,
obstruction detection, HomeKit pairing and existing accessory identities,
VL53L1X detection and vehicle states, configured DHT22 readings, settings after
reboot, and OTA with password protection enabled. Build success does not verify
those device behaviors.

Rev2 uses GPIO26/25 for I2C; Rev1 uses GPIO3/1. The default GRGDO1 builds disable
upstream status outputs because their default pins overlap Rev2 I2C. They also
disable the ratgdo32-disco laser output because GPIO23 is the GRGDO1 obstruction
input. Custom hardware can provide separate `STATUS_DOOR_GPIO`,
`STATUS_OBST_GPIO`, and `LASER_GPIO` build flags after verifying its wiring.
Wall-panel disconnect is unavailable without a door status output.
Encoder support uses the existing open/close inputs and stays off by default.
The DHT22 accessory retains ID 10; the new stop switch uses ID 12.

## Publish

1. Merge the integration PR into Gelidus `main` after review and device testing.
2. Confirm `VERSION` and the Gelidus changelog entry match the intended tag.
3. Create and push `v1.4.0` on the reviewed commit in the Gelidus repository.

The tag triggers builds of that exact commit. Publishing runs only in
`GelidusResearch/homekit-ratgdo32`, after both builds pass. It checks artifact
hashes and commit identities, requires the release commit to be in the default
branch, and rejects a tag that differs from that branch's `VERSION`. It copies
the same binaries into `docs/firmware`, updates all three installer manifests,
pushes a normal documentation commit, and publishes the GitHub release with the
Gelidus changelog entry. Branch protection must allow that documentation commit.
No force push is used.

To rebuild an existing tag, use **Run workflow**, enter the tag, and leave
**publish** unchecked. Check **publish** only to retry publication of the current
release. A repeated run can update the same release without creating a new tag.
