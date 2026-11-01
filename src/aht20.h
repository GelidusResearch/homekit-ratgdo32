/****************************************************************************
 * RATGDO HomeKit
 * https://ratcloud.llc
 * https://github.com/PaulWieland/ratgdo
 *
 * Copyright (c) 2023-25 David A Kerr... https://github.com/dkerr64/
 * All Rights Reserved.
 * Licensed under terms of the GPL-3.0 License.
 *
 */
#pragma once

#if defined(USE_AHT20) && !defined(USE_DHT22)
#error "USE_AHT20 requires USE_DHT22 to also be defined (they share the same HomeKit temperature/humidity service)"
#endif

// aht20_detected, setup_aht20() and read_aht20() are always declared (and are
// harmless no-ops when USE_AHT20 is not defined) so callers never need to
// wrap references to them in #ifdef USE_AHT20.
extern bool aht20_detected;

// Probes for an AHT20 sensor (I2C address 0x38) on the user-selected AUX port
// and, if found, initializes it. Safe to call even when USE_AHT20 is not
// defined (does nothing). Must be called before setup_homekit() so the
// temperature/humidity accessory can be created if a sensor is found.
void setup_aht20();

// Reads the current temperature (Celsius) and relative humidity from the
// detected AHT20. Returns false (and leaves tempC/humidity untouched) if no
// AHT20 was detected or the read failed.
bool read_aht20(float &tempC, float &humidity);
