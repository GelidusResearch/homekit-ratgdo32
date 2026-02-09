# Change Log

**v3.x.x firmware is for ratgdo32 and ratgdo32-disco boards only**

All notable changes to `homekit-ratgdo32` will be documented in this file. This project tries to adhere to [Semantic Versioning](http://semver.org/).

## v1.2.0 (2026-02-09) - Gelidus Research

### What's Changed
* Merge upstream changes from `ratgdo/homekit-ratgdo32` up to v3.4.3.
* Add DHT22 Temperature and Humidity sensor support (GRGDO1 hardware specific).
* Updated `docs/manifest.json` and firmware naming convention for GRGDO1.
* Build verification with PlatformIO and dependency updates.

---

## v3.4.3 (2026-01-11)

### What's Changed

* Bugfix: (Sec+2.0 only) door not closing if ratgdo thinks it is still opening (rightly or wrongly). https://github.com/ratgdo/homekit-ratgdo32/issues/131
* Bugfix: Escape backslash and double quotes inside JSON strings.  https://github.com/ratgdo/homekit-ratgdo32/issues/134
* Bugfix/feature: (Sec+2.0 only) allow user to select sending TOGGLE command instead of CLOSE. https://github.com/ratgdo/homekit-ratgdo32/issues/131
* Feature: Hardwired Sec+ GPIO Controls Mirror Wall Panel, Optional TTC Bypass. https://github.com/ratgdo/homekit-ratgdo32/pull/136
* Feature: Publish ratgdo and door status over mDNS
* Other: Update settings page visuals to disable/enable options rather than hide/show.

### Known Issues

* ESP32 (ratgdo32) only... Some users may get an error during OTA upload that firmware is too large for the OTA partition. See [README.md](https://github.com/ratgdo/homekit-ratgdo32/blob/main/README.md#upgrade-failures) for work-around.
* Sec+ 1.0 doors with digital wall panel (e.g. 889LM) sometimes do not close after a time-to-close delay. Please watch your door to make sure it closes after TTC delay.
* Sec+ 1.0 doors with "0x37" digital wall panel (e.g. 398LM) not working.  We now detect but will not support them. Recommend replacing with 889LM panel.
* When creating automations in Apple Home the garage door may show only lock/unlock and not open/close as triggers. This is a bug in Apple Home. Workaround is to use the Eve App to create the automation, it will show both options.

## v3.4.2 (2025-12-13)

### What's Changed

* Feature: Query the state of emergency back up battery on boot and every 55 minutes (Sec+2.0 only).
* Feature: User can select whether to create HomeKit accessories for motion sensor and light switch (ratgdo32 only). With thanks to https://github.com/DaveLinger
* Bugfix: If firmware upload error detected before update begins, do not require a reboot.
* Other: Average vehicle distance over larger sample size (now 50) to smooth out spurious readings (ratgdo32-disco only).

## v3.4.1 (2025-11-22)

### What's Changed

* Bugfix: Vehicle departing motion sensor may not trigger correctly (ratgdo32-disco only)
* Bugfix: re-Announce ratgdo mDNS every two minutes, so that we remain visible on network (default TTL is 2 minutes)
* Bugfix: The home icon at top/right of the system logs page was not always returning to ratgdo main page https://github.com/ratgdo/homekit-ratgdo/issues/318
* Feature: Add a [webmanifest](https://developer.mozilla.org/en-US/docs/Web/Progressive_web_apps/Manifest) file and update all browser favorite icons for better visuals
* Feature: Add support for Captive Network Assistant (CNA) so that Apple and Android devices will automatically load WiFi provisioning page when connecting to ratgdo Soft Access Point (Soft AP mode)
* Feature: Add a warning and countdown timer to web page when Sec+2.0 doors have automatic door close (TTC) active
* Other: Display "Off" instead of "0" when settings sliders are set to zero seconds/minutes
* Other: Improved web page design for iPhone and iPad devices
* Other: Attempt to recover from out-of-sync Sec+2.0 rolling code https://github.com/ratgdo/homekit-ratgdo/issues/315
* Other: HomeSpan library updated to version 2.1.6 (ratgdo32 only)
* Other: Various log message cleanup to make debugging easier and reduce log clutter at default Info level

## v3.4.0 (2025-11-01)

### What's Changed

* Bugfix: User selected syslog facility not restored on startup. https://github.com/ratgdo/homekit-ratgdo32/issues/116
* Bugfix: Crash when HomeKit tries to open or close a dry contact door. https://github.com/ratgdo/homekit-ratgdo32/issues/117
* Bugfix: Sec+2.0 only, not handling packet transmit errors during initialization
* Feature: Sec+2.0 only, support garage door automatic close after selected delay, [SEE README](https://github.com/ratgdo/homekit-ratgdo32/blob/main/README.md#automatic-close)
* Feature: Add time-to-close countdown timer to web page
* Other: Allow user to disable triggering motion from Sec+2.0 wall panel motion sensors
* Other: Ratgdo-disco only, update vehicle presence algorithm for no vehicle present to match ESPhome
* Other: Adjust some Info-level log messages to Debug- or Error-level... reduces log clutter at default Info level

### Known Issues

* Sec+ 1.0 doors with digital wall panel (e.g. 889LM) sometimes do not close after a time-to-close delay. Please watch your door to make sure it closes after TTC delay.
* Sec+ 1.0 doors with "0x37" digital wall panel (e.g. 398LM) not working.  We now detect but will not support them.  Recommend replacing with 889LM panel.
* When creating automations in Apple Home the garage door may show only lock/unlock and not open/close as triggers.  This is a bug in Apple Home. Workaround is to use the Eve App to create the automation, it will show both options.